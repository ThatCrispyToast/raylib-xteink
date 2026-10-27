#include "raylib.h"
#include "raylib_x4.h"
#include "tetris_game.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CELL 27
#define BOARD_X 24
#define BOARD_Y 116
typedef enum { TITLE, PLAY, PAUSE, HELP, CONFIRM, GAME_OVER, SLEEP } Screen;
static void text(const char *label, int x, int y, int size) { DrawText(label,x,y,size,BLACK); }
static void heading(const char *subtitle)
{
    text("TETRIS",24,24,42); text(subtitle,24,80,18);
    DrawLine(24,104,456,104,BLACK);
}
static void controls(int y, bool title)
{
    text("LEFT / RIGHT: MOVE   UP: ROTATE",24,y,18);
    text("DOWN: SOFT DROP   ENTER: DROP",24,y+28,18);
    text(title ? "BACK: SLEEP    POWER: CLEAN" : "BACK: PAUSE    POWER: CLEAN",24,y+56,16);
}
static void tile(int x, int y, int kind, int size)
{
    if (kind == 2) DrawRectangleLinesEx((Rectangle){x+5,y+5,size-10,size-10},2,BLACK);
    else if (kind) {
        DrawRectangle(x+1,y+1,size-2,size-2,BLACK);
        if (size>=24) DrawRectangle(x+5,y+5,6,2,WHITE);
    }
}
static void board_image(const TetrisGame *g, uint8_t image[TETRIS_ROWS][TETRIS_COLS])
{
    for (int y=0; y<TETRIS_ROWS; y++) for (int x=0; x<TETRIS_COLS; x++) image[y][x]=g->board[y][x] ? 1 : 0;
    if (g->over) return;
    uint16_t shape=TetrisShape(g->piece,g->rotation);
    int landing=TetrisLanding(g);
    for (int pass=0; pass<2; pass++) for (int y=0; y<4; y++) for (int x=0; x<4; x++) {
        if (!(shape & (1u<<(y*4+x)))) continue;
        int bx=g->x+x, by=(pass ? g->y : landing)+y;
        if (by>=0 && by<TETRIS_ROWS && bx>=0 && bx<TETRIS_COLS) image[by][bx]=pass ? 1 : 2;
    }
}
static void sidebar(const TetrisGame *g)
{
    DrawRectangle(310,112,146,548,WHITE);
    text("NEXT",318,120,20);
    for (int i=0; i<TETRIS_PREVIEW; i++) {
        DrawRectangleLines(314,146+i*88,140,76,BLACK);
        uint16_t shape=TetrisShape(g->next[i],0);
        for (int y=0; y<4; y++) for (int x=0; x<4; x++)
            if (shape & (1u<<(y*4+x))) tile(340+x*18,156+i*88+y*18,1,18);
    }
    text("SCORE",318,434,18); text(TextFormat("%u",(unsigned)g->score),318,460,20);
    text("LINES",318,514,18); text(TextFormat("%u",(unsigned)g->lines),318,540,24);
    text("LEVEL",318,594,18); text(TextFormat("%u",TetrisLevel(g)),318,620,24);
}
static void draw_game(const TetrisGame *g, bool full)
{
    static uint8_t previous[TETRIS_ROWS][TETRIS_COLS], next[TETRIS_PREVIEW];
    static uint32_t score, lines;
    static unsigned int clear;
    uint8_t image[TETRIS_ROWS][TETRIS_COLS]; board_image(g,image);
    if (full) {
        ClearBackground(WHITE); heading(g->practice ? "PRACTICE / NO AUTOMATIC FALL" : "RELAXED / TIME TO THINK");
        DrawRectangleLinesEx((Rectangle){BOARD_X-2,BOARD_Y-2,CELL*TETRIS_COLS+4,CELL*TETRIS_ROWS+4},2,BLACK);
        controls(708,false);
    }
    for (int y=0; y<TETRIS_ROWS; y++) for (int x=0; x<TETRIS_COLS; x++) if (full || image[y][x] != previous[y][x]) {
        int px=BOARD_X+x*CELL, py=BOARD_Y+y*CELL;
        DrawRectangle(px,py,CELL,CELL,WHITE); tile(px,py,image[y][x],CELL);
    }
    if (full || score != g->score || lines != g->lines || memcmp(next,g->next,sizeof(next))) sidebar(g);
    if (full || clear != g->last_clear) {
        static const char *messages[]={"OUTLINE = LANDING POSITION","SINGLE!","DOUBLE!","TRIPLE!","FOUR LINES!"};
        DrawRectangle(24,672,432,22,WHITE); text(messages[g->last_clear<=4 ? g->last_clear : 4],24,674,18);
    }
    memcpy(previous,image,sizeof(previous)); memcpy(next,g->next,sizeof(next));
    score=g->score; lines=g->lines; clear=g->last_clear;
}
static void option(const char *name, int y, bool selected)
{
    if (selected) DrawRectangleLinesEx((Rectangle){22,y-10,436,54},3,BLACK);
    text(name,38,y,26);
}
static void draw_page(Screen screen, const TetrisGame *g, int menu, bool practice, bool sleep_confirmation)
{
    ClearBackground(WHITE);
    if (screen == TITLE) {
        heading("STACK. CLEAR. REPEAT.");
        text("Choose your pace",24,154,28);
        option("Relaxed",232,!practice); option("Practice",312,practice);
        text(practice ? "Place pieces at your own pace." : "Gentle falling speed for e-paper.",24,408,22);
        text("UP / DOWN: CHOOSE",24,468,20); text("ENTER: START",24,506,28);
        text("Complete rows to clear them.",24,582,22);
        controls(654,true);
        text("Games are not saved between sessions.",24,760,18);
    } else if (screen == PAUSE) {
        heading("PAUSED");
        const char *items[]={"Resume", "Restart", practice ? "Mode: Practice" : "Mode: Relaxed", "How to play", "Sleep"};
        for (int i=0; i<5; i++) option(items[i],164+i*80,i == menu);
        text("UP / DOWN: CHOOSE   ENTER: OPEN",24,624,18);
        text("BACK: RESUME",24,672,22);
        text("Your game waits while paused.",24,742,20);
    } else if (screen == HELP) {
        heading("HOW TO PLAY");
        text("Fill horizontal rows to clear them.",24,154,22);
        text("Keep the stack away from the top.",24,196,22);
        text("The outline shows where you will land.",24,254,20);
        text("Up rotates clockwise; edges allow",24,300,20);
        text("small shifts to make a rotation fit.",24,332,20);
        text("Enter drops and locks the piece.",24,386,22);
        text("Practice: pieces wait for your input.",24,438,20);
        text("Relaxed: pieces fall, then briefly",24,478,20);
        text("wait on landing so you can adjust.",24,510,20);
        text("Clear 10 lines to gain a level.",24,566,22);
        text("Power cleans the display at any time.",24,618,20);
        text("Games are not saved after sleep.",24,660,20);
        text("ENTER / BACK: RETURN",24,744,22);
    } else if (screen == CONFIRM) {
        heading(sleep_confirmation ? "SLEEP" : "RESTART");
        text(sleep_confirmation ? "End this game and sleep?" : "Restart this game?",24,238,28);
        text("Current progress will be lost.",24,304,22);
        text(sleep_confirmation ? "ENTER: SLEEP" : "ENTER: RESTART",24,414,26);
        text("BACK: CANCEL",24,474,26);
    } else if (screen == GAME_OVER) {
        heading("GAME OVER");
        text("The stack reached the top.",24,196,28);
        text(TextFormat("Score   %u",(unsigned)g->score),24,284,30);
        text(TextFormat("Lines   %u",(unsigned)g->lines),24,336,28);
        text(TextFormat("Level   %u",TetrisLevel(g)),24,388,28);
        text("ENTER: TRY AGAIN",24,532,28); text("BACK: MENU",24,590,24);
    } else if (screen == SLEEP) {
        heading("ASLEEP");
        text("Press Power to wake.",24,264,30);
        text("A new game awaits.",24,326,24);
    }
}
static void new_game(TetrisGame *g, bool practice)
{ TetrisNew(g,(uint32_t)GetRandomValue(1,INT_MAX),practice); }
int main(void)
{
    InitWindow(X4_WIDTH,X4_HEIGHT,"Tetris / raylib for Xteink X4");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL); SetTargetFPS(0); X4SetRefreshInterval(UINT_MAX);
    TetrisGame game={0};
    Screen screen=TITLE, drawn=SLEEP, confirmation_return=PAUSE;
    int menu=0;
    bool practice=false, dirty=true, sleep_confirmation=false, confirmation_shown=false, discard_enter=false;
    double last_tick=GetTime();
    while (!WindowShouldClose() && screen != SLEEP) {
        double now=GetTime(), elapsed=now-last_tick; last_tick=now;
        int keys[64], count=0, key;
        while (count<64 && (key=GetKeyPressed()) != 0)
            if (!(discard_enter && key == KEY_ENTER)) keys[count++]=key;
        discard_enter=false;
        if (!count && screen == PLAY) {
            const int repeat[]={KEY_LEFT,KEY_RIGHT,KEY_DOWN};
            for (int i=0; i<3; i++) if (IsKeyPressedRepeat(repeat[i])) keys[count++]=repeat[i];
        }
        uint32_t pieces=game.pieces;
        bool new_piece=false, block_play=false;
        Screen before=screen;
        for (int i=0; i<count; i++) {
            key=keys[i];
            if (key == KEY_P) { X4RefreshFull(); dirty=true; continue; }
            if (key == KEY_ESCAPE) {
                if (screen == TITLE) { confirmation_return=TITLE; sleep_confirmation=true; confirmation_shown=false; screen=CONFIRM; }
                else if (screen == CONFIRM) screen=confirmation_return;
                else if (screen == HELP) screen=PAUSE;
                else if (screen == PAUSE) screen=game.over ? GAME_OVER : PLAY;
                else { screen=PAUSE; menu=0; }
                dirty=true; continue;
            }
            if (screen == TITLE) {
                if (key == KEY_UP || key == KEY_DOWN) { practice=!practice; dirty=true; }
                if (key == KEY_ENTER) { new_game(&game,practice); screen=PLAY; dirty=new_piece=block_play=true; }
            } else if (screen == PLAY && !block_play) {
                if (key == KEY_LEFT) dirty |= TetrisMove(&game,-1);
                if (key == KEY_RIGHT) dirty |= TetrisMove(&game,1);
                if (key == KEY_UP) dirty |= TetrisRotate(&game);
                if (key == KEY_DOWN) dirty |= TetrisSoftDrop(&game);
                if (key == KEY_ENTER) { TetrisHardDrop(&game); dirty=new_piece=block_play=true; }
            } else if (screen == PAUSE) {
                if (key == KEY_UP) { menu=(menu+4)%5; dirty=true; }
                if (key == KEY_DOWN) { menu=(menu+1)%5; dirty=true; }
                if (key == KEY_ENTER) {
                    dirty=true;
                    if (menu == 0) screen=game.over ? GAME_OVER : PLAY;
                    else if (menu == 2) { practice=!practice; game.practice=practice; game.falling=game.grounded=0; }
                    else if (menu == 3) screen=HELP;
                    else { sleep_confirmation=menu == 4; confirmation_return=PAUSE; confirmation_shown=false; screen=CONFIRM; }
                }
            } else if (screen == HELP && key == KEY_ENTER) { screen=PAUSE; dirty=true; }
            else if (screen == CONFIRM && confirmation_shown && key == KEY_ENTER) {
                if (sleep_confirmation) screen=SLEEP;
                else { new_game(&game,practice); screen=PLAY; new_piece=block_play=true; }
                dirty=true;
            } else if (screen == GAME_OVER && key == KEY_ENTER && !block_play) {
                new_game(&game,practice); screen=PLAY; dirty=new_piece=block_play=true;
            }
        }
        if (screen == PLAY && before == PLAY && !new_piece) dirty |= TetrisAdvance(&game,elapsed);
        if (game.pieces != pieces) new_piece=true;
        if (screen == PLAY && game.over) { screen=GAME_OVER; dirty=true; }
        if (!dirty) { WaitTime(0.02); PollInputEvents(); continue; }
        BeginDrawing();
        if (screen == PLAY) draw_game(&game,drawn != PLAY);
        else draw_page(screen,&game,menu,practice,sleep_confirmation);
        EndDrawing();
        // Only time when a settled board is visible advances gravity and lock delay.
        last_tick=GetTime(); dirty=false;
        if (new_piece || (screen == GAME_OVER && drawn != GAME_OVER)) discard_enter=true;
        if (screen == CONFIRM && !confirmation_shown) { confirmation_shown=true; discard_enter=true; }
        drawn=screen;
        if (X4GetStats().frames == 1) {
            X4Status s=X4GetStatus();
            printf("TETRIS: ready heap=%zu minimum=%zu largest=%zu sd=%d\n",s.free_heap,s.minimum_heap,s.largest_block,s.storage_mounted);
            fflush(stdout);
        }
    }
    X4Stats s=X4GetStats();
    printf("TETRIS: screen=%d score=%u lines=%u pieces=%u x=%d y=%d rotation=%u practice=%d frames=%u full=%u partial=%u errors=%u\n",
        screen,(unsigned)game.score,(unsigned)game.lines,(unsigned)game.pieces,game.x,game.y,game.rotation,practice,s.frames,s.full_refreshes,s.partial_refreshes,s.errors);
    CloseWindow(); return s.errors ? 1 : 0;
}
