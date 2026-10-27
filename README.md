# raylib for Xteink X4

Build 2D applications for the **original ESP32-C3 Xteink X4** with raylib. Draw at the panel's native 480 × 800 resolution, run the same application in a desktop emulator, and deploy it with ESP-IDF.

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

On x86_64 Linux with Nix, run from the project root:

```sh
nix develop path:.
python3 scripts/fetch-emulator.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/x4_demo
```

Run `./build/x4_tetris` for Tetris. Use the arrow keys, Enter to confirm, Escape to go back, and `P` to clean the display. See [example controls](docs/examples.md) for each game.

The [emulator guide](docs/emulator.md) covers building without Nix, virtual SD storage, and screenshots.

## Use it on an X4

Follow the [hardware guide](docs/hardware.md) to build firmware, back up the reader, and install Paper or Tetris. **Installation replaces the device firmware and partition table.** This target supports the original, unlocked ESP32-C3 X4; X3, X4 Pro, and locked variants are unsupported.

On the tested reader, ordinary cursor/menu updates complete in about **0.44 seconds**, and full cleans take about **3.8 seconds**. Paper and Tetris process ordinary controls during refreshes and clean on startup/wake or Power. See [performance measurements](docs/performance.md) for scope and limits.

## Write an application

Start with [examples/minimal.c](examples/minimal.c): include `raylib.h`, open a 480 × 800 window, and use the usual drawing loop. The [application guide](docs/application.md) covers custom builds, input, storage, memory limits, and the X4 refresh API.

This is a 2D target for e-paper interfaces and games. Audio, 3D, shaders, and render textures are outside its scope.

## Documentation

| Guide | Contents |
|---|---|
| [Emulator](docs/emulator.md) | Build, run, virtual SD, screenshots |
| [Hardware](docs/hardware.md) | Back up, flash, switch applications, restore |
| [Application development](docs/application.md) | Build integration, API, input, memory |
| [Examples](docs/examples.md) | Paper and Tetris controls, saves, game rules |
| [Testing](docs/testing.md) | Native, sanitizer, QEMU, and device checks |
| [Design](docs/design.md) | Renderer, memory layout, display driver |

The target has been exercised in microreader, ESP32-C3 QEMU, and on a physical X4. [Validation results](docs/validation.md) distinguish automated checks from physical and optical observations.

## License

The target and firmware integration use the [zlib license](LICENSE). The desktop emulator adapter is **GPL-2.0-only**; it is not linked into the device firmware. Vendored code retains its upstream licenses. See [third-party notices](THIRD_PARTY.md).
