# Desktop emulator

Run these commands from the project root. The emulator uses microreader's actual desktop display and runtime, including its panel crop, rotation, palette, and refresh delays. It runs the application natively; [QEMU testing](testing.md#esp32-c3-qemu) separately exercises the ESP32-C3 build.

## Build

On Linux, provide Git, CMake ≥3.20, a C99/C++17 compiler, Python ≥3.9, pkg-config, SDL2 ≥2.0.22 development files, and Make or Ninja. On Ubuntu 24.04/Debian 12 or newer:

```sh
sudo apt update
sudo apt install build-essential cmake git libsdl2-dev pkg-config python3
```

Then build from the project root:

```sh
python3 scripts/fetch-emulator.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The fetch script pins microreader to the revision in [THIRD_PARTY.md](../THIRD_PARTY.md) and refuses to replace a checkout at a different revision. CMake uses your default build tool; add `-G Ninja` when creating a new build directory if preferred.

On x86_64 Linux, Nix users can run `nix develop path:.` before the same commands to supply a pinned environment. `path:.` works in both a Git checkout and an extracted source archive. Docker provides [headless tests](docker.md#headless-tests); the interactive window runs natively. Other desktop operating systems have not been validated.

## Run

```sh
./build/x4_demo     # Paper
./build/x4_tetris   # Tetris
```

Run either application. Its window receives these keys:

| X4 button | Desktop key |
|---|---|
| Back | Escape or Backspace |
| Confirm | Enter |
| Left / Right | Left / Right arrows |
| Side Up / Down | Up / Down arrows |
| Power | P |

See [example controls](examples.md) for the actions in each game. Choosing Sleep exits the desktop app; on hardware, Power wakes the reader from deep sleep.

## Virtual SD card

By default, storage lives in `./sdcard`. Choose another directory or emulate an absent card with `X4_SD_ROOT`:

```sh
X4_SD_ROOT=./build-test-sd ./build/x4_demo
X4_SD_ROOT='' ./build/x4_demo
```

A missing directory is created. An unavailable root runs without storage; an existing read-only directory can exercise save-failure feedback. Paper displays its storage status and remains playable without persistence.

## Screenshots and automation

Capture Paper through the actual SDL output:

```sh
python3 scripts/capture.py artifacts/demo.png 0
```

Page `0` is the showcase, `1` the puzzle, and `2` device status. The script uses isolated temporary storage and an offscreen SDL session.

Automated tests use `SDL_PushEvent` and pixel readback before presentation. Offscreen sessions default to SDL software rendering, avoiding GPU requirements and offscreen OpenGL resize differences; `SDL_RENDER_DRIVER` can override this for diagnosis. `X4_EMULATOR_FAST=1` bypasses display delays; ordinary interactive runs retain the upstream 400 ms refresh delay. See [testing](testing.md) for timed input tests and retained/full-render comparisons.
