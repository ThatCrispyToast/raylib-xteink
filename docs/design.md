# Design and research

## Hardware findings

The target is the original Xteink X4: ESP32-C3, RV32IMC, 160 MHz, 400 KB total SRAM, no PSRAM, 16 MB flash. The SSD1677 drives an 800 × 480 GDEQ0426T82 panel, normally held in 480 × 800 portrait. Display and SD share SPI; six buttons use two resistor ladders, and Power is a digital GPIO.

Primary sources consulted:

- [Adafruit's X4 hardware and pinout guide](https://learn.adafruit.com/circuitpython-on-the-xteink-x4-ereader/pinouts): MCU, panel, storage, and board wiring.
- [microreader](https://github.com/CidVonHighwind/microreader/tree/a4adfdb9a37b83bcb724cac3dc8990f335b49e8a): `platforms/esp32/epd.h`, `input.h`, `sdcard.h`, desktop display/runtime, and panel geometry. The desktop crop is 786 visible columns, with ten hidden controller columns at one edge; some comments in that upstream snapshot still say 788. This target preserves the full controller resolution and reserves margins in its examples.
- [Crosspoint-Emulator](https://github.com/jonmooreai/Crosspoint-Emulator/tree/31f72be75408a0b66bc769dedacb9ba4f59ecbaf): independently confirms landscape bit packing, clockwise portrait rotation, and conventional keyboard mapping. Its HAL is coupled to Crosspoint, and this snapshot has no top-level license file. It was inspected, not incorporated.
- [raylib software renderer](https://github.com/raysan5/raylib/tree/debc1f6b1f86ca12119824f15974758d2d875394): immediate-mode OpenGL-compatible software rasterizer behind rlgl, with upstream shape, text, and texture modules.
- [raylib-esp32](https://github.com/jensroth-git/raylib-esp32): demonstrates the rlsw/ESP-IDF boundary but requires PSRAM and color/depth framebuffers. It was inspected, not incorporated.
- [Espressif SPI master documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32c3/api-reference/peripherals/spi_master.html): shared-bus arbitration, DMA alignment, and transaction constraints.
- [Espressif QEMU](https://github.com/espressif/qemu/tree/esp-develop-9.2.2-20250817): ESP32-C3 CPU/SoC execution for the renderer and heap smoke test.

| Signal | GPIO |
|---|---:|
| SPI SCLK / MOSI / MISO | 8 / 10 / 7 |
| EPD CS / DC / RESET / BUSY | 21 / 4 / 5 / 6 |
| SD CS | 12 |
| Front button ladder / side ladder | 1 / 2 |
| Power button, active low | 3 |

## Architecture

```text
application.c — normal raylib API
  rcore + rshapes + rtext + rtextures
    rlgl → rlsw → 2-bit grayscale framebuffer (bottom-up portrait)
      X4 presentation → dither + rotate → monochrome panel frame
        ESP-IDF HAL → tested SSD1677 command engine → SPI panel
        microreader HAL → upstream DesktopEmulatorDisplay → SDL
        test HAL → native assertions / ESP32-C3 QEMU
```

The platform implementation is included by `rcore.c`, like upstream backends, so it shares raylib's actual `CORE` state. It supplies lifecycle, fixed screen geometry, timer, keyboard, and buffer presentation. No parallel imitation of raylib's public drawing API is maintained.

## Memory and rendering decision

Native-size RGBA plus float depth would require 3,072,000 bytes. Even one 8-bit luminance framebuffer needs 384,000 bytes before FreeRTOS, stack, and font memory. Lower resolution with upscaling loses most of the value of an e-reader's text display. Tiled replay could preserve color but needs a retained command stream and complicated texture/lifetime semantics. A separate hand-written raylib subset would have much greater compatibility and maintenance risk.

The chosen approach extends rlsw's default framebuffer to packed 2-bit grayscale, with no depth allocation. Its texture formats remain unchanged. Pixel cursors use indices instead of byte addresses, so quads, triangles, lines, points, blending, clipping, and readback share the same packed accessors. Four luminance levels preserve useful shading and alpha composition; the presenter uses a stable 4 × 4 ordered dither to avoid temporal shimmer. Quantization happens after each blend, so this is not color-accurate compositing.

| Buffer | Bytes | Lifetime |
|---|---:|---|
| Packed grayscale framebuffer | 96,000 | raylib window |
| New monochrome panel frame | 48,000 | static |
| Last successful panel frame | 48,000 | static |
| Total pixel buffers | 192,000 | |

The remaining budget covers raylib state, texture metadata, default font, application textures, SPI buffers, FAT, FreeRTOS tasks, and stacks. Textures are limited to 32 live handles in this configuration. The QEMU smoke test measures actual allocations and checks heap integrity and lifecycle recovery. Hardware peripheral allocation costs are additional to that smoke test's test HAL.

Physical testing found two issues invisible to the desktop emulator: the initial SD chip-select assignment was wrong, and the mounted FAT driver fragmented memory enough for upstream's duplicate 32 KB font-atlas allocation to fail. SD uses GPIO12. The default atlas source and 224 glyph images are now generated into 51,488 bytes of immutable flash data by `scripts/generate-font.py`; the software renderer samples the atlas in place and only mutable font metadata allocates RAM. Texture updates copy the atlas to RAM before modification. CPU image text remains supported. The default glyph images must be copied before modification. Texture allocation errors propagate as zero texture IDs, and initialization fails cleanly if the font cannot be loaded. Nonzero application exits keep USB alive for recovery instead of immediately entering sleep. Texture readback is implemented as well; upstream rlsw silently stubbed this operation.

The default framebuffer's packed format is deliberately private. Textures remain standard raylib formats; render textures and depth are unavailable in this profile. No depth-enabled render pipeline can be selected. Unsupported modules are excluded from the link rather than pretended to work.

## Presentation and controller state

`EndDrawing()` produces a full 800 × 480 MSB-first panel buffer, 1 = white. A byte comparison against the last successfully presented frame avoids unnecessary waveforms. First presentation, an explicit cleaning request, recovery after an error, or the partial-update limit selects a full refresh. The platform default allows 12 partial updates between full cleans. Paper overrides that automatic limit and requests its periodic clean after at least 12 partial updates plus three seconds with no navigation or held button.

The command engine is independent of ESP-IDF and tested with a bus model. It configures the SSD1677's internal temperature sensor, booster, driver output, border, and 16-bit pixel-addressed RAM window. Rows are transferred with X increasing and Y decreasing. A full update writes both RAM planes, uses update control `0xF7`, and bypasses the old-image plane. A differential update writes the previous frame to old-image RAM and the new frame to BW RAM, then uses `0xFC`. The old-image RAM is explicitly reseeded after completion. Partial updates are followed by `0x83` power-off, leaving the panel image intact during idle. No custom voltage or grayscale waveforms are installed.

Every BUSY wait yields to FreeRTOS and has a 15-second timeout. SPI errors and timeouts propagate to the presenter; a failed frame never replaces the recorded baseline. The platform requests application exit on a display error, and close attempts panel sleep before releasing resources.

SPI writes acquire the shared bus while manually controlling EPD CS and DC. Transfers are capped at 4092 bytes; all bulk frame chunks are multiples of four. SD uses the ESP-IDF SDSPI and FAT VFS drivers on the same bus, with its own chip select. Applications use a single raylib/render task; they must serialize additional same-device SPI or file access if introducing their own threads.

## Input and power

ADC values are decoded against the established X4 ladder centers: front 3470/2655/1470/3, side 2205/3, tolerance 400, with readings above 3800 treated as released. The timer samples every 5 ms and requires 15 ms stability. The timer and main task share a protected current-state mask and press queue. This preserves brief taps during a blocking waveform, with up to 64 ordered press edges retained until the next poll. `GetKeyPressed()` drains these in order; the boolean pressed-key APIs still report whether each key appeared in the current batch. Queue overflow is counted. Paper applies the ordered batch before drawing its final visual state, and requires a fresh Enter after a destructive confirmation becomes visible.

The raylib backend maintains current/previous key state and repeat timing. Default Escape closes the window; applications can change it with `SetExitKey()`. Power is exposed as `KEY_P`, leaving its behavior to the application. The example uses it to clean the panel. The firmware wrapper enters deep sleep when the application returns and configures Power as a wake source.

`X4GetStatus()` exposes the raw ADC ladders, stable button mask, calibrated battery voltage (GPIO0 with a 2:1 divider), free/minimum heap, largest allocation, and SD mount state. Battery sampling runs once per second in the input timer. Unavailable ADC/voltage readings are -1; emulator heap counters are zero. `X4GetStats()` records frame conversion and actual blocking presentation duration separately.

## Integration boundaries

The hardware target is zlib-licensed and uses ESP-IDF directly. microreader's GPL display/runtime headers are isolated in the desktop emulator adapter, which is GPL-2.0-only. Other researched firmware projects were used to understand hardware behavior and compare architecture, not bundled into the firmware.

Upstream raylib changes are preserved as a reviewable patch: platform include and scheduler sleep, packed framebuffer allocation/access and scissored clear, unavailable depth/render targets, GL object-name pointer portability on RISC-V, and respect for an explicitly disabled partial busy wait. The tested upstream revision is fixed because rlsw is actively evolving.


## Paper save recovery

Paper 1.1.0 keeps two 112-byte records under the SD card's `raylib-x4` directory. Each has an eight-byte version marker, explicitly little-endian sequence/board/initial-board/move-count fields, cursor, bounded undo history, and an FNV-1a checksum. Loading checks the exact file size, checksum, field ranges, and history entries before selecting the newest valid sequence. Save writes the alternate slot, flushes, syncs, and closes it before advancing the in-memory sequence. A partial replacement leaves the previously valid slot available. Recovery is reported on screen; this does not guarantee survival of card-controller or filesystem-wide damage.

The old generic `paper.txt` is read only through a bounded, strict legacy parser and is never overwritten. Saves happen after a move, undo, confirmed restart/new puzzle, and choosing Sleep. Cursor movement alone does not write the card. The UI always exposes save failure or no-card status while permitting in-memory play. Random puzzles are constructed by applying legal toggles to the solved board, guaranteeing solvability; restart remembers each puzzle's initial board. The last 64 moves remain undoable across sleep and relaunch.


## MCU rendering performance

Packed uniform-color spans and constant-color quad blending quantize once per primitive/span instead of doing software floating-point conversion per pixel. The default font's immutable white/binary-alpha atlas has a nearest-neighbor path that calculates texture columns once and reuses them across rows. Rotated, filtered, mutable, and non-default textures continue through the general rasterizer. A reference build disables these fast paths; 32 varied scenes must match its packed bytes exactly. Dither/rotation assembles each panel byte directly and is tested against the original per-pixel conversion on randomized input.

Paper retains its framebuffer between frames. Cursor changes redraw the two affected cells; menu selection changes repaint only the old/new outlines. Full page/state changes still use the complete draw path. These optimizations preserve the panel waveform and previous-frame protocol. Physical timings and the independent wall-clock input tests are documented in `docs/performance.md`.
