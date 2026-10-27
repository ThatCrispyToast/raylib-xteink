#include "x4_hal.h"
#include <string.h>
#include "x4_dither_lut.h"

static uint8_t buffers[2][X4_FRAME_BYTES];
static uint8_t *front=buffers[0], *back=buffers[1];
static X4Stats stats;
static unsigned int interval = 12, partial_count;
static bool force_full = true, have_frame;
static bool asynchronous, in_flight, pending, flight_full, failed;
static double flight_started;

void x4_reset(void)
{
    memset(&stats, 0, sizeof(stats));
    stats.framebuffer_bytes = X4_GRAY_BYTES + 2*X4_FRAME_BYTES;
    force_full = true;
    have_frame = false;
    partial_count = 0;
    asynchronous=in_flight=pending=flight_full=failed=false;
    front=buffers[0]; back=buffers[1];
}

void x4_convert_frame(const uint8_t *gray, uint8_t *out)
{
    const int stride=X4_WIDTH/4;
    for (int x=0; x<X4_WIDTH; x+=4)
    {
        uint8_t *row=out+(X4_WIDTH-1-x)*(X4_HEIGHT/8);
        for (int y=0; y<X4_HEIGHT/8; y++)
        {
            const uint8_t *p=gray+(X4_HEIGHT-1-y*8)*stride+x/4;
            uint32_t bits=(dither_lut[0][p[0]]<<7) | (dither_lut[1][p[-stride]]<<6) |
                (dither_lut[2][p[-2*stride]]<<5) | (dither_lut[3][p[-3*stride]]<<4) |
                (dither_lut[0][p[-4*stride]]<<3) | (dither_lut[1][p[-5*stride]]<<2) |
                (dither_lut[2][p[-6*stride]]<<1) | dither_lut[3][p[-7*stride]];
            row[y]=(uint8_t)bits;
            row[y-X4_HEIGHT/8]=(uint8_t)(bits>>8);
            row[y-2*(X4_HEIGHT/8)]=(uint8_t)(bits>>16);
            row[y-3*(X4_HEIGHT/8)]=(uint8_t)(bits>>24);
        }
    }
}

static bool display_failed(void)
{
    if (!failed) stats.errors++;
    if (in_flight) have_frame=false;
    failed=true; in_flight=pending=false; force_full=true;
    return false;
}
bool x4_service_display(bool wait)
{
    if (failed) return false;
    for (;;) {
        if (in_flight) {
            if (!wait && x4_hal_present_busy()) return true;
            if (!x4_hal_finish_present()) return display_failed();
            stats.last_present_seconds=x4_hal_time()-flight_started;
            in_flight=false; have_frame=true;
            if (flight_full) { stats.full_refreshes++; partial_count=0; }
            else { stats.partial_refreshes++; partial_count++; }
        }
        if (!pending) return x4_hal_display_idle() || display_failed();
        if (have_frame && !force_full && !memcmp(front,back,X4_FRAME_BYTES)) {
            stats.unchanged_frames++; pending=false; return true;
        }
        flight_full=force_full || !have_frame || partial_count>=interval;
        flight_started=x4_hal_time();
        if (!x4_hal_begin_present(back,front,flight_full)) return display_failed();
        uint8_t *swap=front; front=back; back=swap;
        pending=false; in_flight=true; force_full=false;
        // A cleaning refresh always completes before returning to the application.
        if (flight_full) wait=true;
        if (!wait) return true;
    }
}
bool x4_present(const uint8_t *gray)
{
    stats.frames++;
    failed=false;
    if (pending) stats.coalesced_frames++;
    double start=x4_hal_time();
    x4_convert_frame(gray,back);
    stats.last_convert_seconds=x4_hal_time()-start;
    pending=true;
    return x4_service_display(!asynchronous || force_full);
}
void X4SetAsyncRefresh(bool enabled)
{
    asynchronous=enabled;
}
bool X4WaitDisplay(void) { return x4_service_display(true); }
bool X4IsDisplayBusy(void)
{
    return in_flight || pending;
}

void X4RefreshFull(void) { force_full = true; }
void X4SetRefreshInterval(unsigned int count) { interval = count; }
X4Stats X4GetStats(void) { return stats; }
X4Status X4GetStatus(void) { return x4_hal_status(); }
const uint8_t *X4GetPanelFrame(void) { return have_frame && !in_flight ? front : NULL; }
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
