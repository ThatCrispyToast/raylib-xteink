#include "hal_test.h"
X4Input test_input;
bool test_present_ok = true;
double test_time;
unsigned int test_present_count;
bool x4_hal_init(const char *title) { (void)title; return true; }
void x4_hal_close(void) {}
bool x4_hal_present(const uint8_t *frame, const uint8_t *previous, bool full)
{ (void)frame; (void)previous; (void)full; test_present_count++; return test_present_ok; }
X4Input x4_hal_poll(void) { X4Input input = test_input; test_input.pressed = 0; test_input.press_count = 0; return input; }
double x4_hal_time(void) { return test_time; }
void x4_hal_sleep(double seconds) { test_time += seconds; }
const char *x4_hal_storage(void) { return "/tmp"; }
X4Status x4_hal_status(void) { return (X4Status){ .front_adc=-1, .side_adc=-1, .battery_mv=-1 }; }
