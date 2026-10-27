#define main tetris_example_main
#include "../examples/tetris.c"
#undef main
#include <stdlib.h>

static uint8_t retained[X4_FRAME_BYTES];
static unsigned int comparisons;
static void compare(const TetrisGame *g)
{
    BeginDrawing(); draw_game(g,false); EndDrawing();
    memcpy(retained,X4GetPanelFrame(),sizeof(retained));
    BeginDrawing(); draw_game(g,true); EndDrawing();
    if (memcmp(retained,X4GetPanelFrame(),sizeof(retained))) {
        fprintf(stderr,"Retained Tetris drawing differs at comparison %u\n",comparisons); exit(1);
    }
    comparisons++;
}
int main(void)
{
    double next=0;
    if (repeat_steps(true,true,1,&next,0.12,4) != 0 ||
        repeat_steps(true,false,1.7,&next,0.12,4) != 4 ||
        repeat_steps(false,false,2.4,&next,0.12,4) != 0 || next != 0 ||
        repeat_steps(true,false,3,&next,0.06,12) != 0 ||
        repeat_steps(true,false,3.7,&next,0.06,12) != 9 ||
        repeat_steps(true,false,20,&next,0.06,12) != 12) {
        fprintf(stderr,"Held-control timing or release reset failed\n"); return 1;
    }
    InitWindow(X4_WIDTH,X4_HEIGHT,"Tetris rendering QA");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(0); X4SetRefreshInterval(UINT_MAX);
    for (unsigned int piece=0; piece<7; piece++) {
        TetrisGame g; TetrisNew(&g,42,true); g.piece=piece;
        BeginDrawing(); draw_game(&g,true); EndDrawing();
        for (int i=0; i<4; i++) { TetrisRotate(&g); compare(&g); }
        while (TetrisMove(&g,-1)) compare(&g);
        while (TetrisMove(&g,1)) compare(&g);
        while (TetrisSoftDrop(&g)) compare(&g);
        TetrisHardDrop(&g); compare(&g);
        TetrisHardDrop(&g); compare(&g);
    }
    for (int rows=1; rows<=4; rows++) {
        TetrisGame g; TetrisNew(&g,123,true); g.piece=TETRIS_I; g.rotation=1; g.x=2;
        g.score=9999;
        for (int y=20-rows; y<20; y++) for (int x=0; x<10; x++) g.board[y][x]=x == 4 ? 0 : 1;
        BeginDrawing(); draw_game(&g,true); EndDrawing();
        TetrisHardDrop(&g); compare(&g);
        TetrisHardDrop(&g); compare(&g);
    }
    TetrisGame menu_game; TetrisNew(&menu_game,42,false);
    for (int practice=0; practice<2; practice++) for (int selection=0; selection<5; selection++) {
        BeginDrawing(); draw_page(PAUSE,&menu_game,selection,practice,false); EndDrawing();
        BeginDrawing(); menu_cursor(selection,(selection+1)%5); EndDrawing();
        memcpy(retained,X4GetPanelFrame(),sizeof(retained));
        BeginDrawing(); draw_page(PAUSE,&menu_game,(selection+1)%5,practice,false); EndDrawing();
        if (memcmp(retained,X4GetPanelFrame(),sizeof(retained))) {
            fprintf(stderr,"Retained menu selection differs\n"); return 1;
        }
        comparisons++;
    }
    printf("PASS: %u retained/full Tetris renders are byte-identical\n",comparisons);
    CloseWindow();
}
