// Physical factory-waveform benchmark: snapshots, idle power, and temperature expiry.
#include "raylib.h"
#include "raylib_x4.h"
#include "../src/x4_epd.h"
#include <stdio.h>
#include <limits.h>

static int previous=-1;
static void draw(int step)
{
    BeginDrawing();
    if (previous<0) {
        ClearBackground(WHITE);
        DrawText("FACTORY WAVEFORM TEST",24,32,28,BLACK);
        DrawText("Moving block + contrast check",24,88,20,BLACK);
        DrawRectangle(24,380,432,120,BLACK);
        DrawText("BLACK / WHITE",48,418,28,WHITE);
        DrawRectangleLines(24,550,432,120,BLACK);
        DrawText("WHITE / BLACK",48,588,28,BLACK);
    } else DrawRectangle(24+(previous%10)*40,200,36,36,WHITE);
    DrawRectangle(24+(step%10)*40,200,36,36,BLACK);
    EndDrawing(); previous=step;
}
static bool pixel_white(int x, int y)
{
    const uint8_t *p=X4GetPanelFrame();
    unsigned int bit=(479-x)*800+y;
    return p && (p[bit/8] & (0x80u>>(bit%8)));
}
static void idle(double seconds)
{
    double end=GetTime()+seconds;
    while (GetTime()<end) { WaitTime(0.02); PollInputEvents(); }
}
int main(void)
{
    InitWindow(X4_WIDTH,X4_HEIGHT,"X4 display speed");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(0); X4SetRefreshInterval(UINT_MAX);
    bool ok=true;
    draw(0);
    for (int i=1; i<=8; i++) {
        double start=GetTime(); draw(i);
        X4EpdStats e=x4_epd_stats();
        printf("DISPLAY_SYNC step=%d total_ms=%.3f waveform_ms=%.3f\n",i,
            (GetTime()-start)*1000,e.waveform_us/1000.0);
    }
    // Warm up printf's floating-point formatter before measuring retained heap.
    size_t heap=X4GetStatus().free_heap;
    X4SetAsyncRefresh(true);
    double total=0, maximum=0;
    for (int i=0; i<200; i++) {
        double start=GetTime(); draw(i);
        double ms=(GetTime()-start)*1000;
        total+=ms; if (ms>maximum) maximum=ms;
        WaitTime(0.02); PollInputEvents();
    }
    ok &= X4WaitDisplay() && !X4IsDisplayBusy();
    for (int i=0; i<10; i++) ok &= pixel_white(30+i*40,210) == (i!=9);
    X4Stats s=X4GetStats();
    ok &= s.coalesced_frames>100 && s.full_refreshes == 1 && !s.errors;
    printf("DISPLAY_ASYNC frames=200 mean_ms=%.3f max_ms=%.3f coalesced=%u partial=%u heap_before=%zu heap_after=%zu\n",
        total/200,maximum,s.coalesced_frames,s.partial_refreshes,heap,X4GetStatus().free_heap);
    fflush(stdout);
    // A clean requested while BUSY must survive the earlier frame's completion.
    draw(0); draw(1); X4RefreshFull(); draw(2);
    ok &= !X4IsDisplayBusy() && X4GetStats().full_refreshes == 2;
    X4SetAsyncRefresh(false); draw(3);
    idle(2.5);
    double start=GetTime(); draw(4);
    printf("DISPLAY_AFTER_IDLE total_ms=%.3f\n",(GetTime()-start)*1000); fflush(stdout);
    idle(31);
    start=GetTime(); draw(5);
    printf("DISPLAY_AFTER_EXPIRY total_ms=%.3f\n",(GetTime()-start)*1000);
    start=GetTime(); draw(6);
    printf("DISPLAY_AFTER_RELOAD total_ms=%.3f\n",(GetTime()-start)*1000);
    ok &= !X4GetStats().errors && X4GetStatus().free_heap>=heap;
    printf("DISPLAY_SPEED_%s errors=%u full=%u partial=%u\n",ok ? "PASS" : "FAIL",
        X4GetStats().errors,X4GetStats().full_refreshes,X4GetStats().partial_refreshes);
    fflush(stdout);
    for (;;) { WaitTime(0.02); PollInputEvents(); }
}
