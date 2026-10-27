#include "tetris_game.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static unsigned int checks;
static TetrisGame empty(unsigned int piece)
{
    TetrisGame g; TetrisNew(&g,123,false);
    g.piece=piece; return g;
}
static unsigned int cells(const TetrisGame *g)
{
    unsigned int result=0;
    for (int y=0; y<TETRIS_ROWS; y++) for (int x=0; x<TETRIS_COLS; x++) result+=g->board[y][x]!=0;
    return result;
}
static void shapes_and_movement(void)
{
    for (unsigned int piece=0; piece<7; piece++) for (unsigned int r=0; r<4; r++) {
        uint16_t shape=TetrisShape(piece,r);
        unsigned int bits=0;
        for (unsigned int i=0; i<16; i++) bits+=(shape>>i)&1;
        CHECK(bits == 4 && shape == TetrisShape(piece,r+4));
        TetrisGame g=empty(piece); g.rotation=r;
        CHECK(TetrisFits(&g,g.x,g.y,r));
        CHECK(!TetrisFits(&g,-4,0,r) && !TetrisFits(&g,10,0,r));
        int landing=TetrisLanding(&g);
        CHECK(TetrisFits(&g,g.x,landing,r) && !TetrisFits(&g,g.x,landing+1,r));
        while (TetrisMove(&g,-1)) {}
        int left=g.x;
        CHECK(!TetrisMove(&g,-1) && g.x == left);
        while (TetrisMove(&g,1)) {}
        CHECK(!TetrisMove(&g,1) && !TetrisMove(&g,0));
        TetrisHardDrop(&g); CHECK(cells(&g) == 4 && g.pieces == 1 && !g.over);
    }
    CHECK(TetrisShape(7,0) == 0);
    TetrisGame g=empty(TETRIS_T); g.y=5;
    for (int i=0; i<4; i++) CHECK(TetrisRotate(&g));
    CHECK(g.rotation == 0 && g.x == 3 && g.y == 5);
    g=empty(TETRIS_I); g.rotation=1; g.x=-2; g.y=5;
    CHECK(TetrisFits(&g,g.x,g.y,g.rotation));
    CHECK(TetrisRotate(&g) && g.x == 0); // Two-column wall adjustment.
    g=empty(TETRIS_T); g.y=18;
    CHECK(TetrisRotate(&g) && g.y == 17); // Floor adjustment.
    g=empty(TETRIS_O); CHECK(!TetrisRotate(&g) && g.rotation == 0);
    g=empty(TETRIS_T); g.y=8;
    memset(g.board,1,sizeof(g.board));
    uint16_t shape=TetrisShape(g.piece,0);
    for (int y=0; y<4; y++) for (int x=0; x<4; x++)
        if (shape & (1u<<(y*4+x))) g.board[g.y+y][g.x+x]=0;
    CHECK(TetrisFits(&g,g.x,g.y,0));
    CHECK(!TetrisRotate(&g) && g.rotation == 0 && g.x == 3 && g.y == 8);
}
static void clears_and_scoring(void)
{
    const unsigned int scores[]={0,100,300,500,800};
    for (int rows=1; rows<=4; rows++) {
        TetrisGame g=empty(TETRIS_I); g.rotation=1; g.x=2; g.lines=9;
        for (int y=20-rows; y<20; y++) for (int x=0; x<10; x++) g.board[y][x]=x == 4 ? 0 : 2;
        g.board[10][1]=7;
        TetrisHardDrop(&g);
        CHECK(g.lines == 9+(unsigned int)rows && g.last_clear == (unsigned int)rows);
        CHECK(g.score == 32+scores[rows] && g.pieces == 1);
        CHECK(g.board[10+rows][1] == 7 && g.board[10][1] == 0);
        CHECK(cells(&g) == 5-(unsigned int)rows && TetrisLevel(&g) == 2);
    }
    TetrisGame g=empty(TETRIS_O);
    CHECK(TetrisSoftDrop(&g) && g.score == 1 && g.y == 1);
    TetrisHardDrop(&g); CHECK(g.score == 35);
    g=empty(TETRIS_I); g.lines=UINT32_MAX; g.score=UINT32_MAX-1;
    CHECK(TetrisLevel(&g) == 20 && TetrisFallSeconds(&g) == 0.5);
    TetrisHardDrop(&g); CHECK(g.score == UINT32_MAX);
    // Nonadjacent full rows collapse independently and preserve the row between them.
    g=empty(TETRIS_I); g.rotation=1; g.x=2;
    for (int x=0; x<10; x++) { g.board[17][x]=x == 4 ? 0 : 1; g.board[19][x]=x == 4 ? 0 : 1; }
    g.board[18][0]=5;
    TetrisHardDrop(&g); CHECK(g.lines == 2 && g.board[19][0] == 5 && cells(&g) == 3);
}
static void timing_and_end(void)
{
    TetrisGame g=empty(TETRIS_O);
    CHECK(!TetrisAdvance(&g,1.59) && g.y == 0);
    CHECK(TetrisAdvance(&g,0.02) && g.y == 1);
    CHECK(!TetrisAdvance(&g,-1) && !TetrisAdvance(&g,INFINITY) && !TetrisAdvance(&g,NAN));
    g.practice=true; TetrisGame before=g;
    CHECK(!TetrisAdvance(&g,10000) && !memcmp(&g,&before,sizeof(g)));
    g.practice=false; g.y=TetrisLanding(&g); g.falling=g.grounded=0;
    CHECK(!TetrisAdvance(&g,0.5) && g.pieces == 0);
    CHECK(TetrisMove(&g,-1) && g.grounded == 0 && g.lock_resets == 1);
    CHECK(!TetrisAdvance(&g,0.5) && TetrisAdvance(&g,0.06));
    CHECK(g.pieces == 1 && g.y == 0 && cells(&g) == 4);
    g=empty(TETRIS_O); g.y=18;
    for (int i=0; i<15; i++) { CHECK(!TetrisAdvance(&g,0.1)); CHECK(TetrisMove(&g,i%2 ? 1 : -1)); }
    CHECK(g.lock_resets == 15);
    CHECK(!TetrisAdvance(&g,0.5)); CHECK(TetrisMove(&g,1));
    CHECK(TetrisAdvance(&g,0.06) && g.pieces == 1);
    g=empty(TETRIS_O); CHECK(TetrisAdvance(&g,10000));
    CHECK(g.pieces == 1 && g.y == 0); // A long delay cannot consume an unseen next piece.
    g=empty(TETRIS_O); g.y=18; memset(g.board[0],1,10); memset(g.board[1],1,10);
    g.board[0][0]=g.board[1][0]=0;
    TetrisHardDrop(&g); CHECK(g.over);
    before=g; TetrisHardDrop(&g);
    CHECK(!TetrisMove(&g,1) && !TetrisRotate(&g) && !TetrisSoftDrop(&g) && !TetrisAdvance(&g,20));
    CHECK(!memcmp(&g,&before,sizeof(g)));
    g=empty(TETRIS_T); g.y=-1; memset(g.board[1],1,10);
    CHECK(TetrisFits(&g,g.x,g.y,0) && TetrisLanding(&g) == -1);
    before=g; TetrisHardDrop(&g);
    CHECK(g.over && !memcmp(g.board,before.board,sizeof(g.board)));
}
static void bags_and_stress(void)
{
    TetrisGame g, same; TetrisNew(&g,0,false); TetrisNew(&same,0,false);
    CHECK(!memcmp(&g,&same,sizeof(g)));
    for (int bag=0; bag<100; bag++) {
        unsigned int seen=0;
        for (int i=0; i<7; i++) {
            CHECK(!(seen & (1u<<g.piece))); seen|=1u<<g.piece;
            memset(g.board,0,sizeof(g.board));
            TetrisHardDrop(&g); CHECK(!g.over);
        }
        CHECK(seen == 127);
    }
    for (uint32_t seed=1; seed<=100; seed++) {
        TetrisNew(&g,seed,false);
        for (int i=0; i<500 && !g.over; i++) {
            TetrisMove(&g,i%2 ? 1 : -1);
            if (i%3) TetrisRotate(&g);
            if (i%5) TetrisSoftDrop(&g);
            CHECK(TetrisFits(&g,g.x,g.y,g.rotation));
            TetrisHardDrop(&g);
            for (int y=0; y<20; y++) for (int x=0; x<10; x++) CHECK(g.board[y][x]<=7);
        }
    }
}
int main(void)
{
    shapes_and_movement(); clears_and_scoring(); timing_and_end(); bags_and_stress();
    printf("PASS: %u Tetris checks (pieces, bags, kicks, clears, scoring, timing, lock delay, top-out, stress)\n",checks);
}
