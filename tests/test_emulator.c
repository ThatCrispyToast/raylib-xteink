#include "raylib.h"
#include "raylib_x4.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"emulator check failed: %s\n",#x); exit(1); } } while (0)
int main(void)
{
    InitWindow(X4_WIDTH,X4_HEIGHT,"X4 emulator integration"); CHECK(IsWindowReady());
    SetTargetFPS(0); SetExitKey(KEY_NULL);
    for (int i=0; i<6; i++) {
        BeginDrawing(); ClearBackground(WHITE);
        DrawRectangle(20,30,30,40,BLACK);
        DrawRectangle(400,700,25,25,BLACK);
        DrawText("X4",100,100,30,BLACK);
        EndDrawing();
        if (i == 0) CHECK(IsKeyPressed(KEY_RIGHT) && IsKeyDown(KEY_RIGHT));
        if (i == 1) CHECK(IsKeyReleased(KEY_RIGHT));
        if (i == 2) CHECK(IsKeyPressed(KEY_ENTER));
        if (i == 3) { CHECK(IsKeyReleased(KEY_ENTER) && IsKeyPressed(KEY_P)); X4RefreshFull(); }
        if (i == 4) CHECK(IsKeyReleased(KEY_P));
    }
    CHECK(WindowShouldClose());
    CHECK(X4GetStats().full_refreshes == 2 && X4GetStats().unchanged_frames == 4);
    CloseWindow();
    puts("PASS: microreader emulator, SDL events, tap latching, refresh requests, quit");
}
