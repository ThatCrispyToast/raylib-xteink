# Application development

Start with [examples/minimal.c](../examples/minimal.c). Include `raylib.h`, supply `int main(void)`, and call `InitWindow(480, 800, title)`. Check `IsWindowReady()` before drawing. Other window sizes are rejected.

The framebuffer covers the full controller area. Keep content at least 14 pixels from the top and bottom to allow for the bezel. All build commands below run from the project root.

## Build a custom application

After [setting up the emulator](emulator.md), choose an entry point:

```sh
cmake -S . -B build-my-app -G Ninja \
  -DX4_APP_SOURCE="$PWD/examples/minimal.c"
cmake --build build-my-app -j
./build-my-app/x4_demo
```

For firmware, use the [hardware environment](hardware.md) and a separate build directory:

```sh
nix develop path:.#firmware
idf.py -C firmware -B "$PWD/build-my-app-firmware" \
  -DX4_APP_SOURCE="$PWD/examples/minimal.c" \
  -DSDKCONFIG="$PWD/build-my-app-firmware/sdkconfig" build
```

Pass `-DX4_APP_EXTRA_SOURCES="/path/helpers.c;/path/model.c"` to either build for additional source files. Only the entry point's `main` is renamed for firmware.

An external CMake project can link the emulator target directly:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_x4_app C CXX)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(/absolute/path/to/raylib-x4 x4)
add_executable(my_x4_app application.c)
target_link_libraries(my_x4_app PRIVATE raylib::x4 x4_emulator)
```

## Drawing and refreshes

Keep raylib calls on the application/render task. The firmware wrapper invokes `main` on the FreeRTOS main task; returning zero enters sleep, while a nonzero exit keeps USB available for recovery.

Redraw when state changes and call `PollInputEvents()` and `WaitTime()` while idle, as the minimal example does. `EndDrawing()` converts the framebuffer, skips unchanged output, and polls input. A quiet frame therefore causes no display refresh.

Presentation is synchronous by default. Include `raylib_x4.h` and call `X4SetAsyncRefresh(true)` after initialization to submit ordinary updates without waiting for the waveform. `EndDrawing()` and `PollInputEvents()` service completion and start the newest queued snapshot. New submissions replace older queued frames; the in-flight frame remains immutable within the same 192 KB pixel-buffer budget.

Use `X4WaitDisplay()` to drain output. For a confirmation or new game state that must be visible before the next input poll, set async mode to `false` **before** `EndDrawing()`. Changing the mode alone does not drain an existing update. Startup, full cleans, and closing always wait. Continue polling during idle loops so completion and automatic panel power-down can run.

Both presentation modes enforce a single 15-second BUSY deadline from activation. Physical refresh costs and measurement limits are in [performance](performance.md).

### X4 extensions

| Function | Behavior |
|---|---|
| `X4RefreshFull()` | Force the next frame to clean the panel |
| `X4SetRefreshInterval(n)` | Clean after `n` partial updates; default 12, zero always cleans |
| `X4SetAsyncRefresh(bool)` | Select blocking or nonblocking presentation for subsequent frames |
| `X4WaitDisplay()` | Finish in-flight and queued frames; return success |
| `X4IsDisplayBusy()` | Query pending/in-flight output without advancing it |
| `X4GetStats()` | Completed refreshes, coalesced frames, errors, memory, and timings |
| `X4GetStatus()` | Battery voltage, button ADCs/mask, heap, SD status, and input overflow |
| `X4GetPanelFrame()` | Read-only completed 48 KB frame, 800 × 480, MSB first, 1 = white; `NULL` during flight |
| `X4GetStoragePath()` | `/sd` on hardware, virtual SD directory in the emulator, or `NULL` |

Wait before capturing the panel frame. The generic target defaults to periodic cleaning; Paper and Tetris disable that limit and use startup/wake and Power instead.

## Input

| Physical input | raylib key | Emulator key |
|---|---|---|
| Back | `KEY_ESCAPE` | Escape / Backspace |
| Confirm | `KEY_ENTER` | Enter |
| Left / Right | `KEY_LEFT` / `KEY_RIGHT` | Left / Right |
| Side Up / Down | `KEY_UP` / `KEY_DOWN` | Up / Down |
| Power | `KEY_P` | P |

Hardware samples and debounces buttons every 5 ms. Presses are latched across blocking refreshes. Drain `GetKeyPressed()` in a loop when input order or repeated taps matter: up to 64 press edges survive a blocked refresh. `IsKeyPressed()` is a per-frame boolean, and `X4GetStatus().dropped_input_presses` reports queue overflow.

A held key becomes eligible for `IsKeyPressedRepeat()` after 450 ms and every 150 ms thereafter, with at most one repeat per poll. Blocking refreshes therefore reduce the observed rate. Tetris implements its own held-key timing. The resistor ladders cannot distinguish every simultaneous button combination.

## Storage and memory

Use `X4GetStoragePath()` when forming asset/save paths. Working-directory-relative paths are not automatically mapped to the SD card. Check for `NULL` and allow for unavailable or unwritable storage.

Textures and decoded images must fit the remaining heap, and allocations must be contiguous. Use `X4GetStatus().largest_block` when sizing them. A full-screen RGBA image needs about 1.5 MB and is unsuitable for this MCU. Texture uploads return an invalid texture when memory or handles run out; check `IsTextureValid()`.

The default font's atlas and glyph images live in flash. Treat glyph images returned by `GetFontDefault()` as read-only; use `ImageCopy()` before modification. Updating the default font texture creates a RAM copy. `ImageText()` remains supported.

## Supported profile

Supported: 2D shapes, default bitmap text, bitmap fonts, small textures, alpha blending, 2D cameras, scissor clipping, raymath, keyboard edges/repeat, timing, and SD files. Colors become four luminance levels during rendering and are spatially dithered to monochrome for presentation. PNG, BMP, TGA, and QOI images are enabled; TTF, JPEG, and animated GIF are disabled.

Outside this target: 3D/depth, audio, render textures, programmable shaders, multitouch, mouse/gamepads, window management, wireless services, and OTA. The panel's latency suits e-paper interfaces and games rather than conventional high-refresh action games.

See [design](design.md) for the memory layout, renderer changes, and controller protocol, and [testing](testing.md) to validate an application or target change.
