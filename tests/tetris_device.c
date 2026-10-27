// Exercise the real application and panel without writing any game files.
#include "raylib.h"
#include "raylib_x4.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
static const int actions[]={KEY_NULL,KEY_DOWN,KEY_ENTER,KEY_RIGHT,KEY_UP,KEY_DOWN,KEY_ENTER,
    KEY_LEFT,KEY_ENTER,KEY_ESCAPE,KEY_DOWN,KEY_DOWN,KEY_ENTER,KEY_ESCAPE,KEY_P,
    KEY_ESCAPE,KEY_DOWN,KEY_DOWN,KEY_DOWN,KEY_ENTER,KEY_ESCAPE,KEY_UP,KEY_UP,
    KEY_ENTER,KEY_ENTER,KEY_ESCAPE,KEY_UP,KEY_ENTER,KEY_ENTER};
static unsigned int action;
static bool delivered, drain;
static double draw_start;
static bool qa_closed(void)
{
    if (delivered) { action++; delivered=false; drain=true; }
    return action>=sizeof(actions)/sizeof(*actions);
}
static int qa_key(void)
{
    if (drain) { drain=false; return 0; }
    if (delivered || action>=sizeof(actions)/sizeof(*actions)) return 0;
    delivered=true; return actions[action];
}
static bool qa_repeat(int key) { (void)key; return false; }
static int qa_random(int min, int max) { (void)min; (void)max; return 42; }
static void qa_begin(void) { draw_start=GetTime(); BeginDrawing(); }
static void qa_end(void)
{
    double draw=GetTime()-draw_start;
    EndDrawing(); X4Stats s=X4GetStats();
    printf("TETRIS_HW action=%u draw_ms=%.3f present_ms=%.3f full=%u partial=%u errors=%u\n",
        action,draw*1000,s.last_present_seconds*1000,s.full_refreshes,s.partial_refreshes,s.errors);
    fflush(stdout);
}
#undef main
#define main tetris_qa_application
#define WindowShouldClose qa_closed
#define GetKeyPressed qa_key
#define IsKeyPressedRepeat qa_repeat
#define IsKeyDown qa_repeat
#define GetRandomValue qa_random
#define BeginDrawing qa_begin
#define EndDrawing qa_end
#include "../examples/tetris.c"
#undef main
int x4_application_main(void)
{
    int result=tetris_qa_application();
    X4Stats s=X4GetStats();
    bool passed=result == 0 && action == sizeof(actions)/sizeof(*actions) && s.full_refreshes == 2 &&
        s.partial_refreshes>=20 && s.errors == 0 && heap_caps_check_integrity_all(true) &&
        uxTaskGetStackHighWaterMark(NULL)>2048;
    printf("TETRIS_HW_%s actions=%u heap=%zu minimum=%zu stack=%u\n",passed ? "PASS" : "FAIL",action,
        (size_t)esp_get_free_heap_size(),(size_t)esp_get_minimum_free_heap_size(),
        (unsigned int)uxTaskGetStackHighWaterMark(NULL));
    fflush(stdout);
    for (;;) WaitTime(0.05);
}
