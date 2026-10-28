#ifndef XTEINK_PERIPHERALS_H
#define XTEINK_PERIPHERALS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    bool (*read)(uint8_t address, uint16_t reg, unsigned int reg_bytes, uint8_t *data, size_t size);
    bool (*write)(uint8_t address, uint16_t reg, unsigned int reg_bytes, const uint8_t *data, size_t size);
} XteinkI2c;
typedef struct { bool down, home; int x, y; } XteinkTouch;
bool xteink_gt911_poll(const XteinkI2c *io, XteinkTouch *touch);
int xteink_battery_mv(const XteinkI2c *io, bool x3);
void xteink_frontlight_duty(unsigned int brightness, unsigned int warmth, unsigned int *cool, unsigned int *warm);
#endif
