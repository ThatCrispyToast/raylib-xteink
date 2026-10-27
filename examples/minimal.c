#include "raylib.h"
#include "raylib_x4.h"

int main(void)
{
    InitWindow(480, 800, "Hello X4");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(2);
    int count = 0;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ENTER)) count++;
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello, Xteink!", 30, 80, 40, BLACK);
        DrawText(TextFormat("%d button presses", count), 30, 150, 24, BLACK);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
