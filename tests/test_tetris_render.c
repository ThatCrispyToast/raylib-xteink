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
    printf("PASS: %u retained/full Tetris renders are byte-identical\n",comparisons);
    CloseWindow();
}
