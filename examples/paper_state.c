#include "paper_state.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RECORD_SIZE 112
#define BOARD_LIMIT (1u << 25)
static uint32_t read32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void write32(unsigned char *p, uint32_t v)
{ for (int i=0; i<4; i++) p[i] = (unsigned char)(v >> (i*8)); }
static uint32_t checksum(const unsigned char *data)
{
    uint32_t hash = 2166136261u;
    for (int i=0; i<RECORD_SIZE-4; i++) hash = (hash ^ data[i])*16777619u;
    return hash;
}
static bool path_for(const PaperState *s, int slot, char *path, size_t size)
{
    int n = snprintf(path, size, "%s/paper-%c.sav", s->directory, 'a'+slot);
    return n > 0 && (size_t)n < size;
}
// Fixed-size records bound reads even for corrupt or unexpectedly large files.
static int read_record(PaperState *s, int slot)
{
    unsigned char data[RECORD_SIZE];
    char path[544]; struct stat info;
    if (!path_for(s, slot, path, sizeof(path))) return -1;
    if (stat(path, &info)) return errno == ENOENT ? 0 : -1;
    if (!S_ISREG(info.st_mode) || info.st_size != RECORD_SIZE) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    bool ok = fread(data, 1, sizeof(data), f) == sizeof(data) && !ferror(f);
    ok = fclose(f) == 0 && ok;
    if (!ok || memcmp(data, "X4PAPER2", 8) || read32(data+108) != checksum(data)) return -1;
    uint32_t board=read32(data+12), initial=read32(data+16), turns=read32(data+20);
    if (board >= BOARD_LIMIT || initial == 0 || initial >= BOARD_LIMIT || data[24] >= 25 ||
        data[25] > PAPER_UNDO_LIMIT || data[25] > turns) return -1;
    for (int i=0; i<data[25]; i++) if (data[32+i] >= 25) return -1;
    s->sequence=read32(data+8); s->board=board; s->initial=initial; s->turns=turns;
    s->cursor=data[24]; s->undo_count=data[25]; memcpy(s->undo, data+32, PAPER_UNDO_LIMIT);
    s->slot=slot; s->loaded=true;
    return 1;
}
static bool legacy_number(char **end, uint32_t limit, uint32_t *value)
{
    while (isspace((unsigned char)**end)) (*end)++;
    if (!isdigit((unsigned char)**end)) return false;
    char *start=*end; errno=0;
    unsigned long long n=strtoull(start, end, 10);
    if (errno || *end == start || n > limit) return false;
    *value=(uint32_t)n; return true;
}
void PaperLoad(PaperState *s, const char *storage)
{
    memset(s, 0, sizeof(*s));
    s->board=s->initial=PAPER_INITIAL_BOARD; s->slot=-1; s->status=PAPER_NO_CARD;
    if (!storage || !*storage) return;
    int n=snprintf(s->directory, sizeof(s->directory), "%s/raylib-x4", storage);
    if (n < 0 || (size_t)n >= sizeof(s->directory)) {
        s->directory[0]=0; s->status=PAPER_SAVE_FAILED; return;
    }
    s->status=PAPER_READY;
    PaperState other=*s;
    int a=read_record(s, 0), b=read_record(&other, 1);
    if (b == 1 && (a != 1 || (other.sequence != s->sequence &&
        (uint32_t)(other.sequence-s->sequence) < 0x80000000u))) *s=other;
    if (a == 1 || b == 1) {
        s->status=(a < 0 || b < 0) ? PAPER_RECOVERED : PAPER_SAVED;
        return;
    }
    if (a < 0 || b < 0) { s->status=PAPER_INVALID; return; }
    // Import valid old saves without ever modifying the generic filename.
    char path[544], text[80]; struct stat info;
    n=snprintf(path, sizeof(path), "%s/paper.txt", storage);
    if (n < 0 || (size_t)n >= sizeof(path) || stat(path, &info)) return;
    if (!S_ISREG(info.st_mode) || info.st_size <= 0 || info.st_size >= (long)sizeof(text)) {
        s->status=PAPER_INVALID; return;
    }
    FILE *f=fopen(path, "rb");
    if (!f) { s->status=PAPER_INVALID; return; }
    size_t count=fread(text, 1, sizeof(text)-1, f);
    bool ok=!ferror(f); ok=fclose(f) == 0 && ok; text[count]=0;
    char *end=text; uint32_t board, turns;
    ok=ok && legacy_number(&end, BOARD_LIMIT-1, &board);
    ok=ok && isspace((unsigned char)*end) && legacy_number(&end, UINT32_MAX, &turns);
    while (isspace((unsigned char)*end)) end++;
    if (!ok || end != text+count) { s->status=PAPER_INVALID; return; }
    s->board=board; s->turns=turns; s->loaded=true; s->status=PAPER_IMPORTED;
}
bool PaperSave(PaperState *s)
{
    if (!s->directory[0]) return false;
    struct stat info;
    if ((mkdir(s->directory, 0777) && errno != EEXIST) ||
        stat(s->directory, &info) || !S_ISDIR(info.st_mode)) goto failed;
    unsigned char data[RECORD_SIZE]={0};
    memcpy(data, "X4PAPER2", 8);
    write32(data+8, s->sequence+1); write32(data+12, s->board);
    write32(data+16, s->initial); write32(data+20, s->turns);
    data[24]=s->cursor; data[25]=s->undo_count; memcpy(data+32, s->undo, PAPER_UNDO_LIMIT);
    write32(data+108, checksum(data));
    int slot=s->slot == 0 ? 1 : 0;
    char path[544];
    if (!path_for(s, slot, path, sizeof(path))) goto failed;
    FILE *f=fopen(path, "wb");
    if (!f) goto failed;
    bool ok=fwrite(data, 1, sizeof(data), f) == sizeof(data);
    if (ok) ok=fflush(f) == 0;
    if (ok) ok=fsync(fileno(f)) == 0;
    ok=fclose(f) == 0 && ok;
    if (!ok) goto failed;
    s->sequence++; s->slot=slot; s->status=PAPER_SAVED; s->loaded=true;
    printf("Paper: saved board=%u turns=%u cursor=%u slot=%d\n",(unsigned)s->board,(unsigned)s->turns,(unsigned)s->cursor,slot);
    return true;
failed:
    s->status=PAPER_SAVE_FAILED;
    return false;
}
uint32_t PaperToggle(uint32_t board, unsigned cell)
{
    if (cell >= 25) return board;
    board ^= 1u<<cell;
    if (cell%5) board ^= 1u<<(cell-1);
    if (cell%5 < 4) board ^= 1u<<(cell+1);
    if (cell >= 5) board ^= 1u<<(cell-5);
    if (cell < 20) board ^= 1u<<(cell+5);
    return board;
}
bool PaperMove(PaperState *s)
{
    if (s->turns == UINT32_MAX) return false;
    if (s->undo_count == PAPER_UNDO_LIMIT) {
        memmove(s->undo, s->undo+1, PAPER_UNDO_LIMIT-1); s->undo_count--;
    }
    s->undo[s->undo_count++]=s->cursor;
    s->board=PaperToggle(s->board, s->cursor); s->turns++; PaperSave(s);
    return true;
}
bool PaperUndo(PaperState *s)
{
    if (!s->undo_count) return false;
    s->cursor=s->undo[--s->undo_count]; s->board=PaperToggle(s->board, s->cursor);
    s->turns--; PaperSave(s); return true;
}
void PaperRestart(PaperState *s, uint32_t initial)
{
    if (!initial || initial >= BOARD_LIMIT) return;
    s->board=s->initial=initial; s->turns=0; s->cursor=0; s->undo_count=0;
    memset(s->undo, 0, sizeof(s->undo)); PaperSave(s);
}
const char *PaperSaveMessage(const PaperState *s)
{
    switch (s->status) {
        case PAPER_NO_CARD: return "No SD card. Play is not saved.";
        case PAPER_READY: return "SD mounted. Save on first move.";
        case PAPER_SAVED: return "Progress saved to SD.";
        case PAPER_SAVE_FAILED: return "NOT SAVED. Check SD card.";
        case PAPER_RECOVERED: return "Recovered the previous valid save.";
        case PAPER_INVALID: return "Invalid save ignored. New game.";
        case PAPER_IMPORTED: return "Old save loaded; original kept.";
    }
    return "";
}
