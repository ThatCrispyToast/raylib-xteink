#include "x4_epd.h"
#include "raylib_x4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static uint8_t cmd, bw[X4_FRAME_BYTES], red[X4_FRAME_BYTES];
static uint8_t frame[X4_FRAME_BYTES], previous[X4_FRAME_BYTES];
static unsigned int activations, displays, slept, lut_loads;
static uint64_t clock_us;
static unsigned int write_us;
static bool clock_on, analog_on;
static bool fail_write, stuck_busy, ping_pong;
static uint8_t mode;
static int x0, x1, y0, y1, x, y;
static unsigned int le16(const uint8_t *p) { return p[0] | (p[1]<<8); }
static bool write_bus(bool data, const uint8_t *bytes, size_t size)
{
    if (fail_write) return false;
    clock_us+=write_us;
    if (!data) {
        CHECK(size == 1); cmd = *bytes;
        CHECK(cmd != 0x2a && cmd != 0x30 && cmd != 0x36 && cmd != 0x39);
        CHECK(cmd != 0x03 && cmd != 0x04 && cmd != 0x2c && cmd != 0x32 && cmd != 0x1a);
        if (cmd == 0x20) {
            activations++;
            if (mode & 0x80) clock_on=true;
            if (mode & 0x40) analog_on=true;
            CHECK(clock_on);
            if (mode & 0x10) lut_loads++;
            if (mode & 0x04) {
                CHECK(analog_on); displays++;
                CHECK(!memcmp(bw,frame,sizeof(frame)));
                CHECK(!memcmp(red,(mode & 0x08) ? previous : frame,sizeof(frame)));
                if ((mode & 0x08) && ping_pong) for (size_t i=0; i<sizeof(bw); i++) {
                    uint8_t temp=bw[i]; bw[i]=red[i]; red[i]=temp;
                }
            }
            if (mode & 0x02) analog_on=false;
            if (mode & 0x01) clock_on=false;
        }
    } else if (cmd == 0x24 || cmd == 0x26) {
        for (size_t i=0; i<size; i++) {
            CHECK(x>=x0 && x<=x1 && y<=y0 && y>=y1);
            (cmd == 0x24 ? bw : red)[(X4_PANEL_HEIGHT-1-y)*(X4_PANEL_WIDTH/8)+x/8]=bytes[i];
            x+=8;
            if (x>x1) { x=x0; y--; }
        }
    } else if (cmd == 0x22) { CHECK(size == 1); mode=*bytes; }
    else if (cmd == 0x11) { CHECK(size == 1 && *bytes == 1); }
    else if (cmd == 0x44) {
        CHECK(size == 4); x0=le16(bytes); x1=le16(bytes+2);
        CHECK(x0<=x1 && x1<X4_PANEL_WIDTH && x0%8 == 0 && x1%8 == 7);
    } else if (cmd == 0x45) {
        CHECK(size == 4); y0=le16(bytes); y1=le16(bytes+2);
        CHECK(y0>=y1 && y0<X4_PANEL_HEIGHT);
    } else if (cmd == 0x4e) { CHECK(size == 2); x=le16(bytes); CHECK(x == x0); }
    else if (cmd == 0x4f) { CHECK(size == 2); y=le16(bytes); CHECK(y == y0); }
    return true;
}
static bool busy(void) { return stuck_busy; }
static void reset(bool high) { if (!high) clock_on=analog_on=false; }
static void sleep_ms(unsigned int ms) { slept+=ms; clock_us+=(uint64_t)ms*1000; }
static uint64_t time_us(void) { return clock_us; }
static void present(bool full, unsigned int bytes, unsigned int width, unsigned int height)
{
    unsigned int before=displays;
    CHECK(x4_epd_present(frame,full ? NULL : previous,full));
    CHECK(displays == before+1 && ((mode & 0x08) != 0) == !full);
    CHECK(!memcmp(bw,frame,sizeof(frame)) && !memcmp(red,frame,sizeof(frame)));
    X4EpdStats s=x4_epd_stats();
    CHECK(s.ram_bytes == bytes && s.window_width == width && s.window_height == height);
    memcpy(previous,frame,sizeof(frame));
}
int main(void)
{
    X4EpdIO io={write_bus,busy,reset,sleep_ms,time_us};
    CHECK(!x4_epd_init(NULL)); CHECK(x4_epd_init(&io));
    CHECK(!x4_epd_present(frame,previous,false));
    memset(frame,0xa5,sizeof(frame));
    present(true,3*X4_FRAME_BYTES,X4_PANEL_WIDTH,X4_PANEL_HEIGHT);
    CHECK(!clock_on && !analog_on && lut_loads == 1);
    for (int swap=0; swap<2; swap++) {
        ping_pong=swap;
        frame[123]^=0xff; present(false,3,8,1);
        frame[0]^=0xff; present(false,3,8,1);
        frame[X4_FRAME_BYTES-1]^=0xff; present(false,3,8,1);
        // Multi-chunk narrow window; previously updated corners must remain intact.
        frame[10*100+20]^=0xff; frame[110*100+50]^=0xff;
        present(false,3*31*101,31*8,101);
        frame[0]^=0xff; frame[X4_FRAME_BYTES-1]^=0xff;
        present(false,3*X4_FRAME_BYTES,X4_PANEL_WIDTH,X4_PANEL_HEIGHT);
        unsigned int before=activations;
        CHECK(x4_epd_present(frame,previous,false));
        CHECK(activations == before && x4_epd_stats().ram_bytes == 0);
    }
    CHECK(lut_loads == 2 && clock_on && analog_on);
    unsigned int before=activations;
    CHECK(x4_epd_idle()); CHECK(activations == before);
    clock_us+=2000000;
    CHECK(x4_epd_idle()); CHECK(activations == before+1 && !clock_on && !analog_on);
    CHECK(x4_epd_idle()); CHECK(activations == before+1);
    frame[42]^=1; present(false,3,8,1);
    CHECK(mode == 0xcc && lut_loads == 2 && clock_on && analog_on);
    clock_us+=30000000;
    frame[42]^=1; present(false,3,8,1); CHECK(lut_loads == 3);
    present(true,3*X4_FRAME_BYTES,X4_PANEL_WIDTH,X4_PANEL_HEIGHT);
    CHECK(lut_loads == 4 && !analog_on);
    frame[42]^=1; present(false,3,8,1); CHECK(lut_loads == 5);
    CHECK(x4_epd_sleep()); CHECK(!x4_epd_present(frame,previous,false));
    CHECK(!clock_on && !analog_on);
    // A bus without a clock falls back to powering down after every frame.
    io.time_us=NULL; CHECK(x4_epd_init(&io));
    present(true,3*X4_FRAME_BYTES,X4_PANEL_WIDTH,X4_PANEL_HEIGHT);
    frame[42]^=1; present(false,3,8,1); CHECK(!clock_on && !analog_on);
    before=lut_loads; frame[42]^=1; present(false,3,8,1); CHECK(lut_loads == before+1);
    CHECK(x4_epd_sleep()); io.time_us=time_us;
    CHECK(x4_epd_init(&io)); present(true,3*X4_FRAME_BYTES,X4_PANEL_WIDTH,X4_PANEL_HEIGHT);
    write_us=1500;
    frame[42]^=1; CHECK(x4_epd_begin(frame,previous,false));
    write_us=0;
    CHECK(x4_epd_busy()); before=activations;
    clock_us+=1999; CHECK(x4_epd_busy());
    clock_us++; CHECK(!x4_epd_busy());
    CHECK(!x4_epd_begin(frame,previous,false)); CHECK(x4_epd_idle() && activations == before);
    clock_us+=500000; CHECK(!x4_epd_busy()); CHECK(x4_epd_finish());
    CHECK(!memcmp(bw,frame,sizeof(frame)) && !memcmp(red,frame,sizeof(frame)));
    memcpy(previous,frame,sizeof(frame));
    frame[42]^=1; CHECK(x4_epd_begin(frame,previous,false)); stuck_busy=true;
    clock_us+=15000000; CHECK(!x4_epd_busy() && !x4_epd_finish()); stuck_busy=false;
    CHECK(x4_epd_init(&io)); present(true,3*X4_FRAME_BYTES,X4_PANEL_WIDTH,X4_PANEL_HEIGHT);
    frame[42]^=1; CHECK(x4_epd_begin(frame,previous,false)); stuck_busy=true;
    clock_us+=14900000; slept=0;
    CHECK(!x4_epd_finish() && slept<=102); stuck_busy=false;
    CHECK(x4_epd_init(&io)); present(true,3*X4_FRAME_BYTES,X4_PANEL_WIDTH,X4_PANEL_HEIGHT);
    frame[42]^=1; CHECK(x4_epd_begin(frame,previous,false)); clock_us+=15000000;
    CHECK(!x4_epd_finish()); // A late blocking finish must share the same deadline.
    CHECK(x4_epd_init(&io)); fail_write=true;
    CHECK(!x4_epd_present(frame,previous,true)); fail_write=false;
    CHECK(!x4_epd_present(frame,previous,true));
    CHECK(x4_epd_init(&io)); stuck_busy=true; slept=0;
    CHECK(!x4_epd_present(frame,previous,true)); CHECK(slept>=15000 && slept<15100);
    puts("PASS: SSD1677 regions, RAM ping-pong, cached LUT, temperature expiry, idle power, sleep, SPI failure, BUSY timeout");
}
