// Runs against the real FAT SD and renderer in an isolated temporary directory.
#include "raylib.h"
#include "raylib_x4.h"
#include "../examples/paper_state.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { printf("PAPER QA FAIL line=%d: %s\n",__LINE__,#x); return 1; } checks++; } while (0)
int main(void)
{
    unsigned checks=0;
    InitWindow(480,800,"Paper storage QA"); CHECK(IsWindowReady());
    BeginDrawing(); ClearBackground(WHITE); DrawText("PAPER STORAGE QA",28,180,30,BLACK);
    DrawText("Testing SD saves and recovery.",28,250,22,BLACK); EndDrawing();
    WaitTime(3);
    const char *sd=X4GetStoragePath(); CHECK(sd);
    char root[128];
    snprintf(root,sizeof(root),"%s/paper-qa-%08x",sd,GetRandomValue(1,0x7fffffff));
    CHECK(mkdir(root,0700) == 0);
    CHECK(LoadFileText(root) == NULL);
    int bytes=1; CHECK(LoadFileData(root,&bytes) == NULL && bytes == 0);
    PaperState s, loaded; PaperLoad(&s,root); CHECK(s.status == PAPER_READY);
    for (int i=0; i<70; i++) {
        s.cursor=i%25; CHECK(PaperMove(&s)); CHECK(s.status == PAPER_SAVED);
        PaperLoad(&loaded,root);
        CHECK(loaded.board == s.board && loaded.turns == s.turns && loaded.cursor == s.cursor);
    }
    CHECK(loaded.undo_count == 64);
    CHECK(PaperUndo(&loaded) && loaded.turns == 69);
    PaperLoad(&s,root); CHECK(s.board == loaded.board);
    uint32_t previous=s.board; s.cursor=4; CHECK(PaperMove(&s));
    char a[160], b[160], folder[160];
    snprintf(a,sizeof(a),"%s/raylib-x4/paper-a.sav",root);
    snprintf(b,sizeof(b),"%s/raylib-x4/paper-b.sav",root);
    snprintf(folder,sizeof(folder),"%s/raylib-x4",root);
    CHECK(SaveFileText(s.slot == 0 ? a : b,"interrupted"));
    PaperLoad(&loaded,root); CHECK(loaded.status == PAPER_RECOVERED && loaded.board == previous);
    CHECK(PaperMove(&loaded) && loaded.status == PAPER_SAVED);
    PaperLoad(&s,root); CHECK(s.status == PAPER_SAVED);
    PaperRestart(&s,PAPER_INITIAL_BOARD); CHECK(s.status == PAPER_SAVED && !s.turns);
    CHECK(unlink(a) == 0 && unlink(b) == 0 && rmdir(folder) == 0 && rmdir(root) == 0);
    CHECK(heap_caps_check_integrity_all(true)); CHECK(X4GetStats().errors == 0);
    X4Status status=X4GetStatus();
    printf("PAPER QA PASS checks=%u heap=%zu minimum=%zu stack=%u\n",checks,status.free_heap,
        status.minimum_heap,(unsigned)uxTaskGetStackHighWaterMark(NULL));
    BeginDrawing(); ClearBackground(WHITE); DrawText("PAPER QA PASSED",28,200,32,BLACK);
    DrawText(TextFormat("%u checks on physical SD",checks),28,270,24,BLACK);
    DrawText("Game files were kept intact.",28,330,22,BLACK); EndDrawing();
    for (;;) { WaitTime(0.05); PollInputEvents(); }
}
