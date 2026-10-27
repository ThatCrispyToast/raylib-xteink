#include "tetris_game.h"
#include <limits.h>
#include <math.h>
#include <string.h>

uint16_t TetrisShape(unsigned int piece, unsigned int rotation)
{
    static const uint16_t shapes[]={0x00f0,0x0066,0x0072,0x0071,0x0074,0x0036,0x0063};
    if (piece >= 7) return 0;
    uint16_t shape=shapes[piece];
    if (piece == TETRIS_O) return shape;
    int size=piece == TETRIS_I ? 4 : 3;
    for (unsigned int turn=0; turn<(rotation%4); turn++) {
        uint16_t rotated=0;
        for (int y=0; y<size; y++) for (int x=0; x<size; x++)
            if (shape & (1u<<(y*4+x))) rotated |= 1u<<(x*4+size-1-y);
        shape=rotated;
    }
    return shape;
}
bool TetrisFits(const TetrisGame *g, int x, int y, unsigned int rotation)
{
    uint16_t shape=TetrisShape(g->piece,rotation);
    if (!shape) return false;
    for (int row=0; row<4; row++) for (int col=0; col<4; col++) if (shape & (1u<<(row*4+col))) {
        int bx=x+col, by=y+row;
        if (bx<0 || bx>=TETRIS_COLS || by>=TETRIS_ROWS) return false;
        if (by>=0 && g->board[by][bx]) return false;
    }
    return true;
}
static uint32_t random_next(TetrisGame *g)
{
    uint32_t x=g->random;
    x^=x<<13; x^=x>>17; x^=x<<5;
    return g->random=x;
}
static uint8_t draw_bag(TetrisGame *g)
{
    if (g->bag_pos == 7) {
        for (int i=0; i<7; i++) g->bag[i]=(uint8_t)i;
        for (int i=6; i>0; i--) {
            unsigned int j=random_next(g)%(i+1);
            uint8_t temp=g->bag[i]; g->bag[i]=g->bag[j]; g->bag[j]=temp;
        }
        g->bag_pos=0;
    }
    return g->bag[g->bag_pos++];
}
static void spawn(TetrisGame *g)
{
    g->piece=g->next[0];
    for (int i=0; i<TETRIS_PREVIEW-1; i++) g->next[i]=g->next[i+1];
    g->next[TETRIS_PREVIEW-1]=draw_bag(g);
    g->rotation=0; g->x=3; g->y=0;
    g->falling=g->grounded=0; g->lock_resets=0;
    g->over=!TetrisFits(g,g->x,g->y,g->rotation);
}
void TetrisNew(TetrisGame *g, uint32_t seed, bool practice)
{
    memset(g,0,sizeof(*g));
    g->random=seed ? seed : 0x4a1b2c3d; g->bag_pos=7; g->practice=practice;
    for (int i=0; i<TETRIS_PREVIEW; i++) g->next[i]=draw_bag(g);
    spawn(g);
}
unsigned int TetrisLevel(const TetrisGame *g) { return g->lines>=190 ? 20 : g->lines/10+1; }
double TetrisFallSeconds(const TetrisGame *g)
{
    double seconds=0.8-0.04*(TetrisLevel(g)-1);
    return seconds<0.12 ? 0.12 : seconds;
}
static void add_score(TetrisGame *g, unsigned int amount)
{ g->score=UINT32_MAX-g->score<amount ? UINT32_MAX : g->score+amount; }
static bool on_ground(const TetrisGame *g) { return !TetrisFits(g,g->x,g->y+1,g->rotation); }
static void reset_lock(TetrisGame *g, bool was_grounded)
{
    if (was_grounded && g->lock_resets<15) { g->grounded=0; g->lock_resets++; }
    if (!on_ground(g)) g->grounded=0;
}
bool TetrisMove(TetrisGame *g, int direction)
{
    if (g->over || (direction != -1 && direction != 1) || !TetrisFits(g,g->x+direction,g->y,g->rotation)) return false;
    bool was_grounded=on_ground(g);
    g->x+=direction; reset_lock(g,was_grounded); return true;
}
bool TetrisRotate(TetrisGame *g)
{
    if (g->over || g->piece == TETRIS_O) return false;
    static const int offsets[][2]={{0,0},{-1,0},{1,0},{-2,0},{2,0},{0,-1},{-1,-1},{1,-1},{0,-2}};
    unsigned int rotation=(g->rotation+1)%4;
    bool was_grounded=on_ground(g);
    for (unsigned int i=0; i<sizeof(offsets)/sizeof(*offsets); i++) {
        int x=g->x+offsets[i][0], y=g->y+offsets[i][1];
        if (!TetrisFits(g,x,y,rotation)) continue;
        g->x=x; g->y=y; g->rotation=(uint8_t)rotation;
        reset_lock(g,was_grounded); return true;
    }
    return false;
}
bool TetrisSoftDrop(TetrisGame *g)
{
    if (g->over || on_ground(g)) return false;
    g->y++; g->falling=g->grounded=0; add_score(g,1); return true;
}
int TetrisLanding(const TetrisGame *g)
{
    int y=g->y;
    while (TetrisFits(g,g->x,y+1,g->rotation)) y++;
    return y;
}
static void lock_piece(TetrisGame *g)
{
    uint16_t shape=TetrisShape(g->piece,g->rotation);
    // A piece extending above the board ends the game without partially writing it.
    for (int row=0; row<4; row++)
        if (g->y+row<0 && ((shape>>(row*4)) & 15)) { g->over=true; return; }
    for (int row=0; row<4; row++) for (int col=0; col<4; col++)
        if (shape & (1u<<(row*4+col))) g->board[g->y+row][g->x+col]=g->piece+1;
    unsigned int cleared=0, level=TetrisLevel(g);
    int write=TETRIS_ROWS-1;
    for (int read=TETRIS_ROWS-1; read>=0; read--) {
        bool full=true;
        for (int x=0; x<TETRIS_COLS; x++) if (!g->board[read][x]) full=false;
        if (full) cleared++;
        else { if (read != write) memcpy(g->board[write],g->board[read],TETRIS_COLS); write--; }
    }
    while (write>=0) memset(g->board[write--],0,TETRIS_COLS);
    static const unsigned int points[]={0,100,300,500,800};
    add_score(g,points[cleared<=4 ? cleared : 4]*level);
    g->lines=UINT32_MAX-g->lines<cleared ? UINT32_MAX : g->lines+cleared;
    if (g->pieces<UINT32_MAX) g->pieces++;
    g->last_clear=cleared;
    spawn(g);
}
void TetrisHardDrop(TetrisGame *g)
{
    if (g->over) return;
    int y=TetrisLanding(g);
    add_score(g,(unsigned int)(y-g->y)*2); g->y=y;
    lock_piece(g);
}
bool TetrisAdvance(TetrisGame *g, double seconds)
{ return TetrisAdvanceClocks(g,seconds,seconds); }
bool TetrisAdvanceClocks(TetrisGame *g, double falling_seconds, double locking_seconds)
{
    if (g->over || g->practice || falling_seconds<0 || locking_seconds<0 ||
        !isfinite(falling_seconds) || !isfinite(locking_seconds)) return false;
    double seconds=falling_seconds>locking_seconds ? falling_seconds : locking_seconds;
    if (!(seconds>0)) return false;
    double fall_rate=falling_seconds/seconds, lock_rate=locking_seconds/seconds;
    bool changed=false;
    double interval=TetrisFallSeconds(g);
    while (seconds>0) {
        if (on_ground(g)) {
            g->grounded+=seconds*lock_rate; g->falling=0;
            if (g->grounded>=0.55-1e-9) { lock_piece(g); return true; }
            return changed;
        }
        double step=fall_rate>0 ? (interval-g->falling)/fall_rate : seconds;
        if (step>seconds) step=seconds;
        if (step<0) step=0;
        g->falling+=step*fall_rate;
        g->grounded=0;
        seconds-=step;
        if (g->falling>=interval-1e-9) {
            g->falling=0;
            g->y++; changed=true;
        }
    }
    return changed;
}
