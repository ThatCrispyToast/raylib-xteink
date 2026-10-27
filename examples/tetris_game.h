#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H
#include <stdbool.h>
#include <stdint.h>

#define TETRIS_COLS 10
#define TETRIS_ROWS 20
#define TETRIS_PREVIEW 3
enum { TETRIS_I, TETRIS_O, TETRIS_T, TETRIS_J, TETRIS_L, TETRIS_S, TETRIS_Z };
typedef struct TetrisGame {
    uint8_t board[TETRIS_ROWS][TETRIS_COLS]; // Zero is empty; occupied cells are piece + 1.
    uint8_t piece, rotation, next[TETRIS_PREVIEW], bag[7], bag_pos, lock_resets;
    int x, y;
    uint32_t random, score, lines, pieces;
    unsigned int last_clear;
    double falling, grounded;
    bool over, practice;
} TetrisGame;

void TetrisNew(TetrisGame *game, uint32_t seed, bool practice);
uint16_t TetrisShape(unsigned int piece, unsigned int rotation);
bool TetrisFits(const TetrisGame *game, int x, int y, unsigned int rotation);
bool TetrisMove(TetrisGame *game, int direction);
bool TetrisRotate(TetrisGame *game);
bool TetrisSoftDrop(TetrisGame *game);
void TetrisHardDrop(TetrisGame *game);
int TetrisLanding(const TetrisGame *game);
bool TetrisAdvance(TetrisGame *game, double seconds);
unsigned int TetrisLevel(const TetrisGame *game);
double TetrisFallSeconds(const TetrisGame *game);
#endif
