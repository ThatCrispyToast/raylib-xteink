#include "raylib.h"
#include "raylib_x4.h"

int main(void)
{
    InitWindow(X4_WIDTH, X4_HEIGHT, "Hello Xteink");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(10);
    int count = 0;
    bool dirty = true;
    while (!WindowShouldClose()) {
        int key;
        while ((key = GetKeyPressed()) != 0) {
            if (key == KEY_ENTER) { count++; dirty = true; }
        }
        if (!dirty) { WaitTime(0.02); PollInputEvents(); continue; }
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello, Xteink!", 30, 80, 40, BLACK);
        DrawText(TextFormat("%d button presses", count), 30, 150, 24, BLACK);
        EndDrawing();
        dirty = false;
    }
    CloseWindow();
    return 0;
}
