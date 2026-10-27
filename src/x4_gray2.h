#ifndef X4_GRAY2_H
#define X4_GRAY2_H
#include <stdint.h>
#include <string.h>

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

static inline unsigned int x4_gray2_quantize(const float *color)
{
    float luminance = 0.299f*color[0] + 0.587f*color[1] + 0.114f*color[2];
    if (luminance < 0) luminance = 0;
    if (luminance > 1) luminance = 1;
    return (unsigned int)(luminance*3.0f + 0.5f);
}
static inline unsigned int x4_gray2_get(const void *pixels, uint32_t offset)
{ return (((const uint8_t *)pixels)[offset/4] >> (6-2*(offset%4))) & 3; }
static inline void x4_gray2_set(void *pixels, uint32_t offset, unsigned int value)
{
    unsigned int shift = 6 - 2*(offset%4);
    uint8_t *p = &((uint8_t *)pixels)[offset/4];
    *p = (uint8_t)((*p & ~(3u << shift)) | (value << shift));
}
static inline void x4_gray2_span(void *pixels, uint32_t offset, unsigned int count, unsigned int value)
{
    while (count && (offset & 3)) { x4_gray2_set(pixels,offset++,value); count--; }
    unsigned int bytes=count/4;
    memset((uint8_t *)pixels+offset/4,value*0x55u,bytes);
    offset+=bytes*4; count-=bytes*4;
    while (count--) x4_gray2_set(pixels,offset++,value);
}
static inline void x4_gray2_write(void *pixels, const float *color, uint32_t offset)
{ x4_gray2_set(pixels,offset,x4_gray2_quantize(color)); }
#endif
