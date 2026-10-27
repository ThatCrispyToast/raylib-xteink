// Real app and panel timings with scripted gravity and held-key scenarios.
#include "raylib.h"
#include "raylib_x4.h"
#include "../examples/tetris_game.h"
#include <stdio.h>
static TetrisGame *observed;
static unsigned int scenario, frames;
static bool delivered, drain, practice_selected, pressed;
static double epoch, repeat_at;
static void pace_new(TetrisGame *g, uint32_t seed, bool practice)
{ TetrisNew(g,seed,practice); observed=g; }
static int held_key(void) { return scenario == 1 ? KEY_DOWN : KEY_RIGHT; }
static int pace_key(void)
{
    if (drain) { drain=false; return 0; }
    if (delivered || !frames) return 0;
    if (!observed) {
        delivered=true;
        if (scenario && !practice_selected) { practice_selected=true; return KEY_DOWN; }
        return KEY_ENTER;
    }
    if (scenario && !pressed) {
        pressed=delivered=true; repeat_at=GetTime()+0.45; return held_key();
    }
    return 0;
}
static bool pace_down(int key) { return scenario && pressed && key == held_key(); }
static bool pace_repeat(int key)
{
    if (!pace_down(key) || GetTime()<repeat_at) return false;
    repeat_at=GetTime()+0.15; return true;
}
static bool pace_closed(void)
{
    if (!observed || !epoch) return false;
    if (GetTime()-epoch>50) return true;
    return scenario == 0 ? observed->y>=6 : scenario == 1 ?
        !TetrisFits(observed,observed->x,observed->y+1,observed->rotation) :
        !TetrisFits(observed,observed->x+1,observed->y,observed->rotation);
}
static int pace_random(int min, int max) { (void)min; (void)max; return 42; }
static void pace_end(void)
{
    EndDrawing(); frames++; delivered=false; drain=true;
    if (!observed) return;
    if (!epoch) epoch=GetTime();
    X4Stats s=X4GetStats();
    printf("TETRIS_PACE mode=%u seconds=%.3f x=%d y=%d pieces=%u full=%u partial=%u errors=%u\n",
        scenario,GetTime()-epoch,observed->x,observed->y,(unsigned)observed->pieces,
        s.full_refreshes,s.partial_refreshes,s.errors);
    fflush(stdout);
}
#undef main
#define main pace_application
#define TetrisNew pace_new
#define GetKeyPressed pace_key
#define IsKeyDown pace_down
#define IsKeyPressedRepeat pace_repeat
#define GetRandomValue pace_random
#define WindowShouldClose pace_closed
#define EndDrawing pace_end
#include "../examples/tetris.c"
#undef main
int x4_application_main(void)
{
    bool ok=true;
    for (scenario=0; scenario<3; scenario++) {
        observed=NULL; frames=0; epoch=0; delivered=drain=practice_selected=pressed=false;
        int result=pace_application();
        ok &= result == 0 && GetTime()-epoch<50 && X4GetStats().errors == 0;
    }
    printf("TETRIS_PACE_%s\n",ok ? "PASS" : "FAIL"); fflush(stdout);
    for (;;) WaitTime(0.05);
}
