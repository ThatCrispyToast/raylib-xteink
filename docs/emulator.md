# Desktop emulator

Run these commands from the project root. The original X4 profile uses microreader's desktop display and runtime, including its panel crop, rotation, palette, and refresh delays. Other profiles use a native SDL display with their own geometry. All run application code natively; [QEMU testing](testing.md#esp32-c3-and-s3-qemu) separately exercises all four embedded profiles on C3/S3 CPUs.

## Build

On Linux, provide Git, CMake ≥3.20, a C99/C++17 compiler, Python ≥3.9, pkg-config, SDL2 ≥2.0.22 development files, and Make or Ninja. On Ubuntu 24.04/26.04 or Debian 12/13:

```sh
sudo apt update
sudo apt install build-essential ca-certificates cmake git libsdl2-dev pkg-config python3
```

Then build from the project root:

```sh
python3 scripts/fetch-emulator.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

These commands pass the full native suite in clean Ubuntu and Debian containers; see [tested versions](validation.md#ubuntu-and-debian-quickstarts).

The fetch script pins microreader to the revision in [THIRD_PARTY.md](../THIRD_PARTY.md) and refuses to replace a checkout at a different revision. CMake uses your default build tool; add `-G Ninja` when creating a new build directory if preferred.

On x86_64 Linux, Nix users can run `nix develop path:.` before the same commands to supply a pinned environment. `path:.` works in both a Git checkout and an extracted source archive. Docker provides [headless tests](docker.md#headless-tests); the interactive window runs natively. Other desktop operating systems have not been validated.

## Device profiles

Choose `-DXTEINK_DEVICE=x4`, `x3`, `x4-classic`, or `x4-pro` at configure time. Selection applies to the library, emulator, application, and portable tests. Unknown names fail configuration. The default remains `x4`, and executable names remain `x4_demo` and `x4_tetris` for all profiles.

```sh
cmake -S . -B build-x4-pro -DXTEINK_DEVICE=x4-pro -DCMAKE_BUILD_TYPE=Release
cmake --build build-x4-pro --parallel
./build-x4-pro/x4_demo
ctest --test-dir build-x4-pro --output-on-failure
```

| Profile | Application size | SDL visible area | Pixel buffers |
|---|---|---|---|
| `x4` | 480 × 800 | 480 × 786; first 10 and last 4 rows cropped by microreader | 192,000 bytes |
| `x3` | 528 × 792 | Full framebuffer | 209,088 bytes |
| `x4-classic` | 480 × 800 | Full framebuffer | 192,000 bytes |
| `x4-pro` | 480 × 800 | Full framebuffer | 192,000 bytes |

Only `x4` requires the microreader checkout. New profiles preview the full addressable framebuffer because their physical bezel crop has not been verified. All profiles use the existing four-level grayscale renderer, monochrome dithering, and simulated 400 ms refresh delay. This delay is a UI simulation, not a measurement for the new devices. Pro previews warm/cool frontlight colors; profiles do not model CPU, GPIO, electrical panel behavior, sensors, battery, or wireless behavior.

Include `raylib_x4.h` and call `InitWindow(X4_WIDTH, X4_HEIGHT, title)` so applications follow the selected profile. `X4_*` size constants are compatibility aliases for `XTEINK_*`; the existing refresh/storage API is shared. `XTEINK_DEVICE_ID`, `XTEINK_DEVICE_NAME`, `XTEINK_HAS_TOUCH`, and `XTEINK_HAS_FRONTLIGHT` describe the selected device. `XteinkSetFrontlight()` updates the Pro preview without an e-paper refresh. Applications with literal `480, 800` must adopt the size constants to run on X3. The examples fit all supported dimensions.

On `x4-pro`, left mouse press/drag/release maps to one touch point and raylib's left mouse button. Use `GetTouchPointCount()`, `GetTouchPosition(0)`, and `IsMouseButtonPressed(MOUSE_BUTTON_LEFT)`. Touch is ignored on button-only profiles. The full keyboard mapping below remains available as desktop navigation shortcuts; it does not imply that the Pro has all of the original X4's physical keys. Paper and Tetris also have [touch controls](examples.md#pro-touch-controls).

Geometry and feature references: [FreeInk X3/X4 board specifications](https://github.com/Free-Ink/freeink-sdk#supported-boards), [X4 Classic bring-up](https://github.com/Free-Ink/freeink-sdk/blob/main/docs/xteink-x4c-support.md), and [X4 Pro bring-up](https://github.com/Free-Ink/freeink-sdk/blob/main/docs/xteink-x4pro-support.md). These describe other projects' hardware work, not hardware validation of this project. The [S4](https://xtcloud.xteink.cn/app/model-viewer-360) requires a separate Android integration and is tracked as planned in the [compatibility checklist](../README.md#compatibility-and-planned-support).

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

`X4_INPUT_SCRIPT` accepts `POLL KEY down/up` lines and `POLL touch:X:Y down/up` taps on Pro. For example, `2 touch:80:740 down` followed by `3 touch:80:740 up` presses Paper's Play area. `POLL quit down` ends the run. Poll numbers count HAL input polls, not elapsed milliseconds.
