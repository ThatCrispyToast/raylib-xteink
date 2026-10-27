# Testing

Run commands from the project root. The suites exercise different boundaries: microreader renders native application pixels, QEMU executes RISC-V code with peripheral stubs, and a physical X4 tests the actual display/storage drivers. [Validation](validation.md) records results and their limits.

## Native tests

Build the [desktop emulator](emulator.md), then run:

```sh
ctest --test-dir build --output-on-failure
```

This covers the renderer, input, refresh policy, SSD1677 bus model, game logic, retained drawing, save/reload behavior, emulator journeys, and packaging guards. The timed SDL tests use independently scheduled key events to exercise input during refresh pauses.

To run only the portable tests without fetching microreader:

```sh
nix develop path:.
cmake -S . -B build-portable -G Ninja \
  -DX4_BUILD_EMULATOR=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-portable -j
ctest --test-dir build-portable --output-on-failure
```

## Sanitizers

Within the native development environment:

```sh
cmake -S . -B build-test -G Ninja -DX4_SANITIZE=ON \
  -DX4_BUILD_EMULATOR=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

AddressSanitizer and UndefinedBehaviorSanitizer cover the target, examples' portable logic, and SSD1677 protocol tests.

## ESP32-C3 QEMU

Build and execute the smoke image with the pinned environments:

```sh
nix develop path:.#firmware -c idf.py -C firmware -B "$PWD/build-qemu" \
  -DX4_QEMU_SMOKE=ON -DSDKCONFIG="$PWD/build-qemu/sdkconfig" build
nix develop path:.#qemu -c python3 scripts/test-qemu.py
```

**Do not flash this image to a reader.** Its test HAL replaces the physical display, input, and SD peripherals. It validates RISC-V rendering, refresh accounting, allocations, and memory recovery. The runner uses the ESP32-C3 model's required instruction-counting option and rejects panic output.

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
