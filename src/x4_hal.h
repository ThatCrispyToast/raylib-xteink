#ifndef X4_HAL_H
#define X4_HAL_H
#include "raylib_x4.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { X4_BACK, X4_CONFIRM, X4_LEFT, X4_RIGHT, X4_UP, X4_DOWN, X4_POWER, X4_BUTTON_COUNT };
#define X4_INPUT_CAPACITY 64
typedef struct X4Input {
    uint8_t down, pressed;
    bool quit;
    uint8_t press_count, press_order[X4_INPUT_CAPACITY];
    bool touch_down, touch_pressed;
    int touch_x, touch_y;
} X4Input;
bool x4_hal_init(const char *title);
void x4_hal_close(void);
bool x4_hal_begin_present(const uint8_t *frame, const uint8_t *previous, bool full);
bool x4_hal_present_busy(void);
bool x4_hal_finish_present(void);
bool x4_hal_display_idle(void);
X4Input x4_hal_poll(void);
double x4_hal_time(void);
void x4_hal_sleep(double seconds);
const char *x4_hal_storage(void);
X4Status x4_hal_status(void);
bool xteink_hal_frontlight(unsigned int brightness, unsigned int warmth);
uint8_t x4_decode_buttons(int adc1, int adc2, bool power);
void x4_convert_frame(const uint8_t *gray, uint8_t *panel);
bool x4_present(const uint8_t *gray);
bool x4_service_display(bool wait);
void x4_reset(void);
#ifdef __cplusplus
}
#endif
#endif
