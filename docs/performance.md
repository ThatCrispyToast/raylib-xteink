# Paper 1.1.2 physical performance and refresh policy

Paper now cleans the whole screen on startup/wake and on an explicit Power press. Moving around, leaving the device idle, and entering Sleep do not request a full clean. If ghosting becomes distracting, Power cleans the current page. The generic target still supports configurable periodic cleaning through `X4SetRefreshInterval()`; Paper opts out with `UINT_MAX`. Display-error recovery can also require a full refresh.

## Measured improvement

The same scripted actions ran through the real Paper application on the connected ESP32-C3, with saves in an isolated directory on the real SD card. Measurements end when the controller clears BUSY. They exclude mechanical debounce, time waiting in the input queue, and optical settling. The earlier [1.1.1 measurements](performance-1.1.1.md) retain the original slow baseline and methodology.

| Operation | 1.1.1 median | 1.1.2 median | Reduction | 1.1.2 maximum | Samples |
|---|---:|---:|---:|---:|---:|
| Move puzzle cursor | 733.4 ms | 677.0 ms | 7.7% | 677.3 ms | 9 |
| Move menu selection | 728.1 ms | 679.2 ms | 6.7% | 679.7 ms | 7 |
| Flip and synchronously save to SD | 941.9 ms | 772.7 ms | 18.0% | 784.2 ms | 2 |

These are small samples on one reader/card at room temperature, not latency guarantees. Raw lines are in `artifacts/hardware/region-performance.log`, with the parsed summary in `region-performance-report.json`. The benchmark reports drawing, conversion, SPI transfer, waveform duration, and changed-region size separately.

Cursor updates transfer a median 4,446 bytes across the controller RAM writes, versus 144,000 bytes previously: a 96.9% reduction. Menu selection transfers a median 18,312 bytes. Both controller banks are synchronized after each partial update so later updates can safely touch a different region. The documented `0xFF` control sequence includes power-down in the waveform activation. The SPI clock remains 20 MHz; voltage, temperature selection, and OTP waveforms are unchanged.

Puzzle flips redraw changed cells and the move/save footer instead of the whole page. Drawing fell from a median 171.6 ms to 42.2 ms for the two measured flips. Solving the puzzle also updates its heading. Page changes still repaint the complete page.

About 640 ms remains in the stock partial waveform. The first full update measured about 3.77 seconds including transfers. These changes therefore reduce software/transport overhead and remove unsolicited cleaning pauses; they do not make the e-paper panel instantaneous. Input still batches queued presses in order and presents the final state when the current waveform finishes.

## Verification and limits

- The controller bus model covers byte-aligned edge windows, changing update regions, chunked narrow-row transfers, unchanged frames, initial-full requirements, RAM-bank swapping, sleep, SPI failure, and BUSY timeout.
- The emulator exercises 131 partial updates followed by at least six seconds idle without an additional full clean; Power adds exactly one clean and preserves pixels; Sleep adds no clean and saves; relaunch restores the game with one initial clean.
- Retained puzzle drawing is compared pixel-for-pixel against a complete redraw, including moves, solved transitions, notices, counter widths, and storage states. See the [independent user report](qa-user-report.md).
- The physical regression passed 276 checks covering render scenes, unchanged-frame suppression, explicit/periodic cleaning, shared-SPI SD access, allocation failure, and reopening. The included 200-update run disabled automatic cleaning, with stable heap and no transfer errors, and left the moving-bar result visible for inspection. Logs are in `artifacts/hardware/region-regression/`.

Framebuffer hashes and controller BUSY timings cannot establish optical ghosting or pixel settling. The owner was asked for visual inspection to inspect the final 200-update screen. No observation was received before production Paper was restored; `artifacts/hardware/region-optical-report.json` records this outstanding check. No camera measurements or absence-of-ghosting claim is made.

## Reproduce

The benchmark includes `examples/paper.c`, supplies scripted actions, and removes only its own uniquely created SD directory on completion. In the declared firmware shell:

```sh
idf.py -C firmware -B "$PWD/build-paper-perf" \
  -DX4_APP_SOURCE="$PWD/tests/paper_benchmark.c" \
  -DX4_APP_EXTRA_SOURCES="$PWD/examples/paper_state.c" \
  -DSDKCONFIG="$PWD/build-paper-perf/sdkconfig" build flash monitor
```

After flashing the diagnostic application, run the physical regression and leave its final no-clean frame visible:

```sh
python scripts/test-device.py --port /dev/ttyACM0 \
  --output artifacts/hardware/region-regression --partial-soak 200
```

Restore interactive Paper with `idf.py -C firmware build flash`. Existing saved games and the original full-flash backup are preserved.
