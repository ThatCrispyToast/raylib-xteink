# Validation report

Latest validation on 2026-10-27 on NixOS x86_64. Exact tool versions are locked in `flake.lock`; the embedded SDK is ESP-IDF 5.5.2 and the emulator is Espressif QEMU 9.2.2. The native display emulator is microreader revision `a4adfdb9a37b83bcb724cac3dc8990f335b49e8a`.

## Publication preparation

The public source and release packages were rechecked after path sanitization. The native suite passes 12/12 tests, sanitizers pass 8/8, and a fresh source-archive build passes 8/8. The added tests reject private data in firmware/source packages, unexpected release files, unsafe archive entries, and QEMU images; failed packaging preserves the previous release. CI is syntax-checked with actionlint and uses the pinned Nix environment. Its hosted run awaits publication.

Paper and Tetris release binaries retain their previously tested hashes. Diagnostics was rebuilt to remove local `__FILE__` paths and was not flashed again. Historical logs retain their measurements with local checkout paths redacted. See `artifacts/publication-report.json` and the publication test logs for scope, and [release preparation](releasing.md) for the public-file audit.

## Results

| Check | Result | Evidence |
|---|---|---|
| Native Release build | Pass | `build/x4_demo`, `build/libraylib_x4.a` |
| Native CTest suite | 11/11 pass | `artifacts/speed-native-tests.txt` |
| ASan + UBSan | 7/7 pass, no sanitizer findings | `artifacts/speed-sanitizer-tests.txt` |
| Hardware ESP32-C3 compile/link | Pass | `firmware/build/raylib_x4_firmware.bin`, `artifacts/speed-firmware-build.log` |
| Alternative application build | Pass | `build-minimal/raylib_x4_firmware.bin`, `artifacts/minimal-build.log` |
| ESP32-C3 QEMU boot and renderer | Pass | `artifacts/speed-qemu-1.log`, `speed-qemu-2.log`, `speed-qemu-3.log` |
| SDL screenshot inspection | Pass | `artifacts/demo.png`, `artifacts/puzzle.png`, `artifacts/status.png` |
| Physical X4 render/storage and no-clean soak | 276 checks pass, including 200 partial updates without a full clean | `artifacts/hardware/speed-regression/report.json` |
| Seven physical buttons and visual panel check (historical 1.0.0) | Pass | `artifacts/hardware/buttons-report.json` |
| Deep sleep and timer wake | 3/3 cycles pass | `artifacts/hardware/speed-sleep.log` |
| Physical Back/Power sleep and wake (historical 1.0.0) | Pass: owner confirmation and USB disconnect/reconnect | `artifacts/hardware/release-report.json` |
| Paper 1.2.0 on the X4 | Flash verified, Paper boots with SD mounted | `artifacts/hardware/qa-release-report.json` |
| Physical Paper 1.1.0 storage regression | 228 checks pass | `artifacts/hardware/qa-paper.log` |
| Independent user acceptance | Functional and wall-clock timing failures fixed and retested | `docs/qa-user-report.md` |
| Clean build from release source archive | 7/7 tests pass | `artifacts/hardware/speed-archive-check.log` |

The tests deliberately trigger a rejected window size and display failure. Their expected error messages do not indicate a failing test.

## Factory-waveform speed and async presentation (1.2.0)

The target reuses temperature-selected factory Mode 2 during active bursts, shuts the analog rails and clock down after two idle seconds, and reloads temperature/LUT selection after 30 seconds or a full clean. It never uploads a custom LUT or overrides voltages/temperature. The SPI clock remains 20 MHz. The driver now separates activation and completion so applications can process input and replace pending snapshots while the controller is BUSY. Pixel buffers remain 192,000 bytes. Full cleans, critical application transitions, and shutdown still wait for completion.

The final native suite passes 11/11 tests, including 24 Tetris journeys, nine Paper timing cases, renderer comparisons, coalescing, immutable in-flight buffers, deferred failures, and critical-screen guards. All seven ASan/UBSan tests pass. The controller model verifies cached/wake/expiry sequences, both RAM-bank arrangements, prohibited waveform/voltage/OTP writes, and one absolute 15-second deadline shared by polling and blocking completion. The two-millisecond BUSY assertion guard starts after activation transfer.

Repeated ESP32-C3 QEMU boots exposed an intermittent interrupt-watchdog panic in the SDK idle loop. The runner was missing `-icount 3`, which [Espressif's C3 emulator guide](https://github.com/espressif/esp-toolchain-docs/blob/main/qemu/esp32c3/README.md) requires for timer behavior. With that option, 20 consecutive boots pass with the interrupt watchdog enabled. The runner also preserves panic output and rejects a success marker after a panic. Evidence is in `artifacts/speed-qemu-report.json`; the reproduced failure trace is retained as `speed-qemu-before-icount.log`. These emulator timings are not physical performance measurements.

The physical diagnostic regression covers 276 checks, including 200 consecutive partial updates, scene hashes, unchanged-frame suppression, real SD round trips, texture paths, memory integrity, and close/reopen. Long USB diagnostic replies initially lost bytes through the SDK's character-at-a-time VFS path; batched bounded writes and a TX buffer large enough for one status line fix the transport. Three deep-sleep/timer-wake cycles pass with SD remounted and zero errors. The separate async fixture validates 200 submissions, final-frame pixels, coalescing, stable heap, a manual clean requested while BUSY, idle wake, and temperature expiry.

The actual Paper code is benchmarked synchronously for comparable action-through-BUSY timings, using isolated temporary SD saves. The actual Tetris code is exercised by 29 scripted hardware actions and a separate gravity/held-control benchmark that drains the last frame before reporting completion. Exact measurements, current firmware hashes, and limitations are in [performance](performance.md), `artifacts/hardware/speed-report.json`, and the per-application release reports.

The final Paper and Tetris binaries are separately flash-verified and booted with SD mounted; interactive Tetris is left on the reader. Factory-waveform-only operation was explicitly selected by the owner. No custom-waveform candidate was flashed. Human button feel, optical settling, and ghosting were not observed during this pass. Earlier owner observations below remain historical evidence.

## Tetris timing correction before 1.2.0 (2026-10-27)

The owner's pace complaint exposed a poor timing choice in the original example: its 1.6-second gravity interval excluded every display refresh, so actual rows took about 2.27 seconds. Held controls were also limited to one step per refresh. Normal mode now includes ordinary refresh time in gravity, starts at 0.8 seconds per row, and accelerates to a 0.12-second minimum. Landing grace still uses visible time so a piece cannot lock while the player is waiting for its image. Pause, full cleaning, and first presentation of a new piece freeze both clocks. Held controls catch up after blocked frames, with bounded batches and no replay after release.

On the physical X4, the same scripted benchmark measured:

| Scenario | Original | Corrected |
|---|---:|---:|
| Six automatic rows from a settled spawn | 13.640 s | 5.478 s |
| Steady level-one row interval | 2.271 s | 0.809 s |
| Hold Down from spawn to the floor | 12.738 s | 2.105 s |
| Hold Right from spawn to the right edge | 2.740 s | 1.390 s |

The benchmark uses the actual application and physical display. Its synthetic held-key source implements the old backend's 450/150 ms, one-repeat-per-poll behavior for the baseline and supplies held state to the new application repeat logic. Timings end after controller BUSY clears; these are not human button-to-photon or optical measurements. The stock waveform is unchanged, and a first-tap screen update still takes roughly 0.65–0.7 seconds. Raw logs and exact scope are in `artifacts/hardware/tetris-pace-report.json` and its before/after logs.

Gameplay tests cover pieces, bags, rotations, collisions, wall/floor adjustments, row clearing, scoring, levels, lock resets, top-out, independent falling/locking clocks, extreme elapsed intervals, and deterministic stress games. All 279,167 assertions pass. Another 257 retained/full redraw comparisons cover board movement, outlines, rotations, locks, line clears, score changes, and all pause-menu selections. Score changes now leave the unchanged next-piece previews alone, and menu moves redraw only selection outlines.

At that revision, twenty-three SDL journeys covered the application flows plus wall-clock gravity, held movement and release, held soft drop, and input during a two-second cleaning pause. Buffered Enter cannot drop an unseen next piece or confirm an unseen restart dialog. The complete 11-test native suite passes; after hardening the grounded-only clock path, all three Tetris tests and all seven sanitizer tests were rerun successfully. The targeted log is `artifacts/tetris-pace-tests.txt`.

`tests/tetris_device.c` separately exercises the actual application with 29 scripted actions, including pause, mode changes, help, restart, cleaning, and sleep. The final interactive image is flash-verified and booted with SD mounted. Its current hashes and measurements are in `artifacts/hardware/tetris-report.json`; the original example attestation is preserved in `tetris-before-pace-report.json`. No game files are written. Native screenshots were visually inspected; physical ghosting and human button interaction were not visually observed during this pass.

## Paper 1.1.2 refresh and region-update follow-up

Physical measurements improved cursor/menu actions to about 0.68 seconds and flips with SD saves to about 0.77 seconds. Changed-byte window transfers remove most SPI traffic for navigation; retained puzzle drawing avoids full-page redraws on flips. Paper no longer schedules a cleaning flash after navigation, idle, or entering Sleep. Startup/wake and Power still clean. The 20 MHz SPI clock and temperature-selected stock waveform remain unchanged.

The physical diagnostic run passed 276 checks, including 200 consecutive partial updates without a full clean, stable heap, no transfer errors, and scene hashes matching the earlier release. Its 9,876-byte minimum stack headroom includes the new transfer buffer. Framebuffer captures cannot measure physical ghosting.

The independent user pass compared 47 retained puzzle states against full-page redraws and verified their save records. The emulator completed 131 partial updates and six seconds idle without an extra full clean; explicit Power, Sleep, and relaunch policies also passed. The new controller model validates edge regions, chunked writes, changing windows, and both fixed and swapped RAM banks. Physical measurements and optical limits are recorded in [the performance report](performance.md); the exact production binary is attested in `artifacts/hardware/qa-release-report.json`. Production 1.1.2 was flash-verified and booted with the real SD mounted. The owner optical check is still unobserved, as recorded in `artifacts/hardware/region-optical-report.json`.

## Paper 1.1.1 performance follow-up

The owner's physical-latency report prompted direct MCU measurements. In 1.1.1, cursor/menu navigation took about 0.73 seconds, versus 2.45/2.08 seconds previously; most remaining time is the unchanged panel waveform. Drawing, conversion, input ordering, retained selection updates, and idle cleaning were fixed. Nine wall-clock input checks, 32 reference-renderer scenes, 33 retained/full-redraw comparisons, and a fresh 68-check physical regression pass. See [performance measurements and limits](performance.md).

Production 1.1.1 was flash-verified and booted with SD mounted: 69,920 bytes free heap, a 65,820-byte minimum, and a largest free block of 29,696 bytes. That binary is recorded in `artifacts/hardware/qa-1.1.1-report.json`; historical 1.1.0 evidence is preserved separately in `qa-1.1.0-report.json`.

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

Measured heap values from the original run (the current run is recorded in the speed QEMU logs):

| Point | Free heap, bytes |
|---|---:|
| Before `InitWindow` | 231,720 |
| Active with font, texture, and framebuffers | 114,932 |
| Lowest during initialization/rendering | 110,832 |
| After texture unload and `CloseWindow` | 231,524 |

The 192,000-byte buffer figure includes two static 48,000-byte panel frames and the dynamic 96,000-byte grayscale framebuffer. Static buffers are already excluded from the reported boot heap. Additional hardware peripheral allocations are not represented by the smoke HAL; the hardware executable is separately linked with the actual GPIO, ADC, SPI, FAT, and SDSPI drivers. The 16 KB main stack is configured in both images.

The smoke test is **not** evidence of electrical panel behavior or exact 160 MHz drawing speed. It is evidence that the RISC-V build executes its rendering and allocation paths successfully. The desktop emulator similarly models pixels and some display timing, not analog waveforms, temperature response, or every board revision.

## Original 1.0.0 physical device acceptance

An original ESP32-C3 revision 0.4 X4 with 16 MB flash and an inserted FAT microSD card was tested over USB. Before changing firmware, its entire 16 MB flash was read and verified against the device using esptool's digest comparison. The backup and checksum manifest are stored privately outside the project and are excluded from releases.

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
