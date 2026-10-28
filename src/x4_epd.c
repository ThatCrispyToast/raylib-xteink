#include "x4_epd.h"
#include "raylib_x4.h"
#include <string.h>

static X4EpdIO bus;
static bool ready, baseline;
static bool powered, partial_lut;
static uint64_t lut_at, active_at;
static X4EpdStats stats;
typedef struct { int x0, x1, y0, y1; } Window; // Byte columns and buffer rows, inclusive.
static Window pending_window;
static const uint8_t *pending_frame;
static bool pending_full, pending_load;
static uint64_t started_at;
static uint64_t time_us(void) { return bus.time_us ? bus.time_us() : 0; }
X4EpdStats x4_epd_stats(void) { return stats; }

static bool wait_ready(void)
{
    bus.sleep_ms(2);
    for (unsigned int elapsed=0; elapsed<15000; elapsed+=2) {
        if (!bus.busy()) return true;
        bus.sleep_ms(2);
    }
    ready=baseline=powered=partial_lut=false;
    pending_frame=NULL;
    return false;
}
static bool write(bool data, const uint8_t *bytes, size_t size)
{
    if (bus.write(data,bytes,size)) return true;
    ready=baseline=powered=partial_lut=false; pending_frame=NULL; return false;
}
static bool command(uint8_t cmd, const uint8_t *data, size_t size)
{ return write(false,&cmd,1) && (!size || write(true,data,size)); }
static bool byte(uint8_t cmd, uint8_t value) { return command(cmd,&value,1); }
static bool wait_pending(void)
{
    if (!bus.time_us) return wait_ready();
    for (;;) {
        uint64_t elapsed=time_us()-started_at;
        if (elapsed>=15000000) {
            ready=baseline=powered=partial_lut=false; pending_frame=NULL; return false;
        }
        if (elapsed>=2000 && !bus.busy()) return true;
        bus.sleep_ms(2);
    }
}

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
    bus=*io; ready=baseline=powered=partial_lut=false; stats=(X4EpdStats){0};
    pending_frame=NULL;
    bus.reset(true); bus.sleep_ms(20); bus.reset(false); bus.sleep_ms(2);
    bus.reset(true); bus.sleep_ms(20);
    if (!wait_ready() || !command(0x12,NULL,0) || !wait_ready()) return false;
    const uint8_t booster[]={0xae,0xc7,0xc3,0xc0,XTEINK_DEVICE==XTEINK_X4 ? 0x40 : 0x80}, driver[]={0xdf,0x01,0x02};
    ready=byte(0x18,0x80) && command(0x0c,booster,sizeof(booster)) &&
        command(0x01,driver,sizeof(driver)) && byte(0x3c,XTEINK_DEVICE==XTEINK_X4 ? 0x01 : 0x80);
    return ready;
}
bool x4_epd_begin(const uint8_t *frame, const uint8_t *previous, bool full)
{
    if (pending_frame) return false;
    stats=(X4EpdStats){0};
    if (!ready || !frame || (!full && (!previous || !baseline))) return false;
    Window w=full ? (Window){0,X4_PANEL_WIDTH/8-1,0,X4_PANEL_HEIGHT-1} : changed_window(frame,previous);
    if (w.x1<w.x0) return true;
    stats.window_width=(w.x1-w.x0+1)*8; stats.window_height=w.y1-w.y0+1;
    if (bus.busy() && !wait_ready()) return false;
    uint64_t start=time_us();
    if ((full && !write_ram(0x26,frame,w)) || !write_ram(0x24,frame,w)) return false;
    stats.transfer_us=time_us()-start;
    // Reuse temperature-selected Mode 2 OTP during a burst; never override its voltages.
    bool load_lut=XTEINK_DEVICE!=XTEINK_X4 || !partial_lut || !bus.time_us || time_us()-lut_at >= 30000000;
    uint8_t control=full ? 0xf7 : (powered ? 0 : 0xc0) | (load_lut ? 0x3c : 0x0c);
    if (XTEINK_DEVICE!=XTEINK_X4) control=full ? 0xf7 : 0xfc;
    if (!bus.time_us) control |= 0x03;
    if (!byte(0x21,full && XTEINK_DEVICE==XTEINK_X4 ? 0x40 : 0x00) || !byte(0x22,control)) return false;
    if (!command(0x20,NULL,0)) return false;
    pending_frame=frame; pending_window=w; pending_full=full; pending_load=load_lut;
    started_at=time_us();
    return true;
}
bool x4_epd_busy(void)
{
    if (!pending_frame) return false;
    if (bus.time_us && time_us()-started_at>=15000000) {
        ready=baseline=powered=partial_lut=false; pending_frame=NULL; return false;
    }
    return (bus.time_us && time_us()-started_at<2000) || bus.busy();
}
bool x4_epd_finish(void)
{
    if (!ready) return false;
    if (!pending_frame) return true;
    if (!wait_pending()) return false;
    stats.waveform_us=time_us()-started_at;
    powered=!pending_full && bus.time_us;
    partial_lut=!pending_full;
    active_at=time_us();
    if (pending_load) lut_at=active_at;
    uint64_t start=time_us();
    // Synchronize both banks after Mode 2, including controllers using RAM ping-pong.
    if (!write_ram(0x26,pending_frame,pending_window) ||
        (!pending_full && !write_ram(0x24,pending_frame,pending_window))) return false;
    stats.transfer_us+=time_us()-start;
    pending_frame=NULL;
    baseline=true; return true;
}
bool x4_epd_present(const uint8_t *frame, const uint8_t *previous, bool full)
{ return x4_epd_begin(frame,previous,full) && x4_epd_finish(); }
bool x4_epd_idle(void)
{
    // Polling this on the render task serializes shutdown with refresh and RAM reseeding.
    if (!ready || pending_frame || !powered || !bus.time_us || time_us()-active_at<2000000) return ready;
    if (!byte(0x22,0x83) || !command(0x20,NULL,0) || !wait_ready()) return false;
    powered=false; return true;
}
bool x4_epd_sleep(void)
{
    if (!ready) return false;
    bool ok=x4_epd_finish() && wait_ready() && byte(0x22,0x83) && command(0x20,NULL,0) &&
        wait_ready() && byte(0x10,0x01);
    ready=baseline=powered=partial_lut=false; pending_frame=NULL; return ok;
}
