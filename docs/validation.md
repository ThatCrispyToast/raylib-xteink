# Validation report

Validated on 2026-10-26 on NixOS x86_64. Exact tool versions are locked in `flake.lock`; the embedded SDK is ESP-IDF 5.5.2 and the emulator is Espressif QEMU 9.2.2. The native display emulator is microreader revision `a4adfdb9a37b83bcb724cac3dc8990f335b49e8a`.

## Results

| Check | Result | Evidence |
|---|---|---|
| Native Release build | Pass | `build/x4_demo`, `build/libraylib_x4.a` |
| Native CTest suite | 3/3 pass | `artifacts/native-tests.txt` |
| ASan + UBSan | 2/2 pass, no sanitizer findings | `artifacts/sanitizer-tests.txt` |
| Hardware ESP32-C3 compile/link | Pass | `firmware/build/raylib_x4_firmware.bin`, `artifacts/firmware-build.log` |
| Alternative application build | Pass | `build-minimal/raylib_x4_firmware.bin`, `artifacts/minimal-build.log` |
| ESP32-C3 QEMU boot and renderer | Pass | `artifacts/qemu-test.log` |
| SDL screenshot inspection | Pass | `artifacts/demo.png`, `artifacts/puzzle.png`, `artifacts/status.png` |
| Physical X4 render/storage acceptance | 68 checks pass | `artifacts/hardware/final-acceptance/report.json` |
| Seven physical buttons and visual panel check | Pass | `artifacts/hardware/buttons-report.json` |
| Deep sleep and timer wake | 3/3 cycles pass | `artifacts/hardware/sleep-console.log` |
| Physical Back/Power sleep and wake | Pass: owner confirmation and USB disconnect/reconnect | `artifacts/hardware/release-report.json` |
| Packaged production image on the X4 | Flash verified, Paper boots with SD mounted | `artifacts/hardware/release-report.json` |
| Clean build from release source archive | 2/2 tests pass | `artifacts/hardware/archive-check.log` |

The tests deliberately trigger a rejected window size and display failure. Their expected error messages do not indicate a failing test.

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
| Before `InitWindow` | 231,912 |
| Active with font, texture, and framebuffers | 114,980 |
| Lowest during initialization/rendering | 110,880 |
| After texture unload and `CloseWindow` | 231,708 |

The 192,000-byte buffer figure includes two static 48,000-byte panel frames and the dynamic 96,000-byte grayscale framebuffer. Static buffers are already excluded from the reported boot heap. Additional hardware peripheral allocations are not represented by the smoke HAL; the hardware executable is separately linked with the actual GPIO, ADC, SPI, FAT, and SDSPI drivers. The 16 KB main stack is configured in both images.

The smoke test is **not** evidence of electrical panel behavior or exact 160 MHz drawing speed. It is evidence that the RISC-V build executes its rendering and allocation paths successfully. The desktop emulator similarly models pixels and some display timing, not analog waveforms, temperature response, or every board revision.

## Physical device acceptance

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

The exact packaged production image was then flashed with verification. Paper booted with SD mounted, 70,176 bytes free heap, a 66,076-byte minimum, and a largest free block of 30,720 bytes. The owner subsequently completed the Back → wait three seconds → Power test and confirmed that the display refreshed and returned to Paper. The host kernel recorded the X4 disconnecting at 20:41:04 and reconnecting at 20:41:07 on 2026-10-26 (local test time). The serial monitor had already timed out, so this final check is supported by the owner report and USB events rather than a captured MCU wake-reason code.

Hardware testing uncovered and fixed the SD chip-select assignment (GPIO12), default-font allocation failures caused by fragmentation, and USB-client reset pulses on reconnect. Font pixels now stay in flash, texture uploads report failure, and an unsuccessful application exit leaves USB available for recovery. The diagnostic transport uses buffered USB writes with bounded waits and validates complete framebuffer transfers.

Framebuffer PNGs are digital readbacks, not photographs or measurements of panel ghosting. These results cover one original X4 and one FAT card at room temperature. Battery voltage is ADC-calibrated but was not checked against a voltmeter. Idle/deep-sleep current, temperature extremes, other hardware revisions, and exhaustive SD-card compatibility have not been measured. No locked-device or X3/X4 Pro support is claimed.
