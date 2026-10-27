# raylib for Xteink X4

Build raylib 2D applications for the **original, unlocked ESP32-C3 Xteink X4**. Draw at 480 × 800 and run the same code in a desktop emulator.

[Documentation](docs/README.md) · [Hardware setup](docs/hardware.md) · [Contributing](CONTRIBUTING.md) · [Changelog](CHANGELOG.md)

<p>
  <img src="artifacts/puzzle.png" width="240" alt="Paper's Lights Out puzzle in the desktop emulator">
  <img src="artifacts/tetris.png" width="240" alt="Tetris in the desktop emulator">
</p>

*Paper and Tetris running in the microreader desktop emulator.*

- **Familiar raylib APIs:** shapes, text, textures, alpha blending, 2D cameras, input, and files.
- **Fits the X4:** four grayscale levels, dithered to monochrome, with 192 KB of pixel buffers and no PSRAM requirement.
- **Built for e-paper:** changed-region updates, optional asynchronous presentation, and factory waveforms.
- **Two playable examples:** Paper, a Lights Out puzzle with persistent saves and undo; and Tetris, with Normal and Practice modes.

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

The target and firmware integration use the [zlib license](LICENSE). The desktop emulator adapter is **GPL-2.0-only**; it is not linked into the device firmware. Vendored code retains its upstream licenses. See [third-party notices](THIRD_PARTY.md).
