// Exercise the actual Paper drawing code on the MCU without touching saved games.
#include "raylib.h"
#include "raylib_x4.h"
#include "../src/x4_epd.h"
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
static const int actions[]={KEY_NULL,KEY_ENTER,KEY_RIGHT,KEY_RIGHT,KEY_DOWN,KEY_LEFT,KEY_UP,
    KEY_ESCAPE,KEY_DOWN,KEY_DOWN,KEY_DOWN,KEY_DOWN,KEY_DOWN,KEY_UP,KEY_UP,KEY_ESCAPE,
    KEY_LEFT,KEY_RIGHT,KEY_DOWN,KEY_UP,KEY_ENTER,KEY_RIGHT,KEY_ENTER};
static unsigned frame;
static double draw_start, action_start;
static char benchmark_directory[96];
static bool delivered;
static void benchmark_async(bool enabled) { (void)enabled; X4SetAsyncRefresh(false); }
static bool benchmark_done(void);
static int benchmark_key(void) { if (delivered || benchmark_done()) return 0; delivered=true; return actions[frame]; }
static bool benchmark_pressed(int key) { return !benchmark_done() && actions[frame] == key; }
static bool benchmark_repeat(int key) { (void)key; return false; }
static bool benchmark_done(void) { return frame >= sizeof(actions)/sizeof(*actions); }
static bool benchmark_should_close(void) { action_start=GetTime(); return benchmark_done(); }
static const char *benchmark_storage(void)
{
    const char *sd=X4GetStoragePath();
    if (!sd) return NULL;
    snprintf(benchmark_directory,sizeof(benchmark_directory),"%s/paper-perf-%08x",sd,GetRandomValue(1,0x7fffffff));
    if (mkdir(benchmark_directory,0700)) { benchmark_directory[0]=0; return NULL; }
    return benchmark_directory;
}
static void benchmark_close(void)
{
    if (benchmark_directory[0]) {
        char path[160];
        snprintf(path,sizeof(path),"%s/raylib-x4/paper-a.sav",benchmark_directory); unlink(path);
        snprintf(path,sizeof(path),"%s/raylib-x4/paper-b.sav",benchmark_directory); unlink(path);
        snprintf(path,sizeof(path),"%s/raylib-x4",benchmark_directory); rmdir(path);
        rmdir(benchmark_directory);
    }
    CloseWindow();
}
static void benchmark_begin(void) { draw_start=GetTime(); BeginDrawing(); }
static void benchmark_end(void)
{
    double draw=GetTime()-draw_start;
    EndDrawing();
    X4Stats s=X4GetStats();
    X4EpdStats e=x4_epd_stats();
    printf("PAPER_PERF frame=%u key=%d draw_ms=%.3f convert_ms=%.3f present_ms=%.3f total_ms=%.3f action_ms=%.3f full=%u partial=%u errors=%u\n",
        frame,actions[frame],draw*1000,s.last_convert_seconds*1000,s.last_present_seconds*1000,
        (GetTime()-draw_start)*1000,(GetTime()-action_start)*1000,s.full_refreshes,s.partial_refreshes,s.errors);
    printf("PANEL_PERF bytes=%u window=%ux%u transfer_ms=%.3f waveform_ms=%.3f\n",
        e.ram_bytes,e.window_width,e.window_height,e.transfer_us/1000.0,e.waveform_us/1000.0);
    fflush(stdout); frame++; delivered=false;
}
#undef main
#define main paper_benchmark_application
#define IsKeyPressed benchmark_pressed
#define GetKeyPressed benchmark_key
#define IsKeyPressedRepeat benchmark_repeat
#define WindowShouldClose benchmark_should_close
#define CloseWindow benchmark_close
#define BeginDrawing benchmark_begin
#define EndDrawing benchmark_end
#define X4GetStoragePath benchmark_storage
#define X4SetAsyncRefresh benchmark_async
#include "../examples/paper.c"
#undef main
#undef X4GetStoragePath
int x4_application_main(void)
{
    int result=paper_benchmark_application();
    printf("PAPER_PERF_DONE result=%d\n",result); fflush(stdout);
    for (;;) WaitTime(0.05);
}
