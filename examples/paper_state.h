#ifndef PAPER_STATE_H
#define PAPER_STATE_H
#include <stdbool.h>
#include <stdint.h>
#define PAPER_INITIAL_BOARD 0x73315du
#define PAPER_UNDO_LIMIT 64

typedef enum { PAPER_NO_CARD, PAPER_READY, PAPER_SAVED, PAPER_SAVE_FAILED,
    PAPER_RECOVERED, PAPER_INVALID, PAPER_IMPORTED } PaperSaveStatus;
typedef struct {
    uint32_t board, initial, turns, sequence;
    uint8_t cursor, undo_count, undo[PAPER_UNDO_LIMIT];
    int slot;
    bool loaded;
    PaperSaveStatus status;
    char directory[512];
} PaperState;

uint32_t PaperToggle(uint32_t board, unsigned cell);
void PaperLoad(PaperState *state, const char *storage);
bool PaperSave(PaperState *state);
bool PaperMove(PaperState *state);
bool PaperUndo(PaperState *state);
void PaperRestart(PaperState *state, uint32_t initial);
const char *PaperSaveMessage(const PaperState *state);
#endif
