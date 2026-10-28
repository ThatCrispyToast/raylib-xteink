#include "raylib.h"
#include "rlgl.h"
#include "hal_test.h"
#include "x4_gray2.h"
#include "x4_input_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
extern void *swGetColorBuffer(int *width, int *height);

static bool white(int x, int y)
{
    unsigned int p = (X4_WIDTH - 1 - x)*X4_HEIGHT + y;
    return (X4GetPanelFrame()[p/8] & (0x80u >> (p%8))) != 0;
}
static unsigned int gray(int x, int y)
{
    uint8_t c[4];
    x4_gray2_read8(c, swGetColorBuffer(NULL, NULL), (X4_HEIGHT - 1 - y)*X4_WIDTH + x);
    return c[0];
}
static void frame(int x)
{
    BeginDrawing(); ClearBackground(WHITE); DrawRectangle(x, 30, 20, 20, BLACK); EndDrawing();
}

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    char directory[]="/tmp/x4-file-test-XXXXXX";
    CHECK(mkdtemp(directory));
    int bytes=1;
    CHECK(LoadFileText(directory) == NULL);
    CHECK(LoadFileData(directory, &bytes) == NULL && bytes == 0);
    char filename[128]; snprintf(filename,sizeof(filename),"%s/large",directory);
    FILE *file=fopen(filename,"wb"); CHECK(file);
    CHECK(ftruncate(fileno(file),(off_t)INT_MAX+1) == 0); CHECK(fclose(file) == 0);
    CHECK(LoadFileText(filename) == NULL);
    CHECK(LoadFileData(filename,&bytes) == NULL && bytes == 0);
    CHECK(SaveFileText(filename,"small\n"));
    char *text=LoadFileText(filename); CHECK(text && !strcmp(text,"small\n")); UnloadFileText(text);
    CHECK(unlink(filename) == 0 && rmdir(directory) == 0);
    CHECK(x4_decode_buttons(4095, 4095, false) == 0);
    const int front[] = {3470,2655,1470,3};
    for (int i = 0; i < 4; i++) CHECK(x4_decode_buttons(front[i], 4095, false) == (1u << i));
    CHECK(x4_decode_buttons(4095,2205,true) == ((1u<<X4_UP)|(1u<<X4_POWER)));
    CHECK(x4_decode_buttons(-1,-1,false) == 0);
    X4InputQueue queue={0}; X4Input queued={0};
    for (int i=0; i<X4_INPUT_CAPACITY+5; i++) x4_input_push(&queue,i%X4_BUTTON_COUNT);
    x4_input_drain(&queue,&queued);
    CHECK(queued.press_count == X4_INPUT_CAPACITY && queue.dropped == 5 && !queue.count);
    for (int i=0; i<X4_INPUT_CAPACITY; i++) CHECK(queued.press_order[i] == i%X4_BUTTON_COUNT);
    // Compare the byte-oriented conversion against the original per-pixel definition.
    static uint8_t input_gray[X4_GRAY_BYTES], converted[X4_FRAME_BYTES], reference[X4_FRAME_BYTES];
    unsigned int random=7;
    for (int i=0; i<X4_GRAY_BYTES; i++) { random=random*1664525u+1013904223u; input_gray[i]=random>>24; }
    const uint8_t bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    for (int y=0; y<X4_HEIGHT; y++) for (int x=0; x<X4_WIDTH; x++) {
        unsigned int i=(X4_HEIGHT-1-y)*X4_WIDTH+x, shade=(input_gray[i/4]>>(6-2*(i%4))) & 3;
        unsigned int bit=(X4_WIDTH-1-x)*X4_HEIGHT+y;
        if (shade == 3 || (shade && shade*16 > bayer[y%4][x%4]*3+1)) reference[bit/8] |= 0x80u>>(bit%8);
    }
    x4_convert_frame(input_gray,converted); CHECK(!memcmp(reference,converted,sizeof(reference)));
    uint8_t packed = 0;
    for (int i = 0; i < 4; i++) { float c[] = {i/3.0f,i/3.0f,i/3.0f,1}; x4_gray2_write(&packed,c,i); }
    CHECK(packed == 0x1b);

    InitWindow(X4_WIDTH,X4_HEIGHT,"test"); CHECK(IsWindowReady()); SetTargetFPS(0); SetExitKey(KEY_NULL);
    CHECK(GetScreenWidth() == X4_WIDTH && GetScreenHeight() == X4_HEIGHT);
    frame(20); CHECK(!white(25,35) && white(19,35) && white(40,35));
    CHECK(X4GetStats().full_refreshes == 1);
    frame(20); CHECK(X4GetStats().unchanged_frames == 1);
    X4SetRefreshInterval(2); frame(21); frame(22); frame(23);
    CHECK(X4GetStats().full_refreshes == 2 && X4GetStats().partial_refreshes == 2);
    X4RefreshFull(); frame(23); CHECK(X4GetStats().full_refreshes == 3);
    CHECK(X4GetStats().framebuffer_bytes == X4_GRAY_BYTES + 2*X4_FRAME_BYTES);

    BeginDrawing(); ClearBackground(WHITE);
    DrawRectangle(0,0,1,1,BLACK); DrawRectangle(X4_WIDTH-1,0,1,1,BLACK);
    DrawRectangle(0,X4_HEIGHT-1,1,1,BLACK); DrawRectangle(X4_WIDTH-1,X4_HEIGHT-1,1,1,BLACK);
    DrawRectangle(-20,-20,30,30,BLACK);
    BeginScissorMode(100,100,10,10); ClearBackground(BLACK); EndScissorMode();
    DrawRectangle(20,100,10,10,Fade(BLACK,0.5f));
    DrawTriangle((Vector2){200,100},(Vector2){200,150},(Vector2){250,150},BLACK);
    DrawCircle(300,200,20,BLACK);
    Camera2D camera = { .offset={20,20}, .zoom=1 };
    BeginMode2D(camera); DrawRectangle(300,300,10,10,BLACK); EndMode2D();
    DrawLine(-10,400,490,400,BLACK);
    DrawLineEx((Vector2){-10,0},(Vector2){490,0},20,BLACK);
    EndDrawing();
    CHECK(!white(0,0) && !white(X4_WIDTH-1,0) && !white(0,X4_HEIGHT-1) && !white(X4_WIDTH-1,X4_HEIGHT-1));
    CHECK(!white(105,105) && white(99,105) && white(110,105) && white(105,110));
    CHECK(gray(25,105) == 85 || gray(25,105) == 170);
    CHECK(!white(210,140) && !white(300,200) && white(321,200));
    CHECK(!white(325,325) && white(305,305));
    CHECK(!white(250,400));

    Image image = GenImageColor(8,8,BLACK); ImageDrawPixel(&image,4,4,WHITE);
    Texture2D texture = LoadTextureFromImage(image); CHECK(IsTextureValid(texture)); UnloadImage(image);
    BeginDrawing(); ClearBackground(WHITE); DrawTexture(texture,50,50,WHITE); DrawText("X4",100,100,30,BLACK); EndDrawing();
    CHECK(!white(50,50) && white(54,54));
    unsigned int ink = 0;
    for (int y=100;y<130;y++) for (int x=100;x<150;x++) ink += !white(x,y);
    CHECK(ink > 50 && ink < 700);
    UnloadTexture(texture);

    Image label = ImageText("Aa 012", 10, BLACK);
    CHECK(IsImageValid(label));
    Color *colors = LoadImageColors(label);
    int opaque = 0, transparent = 0;
    for (int i=0;i<label.width*label.height;i++) { opaque += colors[i].a == 255; transparent += colors[i].a == 0; }
    CHECK(opaque > 10 && transparent > 10);
    UnloadImageColors(colors); UnloadImage(label);
    Texture2D handles[40] = {0};
    image = GenImageColor(2,2,WHITE);
    int count = 0;
    for (;count<40;count++) { handles[count]=LoadTextureFromImage(image); if (!IsTextureValid(handles[count])) break; }
    CHECK(count > 0 && count < 40);
    for (int i=0;i<count;i++) UnloadTexture(handles[i]);
    texture=LoadTextureFromImage(image); CHECK(IsTextureValid(texture));
    UnloadTexture(texture); UnloadImage(image);

    image=GenImageColor(8,8,(Color){17,83,149,211});
    texture=LoadTextureFromImage(image);
    Image copy=LoadImageFromTexture(texture);
    CHECK(IsImageValid(copy) && memcmp(image.data,copy.data,8*8*4)==0);
    UnloadImage(copy); UnloadTexture(texture); UnloadImage(image);

    unsigned char replacement[] = {0,255};
    UpdateTextureRec(GetFontDefault().texture,(Rectangle){0,0,1,1},replacement);
    Image atlas = LoadImageFromTexture(GetFontDefault().texture);
    CHECK(IsImageValid(atlas));
    Color corner = GetImageColor(atlas,0,0);
    CHECK(corner.r == 0 && corner.a == 255);
    UnloadImage(atlas);

    test_input=(X4Input){.pressed=(1u<<X4_RIGHT)|(1u<<X4_CONFIRM), .press_count=3,
        .press_order={X4_RIGHT,X4_RIGHT,X4_CONFIRM}};
    PollInputEvents();
    CHECK(GetKeyPressed() == KEY_RIGHT && GetKeyPressed() == KEY_RIGHT && GetKeyPressed() == KEY_ENTER && !GetKeyPressed());
    test_input = (X4Input){ .down=1u<<X4_CONFIRM, .pressed=1u<<X4_CONFIRM };
    PollInputEvents(); CHECK(IsKeyPressed(KEY_ENTER) && IsKeyDown(KEY_ENTER)); CHECK(GetKeyPressed() == KEY_ENTER);
    test_time += 0.5; PollInputEvents(); CHECK(!IsKeyPressed(KEY_ENTER) && IsKeyPressedRepeat(KEY_ENTER));
    // Release/repress during a blocking refresh must produce a new edge even if last seen held.
    test_input.pressed = 1u<<X4_CONFIRM; PollInputEvents(); CHECK(IsKeyPressed(KEY_ENTER));
    test_input.down = 0; PollInputEvents(); CHECK(IsKeyReleased(KEY_ENTER));
    test_input.pressed = 1u<<X4_UP; PollInputEvents(); CHECK(IsKeyPressed(KEY_UP));
    PollInputEvents(); CHECK(IsKeyReleased(KEY_UP));

    uint8_t saved[X4_FRAME_BYTES]; memcpy(saved,X4GetPanelFrame(),sizeof(saved));
    test_present_ok = false; frame(33);
    CHECK(X4GetStats().errors == 1 && memcmp(saved,X4GetPanelFrame(),sizeof(saved)) == 0);
    CHECK(WindowShouldClose());
    test_present_ok = true; frame(33); CHECK(X4GetStats().full_refreshes == 4);
    X4SetRefreshInterval(UINT_MAX); X4SetAsyncRefresh(true); test_present_delay=0.4;
    unsigned int submissions=test_present_count, partials=X4GetStats().partial_refreshes;
    double started=test_time;
    frame(34); CHECK(test_time == started && X4IsDisplayBusy() && !X4GetPanelFrame());
    frame(35); frame(36); frame(37);
    CHECK(test_present_count == submissions+1 && X4GetStats().coalesced_frames == 2);
    test_time+=0.4; PollInputEvents();
    CHECK(test_present_count == submissions+2 && X4GetStats().partial_refreshes == partials+1);
    CHECK(X4WaitDisplay() && !X4IsDisplayBusy());
    CHECK(test_present_buffer_stable && !white(37,35) && white(36,35));
    CHECK(X4GetStats().partial_refreshes == partials+2);
    // Explicit clean while busy supersedes queued snapshots and survives completion.
    frame(38); frame(39); X4RefreshFull(); frame(40);
    CHECK(!X4IsDisplayBusy() && X4GetStats().full_refreshes == 5 && !white(40,35));
    frame(41); frame(42);
    CloseWindow(); CHECK(!IsWindowReady());
    CHECK(!X4IsDisplayBusy() && test_present_buffer_stable && !white(42,35));
    test_present_delay=0;
    InitWindow(X4_HEIGHT,X4_WIDTH,"invalid"); CHECK(!IsWindowReady());
    InitWindow(X4_WIDTH,X4_HEIGHT,"reopen"); CHECK(IsWindowReady()); frame(20);
    atlas = LoadImageFromTexture(GetFontDefault().texture);
    corner = GetImageColor(atlas,0,0); CHECK(corner.r == 255 && corner.a == 0);
    UnloadImage(atlas);
    X4SetAsyncRefresh(true); test_present_delay=0.4; frame(21);
    CHECK(X4IsDisplayBusy()); test_present_ok=false; test_time+=0.4;
    PollInputEvents();
    CHECK(WindowShouldClose() && X4GetStats().errors == 1 && !X4GetPanelFrame());
    PollInputEvents(); CHECK(X4GetStats().errors == 1);
    test_present_ok=true; test_present_delay=0; CloseWindow();
    puts("PASS: rendering, clipping, textures, text, camera, input, refresh policy, failure recovery, lifecycle");
    return 0;
}
