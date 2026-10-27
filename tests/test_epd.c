#include "x4_epd.h"
#include "raylib_x4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static uint8_t cmd, bw[X4_FRAME_BYTES], red[X4_FRAME_BYTES];
static uint8_t frame[X4_FRAME_BYTES], previous[X4_FRAME_BYTES];
static unsigned int activations, slept;
static bool fail_write, stuck_busy, ping_pong;
static uint8_t mode;
static int x0, x1, y0, y1, x, y;
static unsigned int le16(const uint8_t *p) { return p[0] | (p[1]<<8); }
static bool write_bus(bool data, const uint8_t *bytes, size_t size)
{
    if (fail_write) return false;
    if (!data) {
        CHECK(size == 1); cmd = *bytes;
        if (cmd == 0x20) {
            activations++;
            if (mode == 0xf7 || mode == 0xff) {
                CHECK(!memcmp(bw,frame,sizeof(frame)));
                CHECK(!memcmp(red,mode == 0xf7 ? frame : previous,sizeof(frame)));
                if (mode == 0xff && ping_pong) for (size_t i=0; i<sizeof(bw); i++) {
                    uint8_t temp=bw[i]; bw[i]=red[i]; red[i]=temp;
                }
            }
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
static void reset(bool high) { (void)high; }
static void sleep_ms(unsigned int ms) { slept+=ms; }
static void present(bool full, unsigned int bytes, unsigned int width, unsigned int height)
{
    unsigned int before=activations;
    CHECK(x4_epd_present(frame,full ? NULL : previous,full));
    CHECK(activations == before+1 && mode == (full ? 0xf7 : 0xff));
    CHECK(!memcmp(bw,frame,sizeof(frame)) && !memcmp(red,frame,sizeof(frame)));
    X4EpdStats s=x4_epd_stats();
    CHECK(s.ram_bytes == bytes && s.window_width == width && s.window_height == height);
    memcpy(previous,frame,sizeof(frame));
}
int main(void)
{
    X4EpdIO io={write_bus,busy,reset,sleep_ms,NULL};
    CHECK(!x4_epd_init(NULL)); CHECK(x4_epd_init(&io));
    CHECK(!x4_epd_present(frame,previous,false));
    memset(frame,0xa5,sizeof(frame));
    present(true,3*X4_FRAME_BYTES,X4_PANEL_WIDTH,X4_PANEL_HEIGHT);
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
    CHECK(x4_epd_sleep()); CHECK(!x4_epd_present(frame,previous,false));
    CHECK(x4_epd_init(&io)); fail_write=true;
    CHECK(!x4_epd_present(frame,previous,true)); fail_write=false;
    CHECK(!x4_epd_present(frame,previous,true));
    CHECK(x4_epd_init(&io)); stuck_busy=true; slept=0;
    CHECK(!x4_epd_present(frame,previous,true)); CHECK(slept>=15000 && slept<15100);
    puts("PASS: SSD1677 region addressing, chunked writes, RAM ping-pong, refresh modes, baseline sync, sleep, SPI failure, BUSY timeout");
}
