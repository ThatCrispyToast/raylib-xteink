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
} X4EpdIO;
bool x4_epd_init(const X4EpdIO *io);
bool x4_epd_present(const uint8_t *frame, const uint8_t *previous, bool full);
bool x4_epd_sleep(void);
#endif
