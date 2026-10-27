// SPDX-License-Identifier: GPL-2.0-only
// Adapter to microreader's existing X4 display emulator.
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
#define SDL_Delay emulator_delay
#include "display.h"
#undef SDL_Delay

static std::unique_ptr<DesktopRuntime> runtime;
static std::unique_ptr<DesktopEmulatorDisplay> display;
static uint8_t down;
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
        runtime = std::make_unique<DesktopRuntime>(20);
        if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0) throw std::runtime_error(SDL_GetError());
        display = std::make_unique<DesktopEmulatorDisplay>(*runtime);
        display->set_rotation(microreader::Rotation::Deg90);
        SDL_SetWindowTitle(SDL_RenderGetWindow(runtime->renderer()), title ? title : "raylib X4");
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
        display.reset(); runtime.reset(); storage.clear();
        return false;
    }
}

extern "C" void x4_hal_close(void)
{
    if (refresh_timer) SDL_RemoveTimer(refresh_timer);
    refresh_timer=0; refresh_busy=false;
    display.reset(); runtime.reset(); storage.clear();
}
extern "C" bool x4_hal_present_busy(void) { return refresh_busy; }
extern "C" bool x4_hal_display_idle(void) { return true; }
extern "C" bool x4_hal_finish_present(void)
{ while (refresh_busy) SDL_Delay(1); return true; }
extern "C" bool x4_hal_begin_present(const uint8_t *frame, const uint8_t *previous, bool full)
{
    if (full) display->full_refresh(frame, microreader::RefreshMode::Full, false);
    else display->partial_refresh(frame, previous);
    if (const char *path = std::getenv("X4_SCREENSHOT")) {
        int width, height;
        SDL_GetRendererOutputSize(runtime->renderer(), &width, &height);
        std::vector<uint8_t> rgb(static_cast<size_t>(width)*height*3);
        if (SDL_RenderReadPixels(runtime->renderer(), nullptr, SDL_PIXELFORMAT_RGB24, rgb.data(), width*3) != 0)
            return false;
        FILE *f = std::fopen(path, "wb");
        if (!f) return false;
        std::fprintf(f, "P6\n%d %d\n255\n", width, height);
        bool ok = std::fwrite(rgb.data(), 1, rgb.size(), f) == rgb.size();
        ok = std::fclose(f) == 0 && ok;
        if (!ok) return false;
    }
    return true;
}

extern "C" X4Input x4_hal_poll(void)
{
    X4Input input = {};
    for (const auto& event : script) if (event.poll == poll_count) {
        SDL_Event e = {};
        if (event.key == "quit") e.type = SDL_QUIT;
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
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) down = 0;
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
    status.storage_mounted = runtime != nullptr && !storage.empty();
    return status;
}
