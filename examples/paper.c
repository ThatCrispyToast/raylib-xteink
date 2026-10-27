#include "raylib.h"
#include "raylib_x4.h"
#include "paper_state.h"
#include <stdio.h>

static void label(const char *text, int x, int y, int size) { DrawText(text, x, y, size, BLACK); }
static bool pressed(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }
static void header(const char *name, const char *controls)
{
    label("PAPER / RAYLIB", 28, 30, 30);
    label(name, 28, 76, 20);
    DrawLine(28, 112, 452, 112, BLACK);
    DrawLine(28, 718, 452, 718, BLACK);
    label(controls, 28, 738, 18);
    label("POWER: CLEAN    BACK: MENU", 28, 764, 16);
}
static uint32_t new_board(uint32_t previous)
{
    uint32_t board;
    do {
        board=0;
        for (int i=0; i<12; i++) board=PaperToggle(board, GetRandomValue(0,24));
    } while (!board || board == previous);
    return board;
}
int main(void)
{
    InitWindow(X4_WIDTH, X4_HEIGHT, "Paper / raylib for Xteink X4");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(10); SetExitKey(KEY_NULL);
    PaperState state; PaperLoad(&state, X4GetStoragePath());
    int page=state.loaded ? 1 : 0, menu=0, confirm=0;
    bool dirty=true, in_menu=false, help=false, sleep=false;
    const char *notice=NULL;
    Image checker=GenImageChecked(32,32,4,4,WHITE,BLACK);
    Texture2D tile=LoadTextureFromImage(checker); UnloadImage(checker);
    while (!WindowShouldClose() && !sleep) {
        if (IsKeyPressed(KEY_P)) { X4RefreshFull(); dirty=true; }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (confirm) confirm=0;
            else if (help) { help=false; in_menu=true; }
            else { in_menu=!in_menu; menu=0; }
            dirty=true;
        } else if (confirm) {
            if (IsKeyPressed(KEY_ENTER)) {
                PaperRestart(&state, confirm == 1 ? state.initial : new_board(state.initial));
                confirm=0; in_menu=false; page=1; notice=NULL; dirty=true;
            }
        } else if (help) {
            if (IsKeyPressed(KEY_ENTER)) { help=false; in_menu=false; page=1; dirty=true; }
        } else if (in_menu) {
            if (pressed(KEY_UP)) { menu=(menu+7)%8; dirty=true; }
            if (pressed(KEY_DOWN)) { menu=(menu+1)%8; dirty=true; }
            if (IsKeyPressed(KEY_ENTER)) {
                dirty=true;
                switch (menu) {
                    case 0: page=1; in_menu=false; break;
                    case 1: notice=PaperUndo(&state) ? "Last move undone." : "No moves to undo.";
                        page=1; in_menu=false; break;
                    case 2: confirm=1; break;
                    case 3: confirm=2; break;
                    case 4: page=0; in_menu=false; break;
                    case 5: page=2; in_menu=false; break;
                    case 6: help=true; in_menu=false; break;
                    case 7: PaperSave(&state); sleep=true; break;
                }
            }
        } else if (page == 1) {
            int x=state.cursor%5, y=state.cursor/5;
            if (pressed(KEY_LEFT)) { x=(x+4)%5; dirty=true; }
            if (pressed(KEY_RIGHT)) { x=(x+1)%5; dirty=true; }
            if (pressed(KEY_UP)) { y=(y+4)%5; dirty=true; }
            if (pressed(KEY_DOWN)) { y=(y+1)%5; dirty=true; }
            state.cursor=(uint8_t)(y*5+x);
            if (IsKeyPressed(KEY_ENTER)) {
                notice=PaperMove(&state) ? NULL : "Move limit reached. Use Restart."; dirty=true;
            }
        } else {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) { page=page == 0 ? 2 : 0; dirty=true; }
            if (IsKeyPressed(KEY_ENTER)) { page=1; dirty=true; }
        }
        if (!dirty) { WaitTime(0.02); PollInputEvents(); continue; }
        BeginDrawing(); ClearBackground(WHITE);
        if (sleep) {
            X4RefreshFull();
            label("PAPER IS ASLEEP", 28, 230, 34);
            label("Press Power to wake.", 28, 308, 26);
            label(PaperSaveMessage(&state), 28, 380, 20);
            label("This image stays without power.", 28, 440, 20);
            label("A cleaning flash on wake is normal.", 28, 476, 20);
        } else if (confirm) {
            header("CONFIRM", "ENTER: CONFIRM    BACK: CANCEL");
            label(confirm == 1 ? "Restart this puzzle?" : "Start a new puzzle?", 28, 240, 30);
            label("This replaces your current game.", 28, 310, 20);
            label("Your move count will reset.", 28, 350, 20);
        } else if (help) {
            header("HOW TO PLAY", "ENTER: PLAY    BACK: MENU");
            label("LIGHTS OUT", 28, 150, 34);
            label("Make every square white.", 28, 222, 24);
            label("Arrows move around the 5 x 5 grid.", 28, 286, 20);
            label("Enter flips the selected square", 28, 330, 20);
            label("and its neighbors: up, down,", 28, 362, 20);
            label("left and right. Diagonals stay put.", 28, 394, 20);
            label("Back opens undo, restart and", 28, 460, 20);
            label("new puzzle. Every puzzle is solvable.", 28, 492, 20);
            label("Undo remembers your last 64 moves.", 28, 548, 20);
            label("Power cleans, then returns here.", 28, 604, 20);
            label("Choose Sleep in the Back menu.", 28, 642, 20);
        } else if (in_menu) {
            header("MENU", "UP/DOWN: CHOOSE    ENTER: OPEN");
            const char *items[]={"Resume puzzle", "Undo last move", "Restart this puzzle", "New puzzle",
                "Drawing showcase", "Device status", "How to play", "Sleep"};
            for (int i=0; i<8; i++) {
                int y=148+i*61;
                if (i == menu) DrawRectangleLinesEx((Rectangle){22,y-8,436,48}, 3, BLACK);
                label(items[i], 36, y, 24);
            }
            label(PaperSaveMessage(&state),28,666,18);
        } else if (page == 0) {
            header("DRAWING SHOWCASE", "ENTER: PLAY    LEFT/RIGHT: STATUS");
            label("Small screen.",28,148,40); label("Real raylib.",28,198,40);
            label("Shapes, text, textures and input",28,268,20);
            label("on a pocket e-paper display.",28,298,20);
            for (int i=0; i<4; i++) {
                DrawRectangle(28+i*106,356,98,96,(Color){i*85,i*85,i*85,255});
                DrawRectangleLines(28+i*106,356,98,96,BLACK);
                label(TextFormat("%i / 3", i),47+i*106,464,16);
            }
            DrawCircleLines(92,550,42,BLACK);
            DrawTriangle((Vector2){178,590},(Vector2){256,590},(Vector2){217,510},BLACK);
            if (IsTextureValid(tile)) DrawTexturePro(tile,(Rectangle){0,0,32,32},(Rectangle){318,510,80,80},(Vector2){0,0},0,WHITE);
            else label("No texture RAM",300,530,16);
            label("480 x 800 / 192 KB buffers",28,628,20);
            label(PaperSaveMessage(&state),28,666,18);
        } else if (page == 1) {
            header("LIGHTS OUT", "ARROWS: MOVE    ENTER: FLIP");
            label(state.board ? "Make every square white." : "Solved! Back for a new puzzle.",28,148,24);
            label("Enter flips a cross of neighbors.",28,196,20);
            for (int i=0; i<25; i++) {
                int x=35+(i%5)*83, y=252+(i/5)*76;
                DrawRectangle(x,y,70,63,(state.board & (1u<<i)) ? BLACK : WHITE);
                DrawRectangleLines(x,y,70,63,BLACK);
                if (i == state.cursor) DrawRectangleLinesEx((Rectangle){x-4,y-4,78,71},3,BLACK);
            }
            label(TextFormat("%u moves",(unsigned)state.turns),28,640,20);
            label(notice ? notice : "BACK: undo / new puzzle / help",28,666,18);
            label(PaperSaveMessage(&state),28,692,16);
        } else {
            header("DEVICE STATUS", "ENTER: PLAY    LEFT/RIGHT: SHOWCASE");
            X4Stats stats=X4GetStats(); X4Status device=X4GetStatus();
            label("BUILT FOR PAPER",28,144,32);
            label("A quiet frame costs no refresh.",28,214,20);
            label("Full cleans keep the panel clear.",28,246,20);
            label("DISPLAY",28,310,20);
            label(TextFormat("%u full / %u partial updates",stats.full_refreshes,stats.partial_refreshes),28,350,22);
            label("STORAGE",28,424,20); label(PaperSaveMessage(&state),28,464,20);
            label("BATTERY",28,526,20);
            label(device.battery_mv < 0 ? "Desktop emulator" : TextFormat("%.2f V",device.battery_mv/1000.0),28,564,22);
            label("2D / SOFTWARE / OPEN SOURCE",28,622,20);
            label("Power: clean and update this status.",28,662,20);
        }
        EndDrawing(); dirty=false;
        if (X4GetStats().frames == 1) {
            X4Status device=X4GetStatus();
            printf("X4: Paper ready heap=%zu minimum=%zu largest=%zu sd=%d\n",
                device.free_heap,device.minimum_heap,device.largest_block,device.storage_mounted);
        }
    }
    UnloadTexture(tile);
    X4Stats stats=X4GetStats();
    printf("X4: frames=%u full=%u partial=%u unchanged=%u errors=%u buffers=%zu\n",
        stats.frames,stats.full_refreshes,stats.partial_refreshes,stats.unchanged_frames,stats.errors,stats.framebuffer_bytes);
    CloseWindow(); return stats.errors ? 1 : 0;
}
