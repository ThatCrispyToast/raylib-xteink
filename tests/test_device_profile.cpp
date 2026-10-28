#include "raylib.h"
#include "raylib_x4.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "profile check failed: %s\n", #x); std::exit(1); } } while (0)

static void pointer(Uint32 type, int x, int y)
{
    SDL_Event event = {};
    event.type = type;
    if (type == SDL_MOUSEMOTION) { event.motion.x = x; event.motion.y = y; }
    else { event.button.button = SDL_BUTTON_LEFT; event.button.x = x; event.button.y = y; }
    CHECK(SDL_PushEvent(&event) == 1);
}

int main()
{
    InitWindow(XTEINK_WIDTH, XTEINK_HEIGHT, "Device profile QA");
    CHECK(IsWindowReady());
    SetTargetFPS(0);
    SetExitKey(KEY_NULL);
    CHECK(GetScreenWidth() == XTEINK_WIDTH && GetScreenHeight() == XTEINK_HEIGHT);
    CHECK(GetMonitorWidth(0) == XTEINK_WIDTH && GetMonitorHeight(0) == XTEINK_HEIGHT);
    CHECK(std::strcmp(GetMonitorName(0), XTEINK_DEVICE_NAME) == 0);
    BeginDrawing();
    ClearBackground(WHITE);
    int top = XTEINK_VISIBLE_TOP, bottom = top + XTEINK_VISIBLE_HEIGHT;
    DrawRectangle(0, top, 8, 8, BLACK);
    DrawRectangle(XTEINK_WIDTH-8, top, 8, 8, BLACK);
    DrawRectangle(0, bottom-8, 8, 8, BLACK);
    DrawRectangle(XTEINK_WIDTH-8, bottom-8, 8, 8, BLACK);
    EndDrawing();
    CHECK(X4GetStats().framebuffer_bytes == XTEINK_WIDTH*XTEINK_HEIGHT/2);
    CHECK(X4GetPanelFrame());
    auto stats = X4GetStats();
    CHECK(XteinkSetFrontlight(100, 0) == XTEINK_HAS_FRONTLIGHT);
    CHECK(XteinkSetFrontlight(50, 100) == XTEINK_HAS_FRONTLIGHT);
    CHECK(!XteinkSetFrontlight(101, 50) && !XteinkSetFrontlight(50, 101));
    CHECK(XteinkSetFrontlight(0, 0) == XTEINK_HAS_FRONTLIGHT);
    CHECK(X4GetStats().frames == stats.frames && X4GetStats().full_refreshes == stats.full_refreshes);
    pointer(SDL_MOUSEBUTTONDOWN, 37, 51);
    PollInputEvents();
    CHECK(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) == XTEINK_HAS_TOUCH);
    CHECK(GetTouchPointCount() == (XTEINK_HAS_TOUCH ? 1 : 0));
    if (XTEINK_HAS_TOUCH) {
        CHECK(GetTouchPosition(0).x == 37 && GetTouchPosition(0).y == 51);
        CHECK(GetTouchPointId(0) == 0);
    }
    pointer(SDL_MOUSEMOTION, XTEINK_WIDTH+20, -20);
    PollInputEvents();
    if (XTEINK_HAS_TOUCH) {
        CHECK(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
        CHECK(GetTouchPosition(0).x == XTEINK_WIDTH-1 && GetTouchPosition(0).y == 0);
    }
    pointer(SDL_MOUSEBUTTONUP, 73, 91);
    PollInputEvents();
    CHECK(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) == XTEINK_HAS_TOUCH);
    CHECK(GetTouchPointCount() == 0);
    // A complete tap queued during a refresh survives for one application frame.
    pointer(SDL_MOUSEBUTTONDOWN, 11, 13);
    pointer(SDL_MOUSEBUTTONUP, 11, 13);
    PollInputEvents();
    CHECK(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) == XTEINK_HAS_TOUCH);
    PollInputEvents();
    CHECK(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) == XTEINK_HAS_TOUCH);
    pointer(SDL_MOUSEBUTTONDOWN, 11, 13);
    PollInputEvents();
    SDL_Event focus = {};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    CHECK(SDL_PushEvent(&focus) == 1);
    PollInputEvents();
    CHECK(!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && GetTouchPointCount() == 0);
    pointer(SDL_MOUSEBUTTONDOWN, 11, 13);
    PollInputEvents();
    CloseWindow();
    CHECK(!XteinkSetFrontlight(50, 50));
    InitWindow(XTEINK_WIDTH, XTEINK_HEIGHT, "Reopen profile");
    CHECK(IsWindowReady() && GetTouchPointCount() == 0 && !IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    CloseWindow();
    std::printf("PASS: %s geometry, panel edges, touch gating, drag, tap, focus and reopen\n", XTEINK_DEVICE_ID);
}
