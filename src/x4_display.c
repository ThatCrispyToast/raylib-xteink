#include "x4_hal.h"
#include <string.h>

static uint8_t panel[X4_FRAME_BYTES], previous[X4_FRAME_BYTES];
static X4Stats stats;
static unsigned int interval = 12, partial_count;
static bool force_full = true, have_frame;

void x4_reset(void)
{
    memset(&stats, 0, sizeof(stats));
    stats.framebuffer_bytes = X4_GRAY_BYTES + 2*X4_FRAME_BYTES;
    force_full = true;
    have_frame = false;
    partial_count = 0;
}

void x4_convert_frame(const uint8_t *gray, uint8_t *out)
{
    // Each mask selects which of the four shades is white at this Bayer position.
    static const uint8_t masks[4][4] = {
        {14,12,14,12}, {8,14,8,12}, {14,8,14,12}, {8,12,8,12}
    };
    const int stride=X4_WIDTH/4;
    for (int x=0; x<X4_WIDTH; x++)
    {
        unsigned int shift=6-2*(x%4);
        const uint8_t m0=masks[0][x%4], m1=masks[1][x%4];
        const uint8_t m2=masks[2][x%4], m3=masks[3][x%4];
        uint8_t *row=out+(X4_WIDTH-1-x)*(X4_HEIGHT/8);
        for (int y=0; y<X4_HEIGHT/8; y++)
        {
            const uint8_t *p=gray+(X4_HEIGHT-1-y*8)*stride+x/4;
            #define WHITE_AT(n,m) (((m) >> ((p[-(n)*stride] >> shift) & 3)) & 1)
            row[y]=(uint8_t)((WHITE_AT(0,m0)<<7) | (WHITE_AT(1,m1)<<6) |
                (WHITE_AT(2,m2)<<5) | (WHITE_AT(3,m3)<<4) | (WHITE_AT(4,m0)<<3) |
                (WHITE_AT(5,m1)<<2) | (WHITE_AT(6,m2)<<1) | WHITE_AT(7,m3));
            #undef WHITE_AT
        }
    }
}

bool x4_present(const uint8_t *gray)
{
    stats.frames++;
    double start = x4_hal_time();
    x4_convert_frame(gray, panel);
    stats.last_convert_seconds = x4_hal_time() - start;
    stats.last_present_seconds = 0;
    if (have_frame && !force_full && memcmp(panel, previous, sizeof(panel)) == 0)
    {
        stats.unchanged_frames++;
        return true;
    }
    bool full = force_full || !have_frame || partial_count >= interval;
    start = x4_hal_time();
    if (!x4_hal_present(panel, previous, full))
    {
        stats.errors++;
        force_full = true;
        return false;
    }
    stats.last_present_seconds = x4_hal_time() - start;
    memcpy(previous, panel, sizeof(panel));
    have_frame = true;
    force_full = false;
    if (full) { stats.full_refreshes++; partial_count = 0; }
    else { stats.partial_refreshes++; partial_count++; }
    return true;
}

void X4RefreshFull(void) { force_full = true; }
void X4SetRefreshInterval(unsigned int count) { interval = count; }
X4Stats X4GetStats(void) { return stats; }
X4Status X4GetStatus(void) { return x4_hal_status(); }
const uint8_t *X4GetPanelFrame(void) { return have_frame ? previous : NULL; }
const char *X4GetStoragePath(void) { return x4_hal_storage(); }

uint8_t x4_decode_buttons(int adc1, int adc2, bool power)
{
    const int levels1[] = { 3470, 2655, 1470, 3 };
    const int levels2[] = { 2205, 3 };
    uint8_t mask = power ? (1u << X4_POWER) : 0;
    // An ADC ladder can reliably distinguish only one key per group.
    if (adc1 >= 0 && adc1 <= 3800)
        for (int i = 0; i < 4; i++)
            if (adc1 > levels1[i] - 400 && adc1 < levels1[i] + 400) { mask |= 1u << i; break; }
    if (adc2 >= 0 && adc2 <= 3800)
        for (int i = 0; i < 2; i++)
            if (adc2 > levels2[i] - 400 && adc2 < levels2[i] + 400) { mask |= 1u << (X4_UP + i); break; }
    return mask;
}
