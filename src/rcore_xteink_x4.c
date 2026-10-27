/**********************************************************************************************
*
*   rcore_xteink_x4 - Xteink X4 platform backend (modified from rcore_memory)
*   Native portrait 2D software rendering, seven buttons, and e-paper presentation.
*   The hardware and microreader emulator share this implementation.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2025-2026 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

// X4 port, derived from rcore_memory. Included by rcore.c.
#include "x4_hal.h"
static double x4_time_base;
static double x4_repeat_at[X4_BUTTON_COUNT];
static const int x4_keys[] = { KEY_ESCAPE, KEY_ENTER, KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_P };

// The software renderer can sample the immutable built-in atlas directly from flash.
unsigned int x4_load_font_texture(const void *pixels, int width, int height)
{
    unsigned int id = 0;
    swGenTextures(1, &id);
    if (!id) return 0;
    sw_texture_t *texture = sw_pool_get(&RLSW.texturePool, id);
    *texture = (sw_texture_t){ .pixels=(void *)pixels, .allocSz=width*height*2, .borrowed=true,
        .format=SW_PIXELFORMAT_COLOR_GRAYALPHA, .alpha=SW_PIXELFORMAT_ALPHA[SW_PIXELFORMAT_COLOR_GRAYALPHA],
        .width=width, .height=height, .wMinus1=width-1, .hMinus1=height-1,
        .tx=1.0f/width, .ty=1.0f/height, .minFilter=SW_NEAREST, .magFilter=SW_NEAREST,
        .sWrap=SW_REPEAT, .tWrap=SW_REPEAT,
        .readColor=sw_pixel_get_read_color_func(SW_PIXELFORMAT_COLOR_GRAYALPHA),
        .readColor8=sw_pixel_get_read_color8_func(SW_PIXELFORMAT_COLOR_GRAYALPHA) };
    return id;
}

// Check if application should close
bool WindowShouldClose(void)
{
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    else return true;
}

// Toggle fullscreen mode
void ToggleFullscreen(void)
{
    TRACELOG(LOG_WARNING, "ToggleFullscreen() not available on target platform");
}

// Toggle borderless windowed mode
void ToggleBorderlessWindowed(void)
{
    TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on target platform");
}

// Set window state: maximized, if resizable
void MaximizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MaximizeWindow() not available on target platform");
}

// Set window state: minimized
void MinimizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MinimizeWindow() not available on target platform");
}

// Restore window from being minimized/maximized
void RestoreWindow(void)
{
    TRACELOG(LOG_WARNING, "RestoreWindow() not available on target platform");
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "SetWindowState() not available on target platform");
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "ClearWindowState() not available on target platform");
}

// Set icon for window
void SetWindowIcon(Image image)
{
    TRACELOG(LOG_WARNING, "SetWindowIcon() not available on target platform");
}

// Set icon for window
void SetWindowIcons(Image *images, int count)
{
    TRACELOG(LOG_WARNING, "SetWindowIcons() not available on target platform");
}

// Set title for window
void SetWindowTitle(const char *title)
{
    CORE.Window.title = title;
}

// Set window position on screen (windowed mode)
void SetWindowPosition(int x, int y)
{
    TRACELOG(LOG_WARNING, "SetWindowPosition() not available on target platform");
}

// Set monitor for the current window
void SetWindowMonitor(int monitor)
{
    TRACELOG(LOG_WARNING, "SetWindowMonitor() not available on target platform");
}

// Set window minimum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMinSize(int width, int height)
{
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
}

// Set window maximum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMaxSize(int width, int height)
{
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;
}

// Set window dimensions
void SetWindowSize(int width, int height)
{
    TRACELOG(LOG_WARNING, "SetWindowSize() not available on target platform");
}

// Set window opacity, value opacity is between 0.0 and 1.0
void SetWindowOpacity(float opacity)
{
    TRACELOG(LOG_WARNING, "SetWindowOpacity() not available on target platform");
}

// Set window focused
void SetWindowFocused(void)
{
    TRACELOG(LOG_WARNING, "SetWindowFocused() not available on target platform");
}

// Get native window handle
void *GetWindowHandle(void)
{
    TRACELOG(LOG_WARNING, "GetWindowHandle() not implemented on target platform");
    return NULL;
}

// Get number of monitors
int GetMonitorCount(void)
{
    return 1;
}

// Get current monitor where window is placed
int GetCurrentMonitor(void)
{
    return 0;
}

// Get selected monitor position
Vector2 GetMonitorPosition(int monitor)
{
    return (Vector2){ 0, 0 };
}

// Get selected monitor width (currently used by monitor)
int GetMonitorWidth(int monitor) { (void)monitor; return X4_WIDTH; }

// Get selected monitor height (currently used by monitor)
int GetMonitorHeight(int monitor) { (void)monitor; return X4_HEIGHT; }

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor) { (void)monitor; return 56; }

// Get selected monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor) { (void)monitor; return 93; }

// Get selected monitor refresh rate
int GetMonitorRefreshRate(int monitor) { (void)monitor; return 2; }

// Get the human-readable, UTF-8 encoded name of the selected monitor
const char *GetMonitorName(int monitor) { (void)monitor; return "Xteink X4"; }

// Get window position XY on monitor
Vector2 GetWindowPosition(void)
{
    return (Vector2){ 0, 0 };
}

// Get window scale DPI factor for current monitor
Vector2 GetWindowScaleDPI(void)
{
    return (Vector2){ 1.0f, 1.0f };
}

// Set clipboard text content
void SetClipboardText(const char *text)
{
    TRACELOG(LOG_WARNING, "SetClipboardText() not implemented on target platform");
}

// Get clipboard text content
// NOTE: returned string is allocated and freed by GLFW
const char *GetClipboardText(void)
{
    TRACELOG(LOG_WARNING, "GetClipboardText() not implemented on target platform");
    return NULL;
}

// Get clipboard image
Image GetClipboardImage(void)
{
    Image image = { 0 };

    TRACELOG(LOG_WARNING, "GetClipboardImage() not implemented on target platform");

    return image;
}

// Show mouse cursor
void ShowCursor(void)
{
    CORE.Input.Mouse.cursorHidden = false;
}

// Hide mouse cursor
void HideCursor(void)
{
    CORE.Input.Mouse.cursorHidden = true;
}

// Enable cursor (unlock cursor)
void EnableCursor(void)
{
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);

    CORE.Input.Mouse.cursorHidden = false;
}

// Disable cursor (lock cursor)
void DisableCursor(void)
{
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);

    CORE.Input.Mouse.cursorHidden = true;
}

// Set internal gamepad mappings
int SetGamepadMappings(const char *mappings)
{
    TRACELOG(LOG_WARNING, "SetGamepadMappings() not implemented on target platform");
    return 0;
}

// Set gamepad vibration
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration)
{
    TRACELOG(LOG_WARNING, "SetGamepadVibration() not implemented on target platform");
}

// Set mouse position XY
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
}

// Set mouse cursor
void SetMouseCursor(int cursor)
{
    TRACELOG(LOG_WARNING, "SetMouseCursor() not implemented on target platform");
}

// Get physical key name
const char *GetKeyName(int key)
{
    switch (key) {
        case KEY_ESCAPE: return "Back";
        case KEY_ENTER: return "Confirm";
        case KEY_LEFT: return "Left";
        case KEY_RIGHT: return "Right";
        case KEY_UP: return "Up";
        case KEY_DOWN: return "Down";
        case KEY_P: return "Power";
        default: return "";
    }
}

void SwapScreenBuffer(void)
{
    int width, height;
    const uint8_t *pixels = swGetColorBuffer(&width, &height);
    if (!pixels || width != X4_WIDTH || height != X4_HEIGHT || !x4_present(pixels))
    {
        TRACELOG(LOG_ERROR, "X4: display update failed");
        CORE.Window.shouldClose = true;
    }
}

double GetTime(void) { return x4_hal_time() - x4_time_base; }
void OpenURL(const char *url) { (void)url; TRACELOG(LOG_WARNING, "X4: browser unavailable"); }

void PollInputEvents(void)
{
    if (!x4_service_display(false)) CORE.Window.shouldClose=true;
    X4Input input = x4_hal_poll();
    double now = GetTime();
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;
    memcpy(CORE.Input.Keyboard.previousKeyState, CORE.Input.Keyboard.currentKeyState,
           sizeof(CORE.Input.Keyboard.currentKeyState));
    memset(CORE.Input.Keyboard.currentKeyState, 0, sizeof(CORE.Input.Keyboard.currentKeyState));
    memset(CORE.Input.Keyboard.keyRepeatInFrame, 0, sizeof(CORE.Input.Keyboard.keyRepeatInFrame));
    for (int i = 0; i < X4_BUTTON_COUNT; i++)
    {
        int key = x4_keys[i];
        bool pressed = (input.pressed & (1u << i)) != 0;
        bool down = (input.down & (1u << i)) != 0;
        // A latched tap survives a blocking e-paper refresh for one application frame.
        if (pressed) CORE.Input.Keyboard.previousKeyState[key] = 0;
        CORE.Input.Keyboard.currentKeyState[key] = down || pressed;
        if (pressed || (down && !CORE.Input.Keyboard.previousKeyState[key]))
        {
            if (!input.press_count && CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE)
                CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount++] = key;
            x4_repeat_at[i] = now + 0.45;
            if (key == CORE.Input.Keyboard.exitKey) CORE.Window.shouldClose = true;
        }
        else if (down && now >= x4_repeat_at[i])
        {
            CORE.Input.Keyboard.keyRepeatInFrame[key] = 1;
            x4_repeat_at[i] = now + 0.15;
        }
    }
    for (int i=0; i<input.press_count && CORE.Input.Keyboard.keyPressedQueueCount<MAX_KEY_PRESSED_QUEUE; i++)
        if (input.press_order[i]<X4_BUTTON_COUNT)
            CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount++]=x4_keys[input.press_order[i]];
    if (input.quit) CORE.Window.shouldClose = true;
}

int InitPlatform(void)
{
    if (CORE.Window.screen.width != X4_WIDTH || CORE.Window.screen.height != X4_HEIGHT)
    {
        TRACELOG(LOG_ERROR, "X4: InitWindow requires 480x800 portrait resolution");
        return -1;
    }
    if (!x4_hal_init(CORE.Window.title)) return -1;
    x4_reset();
    x4_time_base = x4_hal_time();
    memset(x4_repeat_at, 0, sizeof(x4_repeat_at));
    memset(&CORE.Input.Keyboard.currentKeyState, 0, sizeof(CORE.Input.Keyboard.currentKeyState));
    memset(&CORE.Input.Keyboard.previousKeyState, 0, sizeof(CORE.Input.Keyboard.previousKeyState));
    CORE.Window.display.width = CORE.Window.render.width = X4_WIDTH;
    CORE.Window.display.height = CORE.Window.render.height = X4_HEIGHT;
    CORE.Window.currentFbo.width = X4_WIDTH;
    CORE.Window.currentFbo.height = X4_HEIGHT;
    CORE.Window.renderOffset = (Point){ 0, 0 };
    CORE.Window.screenScale = MatrixIdentity();
    CORE.Window.shouldClose = false;
    CORE.Window.ready = true;
    CORE.Storage.basePath = x4_hal_storage();
    InitTimer();
    return 0;
}

void ClosePlatform(void) { X4WaitDisplay(); x4_hal_close(); CORE.Window.ready = false; }
