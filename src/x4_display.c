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
    static const uint8_t bayer[4][4] = {
        { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 }
    };
    memset(out, 0, X4_FRAME_BYTES);
    for (int y = 0; y < X4_HEIGHT; y++)
        for (int x = 0; x < X4_WIDTH; x++)
        {
            // rlsw stores bottom-up; the panel is landscape, rotated clockwise by the emulator.
            unsigned int i = (X4_HEIGHT - 1 - y)*X4_WIDTH + x;
            unsigned int shade = (gray[i/4] >> (6 - 2*(i%4))) & 3;
            bool white = shade == 3 || (shade != 0 && shade*16 > bayer[y%4][x%4]*3 + 1);
            unsigned int p = (X4_WIDTH - 1 - x)*X4_PANEL_WIDTH + y;
            if (white) out[p/8] |= (uint8_t)(0x80u >> (p%8));
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
