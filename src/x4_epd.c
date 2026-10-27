#include "x4_epd.h"
#include "raylib_x4.h"
#include <string.h>

static X4EpdIO bus;
static bool ready, baseline;
static X4EpdStats stats;
typedef struct { int x0, x1, y0, y1; } Window; // Byte columns and buffer rows, inclusive.
static uint64_t time_us(void) { return bus.time_us ? bus.time_us() : 0; }
X4EpdStats x4_epd_stats(void) { return stats; }

static bool wait_ready(void)
{
    bus.sleep_ms(2);
    for (unsigned int elapsed=0; elapsed<15000; elapsed+=2) {
        if (!bus.busy()) return true;
        bus.sleep_ms(2);
    }
    ready=baseline=false;
    return false;
}
static bool write(bool data, const uint8_t *bytes, size_t size)
{
    if (bus.write(data,bytes,size)) return true;
    ready=baseline=false; return false;
}
static bool command(uint8_t cmd, const uint8_t *data, size_t size)
{ return write(false,&cmd,1) && (!size || write(true,data,size)); }
static bool byte(uint8_t cmd, uint8_t value) { return command(cmd,&value,1); }

static bool ram_window(Window w)
{
    unsigned int left=w.x0*8, right=(w.x1+1)*8-1;
    unsigned int top=X4_PANEL_HEIGHT-1-w.y0, bottom=X4_PANEL_HEIGHT-1-w.y1;
    const uint8_t x[]={left,left>>8,right,right>>8}, y[]={top,top>>8,bottom,bottom>>8};
    return byte(0x11,0x01) && command(0x44,x,sizeof(x)) && command(0x45,y,sizeof(y)) &&
        command(0x4e,x,2) && command(0x4f,y,2);
}
static bool write_ram(uint8_t cmd, const uint8_t *frame, Window w)
{
    if (!ram_window(w) || !command(cmd,NULL,0)) return false;
    const int stride=X4_PANEL_WIDTH/8, width=w.x1-w.x0+1;
    stats.ram_bytes+=(unsigned int)(width*(w.y1-w.y0+1));
    if (width == stride) return write(true,frame+w.y0*stride,(w.y1-w.y0+1)*stride);
    // Pack narrow rows so SPI transaction overhead does not dominate small updates.
    uint8_t block[1024]; size_t used=0;
    for (int y=w.y0; y<=w.y1; y++) {
        if (used+width>sizeof(block)) {
            if (!write(true,block,used)) return false;
            used=0;
        }
        memcpy(block+used,frame+y*stride+w.x0,width); used+=width;
    }
    return !used || write(true,block,used);
}
static Window changed_window(const uint8_t *frame, const uint8_t *previous)
{
    const int stride=X4_PANEL_WIDTH/8;
    Window w={stride,-1,X4_PANEL_HEIGHT,-1};
    for (int y=0; y<X4_PANEL_HEIGHT; y++) {
        const uint8_t *a=frame+y*stride, *b=previous+y*stride;
        if (!memcmp(a,b,stride)) continue;
        if (w.y0>y) w.y0=y;
        w.y1=y;
        for (int x=0; x<stride; x++) if (a[x] != b[x]) {
            if (w.x0>x) w.x0=x;
            if (w.x1<x) w.x1=x;
        }
    }
    return w;
}
bool x4_epd_init(const X4EpdIO *io)
{
    if (!io || !io->write || !io->busy || !io->reset || !io->sleep_ms) return false;
    bus=*io; ready=baseline=false; stats=(X4EpdStats){0};
    bus.reset(true); bus.sleep_ms(20); bus.reset(false); bus.sleep_ms(2);
    bus.reset(true); bus.sleep_ms(20);
    if (!wait_ready() || !command(0x12,NULL,0) || !wait_ready()) return false;
    const uint8_t booster[]={0xae,0xc7,0xc3,0xc0,0x40}, driver[]={0xdf,0x01,0x02};
    ready=byte(0x18,0x80) && command(0x0c,booster,sizeof(booster)) &&
        command(0x01,driver,sizeof(driver)) && byte(0x3c,0x01);
    return ready;
}
bool x4_epd_present(const uint8_t *frame, const uint8_t *previous, bool full)
{
    stats=(X4EpdStats){0};
    if (!ready || !frame || (!full && (!previous || !baseline))) return false;
    Window w=full ? (Window){0,X4_PANEL_WIDTH/8-1,0,X4_PANEL_HEIGHT-1} : changed_window(frame,previous);
    if (w.x1<w.x0) return true;
    stats.window_width=(w.x1-w.x0+1)*8; stats.window_height=w.y1-w.y0+1;
    if (!wait_ready()) return false;
    uint64_t start=time_us();
    if ((full && !write_ram(0x26,frame,w)) || !write_ram(0x24,frame,w)) return false;
    stats.transfer_us=time_us()-start;
    // Mode 2 plus analog/clock shutdown uses the same OTP waveform as FC + 83.
    if (!byte(0x21,full ? 0x40 : 0x00) || !byte(0x22,full ? 0xf7 : 0xff)) return false;
    start=time_us();
    if (!command(0x20,NULL,0) || !wait_ready()) return false;
    stats.waveform_us=time_us()-start;
    start=time_us();
    // Synchronize both banks after Mode 2, including controllers using RAM ping-pong.
    if (!write_ram(0x26,frame,w) || (!full && !write_ram(0x24,frame,w))) return false;
    stats.transfer_us+=time_us()-start;
    baseline=true; return true;
}
bool x4_epd_sleep(void)
{
    if (!ready) return false;
    bool ok=wait_ready() && byte(0x22,0x83) && command(0x20,NULL,0) &&
        wait_ready() && byte(0x10,0x01);
    ready=baseline=false; return ok;
}
