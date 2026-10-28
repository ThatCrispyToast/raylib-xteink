# Application development

Start with [examples/minimal.c](../examples/minimal.c). Include `raylib.h` and `raylib_x4.h`, supply `int main(void)`, and call `InitWindow(X4_WIDTH, X4_HEIGHT, title)`. Check `IsWindowReady()` before drawing. The size constants follow the selected [device profile](emulator.md#device-profiles); other window sizes are rejected. Existing X4 API and target names work across profiles.

The framebuffer covers the full controller area. On X4, keep content at least 14 pixels from the top and bottom to allow for the bezel; other models' physical crops are unverified. All build commands below run from the project root. Add `-DXTEINK_DEVICE=x3`, `x4-classic`, or `x4-pro` for desktop builds. The same selection applies to [experimental firmware builds](experimental-hardware.md), which also select a panel controller.

## Build a custom application

After [setting up the emulator](emulator.md), choose an entry point:

```sh
cmake -S . -B build-my-app \
  -DX4_APP_SOURCE="$PWD/examples/minimal.c"
cmake --build build-my-app -j
./build-my-app/x4_demo
```

For firmware, [Docker](docker.md#custom-applications) supplies the SDK:

```sh
docker build --build-arg X4_APP_SOURCE=examples/minimal.c \
  --output type=local,dest=build-docker/minimal .
```

Alternatively, use a [local ESP-IDF environment](hardware.md#local-esp-idf-build) and a separate build directory:

```sh
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

Presentation is synchronous by default. Include `raylib_x4.h` and call `X4SetAsyncRefresh(true)` after initialization to submit ordinary updates without waiting for the waveform. `EndDrawing()` and `PollInputEvents()` service completion and start the newest queued snapshot. New submissions replace older queued frames; the in-flight frame remains immutable within the same pixel-buffer budget (192,000 bytes on X4/Classic/Pro; 209,088 on X3).

Use `X4WaitDisplay()` to drain output. For a confirmation or new game state that must be visible before the next input poll, set async mode to `false` **before** `EndDrawing()`. Changing the mode alone does not drain an existing update. Startup, full cleans, and closing always wait. Continue polling during idle loops so completion and automatic panel power-down can run.

Both presentation modes enforce a 15-second BUSY deadline per controller activation. The experimental X3 UC8253 full update includes a separate conditioning activation. Physical original-X4 refresh costs and measurement limits are in [performance](performance.md).

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
| `X4GetPanelFrame()` | Read-only completed `X4_FRAME_BYTES` frame, `X4_PANEL_WIDTH` × `X4_PANEL_HEIGHT`, MSB first, 1 = white; `NULL` during flight |
| `XteinkSetFrontlight(brightness, warmth)` | Pro only, both values 0–100; 0 brightness turns off; 0 warmth is cool, 100 warm. Returns false if unavailable/out of range. Starts off; independent of e-paper refresh. |
| `X4GetStoragePath()` | `/sd` on hardware, virtual SD directory in the emulator, or `NULL` |

Wait before capturing the panel frame. The generic target defaults to periodic cleaning; Paper and Tetris disable that limit and use startup/wake and Power instead.

## Input

The table maps the original X4's buttons and shared desktop keyboard shortcuts. X3 uses the same ADC ladders; Classic uses discrete GPIO keys. Pro has physical Up, Down, and Power plus touch/Home; it maps Home to Back. Its first touch contact maps to `GetTouchPosition(0)` and the left mouse button. Mouse press/drag/release supplies the same input in the [emulator](emulator.md#device-profiles). Paper and Tetris include touch controls for the missing keys. The desktop's extra keyboard shortcuts remain available.

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

Outside this target: 3D/depth, audio, render textures, programmable shaders, multitouch, general mouse/gamepad support, window management, wireless services, and OTA. The Pro desktop profile supports the left mouse button as a single touch point. The panel's latency suits e-paper interfaces and games rather than conventional high-refresh action games.

See [design](design.md) for the memory layout, renderer changes, and controller protocol, and [testing](testing.md) to validate an application or target change.
