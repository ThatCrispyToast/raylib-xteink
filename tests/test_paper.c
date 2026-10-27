#include "paper_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static void put(const char *path, const char *text)
{ FILE *f=fopen(path,"wb"); CHECK(f); CHECK(fputs(text,f) >= 0); CHECK(fclose(f) == 0); }
int main(void)
{
    PaperState s, loaded;
    PaperLoad(&s,NULL); CHECK(s.status == PAPER_NO_CARD); CHECK(PaperMove(&s));
    CHECK(s.turns == 1 && s.status == PAPER_NO_CARD); CHECK(PaperUndo(&s));
    CHECK(s.board == PAPER_INITIAL_BOARD && s.turns == 0 && !PaperUndo(&s));
    unsigned moves[]={0,3,6,12,21};
    for (unsigned i=0; i<5; i++) { s.cursor=moves[i]; CHECK(PaperMove(&s)); }
    CHECK(!s.board && s.turns == 5);
    CHECK(PaperUndo(&s) && s.board && s.turns == 4);
    PaperRestart(&s,PAPER_INITIAL_BOARD); CHECK(s.board == PAPER_INITIAL_BOARD && !s.turns && !s.undo_count);
    s.turns=UINT32_MAX; CHECK(!PaperMove(&s) && s.turns == UINT32_MAX);
    char directory[]="/tmp/x4-paper-test-XXXXXX"; CHECK(mkdtemp(directory));
    char legacy[256], folder[256], a[256], b[256];
    snprintf(legacy,sizeof(legacy),"%s/paper.txt",directory);
    snprintf(folder,sizeof(folder),"%s/raylib-x4",directory);
    snprintf(a,sizeof(a),"%s/raylib-x4/paper-a.sav",directory);
    snprintf(b,sizeof(b),"%s/raylib-x4/paper-b.sav",directory);
    CHECK(mkdir(legacy,0700) == 0); PaperLoad(&s,directory);
    CHECK(s.status == PAPER_INVALID && !s.loaded); CHECK(rmdir(legacy) == 0);
    const char *invalid[]={"personal notes\n","8388607 4 garbage\n","1 4294967296\n","33554432 0\n","1 -1\n","1 2 3\n","1+2\n","184467440737095516161 0\n"};
    for (unsigned i=0; i<sizeof(invalid)/sizeof(*invalid); i++) {
        put(legacy,invalid[i]); PaperLoad(&s,directory); CHECK(!s.loaded && s.status == PAPER_INVALID);
    }
    put(legacy,"7549277 2147483647\n"); PaperLoad(&s,directory);
    CHECK(s.loaded && s.status == PAPER_IMPORTED && s.turns == 2147483647u);
    CHECK(PaperMove(&s) && s.turns == 2147483648u);
    FILE *f=fopen(legacy,"rb"); char original[64]={0}; CHECK(f);
    CHECK(fread(original,1,sizeof(original)-1,f) > 0); CHECK(fclose(f) == 0);
    CHECK(!strcmp(original,"7549277 2147483647\n"));
    PaperLoad(&loaded,directory); CHECK(loaded.turns == s.turns && loaded.board == s.board);
    CHECK(PaperUndo(&loaded) && loaded.turns == 2147483647u && loaded.board == 7549277u);
    PaperRestart(&s,PAPER_INITIAL_BOARD);
    for (int i=0; i<70; i++) { s.cursor=i%25; CHECK(PaperMove(&s)); }
    CHECK(s.undo_count == 64 && s.turns == 70);
    PaperLoad(&loaded,directory); CHECK(loaded.board == s.board && loaded.cursor == s.cursor && loaded.undo_count == 64);
    for (int i=0; i<64; i++) CHECK(PaperUndo(&loaded));
    CHECK(loaded.turns == 6 && !PaperUndo(&loaded));
    PaperLoad(&s,directory); uint32_t previous=s.board;
    s.cursor=8; CHECK(PaperMove(&s));
    put(s.slot == 0 ? a : b,"interrupted write"); PaperLoad(&loaded,directory);
    CHECK(loaded.status == PAPER_RECOVERED && loaded.board == previous);
    CHECK(PaperMove(&loaded)); PaperLoad(&s,directory); CHECK(s.status == PAPER_SAVED);
    put(a,"corrupt"); put(b,"corrupt"); PaperLoad(&s,directory);
    CHECK(s.status == PAPER_INVALID && !s.loaded);
    CHECK(unlink(a) == 0 && unlink(b) == 0 && rmdir(folder) == 0 && unlink(legacy) == 0);
    put(folder,"conflicting file"); PaperLoad(&s,directory);
    CHECK(PaperMove(&s) && s.status == PAPER_SAVE_FAILED);
    CHECK(unlink(folder) == 0 && rmdir(directory) == 0);
    puts("PASS: gameplay, undo, strict legacy import, overflow, namespaced journal, corruption recovery, unavailable storage");
    return 0;
}
