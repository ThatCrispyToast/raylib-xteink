# Paper 1.1.1 physical performance

The owner's sluggishness report was correct. The earlier functional QA did not validate MCU drawing time or presses arriving while the application was blocked. On the original connected X4, ordinary puzzle updates took about 2.45 seconds and menu updates about 2.08 seconds; the periodic clean caused a roughly 5.15-second navigation pause.

## Measured improvement

These are application-action-to-controller-ready timings from the actual Paper code on the physical ESP32-C3, ending after panel BUSY clears. They are not camera measurements of pixel settling or physical-button-to-photon latency. Mechanical debounce, the idle poll interval, and waiting behind an already-running refresh add to input latency. Both runs used the same panel waveform; before navigation omitted app saves, while after navigation and two flips used a separate temporary directory on the real SD card.

| Operation | Before median | After median | After p95 / max | Samples per run |
|---|---:|---:|---:|---:|
| Move puzzle cursor | 2453.5 ms | 733.4 ms | 733.4 / 733.4 ms | 9 |
| Move menu selection | 2078.5 ms | 728.1 ms | 728.2 / 728.2 ms | 7 |
| Flip and synchronously save to SD | Not measured | 941.9 ms | 953.4 / 953.4 ms | 2 after |

The small sample counts make the upper percentile equal to the observed maximum. Treat these as a reproducible workload on one reader/card at room temperature, not a broad statistical latency guarantee. Raw timing lines are in `artifacts/hardware/performance-before.log` and `performance-after.log`; the parsed summary is `performance-report.json` in the same directory.

Cursor drawing fell from about 1,682 ms to 8.5 ms; menu-selection drawing fell from about 1,308 ms to 3.1 ms. Packed conversion fell from about 65 ms to 19 ms. Full-page drawing is also accelerated: the initial puzzle redraw fell from about 1,680 ms to roughly 172 ms. A flip currently redraws the page and saves before presentation; the two observed sync/action costs were about 34–57 ms in addition to rendering/presentation.

The remaining partial waveform takes about 706 ms, including transport and power-down. Full cleans still take about 3.77 seconds. No experimental waveform, voltage, or SPI-clock change was made. Paper now waits for three seconds of no navigation/held button before its periodic clean, after at least twelve partial updates. An explicit Power clean and initial wake still block on the full waveform. Input arriving during one is retained, within the 64-edge queue capacity, but cannot produce another visible frame until the current waveform finishes.

## Changes and acceptance

- Packed solid spans, constant-color blending, and the immutable default-font mask avoid repeated software floating-point work per pixel. General texture/filter/rotation paths remain available.
- Dithering and rotation assemble one output byte from eight source pixels. Randomized conversion is compared with the original definition.
- Cursor and menu movement redraw only their changed selections. All 25 cursor positions and eight menu selections matched full redraws byte-for-byte.
- The HAL retains ordered press edges. Paper applies the batch before drawing one final state. Buffered Enter cannot auto-confirm a destructive action; Back cancellation still works during confirmation presentation.
- Seven native CTest tests pass, including nine independently clocked input scenarios and 32 accelerated/reference raster scenes. Five sanitizer tests and the real ESP32-C3 QEMU smoke test pass.
- The updated physical diagnostic app passed 68 rendering, SD, refresh, allocation-failure, reopen, and heap checks. Its scene hashes match the original acceptance scenes. See `artifacts/hardware/performance-regression/report.json`.

The [independent acceptance report](qa-user-report.md#wall-clock-input-and-responsiveness-follow-up) records the baseline input failures and their retest. The final production binary was flash-verified and booted with the real SD mounted; its hash and heap measurements are in `artifacts/hardware/qa-1.1.1-report.json`. Saved games and the original full-flash backup were preserved.

## Reproduce

The benchmark includes the real Paper application, supplies scripted actions, and uses a uniquely created SD directory that it removes on completion. It stays awake after completion for restoring production firmware. In the declared firmware shell, after backing up the device and selecting the correct serial port:

```sh
idf.py -C firmware -B "$PWD/build-paper-perf" \
  -DX4_APP_SOURCE="$PWD/tests/paper_benchmark.c" \
  -DX4_APP_EXTRA_SOURCES="$PWD/examples/paper_state.c" \
  -DSDKCONFIG="$PWD/build-paper-perf/sdkconfig" build flash monitor
# Exit the monitor, then restore the interactive app:
idf.py -C firmware build flash
```

For input during blocked frames, run `python3 scripts/test-input-timing.py build --output /tmp/x4-input-timing` in the native shell. Its wall-clock injector runs independently of application polls; its selected 1.6-second pause is a controlled software test, not a physical waveform model.
