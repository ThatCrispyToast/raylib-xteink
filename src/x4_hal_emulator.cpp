// SPDX-License-Identifier: GPL-2.0-only
// X4 uses microreader; other profiles use a native SDL panel preview.
#include "x4_hal.h"
#include "x4_input_queue.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <filesystem>
#include <vector>
#include <atomic>
#include <algorithm>

static std::atomic<bool> refresh_busy{false};
static SDL_TimerID refresh_timer;
static Uint32 refresh_done(Uint32 interval, void *unused)
{ (void)interval; (void)unused; refresh_busy=false; return 0; }
static void emulator_delay(Uint32 ms)
{
    if (!std::getenv("X4_EMULATOR_FAST")) {
        if (refresh_timer) SDL_RemoveTimer(refresh_timer);
        refresh_busy=true;
        refresh_timer=SDL_AddTimer(ms,refresh_done,nullptr);
        if (!refresh_timer) { SDL_Delay(ms); refresh_busy=false; }
    }
}
static bool capture_ok = true;
static bool capture_frame(SDL_Renderer *renderer)
{
    const char *path = std::getenv("X4_SCREENSHOT");
    if (!path) return true;
    int width, height;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) return false;
    std::vector<uint8_t> rgb(static_cast<size_t>(width)*height*3);
    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24, rgb.data(), width*3) != 0) return false;
    FILE *f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    bool ok = std::fwrite(rgb.data(), 1, rgb.size(), f) == rgb.size();
    return std::fclose(f) == 0 && ok;
}
static void emulator_present(SDL_Renderer *renderer)
{
    // SDL invalidates the backbuffer after presenting it.
    capture_ok = capture_frame(renderer) && capture_ok;
    SDL_RenderPresent(renderer);
}
#if XTEINK_DEVICE == XTEINK_X4
#define SDL_Delay emulator_delay
#define SDL_RenderPresent emulator_present
#include "display.h"
#undef SDL_RenderPresent
#undef SDL_Delay

static std::unique_ptr<DesktopRuntime> runtime;
static std::unique_ptr<DesktopEmulatorDisplay> display;
#else
#include "xteink_emulator_display.h"
static std::unique_ptr<XteinkEmulatorDisplay> display;
#endif
static uint8_t down;
static bool touch_down;
static int touch_x, touch_y;
static X4InputQueue presses;
static std::string storage;
static unsigned int poll_count;
struct ScriptEvent { unsigned int poll; std::string key, action; };
static std::vector<ScriptEvent> script;

static int button(SDL_Keycode key)
{
    switch (key) {
        case SDLK_ESCAPE: case SDLK_BACKSPACE: return X4_BACK;
        case SDLK_RETURN: return X4_CONFIRM;
        case SDLK_LEFT: return X4_LEFT;
        case SDLK_RIGHT: return X4_RIGHT;
        case SDLK_UP: return X4_UP;
        case SDLK_DOWN: return X4_DOWN;
        case SDLK_p: return X4_POWER;
        default: return -1;
    }
}

extern "C" bool x4_hal_init(const char *title)
{
    try {
        const char *video = std::getenv("SDL_VIDEODRIVER");
        if (video && std::strcmp(video, "offscreen") == 0)
            SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_DEFAULT);
        std::string window_title = std::string(title ? title : "raylib") + " / " + XTEINK_DEVICE_NAME;
#if XTEINK_DEVICE == XTEINK_X4
        runtime = std::make_unique<DesktopRuntime>(20);
        if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0) throw std::runtime_error(SDL_GetError());
        display = std::make_unique<DesktopEmulatorDisplay>(*runtime);
        display->set_rotation(microreader::Rotation::Deg90);
        SDL_SetWindowTitle(SDL_RenderGetWindow(runtime->renderer()), window_title.c_str());
#else
        display = std::make_unique<XteinkEmulatorDisplay>(window_title.c_str());
#endif
        storage = std::getenv("X4_SD_ROOT") ? std::getenv("X4_SD_ROOT") : "sdcard";
        std::error_code error;
        if (!storage.empty()) {
            std::filesystem::create_directories(storage, error);
            if (error || !std::filesystem::is_directory(storage, error)) {
                std::fprintf(stderr, "X4 emulator: storage unavailable: %s\n", storage.c_str());
                storage.clear();
            }
        }
        down = 0;
        touch_down = false;
        touch_x = touch_y = 0;
        presses = {};
        poll_count = 0;
        script.clear();
        refresh_busy=false; refresh_timer=0;
        if (const char *path = std::getenv("X4_INPUT_SCRIPT")) {
            FILE *file = std::fopen(path, "r");
            if (!file) throw std::runtime_error("Cannot open input script");
            unsigned int tick; char key[32], action[16];
            while (std::fscanf(file, "%u %31s %15s", &tick, key, action) == 3)
                script.push_back({tick, key, action});
            std::fclose(file);
        }
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "X4 emulator: %s\n", e.what());
        x4_hal_close();
        return false;
    }
}

extern "C" void x4_hal_close(void)
{
    if (refresh_timer) SDL_RemoveTimer(refresh_timer);
    refresh_timer=0; refresh_busy=false;
    display.reset();
#if XTEINK_DEVICE == XTEINK_X4
    runtime.reset();
#endif
    storage.clear();
}
extern "C" bool x4_hal_present_busy(void) { return refresh_busy; }
extern "C" bool xteink_hal_frontlight(unsigned int brightness, unsigned int warmth)
{
#if XTEINK_HAS_FRONTLIGHT
    if (!display || brightness>100 || warmth>100 || !display->frontlight(brightness,warmth)) return false;
    capture_ok = true;
    emulator_present(display->renderer());
    return capture_ok;
#else
    (void)brightness; (void)warmth;
    return false;
#endif
}
extern "C" bool x4_hal_display_idle(void) { return true; }
extern "C" bool x4_hal_finish_present(void)
{ while (refresh_busy) SDL_Delay(1); return true; }
extern "C" bool x4_hal_begin_present(const uint8_t *frame, const uint8_t *previous, bool full)
{
    capture_ok = true;
#if XTEINK_DEVICE == XTEINK_X4
    if (full) display->full_refresh(frame, microreader::RefreshMode::Full, false);
    else display->partial_refresh(frame, previous);
#else
    (void)previous; (void)full;
    if (!display->draw(frame)) return false;
    emulator_present(display->renderer());
    emulator_delay(400);
#endif
    return capture_ok;
}

extern "C" X4Input x4_hal_poll(void)
{
    X4Input input = {};
    for (const auto& event : script) if (event.poll == poll_count) {
        SDL_Event e = {};
        int x, y;
        if (event.key == "quit") e.type = SDL_QUIT;
        else if (std::sscanf(event.key.c_str(), "touch:%d:%d", &x, &y) == 2) {
            e.type = event.action == "down" ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            e.button.button = SDL_BUTTON_LEFT;
            e.button.x = x; e.button.y = y;
        }
        else {
            e.type = event.action == "down" ? SDL_KEYDOWN : SDL_KEYUP;
            e.key.keysym.sym = SDL_GetKeyFromName(event.key.c_str());
        }
        SDL_PushEvent(&e);
    }
    poll_count++;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) input.quit = true;
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            down = 0;
            touch_down = false;
            input.touch_pressed = false;
        }
        if (XTEINK_HAS_TOUCH) {
            if (e.type == SDL_MOUSEMOTION) {
                touch_x = std::clamp(e.motion.x, 0, XTEINK_WIDTH-1);
                touch_y = std::clamp(e.motion.y, 0, XTEINK_HEIGHT-1);
            }
            if ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && e.button.button == SDL_BUTTON_LEFT) {
                touch_x = std::clamp(e.button.x, 0, XTEINK_WIDTH-1);
                touch_y = std::clamp(e.button.y, 0, XTEINK_HEIGHT-1);
                if (e.type == SDL_MOUSEBUTTONDOWN && !touch_down) input.touch_pressed = true;
                touch_down = e.type == SDL_MOUSEBUTTONDOWN;
            }
        }
        if (e.type != SDL_KEYDOWN && e.type != SDL_KEYUP) continue;
        int b = button(e.key.keysym.sym);
        if (b < 0) continue;
        uint8_t bit = static_cast<uint8_t>(1u << b);
        if (e.type == SDL_KEYDOWN) {
            if (!(down & bit)) x4_input_push(&presses,b);
            down |= bit;
        } else down &= static_cast<uint8_t>(~bit);
    }
    input.down = down;
    input.touch_down = touch_down;
    input.touch_x = touch_x;
    input.touch_y = touch_y;
    x4_input_drain(&presses,&input);
    return input;
}
extern "C" double x4_hal_time(void)
{ return static_cast<double>(SDL_GetPerformanceCounter())/SDL_GetPerformanceFrequency(); }
extern "C" void x4_hal_sleep(double seconds) { SDL_Delay(static_cast<Uint32>(seconds*1000)); }
extern "C" const char *x4_hal_storage(void) { return storage.empty() ? nullptr : storage.c_str(); }
extern "C" X4Status x4_hal_status(void)
{
    X4Status status = {};
    status.front_adc = status.side_adc = status.battery_mv = -1;
    status.buttons = down;
    status.dropped_input_presses = presses.dropped;
    status.storage_mounted = display != nullptr && !storage.empty();
    return status;
}
