# raylib for XTEINK

Build raylib 2D applications for **X4, X3, X4 Classic, and X4 Pro**. All four have desktop emulators and ESP-IDF firmware targets. The original unlocked X4 has prior hardware validation; the new ports are **experimental and hardware-untested**.

[Documentation](docs/README.md) · [Hardware setup](docs/hardware.md) · [Contributing](CONTRIBUTING.md) · [Changelog](CHANGELOG.md)

<p>
  <img src="artifacts/puzzle.png" width="240" alt="Paper's Lights Out puzzle in the desktop emulator">
  <img src="artifacts/tetris.png" width="240" alt="Tetris in the desktop emulator">
</p>

*Paper and Tetris running in the microreader desktop emulator.*

- **Familiar raylib APIs:** shapes, text, textures, alpha blending, 2D cameras, input, and files.
- **Fits the X4:** four grayscale levels, dithered to monochrome, with 192 KB of pixel buffers and no PSRAM requirement.
- **Built for e-paper:** monochrome panel drivers, optional asynchronous presentation, and SSD1677 changed-region updates.
- **Two playable examples:** Paper, a Lights Out puzzle with persistent saves and undo; and Tetris, with Normal and Practice modes.

## Compatibility and planned support

| Product | Desktop | Hardware |
|---|---|---|
| Original X4 | Supported (default) | Previously tested on unlocked X4 |
| X3, X4 Classic / V2, X4 Pro | Supported | **Experimental; untested** |
| S4 (Android) | Planned | No target |

Remaining work:

- [ ] Validate every new model and panel variant on hardware.
- [ ] Panel auto-detection, RTC/IMU APIs, calibrated battery percentage.
- [ ] S4 Android build, emulator integration, and device validation.

See [device profiles](docs/emulator.md#device-profiles), [firmware builds and limits](docs/experimental-hardware.md), and [validation results](docs/validation.md).

## Quickstart

From the project root, build Paper firmware with Docker:

```sh
docker build --output type=local,dest=build-docker/paper .
```

Firmware and checksums are exported to `build-docker/paper/`. See the [Docker guide](docs/docker.md) for Tetris, custom apps, and headless tests.

Follow the [hardware guide](docs/hardware.md) to back up and flash your X4. **Installation replaces its firmware and partition table.**

On the tested reader, cursor/menu updates take about **0.44 seconds** and full cleans **3.8 seconds**. See [measurements and limits](docs/performance.md).

## Desktop emulator

Choose your setup and run from the project root:

<details>
<summary>Ubuntu / Debian</summary>

Tested on Ubuntu 24.04/26.04 and Debian 12/13:

```sh
sudo apt update
sudo apt install build-essential ca-certificates cmake git libsdl2-dev pkg-config python3
python3 scripts/fetch-emulator.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/x4_demo
```

</details>

<details>
<summary>Other Linux distributions (CMake)</summary>

Install the [build dependencies](docs/emulator.md#build), then:

```sh
python3 scripts/fetch-emulator.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/x4_demo
```

</details>

<details>
<summary>Nix (x86_64 Linux)</summary>

Nix supplies the pinned build dependencies:

```sh
nix develop path:.
python3 scripts/fetch-emulator.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/x4_demo
```

</details>

Run `./build/x4_tetris` for Tetris. Controls: arrow keys, Enter, Escape, and `P` to clean the display. See [game controls](docs/examples.md).

Select another device at configure time (the default is `x4`):

```sh
cmake -S . -B build-x3 -DXTEINK_DEVICE=x3 -DCMAKE_BUILD_TYPE=Release
cmake --build build-x3 --parallel
./build-x3/x4_demo
```

Use `x3`, `x4-classic`, or `x4-pro`; separate build directories let you compare devices. Existing executable names and the `raylib_x4.h` API remain compatible.

The [emulator guide](docs/emulator.md) covers setup, virtual SD storage, and screenshots.

## Write an application

Start with [minimal.c](examples/minimal.c). The [application guide](docs/application.md) covers custom builds, input, storage, memory limits, and refresh control.

This is a 2D target for e-paper interfaces and games. Audio, 3D, shaders, and render textures are outside its scope.

## Documentation

| Guide | Contents |
|---|---|
| [Emulator](docs/emulator.md) | Build, run, virtual SD, screenshots |
| [Docker](docs/docker.md) | Firmware builds, caching, headless tests |
| [Hardware](docs/hardware.md) | Back up, flash, switch applications, restore |
| [Application development](docs/application.md) | Build integration, API, input, memory |
| [Examples](docs/examples.md) | Paper and Tetris controls, saves, game rules |
| [Testing](docs/testing.md) | Native, sanitizer, QEMU, and device checks |
| [Design](docs/design.md) | Renderer, memory layout, display driver |

See [validation results](docs/validation.md) for emulator, QEMU, and physical-device coverage.

## License

The target and firmware integration use the [zlib license](LICENSE); adapted UltraChip drivers and LUTs retain FreeInk's MIT license. The desktop emulator adapter is **GPL-2.0-only**; it is not linked into the device firmware. Vendored code retains its upstream licenses. See [third-party notices](THIRD_PARTY.md).
