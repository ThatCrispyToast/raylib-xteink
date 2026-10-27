# Version 1.2.0: responsive input with factory waveforms

The display no longer powers its analog rails and clock off after every partial update. It keeps them on during activity, turns them off after two idle seconds, and reuses the factory Mode 2 waveform with real temperature selection. Temperature/LUT selection reloads after 30 seconds and after a full clean. No waveform, voltage, temperature override, or SPI overclock is installed. The SPI clock remains 20 MHz.

## Measured costs

The physical Paper benchmark runs the same actions as 1.1.2, with isolated real SD saves. It deliberately uses synchronous presentation so each action measurement includes controller BUSY completion. These are small samples on one X4 at room temperature; they exclude mechanical debounce, waiting behind an earlier frame, and optical settling.

| Operation | 1.1.2 | 1.2.0 median |
|---|---:|---:|
| Cursor action through completion | 677 ms | 438 ms |
| Menu action through completion | 679 ms | 441 ms |
| Flip and save to SD | 773 ms | 536 ms |
| Packed conversion | 18.8 ms | 5.4 ms |
| Active partial waveform | 640 ms, including power cycle | 417 ms |
| First update after idle | — | 508 ms |
| Full cleaning update | 3.77 s | 3.77 s |

Raw measurements and exact binary hashes are in `artifacts/hardware/speed-performance.log` and `speed-report.json`. Historical results remain in [1.1.2 performance](performance-1.1.2.md) and [1.1.1 performance](performance-1.1.1.md). The converter assembles four output bytes together using a 4 KB flash lookup, reducing source reads and software arithmetic. Polling SPI avoids interrupt/queue overhead on short transactions. Opaque font spans also avoid redundant blending and per-pixel packed writes.

## Input processing and display completion

Paper and Tetris opt into nonblocking updates during ordinary interaction. The render task processes input while the controller is BUSY. Two 48 KB monochrome buffers hold an immutable in-flight snapshot and the latest queued snapshot; newer frames replace the pending snapshot rather than creating a backlog. Pixel-buffer memory remains 192,000 bytes. A queued snapshot starts only after the prior waveform finishes and both controller RAM banks are synchronized.

On the physical device, 200 rapid submissions averaged **7.18 ms**, with a **12.33 ms maximum** for the small moving-block workload. It coalesced 186 superseded snapshots, kept heap usage constant, and finished on the expected final frame. The actual Tetris app processed ordinary movement frames in about 40–50 ms including drawing/conversion. These are CPU/submission timings, not visible refresh rates. The panel still completes an active update in about 0.42 seconds; a press during a running update can wait for that update and the next one. This change improves control processing and prevents stale frames from accumulating; it does not turn the panel into a high-refresh display.

The actual Tetris benchmark also drains the last frame before recording each result:

| Scenario | Previous build (`fed5877`) | 1.2.0 |
|---|---:|---:|
| Hold Right from spawn to the edge | 1.390 s | 0.897 s |
| Hold Down from spawn to the floor | 2.105 s | 1.790 s |
| Six automatic rows from settled spawn | 5.478 s | 5.246 s |

The level-one gravity interval remains 0.8 seconds. The first held Right and Down frames were submitted after 39 ms and 49 ms respectively; the completion figures above include the final controller update. The run uses scripted keys and the actual game on the physical reader.

Critical states remain synchronous. New pieces, landings, confirmations, screen transitions, full cleans, and sleep finish before the next input poll. Tetris therefore retains its 0.55-second visible landing grace, and presses buffered while a dialog or new piece is appearing cannot confirm or hard-drop it accidentally. Directional releases stop repeats while the panel is busy. Explicit Power requests survive an in-flight completion. Neither example adds periodic cleaning flashes.

## Verification

Native tests cover snapshot ownership, coalescing, final-frame drains, refresh ordering, rendering equivalence, failure recovery, and lifecycle. The controller model covers both RAM-bank arrangements, idle shutdown/wake, temperature expiry, early BUSY assertion, timeout, and prohibited custom-LUT/voltage/OTP writes. The microreader adapter uses SDL timers so independently timed inputs also exercise BUSY during nonblocking updates. Tetris has 24 emulator journeys; Paper retains nine wall-clock input cases. ASan/UBSan and ESP32-C3 QEMU cover memory and renderer behavior.

The on-device stress fixture additionally checks 200 coalesced snapshots, manual clean ordering, stable heap, idle wake, and temperature expiry. The full diagnostic regression passes 276 checks, including real SD access and 200 consecutive completed partial refreshes with stable heap and no display errors. Three deep-sleep/timer-wake cycles and all 29 scripted Tetris actions also pass. See [validation](validation.md) and the matching hardware reports for the final counts.

No optical or button-to-photon measurement is claimed. This validation uses factory waveforms and does not include a new visual inspection. The experimental candidate was never flashed and is absent from the delivered target.

## Reproduce

Build `tests/paper_benchmark.c` with `examples/paper_state.c` as an extra source for synchronous action timings. Build `tests/display_speed.c` for the bounded async/idle/expiry fixture. Build `tests/tetris_pace.c` with `examples/tetris_game.c` to measure the actual game's gravity and held controls. Each uses the [firmware build procedure](hardware.md), with a separate absolute build directory. The fixtures leave USB awake for restoring the interactive application.

Research cross-checks: the [SSD1677 specification](https://files.waveshare.com/upload/2/2a/SSD1677_1.0.pdf), [GxEPD2 panel driver](https://github.com/ZinggJM/GxEPD2/blob/master/src/gdeq/GxEPD2_426_GDEQ0426T82.cpp), and [community SDK used by Biscuit](https://github.com/open-x4-epaper/community-sdk/blob/157d724d7a7389d49fe108e6dd5da2455a5340ba/libs/display/EInkDisplay/src/EInkDisplay.cpp) establish the factory refresh/power sequences. The [Bad Apple implementation](https://github.com/CidVonHighwind/xteink-bad-apple) demonstrates custom-waveform speed but explicitly warns of possible panel damage; that approach was declined and not used.
