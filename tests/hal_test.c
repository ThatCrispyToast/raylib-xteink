#include "hal_test.h"
X4Input test_input;
bool test_present_ok = true;
double test_time;
double test_present_delay, test_present_until;
bool test_present_buffer_stable=true;
static const uint8_t *pending_frame;
static unsigned int pending_hash;
static unsigned int hash(const uint8_t *p)
{ unsigned int h=2166136261u; for (int i=0; i<X4_FRAME_BYTES; i++) h=(h^p[i])*16777619u; return h; }
unsigned int test_present_count;
bool x4_hal_init(const char *title) { (void)title; return true; }
void x4_hal_close(void) {}
bool x4_hal_begin_present(const uint8_t *frame, const uint8_t *previous, bool full)
{ (void)previous; (void)full; test_present_count++; test_present_until=test_time+test_present_delay;
  pending_frame=frame; pending_hash=hash(frame); return test_present_ok; }
bool x4_hal_present_busy(void) { return test_time<test_present_until; }
bool x4_hal_display_idle(void) { return true; }
bool x4_hal_finish_present(void)
{ if (test_time<test_present_until) test_time=test_present_until;
  test_present_buffer_stable &= !pending_frame || hash(pending_frame) == pending_hash;
  pending_frame=NULL; return test_present_ok; }
X4Input x4_hal_poll(void) { X4Input input = test_input; test_input.pressed = 0; test_input.press_count = 0; return input; }
double x4_hal_time(void) { return test_time; }
void x4_hal_sleep(double seconds) { test_time += seconds; }
const char *x4_hal_storage(void) { return "/tmp"; }
X4Status x4_hal_status(void) { return (X4Status){ .front_adc=-1, .side_adc=-1, .battery_mv=-1 }; }
