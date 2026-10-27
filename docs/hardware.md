# Hardware installation

This target supports the **original, unlocked ESP32-C3 Xteink X4** with 16 MB flash. X3, X4 Pro, and locked variants are unsupported. Run commands from the project root.

**A first installation replaces the reader's firmware and partition table. Back up the full flash before installing.** Keep the backup outside the project.

## Build Paper

With Docker running:

```sh
docker build --output type=local,dest=build-docker/paper .
```

This exports a complete Paper image, individual firmware components, checksums, and a manifest. See [Docker builds](docker.md) for requirements, caching, custom applications, and tests. A [local ESP-IDF/Nix build](#local-esp-idf-build) is also available.

## Set up the host flashing tool

Use Python 3.10 or newer and an isolated environment. On Linux/macOS:

```sh
python3 -m venv .venv-flash
. .venv-flash/bin/activate
python -m pip install esptool==5.1.0
```

On Windows PowerShell, create the environment with `py -3 -m venv .venv-flash`, activate it with `.\.venv-flash\Scripts\Activate.ps1`, then run the same pip command. Reactivate this environment when opening a new terminal. Ubuntu/Debian users may need the `python3-venv` package. See [Espressif's installation guide](https://docs.espressif.com/projects/esptool/en/latest/esp32c3/installation.html) for platform details.

Flashing runs on the host, so Docker needs no USB access. The commands below use POSIX shell variables; in PowerShell substitute the actual port and backup path directly, for example `--port COM3`. Typical ports are `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem…` on macOS, and `COM3` on Windows.

## Back up and install

Choose the reader's serial port and an existing backup directory. Replace the example values before running the commands:

```sh
X4_PORT=/dev/ttyACM0
X4_BACKUP=/path/outside/project/x4-backup.bin

python -m esptool --chip esp32c3 --port "$X4_PORT" read-flash 0 0x1000000 "$X4_BACKUP"
python -m esptool --chip esp32c3 --port "$X4_PORT" verify-flash 0 "$X4_BACKUP"
```

After the backup verifies, install Paper and verify the written image:

```sh
python -m esptool --chip esp32c3 --port "$X4_PORT" write-flash 0 build-docker/paper/raylib-x4.bin
python -m esptool --chip esp32c3 --port "$X4_PORT" verify-flash 0 build-docker/paper/raylib-x4.bin
```

The reader resets after flashing. The firmware mounts an existing FAT microSD card at `/sd` and never formats it. Without a card, Paper runs without persistence. Power cleans the display; choosing Sleep leaves a wake instruction on the panel, and Power wakes the reader.

## Build and switch to Tetris

```sh
docker build --build-arg X4_APP_SOURCE=examples/tetris.c \
  --output type=local,dest=build-docker/tetris .
```

On a reader already running this target's bootloader and partition table, replace only the application:

```sh
python -m esptool --chip esp32c3 --port "$X4_PORT" write-flash 0x10000 build-docker/tetris/application.bin
```

For a first installation, use `build-docker/tetris/raylib-x4.bin` at address `0` instead, after backing up. To return to Paper on an existing installation:

```sh
python -m esptool --chip esp32c3 --port "$X4_PORT" write-flash 0x10000 build-docker/paper/application.bin
```

Both examples preserve Paper's SD saves. Tetris itself writes no game files. See [controls and rules](examples.md#tetris).

## Image offsets

| Image | Flash offset | Requirement |
|---|---:|---|
| `build-docker/paper/raylib-x4.bin` | `0` | Full Paper installation; replaces firmware and partition table |
| `build-docker/tetris/raylib-x4.bin` | `0` | Full Tetris installation; same replacement |
| Either directory's `application.bin` | `0x10000` | Existing target bootloader and partition table |

Never flash an application-only image at address `0`. Each directory includes `SHA256SUMS` and `manifest.json`. Docker exports carry `hardware_tested: false`; historical physical tests do not attest a newly built binary.

## Restore the original firmware

Use the verified full-flash backup at address **0**:

```sh
python -m esptool --chip esp32c3 --port "$X4_PORT" write-flash 0 "$X4_BACKUP"
python -m esptool --chip esp32c3 --port "$X4_PORT" verify-flash 0 "$X4_BACKUP"
```

## Local ESP-IDF build

With ESP-IDF 5.5.2 installed and its environment active:

```sh
idf.py -C firmware build
idf.py -C firmware size
```

On x86_64 Linux, `nix develop path:.#firmware` supplies that SDK and its RISC-V toolchain. For Tetris, keep a separate build directory:

```sh
idf.py -C firmware -B "$PWD/build-tetris" \
  -DX4_APP_SOURCE="$PWD/examples/tetris.c" \
  -DSDKCONFIG="$PWD/build-tetris/sdkconfig" build
```

After backing up, `idf.py -C firmware -p /dev/ttyACM0 flash monitor` installs and monitors Paper. Substitute the actual port. Application-only images are `firmware/build/raylib_x4_firmware.bin` and `build-tetris/raylib_x4_firmware.bin`, both at `0x10000` on an existing target installation.

`python3 scripts/package.py` packages these builds into `dist/`, including a merged Paper image at `dist/raylib-x4.bin` for offset `0`, optional Tetris/diagnostic images for `0x10000`, and the source archive. The `--hardware-tested` option requires evidence for the exact binary; use it only as described in [release preparation](releasing.md).

## Linux serial permissions

If serial access is denied, an administrator can grant temporary access to one reader:

```sh
sudo bash scripts/grant-x4-access.sh SERIAL USER
```

Substitute the serial shown in `/dev/serial/by-id/` and the account that will flash it. This rule targets one Espressif USB device and expires at reboot. Avoid publishing device serials or full flash backups.

For measured behavior and compatibility limits, see [validation](validation.md) and [performance](performance.md).
