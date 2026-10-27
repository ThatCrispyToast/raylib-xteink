// Hardware acceptance application. USB commands use the same renderer and device drivers as Paper.
#include "raylib.h"
#include "rlgl.h"
#include "raylib_x4.h"
#include "diagnostic_image.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_heap_caps.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned int scene, revision, input_events;
static unsigned int pressed_seen;
static void record_presses(void)
{
    const int keys[]={KEY_ESCAPE,KEY_ENTER,KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN,KEY_P};
    for (int i=0;i<7;i++) if (IsKeyPressed(keys[i])) pressed_seen |= 1u<<i;
}
static uint32_t hash_frame(void)
{
    const uint8_t *frame = X4GetPanelFrame();
    uint32_t hash = 2166136261u;
    if (frame) for (int i=0; i<X4_FRAME_BYTES; i++) hash = (hash ^ frame[i])*16777619u;
    return frame ? hash : 0;
}

static void status(const char *event)
{
    X4Stats s = X4GetStats(); X4Status h = X4GetStatus();
    printf("X4 {\"event\":\"%s\",\"scene\":%u,\"revision\":%u,\"frames\":%u,\"full\":%u,\"partial\":%u,"
        "\"skipped\":%u,\"errors\":%u,\"convert_ms\":%.3f,\"present_ms\":%.3f,\"hash\":%lu,"
        "\"heap\":%zu,\"minimum_heap\":%zu,\"largest_block\":%zu,\"stack_free\":%u,\"heap_ok\":%s,"
        "\"sd\":%s,\"adc_front\":%d,\"adc_side\":%d,\"battery_mv\":%d,\"buttons\":%u,\"input_events\":%u,\"pressed_seen\":%u,\"reset_reason\":%d,\"wake_reason\":%d}\n",
        event,scene,revision,s.frames,s.full_refreshes,s.partial_refreshes,s.unchanged_frames,s.errors,
        s.last_convert_seconds*1000,s.last_present_seconds*1000,(unsigned long)hash_frame(),
        h.free_heap,h.minimum_heap,h.largest_block,(unsigned int)uxTaskGetStackHighWaterMark(NULL),
        heap_caps_check_integrity_all(true) ? "true":"false",h.storage_mounted ? "true":"false",
        h.front_adc,h.side_adc,h.battery_mv,h.buttons,input_events,pressed_seen,esp_reset_reason(),esp_sleep_get_wakeup_cause());
    fflush(stdout);
}

static void draw_scene(void)
{
    double start = GetTime();
    BeginDrawing(); ClearBackground(WHITE);
    if (scene == 0) {
        DrawRectangleLinesEx((Rectangle){14,14,452,772},2,BLACK);
        DrawText("TOP / X4 LAB",30,30,30,BLACK);
        DrawText("LEFT",20,112,20,BLACK); DrawText("RIGHT",380,112,20,BLACK);
        DrawText("Hardware acceptance",30,158,24,BLACK);
        for (int i=0;i<4;i++) DrawRectangle(30+i*105,220,105,120,(Color){i*85,i*85,i*85,255});
        DrawCircle(100,440,50,BLACK); DrawCircleLines(245,440,50,BLACK);
        DrawTriangle((Vector2){350,490},(Vector2){430,490},(Vector2){390,390},BLACK);
        DrawText("Press each of the 7 buttons.",30,570,20,BLACK);
        DrawText("BOTTOM / 480 x 800",30,742,25,BLACK);
    } else if (scene == 1) {
        DrawText("PARTIAL REFRESH",28,30,30,BLACK);
        DrawText("The border and text stay still.",28,84,20,BLACK);
        DrawRectangleLines(30,150,420,430,BLACK);
        DrawRectangle(45+(revision%8)*48,180,36,360,BLACK);
        DrawText(TextFormat("UPDATE %u",revision),28,650,30,BLACK);
    } else if (scene == 2) {
        DrawText("SHAPES / CLIP / ALPHA",24,30,25,BLACK);
        for (int i=0;i<8;i++) DrawRectangle(30+i*50,100,50,100,Fade(BLACK,i/7.0f));
        BeginScissorMode(40,240,160,120);
        DrawCircle(40,240,180,BLACK); EndScissorMode();
        DrawRectangleLines(40,240,160,120,GRAY);
        DrawLineEx((Vector2){260,240},(Vector2){420,360},8,BLACK);
        Camera2D camera = {.offset={240,510},.rotation=25,.zoom=1};
        BeginMode2D(camera); DrawRectangle(-100,-50,200,100,BLACK); EndMode2D();
        DrawText("Aa Bb Cc 0123456789",30,650,28,BLACK);
    } else if (scene == 3) {
        DrawText("TEXTURES / ROTATION",24,30,26,BLACK);
        Image image = GenImageChecked(32,32,4,4,WHITE,BLACK);
        Texture2D texture = LoadTextureFromImage(image); UnloadImage(image);
        if (IsTextureValid(texture)) {
            DrawTexturePro(texture,(Rectangle){0,0,32,32},(Rectangle){40,140,160,160},(Vector2){0,0},0,WHITE);
            DrawTexturePro(texture,(Rectangle){0,0,-32,32},(Rectangle){340,440,180,180},(Vector2){90,90},30,WHITE);
            UnloadTexture(texture);
        }
        DrawText("Small textures. No PSRAM.",28,660,22,BLACK);
    } else if (scene == 4) ClearBackground(BLACK);
    else if (scene == 5) ClearBackground(WHITE);
    EndDrawing();
    record_presses();
    printf("X4 {\"event\":\"render_time\",\"ms\":%.3f}\n",(GetTime()-start)*1000);
    status("frame");
}

static void sd_test(void)
{
    const char *root = X4GetStoragePath();
    bool ok = root != NULL;
    char path[256];
    if (ok) {
        snprintf(path,sizeof(path),"%s/.raylib-x4",root); mkdir(path,0775);
        snprintf(path,sizeof(path),"%s/.raylib-x4/acceptance.tmp",root);
        FILE *file = fopen(path,"wb");
        if (!file) ok = false;
        else {
            for (unsigned int i=0;i<4096 && ok;i++) ok = fputc((i*37+19)&255,file) != EOF;
            if (fflush(file) || fsync(fileno(file))) ok = false;
            if (fclose(file)) ok = false;
            file = fopen(path,"rb");
            if (!file) ok = false;
            else {
                for (unsigned int i=0;i<4096 && ok;i++) ok = fgetc(file) == (int)((i*37+19)&255);
                if (fgetc(file) != EOF) ok = false;
                fclose(file);
            }
            if (unlink(path)) ok = false;
        }
    }
    printf("X4 {\"event\":\"sd_test\",\"ok\":%s}\n",ok ? "true":"false");
    status("status");
}

static void asset_test(void)
{
    char path[256];
    const char *root=X4GetStoragePath();
    if (!root) { puts("X4 {\"event\":\"asset_test\",\"ok\":false}"); return; }
    snprintf(path,sizeof(path),"%s/.raylib-x4",root); mkdir(path,0775);
    snprintf(path,sizeof(path),"%s/.raylib-x4/acceptance.png",root);
    bool ok=SaveFileData(path,(void *)diagnostic_png,sizeof(diagnostic_png));
    Image image = ok ? LoadImage(path) : (Image){0};
    ok=ok && IsImageValid(image) && image.width==8 && image.height==8;
    for (int y=0;y<8 && ok;y++) for (int x=0;x<8;x++) {
        Color pixel=GetImageColor(image,x,y);
        if (pixel.r!=(((x/2+y/2)%2) ? 255:0) || pixel.a!=255) ok=false;
    }
    Texture2D texture=ok ? LoadTextureFromImage(image) : (Texture2D){0};
    ok=ok && IsTextureValid(texture);
    UnloadImage(image);
    if (ok) {
        BeginDrawing(); ClearBackground(WHITE);
        DrawText("PNG FROM MICROSD",28,30,28,BLACK);
        DrawTextureEx(texture,(Vector2){48,160},0,48,WHITE);
        EndDrawing(); record_presses();
    }
    UnloadTexture(texture);
    if (unlink(path)) ok=false;
    printf("X4 {\"event\":\"asset_test\",\"ok\":%s}\n",ok ? "true":"false");
    status("status");
}

static bool command(char *line)
{
    unsigned int value;
    if (!line[0]) return true;
    if (!strcmp(line,"status")) status("status");
    else if (sscanf(line,"scene %u",&value) == 1 && value < 6) { scene=value; draw_scene(); }
    else if (!strcmp(line,"step")) { revision++; draw_scene(); }
    else if (!strcmp(line,"redraw")) draw_scene();
    else if (!strcmp(line,"full")) { X4RefreshFull(); draw_scene(); }
    else if (sscanf(line,"interval %u",&value) == 1) { X4SetRefreshInterval(value); status("status"); }
    else if (!strcmp(line,"sdtest")) sd_test();
    else if (!strcmp(line,"assets")) asset_test();
    else if (!strcmp(line,"texturetest")) {
        Image source=GenImageColor(8,8,WHITE);
        Texture2D texture=LoadTextureFromImage(source); UnloadImage(source);
        Color pixel=BLACK;
        UpdateTextureRec(texture,(Rectangle){3,4,1,1},&pixel);
        Image result=LoadImageFromTexture(texture);
        bool ok=IsImageValid(result);
        if (ok) for (int y=0;y<8;y++) for (int x=0;x<8;x++) {
            pixel=GetImageColor(result,x,y);
            if (pixel.r!=((x==3 && y==4) ? 0:255) || pixel.a!=255) ok=false;
        }
        UnloadImage(result); UnloadTexture(texture);
        printf("X4 {\"event\":\"texture_test\",\"ok\":%s}\n",ok ? "true":"false");
    }
    else if (!strcmp(line,"oom")) {
        size_t before=esp_get_free_heap_size();
        unsigned int texture=rlLoadTexture(NULL,512,512,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,1);
        if (texture) rlUnloadTexture(texture);
        Image screenshot=LoadImageFromScreen();
        bool rejected=!IsImageValid(screenshot); UnloadImage(screenshot);
        printf("X4 {\"event\":\"oom_test\",\"ok\":%s}\n",
            !texture && rejected && esp_get_free_heap_size()==before && heap_caps_check_integrity_all(true) ? "true":"false");
    }
    else if (!strcmp(line,"reopen")) {
        CloseWindow(); status("closed");
        InitWindow(480,800,"X4 Lab");
        if (!IsWindowReady()) return false;
        SetExitKey(KEY_NULL); SetTargetFPS(0); draw_scene(); status("ready");
    }
    else if (!strcmp(line,"reboot")) { status("reboot"); vTaskDelay(pdMS_TO_TICKS(100)); esp_restart(); }
    else if (!strcmp(line,"sleep")) { status("sleep"); vTaskDelay(pdMS_TO_TICKS(100)); return false; }
    else if (!strcmp(line,"sleep3")) { esp_sleep_enable_timer_wakeup(3000000); status("sleep"); vTaskDelay(pdMS_TO_TICKS(100)); return false; }
    else if (!strcmp(line,"frame")) {
        const uint8_t *frame = X4GetPanelFrame();
        if (!frame) puts("X4 {\"event\":\"error\",\"message\":\"No frame\"}");
        else {
            printf("X4 FRAME %d %lu\n",X4_FRAME_BYTES,(unsigned long)hash_frame());
            for (int i=0;i<X4_FRAME_BYTES;i+=128) {
                char hex[257]; static const char digits[]="0123456789abcdef";
                for (int j=0;j<128;j++) { hex[j*2]=digits[frame[i+j]>>4]; hex[j*2+1]=digits[frame[i+j]&15]; }
                hex[256]='\n';
                if (usb_serial_jtag_write_bytes(hex,sizeof(hex),pdMS_TO_TICKS(2000)) != sizeof(hex)) break;
            }
            puts("X4 END");
        }
    } else puts("X4 {\"event\":\"error\",\"message\":\"Unknown command\"}");
    fflush(stdout);
    return true;
}

int main(void)
{
    usb_serial_jtag_driver_config_t usb = {.tx_buffer_size=1024,.rx_buffer_size=256};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    usb_serial_jtag_vfs_use_driver();
    setvbuf(stdout,NULL,_IONBF,0);
    printf("X4 {\"event\":\"boot\",\"reset_reason\":%d,\"wake_reason\":%d}\n",esp_reset_reason(),esp_sleep_get_wakeup_cause());
    InitWindow(480,800,"X4 Lab"); if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL); SetTargetFPS(0);
    draw_scene(); status("ready");
    char line[128]; size_t used=0;
    unsigned int previous=0;
    bool running=true;
    while (running && !WindowShouldClose()) {
        record_presses();
        uint8_t bytes[64]; int n=usb_serial_jtag_read_bytes(bytes,sizeof(bytes),0);
        for (int i=0;i<n && running;i++) {
            if (bytes[i]=='\n') { line[used]=0; running=command(line); used=0; }
            else if (bytes[i]!='\r' && used+1<sizeof(line)) line[used++]=bytes[i];
        }
        PollInputEvents();
        record_presses();
        unsigned int buttons=X4GetStatus().buttons;
        if (buttons!=previous) { previous=buttons; input_events++; status("buttons"); }
        WaitTime(0.01);
    }
    CloseWindow();
    usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(2000));
    usb_serial_jtag_vfs_use_nonblocking();
    usb_serial_jtag_driver_uninstall();
    return 0;
}
