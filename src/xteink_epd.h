#ifndef XTEINK_EPD_H
#define XTEINK_EPD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { XTEINK_UC8253, XTEINK_UC8279_X3, XTEINK_UC8179, XTEINK_UC8279_X4 } XteinkPanel;
typedef struct {
    bool (*begin)(uint8_t command); // Holds chip select until end(), including all data chunks.
    bool (*data)(const uint8_t *data, size_t size);
    void (*end)(void);
    bool (*busy)(void); // Logical busy, normalized by the HAL.
    void (*reset)(bool high);
    void (*sleep_ms)(unsigned int ms);
    uint64_t (*time_us)(void);
} XteinkEpdIO;
typedef struct {
    XteinkEpdIO io;
    XteinkPanel panel;
    unsigned int width, height;
    bool ready, baseline, powered, classic, full;
    const uint8_t *pending;
    uint64_t started, activity;
} XteinkEpd;
bool xteink_epd_init(XteinkEpd *epd, const XteinkEpdIO *io, XteinkPanel panel, bool classic);
bool xteink_epd_begin(XteinkEpd *epd, const uint8_t *frame, bool full);
bool xteink_epd_busy(XteinkEpd *epd);
bool xteink_epd_finish(XteinkEpd *epd);
bool xteink_epd_idle(XteinkEpd *epd);
bool xteink_epd_sleep(XteinkEpd *epd);
#endif
