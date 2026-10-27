# Validation report

Validated on 2026-10-26 on NixOS x86_64. Exact tool versions are locked in `flake.lock`; the embedded SDK is ESP-IDF 5.5.2 and the emulator is Espressif QEMU 9.2.2. The native display emulator is microreader revision `a4adfdb9a37b83bcb724cac3dc8990f335b49e8a`.

## Results

| Check | Result | Evidence |
|---|---|---|
| Native Release build | Pass | `build/x4_demo`, `build/libraylib_x4.a` |
| Native CTest suite | 7/7 pass | `artifacts/native-tests.txt` |
| ASan + UBSan | 5/5 pass, no sanitizer findings | `artifacts/sanitizer-tests.txt` |
| Hardware ESP32-C3 compile/link | Pass | `firmware/build/raylib_x4_firmware.bin`, `artifacts/firmware-build.log` |
| Alternative application build | Pass | `build-minimal/raylib_x4_firmware.bin`, `artifacts/minimal-build.log` |
| ESP32-C3 QEMU boot and renderer | Pass | `artifacts/qemu-test.log` |
| SDL screenshot inspection | Pass | `artifacts/demo.png`, `artifacts/puzzle.png`, `artifacts/status.png` |
| Physical X4 render/storage acceptance | 68 checks pass | `artifacts/hardware/final-acceptance/report.json` |
| Seven physical buttons and visual panel check | Pass | `artifacts/hardware/buttons-report.json` |
| Deep sleep and timer wake | 3/3 cycles pass | `artifacts/hardware/sleep-console.log` |
| Physical Back/Power sleep and wake | Pass: owner confirmation and USB disconnect/reconnect | `artifacts/hardware/release-report.json` |
| Paper 1.1.1 on the X4 | Flash verified, Paper boots with SD mounted | `artifacts/hardware/qa-release-report.json` |
| Physical Paper 1.1.0 storage regression | 228 checks pass | `artifacts/hardware/qa-paper.log` |
| Independent user acceptance | Functional and wall-clock timing failures fixed and retested | `docs/qa-user-report.md` |
| Clean build from release source archive | 5/5 tests pass | `artifacts/hardware/archive-check.log` |

The tests deliberately trigger a rejected window size and display failure. Their expected error messages do not indicate a failing test.

## Paper 1.1.1 performance follow-up

The owner's physical-latency report prompted direct MCU measurements. Cursor/menu navigation now takes about 0.73 seconds, versus 2.45/2.08 seconds previously; most remaining time is the unchanged panel waveform. Drawing, conversion, input ordering, retained selection updates, and idle cleaning were fixed. Nine wall-clock input checks, 32 reference-renderer scenes, 33 retained/full-redraw comparisons, and a fresh 68-check physical regression pass. See [performance measurements and limits](performance.md).

Production 1.1.1 was flash-verified and booted with SD mounted: 69,920 bytes free heap, a 65,820-byte minimum, and a largest free block of 29,696 bytes. The current exact binary is recorded in `artifacts/hardware/qa-release-report.json`; historical 1.1.0 evidence is preserved separately in `qa-1.1.0-report.json`.

## Paper 1.1.0 QA

The independent user pass reproduced a directory-read crash in the public file loader, accidental overwriting of unrelated `paper.txt` files, invisible save failures, absent replay/undo workflows, invalid-save and integer-overflow handling, and unavailable-storage emulation. All seven reported findings were fixed and independently retested. The full [user report](qa-user-report.md) preserves baseline reproductions and distinguishes emulator evidence from hardware evidence.

The expanded native suite tests bounded file reads (directories and files larger than `INT_MAX`), strict legacy import, unsigned move-count limits, 64-move undo retention, alternating checksummed saves, truncated-record recovery, no storage, conflicting storage paths, and missing/stale hardware attestations. Emulator journeys exercise solving, direct grid navigation, persistent undo, restart cancellation/confirmation, fresh puzzles, help, and sleep. An independent solver also completed a newly generated puzzle, and a second native app exercised the new multi-source build options.

On the physical X4, `tests/paper_device.c` ran 228 checks against the actual FAT SD card and raylib renderer. These include 70 save/reload cycles, persisted undo, deliberate truncation of the newest journal slot and recovery from the older slot, subsequent save repair, restart, safe directory rejection by both file-loading APIs, and heap integrity. It used a unique temporary directory, removed its test files, and preserved existing game data. The test reported 74,408 bytes free heap, a 57,592-byte minimum, and 10,868 bytes of main-task stack headroom. This tests interrupted-record handling, not arbitrary electrical failure or whole-filesystem corruption.

Production Paper 1.1.0 was then restored with flash digest verification and booted with SD mounted: 70,192 bytes free heap, a 66,092-byte minimum, and a largest free block of 30,720 bytes. Its exact binary hash is recorded in `artifacts/hardware/qa-1.1.0-report.json`. Packaging with `--hardware-tested` rejects a different binary. The new menu and sleep-screen flows were tested in the emulator; physical button and Power-wake observations below are historical 1.0.0 evidence, not a repeated owner check of the new UI.

To reproduce the isolated physical save regression, build and flash it as an alternate application, then restore the default Paper firmware:

```sh
idf.py -C firmware -B "$PWD/build-paper-qa" \
  -DX4_APP_SOURCE="$PWD/tests/paper_device.c" \
  -DX4_APP_EXTRA_SOURCES="$PWD/examples/paper_state.c" \
  -DSDKCONFIG="$PWD/build-paper-qa/sdkconfig" build flash
python scripts/device.py --raw --listen 90
```

The success marker is `PAPER QA PASS`; the test remains awake for reflashing. A full-flash backup and correct serial port are required as described in the README.

## Native and emulator coverage

- Native-size packed rendering: all four corners, out-of-screen geometry, scissor boundaries, scissored clear, thick/thin lines, triangles, circles, alpha composition, default text, textures, camera transforms, and repeated open/close.
- Presentation: first full refresh, unchanged-frame suppression, bounded partial refresh count, explicit clean, failed-frame baseline preservation, recovery, and fixed buffer footprint.
- Input: ladder decoding, simultaneous keys from different groups, press/down/release, repeat timing, a brief tap latched across polling, and quit behavior.
- SSD1677 bus-model tests: 16-bit RAM addresses, the previous and new images at activation, both refresh modes, baseline reseeding, analog power-off, sleep, SPI failure, and bounded BUSY timeout.
- Existing microreader emulator: application pixels pass through its actual display implementation and are read back from SDL. Assertions check the exact paper/ink RGB values, clockwise orientation, and the visible 480 × 786 crop.
- Application interaction: navigate pages, move a puzzle cursor, toggle a cell, request a full clean, save state, restart and reload, and solve a known board completely through key events. The initial board is generated from a solved state and is solvable.

Emulator automation uses `SDL_PushEvent`, the same event queue used by physical desktop keyboard input. `X4_EMULATOR_FAST=1` bypasses upstream delays during tests. Interactive runs retain the upstream 400 ms refresh delay. SDL's offscreen driver permits actual rendering and pixel readback without a desktop session.

## ESP32-C3 execution and memory

The QEMU image executes the same raylib core, software rasterizer, packed framebuffer, conversion, and refresh policy under real ESP-IDF/FreeRTOS on an emulated ESP32-C3. Its test HAL replaces display, input, and SD peripherals. It loads the default font and a 32 × 32 texture, draws three changing frames, checks the refresh counts and heap integrity, unloads resources, and checks memory recovery.

Measured heap values from that run:

| Point | Free heap, bytes |
|---|---:|
| Before `InitWindow` | 231,720 |
| Active with font, texture, and framebuffers | 114,932 |
| Lowest during initialization/rendering | 110,832 |
| After texture unload and `CloseWindow` | 231,524 |

The 192,000-byte buffer figure includes two static 48,000-byte panel frames and the dynamic 96,000-byte grayscale framebuffer. Static buffers are already excluded from the reported boot heap. Additional hardware peripheral allocations are not represented by the smoke HAL; the hardware executable is separately linked with the actual GPIO, ADC, SPI, FAT, and SDSPI drivers. The 16 KB main stack is configured in both images.

The smoke test is **not** evidence of electrical panel behavior or exact 160 MHz drawing speed. It is evidence that the RISC-V build executes its rendering and allocation paths successfully. The desktop emulator similarly models pixels and some display timing, not analog waveforms, temperature response, or every board revision.

## Original 1.0.0 physical device acceptance

An original ESP32-C3 revision 0.4 X4 with 16 MB flash and an inserted FAT microSD card was tested over USB. Before changing firmware, its entire 16 MB flash was read and verified against the device using esptool's digest comparison. The backup and checksum manifest remain local under `artifacts/hardware/backups/` and are excluded from releases.

The final acceptance runner passed 68 checks, including:

- All-black and all-white buffer hashes, portrait orientation, readable default text, clipping, alpha, camera transforms, and rotated textures.
- Checksummed 48 KB framebuffer captures, unchanged-frame suppression, explicit full cleans, periodic cleans, a 40-step moving-bar soak, and three close/reopen cycles.
- Repeated 4 KB write/sync/read/delete round trips on SD while sharing the display SPI bus; raylib PNG loading from a temporary SD asset and checking every source pixel.
- Texture region updates and texture readback; a rejected 1 MB texture allocation and full-screen RGBA readback without a crash or leak.
- Heap integrity and stack headroom after every reported frame; no steady-state heap loss during the soak or successive reopens.

Measured on this board:

| Measurement | Result |
|---|---:|
| Full panel presentation | about 3.77 s |
| Partial panel presentation, including power-off | about 0.706 s |
| Packed frame conversion | about 65–70 ms |
| Active diagnostic free heap after asset tests | 67,212 bytes |
| Lowest free heap in the acceptance workload | 58,924 bytes |
| Largest free block after reopen | 28,672 bytes |
| Minimum reported main-task stack headroom | 9,908 bytes |

The owner confirmed readable upright text and distinct black/dark-gray/light-gray/white swatches. White intentionally matches the background. All seven physical buttons were pressed and released in order; 14 transitions were captured, and raylib's observed press mask reached `127`. Measured front ADC centers were 3450, 2633, 1456, and 4; side centers were 2187 and 4, within the configured tolerances.

Three timed deep-sleep cycles also passed: the USB device disappeared during sleep, then returned with reset reason 8 (deep sleep) and wake reason 4 (timer). Every wake remounted SD, rendered the same orientation frame, and retained healthy heap state.

The exact 1.0.0 packaged production image was then flashed with verification. Paper booted with SD mounted, 70,176 bytes free heap, a 66,076-byte minimum, and a largest free block of 30,720 bytes. The owner subsequently completed the Back → wait three seconds → Power test and confirmed that the display refreshed and returned to Paper. The host kernel recorded the X4 disconnecting at 20:41:04 and reconnecting at 20:41:07 on 2026-10-26 (local test time). The serial monitor had already timed out, so this final check is supported by the owner report and USB events rather than a captured MCU wake-reason code.

Hardware testing uncovered and fixed the SD chip-select assignment (GPIO12), default-font allocation failures caused by fragmentation, and USB-client reset pulses on reconnect. Font pixels now stay in flash, texture uploads report failure, and an unsuccessful application exit leaves USB available for recovery. The diagnostic transport uses buffered USB writes with bounded waits and validates complete framebuffer transfers.

Framebuffer PNGs are digital readbacks, not photographs or measurements of panel ghosting. These results cover one original X4 and one FAT card at room temperature. Battery voltage is ADC-calibrated but was not checked against a voltmeter. Idle/deep-sleep current, temperature extremes, other hardware revisions, and exhaustive SD-card compatibility have not been measured. No locked-device or X3/X4 Pro support is claimed.
