#include "raylib.h"
#include "raylib_x4.h"
#include <stdio.h>
#include <string.h>

static void label(const char *text, int x, int y, int size) { DrawText(text, x, y, size, BLACK); }
static void header(int page)
{
    label("PAPER / RAYLIB", 28, 30, 30);
    label(TextFormat("XTEINK X4     %02i / 03", page + 1), 28, 76, 20);
    DrawLine(28, 112, 452, 112, BLACK);
    DrawLine(28, 718, 452, 718, BLACK);
    label("< > PAGE    ENTER SELECT", 28, 738, 20);
    label("P CLEAN     BACK EXIT", 28, 764, 16);
}

int main(void)
{
    InitWindow(X4_WIDTH, X4_HEIGHT, "Paper / raylib for Xteink X4");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(10);
    int page = 0, cursor = 0, turns = 0;
    unsigned int board = 0x73315d; // Generated from a solved board, so the puzzle is solvable.
    bool dirty = true, save_ok = true;
    const char *storage = X4GetStoragePath();
    char save_path[512] = {0};
    if (storage) {
        snprintf(save_path, sizeof(save_path), "%s/paper.txt", storage);
        char *saved = LoadFileText(save_path);
        if (saved) { unsigned int b; int t; if (sscanf(saved, "%u %d", &b, &t) == 2 && b < (1u << 25) && t >= 0) { board = b; turns = t; } UnloadFileText(saved); }
    }
    Image checker = GenImageChecked(32, 32, 4, 4, WHITE, BLACK);
    Texture2D tile = LoadTextureFromImage(checker);
    UnloadImage(checker);
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_LEFT)) { page = (page + 2)%3; dirty = true; }
        if (IsKeyPressed(KEY_RIGHT)) { page = (page + 1)%3; dirty = true; }
        if (IsKeyPressed(KEY_P)) { X4RefreshFull(); dirty = true; }
        if (page == 1) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) { cursor = (cursor + 24)%25; dirty = true; }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) { cursor = (cursor + 1)%25; dirty = true; }
            if (IsKeyPressed(KEY_ENTER)) {
                int x = cursor%5, y = cursor/5;
                board ^= 1u << cursor;
                if (x > 0) board ^= 1u << (cursor - 1);
                if (x < 4) board ^= 1u << (cursor + 1);
                if (y > 0) board ^= 1u << (cursor - 5);
                if (y < 4) board ^= 1u << (cursor + 5);
                turns++; dirty = true;
                if (storage) { char state[64]; snprintf(state, sizeof(state), "%u %d\n", board, turns); save_ok = SaveFileText(save_path, state); }
            }
        } else if (IsKeyPressed(KEY_ENTER)) { page = (page + 1)%3; dirty = true; }
        if (dirty) {
            BeginDrawing(); ClearBackground(WHITE); header(page);
            if (page == 0) {
                label("Small screen.", 28, 148, 40);
                label("Real raylib.", 28, 198, 40);
                label("Shapes, text, textures and input", 28, 268, 20);
                label("on a pocket e-paper display.", 28, 298, 20);
                for (int i = 0; i < 4; i++) DrawRectangle(28 + i*106, 356, 106, 96, (Color){i*85,i*85,i*85,255});
                DrawCircleLines(92, 526, 42, BLACK);
                DrawTriangle((Vector2){178,566}, (Vector2){256,566}, (Vector2){217,486}, BLACK);
                DrawTexturePro(tile, (Rectangle){0,0,32,32}, (Rectangle){318,486,80,80}, (Vector2){0,0}, 0, WHITE);
                label("480 x 800 / 192 KB buffers", 28, 622, 20);
                label("Turn the page to play Lights Out.", 28, 658, 20);
            } else if (page == 1) {
                label("LIGHTS OUT", 28, 144, 36);
                label(board ? "Turn every square white." : "Solved. Beautifully done.", 28, 200, 22);
                for (int i = 0; i < 25; i++) {
                    int x = 35 + (i%5)*83, y = 262 + (i/5)*76;
                    DrawRectangle(x, y, 70, 63, (board & (1u << i)) ? BLACK : WHITE);
                    DrawRectangleLines(x, y, 70, 63, BLACK);
                    if (i == cursor) DrawRectangleLinesEx((Rectangle){x-4,y-4,78,71}, 3, BLACK);
                }
                label(TextFormat("%i moves    UP/DOWN: move cursor", turns), 28, 666, 20);
            } else {
                X4Stats stats = X4GetStats();
                label("BUILT FOR PAPER", 28, 144, 32);
                label("A quiet frame costs no refresh.", 28, 214, 20);
                label("Full cleans keep the panel clear.", 28, 246, 20);
                label("DISPLAY", 28, 326, 20);
                label(TextFormat("%u full / %u partial updates", stats.full_refreshes, stats.partial_refreshes), 28, 366, 22);
                label("STORAGE", 28, 446, 20);
                label(!storage ? "No SD card mounted." : save_ok ? "SD ready. Moves save automatically." : "Save failed. Check the SD card.", 28, 486, 20);
                X4Status device = X4GetStatus();
                label("BATTERY", 28, 538, 20);
                label(device.battery_mv < 0 ? "Desktop emulator" : TextFormat("%.2f V", device.battery_mv/1000.0), 28, 574, 22);
                label("2D / SOFTWARE / OPEN SOURCE", 28, 622, 20);
                label("Press P for a full cleaning refresh.", 28, 662, 20);
            }
            EndDrawing(); dirty = false;
            if (X4GetStats().frames == 1) {
                X4Status device = X4GetStatus();
                printf("X4: Paper ready heap=%zu minimum=%zu largest=%zu sd=%d\n",
                    device.free_heap,device.minimum_heap,device.largest_block,device.storage_mounted);
            }
        } else { WaitTime(0.02); PollInputEvents(); }
    }
    UnloadTexture(tile);
    X4Stats stats = X4GetStats();
    printf("X4: frames=%u full=%u partial=%u unchanged=%u errors=%u buffers=%zu\n",
        stats.frames, stats.full_refreshes, stats.partial_refreshes, stats.unchanged_frames, stats.errors, stats.framebuffer_bytes);
    CloseWindow();
    return stats.errors ? 1 : 0;
}
