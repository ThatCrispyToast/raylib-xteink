# Testing

Run commands from the project root. The suites exercise different boundaries: SDL renders native application pixels, QEMU executes C3 RISC-V and S3 Xtensa code with peripheral stubs, simulated buses exercise controller protocols, and a physical X4 tests actual display/storage drivers. [Validation](validation.md) records results and their limits.

## Docker

Run the emulator, sanitizer, and QEMU suites without installing their toolchains:

```sh
docker build --target test --output type=local,dest=build-docker/tests .
```

The output contains two CTest XML reports and the QEMU serial log. Docker can cache successful tests; see [forcing a fresh test run](docker.md#headless-tests). The same targets run in [Docker CI](../.github/workflows/docker.yml), alongside Paper and Tetris builds. Physical-device checks remain separate.

## Native tests

Build the [desktop emulator](emulator.md), then run:

```sh
ctest --test-dir build --output-on-failure
```

This covers the renderer, input, refresh policy, SSD1677 bus model, game logic, retained drawing, save/reload behavior, emulator journeys, and packaging guards. The timed SDL tests use independently scheduled key events to exercise input during refresh pauses.

Repeat the native suite with `-DXTEINK_DEVICE=x3`, `x4-classic`, and `x4-pro` in separate build directories. The profile test checks SDL dimensions and all four visible corners, monitor identity, mouse/touch gating, tap latching, dragging, focus loss, and reopening. Core conversion and raster equivalence tests use the selected geometry, including X3's larger buffers. Paper and Tetris journeys run for each profile. The `x4_epd` test continues to validate only the original X4's SSD1677 protocol, regardless of desktop profile.

The `xteink_ssd` test exercises the S3 SSD1677 initialization/update variant. `xteink_epd` checks all four UltraChip variants, row order/padding, OLD-plane synchronization, LUT transfer lengths, windows, idle/sleep, bus failures, and BUSY deadlines. `xteink_peripherals` checks GT911 status/coordinates/acknowledgement/failures, fuel-gauge voltage formats, and the frontlight's combined PWM budget. Pro additionally runs touch-only example journeys and frontlight pixel readback. Configuration and packaging tests reject incompatible board/panel/MCU selections and prevent labeling new-device firmware as an original-X4 release.

These are software models based on documentation, not physical validation. All new-device hardware validation remains unchecked in the [compatibility checklist](../README.md#compatibility-and-planned-support).

To run only the portable tests without fetching microreader:

```sh
cmake -S . -B build-portable \
  -DX4_BUILD_EMULATOR=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-portable -j
ctest --test-dir build-portable --output-on-failure
```

## Sanitizers

Within the native development environment:

```sh
cmake -S . -B build-test -DX4_SANITIZE=ON \
  -DX4_BUILD_EMULATOR=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

AddressSanitizer and UndefinedBehaviorSanitizer cover the target, examples' portable logic, panel drivers, and peripheral protocol tests. CI runs both X4 and X3 geometry configurations.

## ESP32-C3 and S3 QEMU

Build and execute the smoke image with the pinned environments:

```sh
nix develop path:.#firmware -c idf.py -C firmware -B "$PWD/build-qemu" \
  -DX4_QEMU_SMOKE=ON -DSDKCONFIG="$PWD/build-qemu/sdkconfig" build
nix develop path:.#qemu -c python3 scripts/test-qemu.py
```

**Do not flash this image to a reader.** Its test HAL replaces the physical display, input, and SD peripherals. It validates RISC-V rendering, refresh accounting, allocations, and memory recovery. The runner uses the ESP32-C3 model's required instruction-counting option and rejects panic output.

Select `-DXTEINK_DEVICE=x3` in a separate directory to check X3's larger framebuffer. For either S3 device, use the S3 toolchain; for example:

```sh
nix develop path:.#firmware-s3 -c idf.py -C firmware -B "$PWD/build-qemu-pro" \
  -DXTEINK_DEVICE=x4-pro -DX4_QEMU_SMOKE=ON \
  -DSDKCONFIG="$PWD/build-qemu-pro/sdkconfig" build
nix develop path:.#qemu -c python3 scripts/test-qemu.py build-qemu-pro
```

The runner reads the chip and flash offsets from IDF's `flasher_args.json`, selects `qemu-system-riscv32` or `qemu-system-xtensa`, and requires an explicit success marker. The smoke app checks representative rendered pixels, framebuffer edges, heap integrity and release, and full/partial accounting. It uses the same renderer configuration as hardware builds. The S3 target uses compiler-provided division instead of rlsw's reciprocal assembly sequence, which caused an illegal-instruction trap in QEMU.

The C3 runner uses `-icount shift=3,sleep=off`: instruction counting plus virtual-time jumps to the next timer deadline while idle. This avoids an intermittent idle-task watchdog failure with the default host-clock sleep behavior; see [QEMU's virtual-time options](https://www.qemu.org/docs/master/system/invocation.html). The interrupt watchdog remains enabled, and the runner retains its 120-second deadline and panic rejection. The test configuration disables the task watchdog. These checks do not measure physical scheduling latency; production firmware keeps its watchdog configuration.

The firmware CI matrix compiles all nine board/controller combinations and boots a smoke image for each of the four profiles. QEMU cannot validate the readers' external panel, GT911, SD wiring, frontlight, battery, or sleep power consumption. The [experimental hardware guide](experimental-hardware.md) lists those limits.

## Physical device

First follow the [hardware guide](hardware.md) to verify compatibility and back up the reader. Use a test SD card or back it up: the diagnostic app writes temporary files for storage checks. Substitute the correct serial port below.

```sh
nix develop path:.#firmware
idf.py -C firmware -B "$PWD/build-hardware" \
  -DX4_APP_SOURCE="$PWD/examples/diagnostics.c" \
  -DSDKCONFIG="$PWD/build-hardware/sdkconfig" build
idf.py -C firmware -B "$PWD/build-hardware" -p /dev/ttyACM0 flash
python scripts/test-device.py --port /dev/ttyACM0
python scripts/device.py --port /dev/ttyACM0 status --capture artifacts/device-frame.png
```

The acceptance runner checks pixel hashes, SD writes and PNG loading, texture allocation failure, unchanged-frame suppression, cleaning policy, repeated refreshes, and close/reopen behavior. Captures are digital framebuffer readbacks, not photographs or measurements of ghosting.

Additional checks:

```sh
python scripts/test-sleep.py --port /dev/ttyACM0
python scripts/device.py --port /dev/ttyACM0 --listen 60
```

The sleep test verifies three deep-sleep/timer-wake cycles; the listener records button events. To monitor ordinary firmware without the diagnostic handshake, use `--raw --listen 60`.

The diagnostic command set includes `scene 0` through `scene 5`, `step`, `redraw`, `full`, `interval N`, `sdtest`, `assets`, `texturetest`, `oom`, `reopen`, `reboot`, `sleep`, and `sleep3` (a three-second timer wake). Reinstall Paper or Tetris using the [application-switching instructions](hardware.md#build-and-switch-to-tetris) when finished.

Application-specific fixtures are in `tests/paper_device.c`, `tests/tetris_device.c`, `tests/paper_benchmark.c`, `tests/tetris_pace.c`, and `tests/display_speed.c`. [Performance](performance.md#reproduce) describes the timing fixtures; [validation](validation.md) records their coverage.

## Before publishing

Run the public-file checks in [CONTRIBUTING.md](../CONTRIBUTING.md) and follow [release preparation](releasing.md). Do not attach full flash backups, SD contents, device serials, or unredacted local paths to public test reports.
