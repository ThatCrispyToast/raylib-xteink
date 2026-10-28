#include "raylib.h"
#include "raylib_x4.h"
#include "paper_state.h"
#include <stdio.h>
#include <limits.h>

static void label(const char *text, int x, int y, int size) { DrawText(text, x, y, size, BLACK); }
static void header(const char *name, const char *controls)
{
    label("PAPER / RAYLIB", 28, 30, 30);
    label(name, 28, 76, 20);
    DrawLine(28, 112, 452, 112, BLACK);
    DrawLine(28, 718, 452, 718, BLACK);
    label(XTEINK_HAS_TOUCH ? "TAP TO SELECT / PLAY" : controls, 28, 738, 18);
    if (XTEINK_HAS_TOUCH) { label("CLEAN",28,764,16); label("MENU",272,764,16); }
    else label("POWER: CLEAN    BACK: MENU",28,764,16);
}
static void cell(uint32_t board, int index, bool selected)
{
    int x=35+(index%5)*83, y=252+(index/5)*76;
    DrawRectangle(x-4,y-4,78,71,WHITE);
    DrawRectangle(x,y,70,63,(board & (1u<<index)) ? BLACK : WHITE);
    DrawRectangleLines(x,y,70,63,BLACK);
    if (selected) DrawRectangleLinesEx((Rectangle){x-4,y-4,78,71},3,BLACK);
}
static void menu_selection(int menu, Color color)
{ DrawRectangleLinesEx((Rectangle){22,140+menu*61,436,48},3,color); }
static void puzzle_heading(uint32_t board)
{ label(board ? "Make every square white." : "Solved! Back for a new puzzle.",28,148,24); }
static void puzzle_footer(const PaperState *state, const char *notice)
{
    label(TextFormat("%u moves",(unsigned)state->turns),28,640,20);
    label(notice ? notice : "BACK: undo / new puzzle / help",28,666,18);
    label(PaperSaveMessage(state),28,692,16);
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
    InitWindow(X4_WIDTH, X4_HEIGHT, "Paper / raylib for Xteink");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(10); SetExitKey(KEY_NULL);
    X4SetRefreshInterval(UINT_MAX); // Clean on wake or Power; navigation does not schedule flashes.
    PaperState state; PaperLoad(&state, X4GetStoragePath());
    int page=state.loaded ? 1 : 0, menu=0, confirm=0;
    bool dirty=true, in_menu=false, help=false, sleep=false;
    const char *notice=NULL;
    bool confirmation_shown=false, discard_queued_enter=false;
    struct { int kind, page, cursor, menu; uint32_t board, turns; PaperSaveStatus status; const char *notice; } drawn={0};
    Image checker=GenImageChecked(32,32,4,4,WHITE,BLACK);
    Texture2D tile=LoadTextureFromImage(checker); UnloadImage(checker);
    while (!WindowShouldClose() && !sleep) {
        int keys[68], count=0, key;
        while (count<64 && (key=GetKeyPressed()) != 0)
            if (!(discard_queued_enter && key == KEY_ENTER)) keys[count++]=key;
        if (XTEINK_HAS_TOUCH && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && count<64) {
            Vector2 p=GetTouchPosition(0);
            int tap=0;
            if (p.y>=760) tap=p.x<240 ? KEY_P : KEY_ESCAPE;
            else if (confirm || help) tap=KEY_ENTER;
            else if (in_menu && p.y>=140 && p.y<628) {
                menu=((int)p.y-140)/61; tap=KEY_ENTER;
            } else if (!in_menu && page==1 && p.x>=35 && p.x<445 && p.y>=252 && p.y<632) {
                int x=((int)p.x-35)/83, y=((int)p.y-252)/76;
                if ((int)p.x-(35+x*83)<70 && (int)p.y-(252+y*76)<63) {
                    state.cursor=(uint8_t)(y*5+x); tap=KEY_ENTER;
                }
            } else if (!in_menu && page==2 && p.y>=608 && p.y<650) {
                XteinkSetFrontlight(p.x<170 ? 0 : 60,p.x>=320 ? 100 : 0);
            } else if (!in_menu && page!=1 && p.y>=720) tap=p.x<240 ? KEY_ENTER : KEY_RIGHT;
            if (tap && !(discard_queued_enter && tap==KEY_ENTER)) keys[count++]=tap;
        }
        discard_queued_enter=false;
        const int arrows[]={KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN};
        if (!count) for (int i=0; i<4; i++) if (IsKeyPressedRepeat(arrows[i])) keys[count++]=arrows[i];
        for (int event=0; event<count; event++) {
            key=keys[event];
            if (key == KEY_P) { X4RefreshFull(); dirty=true; }
            if (key == KEY_ESCAPE) {
                if (confirm) confirm=0;
                else if (help) { help=false; in_menu=true; }
                else { in_menu=!in_menu; menu=0; }
                dirty=true;
            } else if (confirm) {
                if ((key == KEY_ENTER) && confirmation_shown) {
                    PaperRestart(&state, confirm == 1 ? state.initial : new_board(state.initial));
                    confirm=0; in_menu=false; page=1; notice=NULL; dirty=true;
                }
            } else if (help) {
                if (key == KEY_ENTER) { help=false; in_menu=false; page=1; dirty=true; }
            } else if (in_menu) {
                if (key == KEY_UP) { menu=(menu+7)%8; dirty=true; }
                if (key == KEY_DOWN) { menu=(menu+1)%8; dirty=true; }
                if (key == KEY_ENTER) {
                    dirty=true;
                    switch (menu) {
                        case 0: page=1; in_menu=false; break;
                        case 1: notice=PaperUndo(&state) ? "Last move undone." : "No moves to undo.";
                            page=1; in_menu=false; break;
                        case 2: confirm=1; confirmation_shown=false; break;
                        case 3: confirm=2; confirmation_shown=false; break;
                        case 4: page=0; in_menu=false; break;
                        case 5: page=2; in_menu=false; break;
                        case 6: help=true; in_menu=false; break;
                        case 7: PaperSave(&state); sleep=true; break;
                    }
                }
            } else if (page == 1) {
                int x=state.cursor%5, y=state.cursor/5;
                if (key == KEY_LEFT) { x=(x+4)%5; dirty=true; }
                if (key == KEY_RIGHT) { x=(x+1)%5; dirty=true; }
                if (key == KEY_UP) { y=(y+4)%5; dirty=true; }
                if (key == KEY_DOWN) { y=(y+1)%5; dirty=true; }
                state.cursor=(uint8_t)(y*5+x);
                if (key == KEY_ENTER) {
                    notice=PaperMove(&state) ? NULL : "Move limit reached. Use Restart."; dirty=true;
                }
            } else {
                if ((key == KEY_LEFT) || (key == KEY_RIGHT)) { page=page == 0 ? 2 : 0; dirty=true; }
                if (key == KEY_ENTER) { page=1; dirty=true; }
            }
            if (sleep) break;
        }
        if (!dirty) { WaitTime(0.02); PollInputEvents(); continue; }
        int kind=(!sleep && !confirm && !help) ? (in_menu ? 2 : page == 1 ? 1 : 0) : 0;
        bool puzzle_only=kind == 1 && drawn.kind == 1;
        bool menu_only=kind == 2 && drawn.kind == 2 && drawn.status == state.status;
        BeginDrawing();
        if (puzzle_only) {
            uint32_t changed=drawn.board^state.board;
            for (int i=0; i<25; i++) if ((changed & (1u<<i)) || i == drawn.cursor || i == state.cursor)
                cell(state.board,i,i == state.cursor);
            if (!drawn.board != !state.board) {
                DrawRectangle(28,148,424,24,WHITE); puzzle_heading(state.board);
            }
            if (drawn.turns != state.turns || drawn.status != state.status || drawn.notice != notice) {
                DrawRectangle(28,640,424,70,WHITE); puzzle_footer(&state,notice);
            }
        } else if (menu_only) {
            menu_selection(drawn.menu,WHITE); menu_selection(menu,BLACK);
        } else {
            ClearBackground(WHITE);
            if (sleep) {
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
                label(XTEINK_HAS_TOUCH ? "Tap a square on the 5 x 5 grid." : "Arrows move around the 5 x 5 grid.",28,286,20);
                label(XTEINK_HAS_TOUCH ? "Your tap flips that square" : "Enter flips the selected square",28,330,20);
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
                    if (i == menu) menu_selection(i,BLACK);
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
                label(TextFormat("%d x %d / %d KB buffers", X4_WIDTH, X4_HEIGHT,
                    (X4_GRAY_BYTES + 2*X4_FRAME_BYTES)/1000),28,628,20);
                label(PaperSaveMessage(&state),28,666,18);
                if (XTEINK_HAS_TOUCH) {
                    DrawRectangle(28,732,424,24,WHITE); label("PLAY",28,738,18); label("STATUS",272,738,18);
                }
            } else if (page == 1) {
                header("LIGHTS OUT", "ARROWS: MOVE    ENTER: FLIP");
                puzzle_heading(state.board);
                label(XTEINK_HAS_TOUCH ? "Tap flips a cross of neighbors." : "Enter flips a cross of neighbors.",28,196,20);
                for (int i=0; i<25; i++) cell(state.board,i,i == state.cursor);
                puzzle_footer(&state,notice);
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
                label(device.battery_mv < 0 ? "Voltage unavailable" : TextFormat("%.2f V",device.battery_mv/1000.0),28,564,22);
                if (XTEINK_HAS_FRONTLIGHT) {
                    label("LIGHT",28,594,16);
                    const char *lights[]={"OFF","COOL","WARM"};
                    for (int i=0; i<3; i++) {
                        int x=28+i*149;
                        DrawRectangleLines(x,614,126,36,BLACK);
                        label(lights[i],x+(126-MeasureText(lights[i],18))/2,624,18);
                    }
                } else label("2D / SOFTWARE / OPEN SOURCE",28,622,20);
                label("Power: clean and update this status.",28,662,20);
                if (XTEINK_HAS_TOUCH) {
                    DrawRectangle(28,732,424,24,WHITE); label("PLAY",28,738,18); label("SHOWCASE",272,738,18);
                }
            }
        }
        X4SetAsyncRefresh(kind == drawn.kind && page == drawn.page && !confirm && !sleep);
        EndDrawing();
        dirty=false;
        drawn.kind=kind; drawn.page=page; drawn.cursor=state.cursor; drawn.menu=menu;
        drawn.board=state.board; drawn.turns=state.turns; drawn.status=state.status; drawn.notice=notice;
        if (confirm && !confirmation_shown) {
            // A confirmation requires a fresh press after its dialog is visible.
            discard_queued_enter=true;
            confirmation_shown=true;
        }
        if (X4GetStats().frames == 1) {
            X4Status device=X4GetStatus();
            printf("X4: Paper ready heap=%zu minimum=%zu largest=%zu sd=%d\n",
                device.free_heap,device.minimum_heap,device.largest_block,device.storage_mounted);
        }
    }
    X4WaitDisplay();
    UnloadTexture(tile);
    X4Stats stats=X4GetStats();
    printf("X4: frames=%u full=%u partial=%u unchanged=%u errors=%u buffers=%zu\n",
        stats.frames,stats.full_refreshes,stats.partial_refreshes,stats.unchanged_frames,stats.errors,stats.framebuffer_bytes);
    CloseWindow(); return stats.errors ? 1 : 0;
}
