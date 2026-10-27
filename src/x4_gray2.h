#ifndef X4_GRAY2_H
#define X4_GRAY2_H
#include <stdint.h>

static inline void x4_gray2_read8(uint8_t *color, const void *pixels, uint32_t offset)
{
    const uint8_t value = (((const uint8_t *)pixels)[offset/4] >> (6 - 2*(offset%4))) & 3;
    color[0] = color[1] = color[2] = value*85;
    color[3] = 255;
}

static inline void x4_gray2_read(float *color, const void *pixels, uint32_t offset)
{
    uint8_t c[4];
    x4_gray2_read8(c, pixels, offset);
    color[0] = color[1] = color[2] = c[0]/255.0f;
    color[3] = 1.0f;
}

static inline void x4_gray2_write(void *pixels, const float *color, uint32_t offset)
{
    float luminance = 0.299f*color[0] + 0.587f*color[1] + 0.114f*color[2];
    if (luminance < 0) luminance = 0;
    if (luminance > 1) luminance = 1;
    unsigned int value = (unsigned int)(luminance*3.0f + 0.5f);
    unsigned int shift = 6 - 2*(offset%4);
    uint8_t *p = &((uint8_t *)pixels)[offset/4];
    *p = (uint8_t)((*p & ~(3u << shift)) | (value << shift));
}
#endif
