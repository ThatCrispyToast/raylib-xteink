#ifndef X4_EPD_H
#define X4_EPD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct X4EpdIO {
    bool (*write)(bool data, const uint8_t *bytes, size_t size);
    bool (*busy)(void);
    void (*reset)(bool high);
    void (*sleep_ms)(unsigned int ms);
    uint64_t (*time_us)(void); // Optional profiling clock.
} X4EpdIO;
typedef struct X4EpdStats {
    unsigned int ram_bytes, window_width, window_height;
    uint64_t transfer_us, waveform_us;
} X4EpdStats;
X4EpdStats x4_epd_stats(void);
bool x4_epd_init(const X4EpdIO *io);
bool x4_epd_present(const uint8_t *frame, const uint8_t *previous, bool full);
// Frame must stay immutable until finish; no panel commands while busy.
bool x4_epd_begin(const uint8_t *frame, const uint8_t *previous, bool full);
bool x4_epd_busy(void);
bool x4_epd_finish(void);
bool x4_epd_idle(void);
bool x4_epd_sleep(void);
#endif
