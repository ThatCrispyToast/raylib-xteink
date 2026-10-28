// Real C3/S3 renderer/FreeRTOS memory smoke test; physical peripherals are stubbed.
#include "raylib.h"
#include "x4_hal.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { printf("X4_QEMU_FAIL: %s\n",#x); abort(); } } while (0)
static size_t free_at_boot;
static bool white_at(int x, int y)
{
    unsigned int bit=(X4_WIDTH-1-x)*X4_HEIGHT+y;
    return (X4GetPanelFrame()[bit/8] & (0x80u>>(bit%8)))!=0;
}
bool xteink_hal_frontlight(unsigned int brightness, unsigned int warmth)
{ (void)brightness; (void)warmth; return XTEINK_HAS_FRONTLIGHT; }
bool x4_hal_init(const char *title) { (void)title; return true; }
void x4_hal_close(void) {}
bool x4_hal_begin_present(const uint8_t *frame, const uint8_t *previous, bool full)
{ (void)frame; (void)previous; (void)full; return true; }
bool x4_hal_present_busy(void) { return false; }
bool x4_hal_display_idle(void) { return true; }
bool x4_hal_finish_present(void) { return true; }
X4Input x4_hal_poll(void) { return (X4Input){0}; }
double x4_hal_time(void) { return esp_timer_get_time()/1000000.0; }
void x4_hal_sleep(double seconds) { vTaskDelay(pdMS_TO_TICKS((unsigned int)(seconds*1000))+1); }
const char *x4_hal_storage(void) { return NULL; }
X4Status x4_hal_status(void) { return (X4Status){ .front_adc=-1, .side_adc=-1, .battery_mv=-1 }; }
void app_main(void)
{
    free_at_boot = esp_get_free_heap_size();
    printf("X4_QEMU: device=%s size=%dx%d boot_heap=%zu\n",XTEINK_DEVICE_ID,X4_WIDTH,X4_HEIGHT,free_at_boot);
    InitWindow(X4_WIDTH,X4_HEIGHT,"smoke"); CHECK(IsWindowReady()); SetTargetFPS(0);
    CHECK(IsFontValid(GetFontDefault()));
    Image image = GenImageChecked(32,32,4,4,WHITE,BLACK);
    Texture2D texture = LoadTextureFromImage(image); CHECK(IsTextureValid(texture)); UnloadImage(image);
    for (int i=0;i<3;i++) {
        BeginDrawing(); ClearBackground(WHITE);
        DrawText(XTEINK_DEVICE_NAME,28,30,30,BLACK);
        DrawRectangle(30+i,100,50,50,BLACK);
        DrawCircle(200,200,40,GRAY);
        DrawTexture(texture,30,300,WHITE);
        DrawPixel(X4_WIDTH-1,X4_HEIGHT-1,BLACK);
        EndDrawing();
        CHECK(!WindowShouldClose());
        CHECK(X4GetPanelFrame() != NULL);
        CHECK(!white_at(35,110) && white_at(10,110));
        CHECK(white_at(30,300) != white_at(34,300));
        CHECK(!white_at(X4_WIDTH-1,X4_HEIGHT-1));
        vTaskDelay(1);
    }
    X4Stats stats = X4GetStats();
    CHECK(stats.full_refreshes == 1 && stats.partial_refreshes == 2 && stats.errors == 0);
    CHECK(esp_get_free_heap_size() > 16000);
    CHECK(heap_caps_check_integrity_all(true));
    printf("X4_QEMU: active_heap=%zu minimum_heap=%zu buffers=%zu\n",(size_t)esp_get_free_heap_size(),
        (size_t)esp_get_minimum_free_heap_size(),stats.framebuffer_bytes);
    UnloadTexture(texture); CloseWindow();
    CHECK(esp_get_free_heap_size() > free_at_boot - 4096);
    printf("X4_QEMU_PASS: frames=%u full=%u partial=%u final_heap=%zu\n",stats.frames,
        stats.full_refreshes,stats.partial_refreshes,(size_t)esp_get_free_heap_size());
    fflush(stdout);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
