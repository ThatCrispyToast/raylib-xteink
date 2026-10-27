# Hardware installation

This target supports the **original, unlocked ESP32-C3 Xteink X4** with 16 MB flash. X3, X4 Pro, and locked variants are unsupported. Run commands from the project root.

**A first installation replaces the reader's firmware and partition table. Back up the full flash before installing.** Keep the backup outside the project.

## Build Paper

The x86_64 Linux Nix environment pins ESP-IDF 5.5.2 and the RISC-V toolchain:

```sh
nix develop path:.#firmware
idf.py -C firmware build
idf.py -C firmware size
```

This produces `firmware/build/raylib_x4_firmware.bin`, a bootloader, and a partition table. Paper is the default application.

## Back up and install

Choose the reader's serial port and an existing backup directory. Replace the example values before running the commands:

```sh
X4_PORT=/dev/ttyACM0
X4_BACKUP=/path/outside/project/x4-backup.bin

python -m esptool --chip esp32c3 --port "$X4_PORT" read_flash 0 0x1000000 "$X4_BACKUP"
python -m esptool --chip esp32c3 --port "$X4_PORT" verify_flash 0 "$X4_BACKUP"
```

After the backup verifies, install and monitor Paper:

```sh
idf.py -C firmware -p "$X4_PORT" flash monitor
```

The firmware mounts an existing FAT microSD card at `/sd` and never formats it. Without a card, Paper runs without persistence. Power cleans the display; choosing Sleep leaves a wake instruction on the panel, and Power wakes the reader.

## Build and switch to Tetris

Use a separate build directory so Paper remains available:

```sh
idf.py -C firmware -B "$PWD/build-tetris" \
  -DX4_APP_SOURCE="$PWD/examples/tetris.c" \
  -DSDKCONFIG="$PWD/build-tetris/sdkconfig" build
```

On a reader already running this target's bootloader and partition table, replace only the application:

```sh
python -m esptool --chip esp32c3 --port "$X4_PORT" write_flash 0x10000 build-tetris/raylib_x4_firmware.bin
```

To return to Paper:

```sh
python -m esptool --chip esp32c3 --port "$X4_PORT" write_flash 0x10000 firmware/build/raylib_x4_firmware.bin
```

Both examples preserve Paper's SD saves. Tetris itself writes no game files. See [controls and rules](examples.md#tetris).

## Packaged firmware

After building, create `dist/` with:

```sh
python3 scripts/package.py
```

The merged image contains Paper, the bootloader, and the partition table. Flash it at address **0**:

```sh
python -m esptool --chip esp32c3 --port "$X4_PORT" write_flash 0 dist/raylib-x4.bin
```

| Image | Flash offset | Requirement |
|---|---:|---|
| `dist/raylib-x4.bin` | `0` | Full installation; replaces firmware and partition table |
| `dist/application.bin` | `0x10000` | Existing target bootloader and partition table |
| `dist/tetris.bin` | `0x10000` | Same; included when Tetris has been built |
| `dist/diagnostics.bin` | `0x10000` | Same; optional [test application](testing.md#physical-device) |

Never flash an application-only image at address `0`. Packaging includes checksums and a manifest. The `--hardware-tested` option requires evidence for the exact binary; use it only as described in [release preparation](releasing.md).

## Restore the original firmware

Use the verified full-flash backup at address **0**:

```sh
python -m esptool --chip esp32c3 --port "$X4_PORT" write_flash 0 "$X4_BACKUP"
```

## Linux serial permissions

If serial access is denied, an administrator can grant temporary access to one reader:

```sh
sudo bash scripts/grant-x4-access.sh SERIAL USER
```

Substitute the serial shown in `/dev/serial/by-id/` and the account that will flash it. This rule targets one Espressif USB device and expires at reboot. Avoid publishing device serials or full flash backups.

For measured behavior and compatibility limits, see [validation](validation.md) and [performance](performance.md).
