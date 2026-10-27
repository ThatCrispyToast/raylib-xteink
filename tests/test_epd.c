#include "x4_epd.h"
#include "raylib_x4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static uint8_t cmd, bw[X4_FRAME_BYTES], red[X4_FRAME_BYTES];
static uint8_t frame[X4_FRAME_BYTES], previous[X4_FRAME_BYTES];
static unsigned int activations, slept;
static bool fail_write, stuck_busy;
static uint8_t mode, last_refresh_mode;
static bool write_bus(bool data, const uint8_t *bytes, size_t size)
{
    if (fail_write) return false;
    if (!data) {
        CHECK(size == 1); cmd = *bytes;
        if(cmd == 0x20) {
            activations++;
            if (mode == 0xf7 || mode == 0xfc) {
                last_refresh_mode = mode;
                CHECK(!memcmp(bw,frame,sizeof(frame)));
                CHECK(!memcmp(red,mode == 0xf7 ? frame : previous,sizeof(frame)));
            }
        }
    }
    else if (cmd == 0x24 || cmd == 0x26) {
        CHECK(size == X4_FRAME_BYTES);
        memcpy(cmd == 0x24 ? bw : red, bytes, size);
    } else if (cmd == 0x22) { CHECK(size == 1); mode = *bytes; }
    else if (cmd == 0x44) { const uint8_t expected[] = {0,0,0x1f,3}; CHECK(size == 4 && !memcmp(bytes,expected,4)); }
    else if (cmd == 0x45) { const uint8_t expected[] = {0xdf,1,0,0}; CHECK(size == 4 && !memcmp(bytes,expected,4)); }
    return true;
}
static bool busy(void) { return stuck_busy; }
static void reset(bool high) { (void)high; }
static void sleep_ms(unsigned int ms) { slept += ms; }
int main(void)
{
    X4EpdIO io = {write_bus,busy,reset,sleep_ms};
    CHECK(!x4_epd_init(NULL)); CHECK(x4_epd_init(&io));
    memset(frame,0xa5,sizeof(frame)); memset(previous,0xff,sizeof(previous));
    CHECK(x4_epd_present(frame,NULL,true));
    CHECK(activations == 1 && mode == 0xf7);
    CHECK(!memcmp(bw,frame,sizeof(frame)) && !memcmp(red,frame,sizeof(frame)));
    frame[123] = 0x42;
    CHECK(x4_epd_present(frame,previous,false));
    CHECK(activations == 3 && last_refresh_mode == 0xfc && mode == 0x83);
    CHECK(!memcmp(red,frame,sizeof(frame)));
    CHECK(x4_epd_sleep()); CHECK(!x4_epd_present(frame,previous,false));
    CHECK(x4_epd_init(&io)); fail_write = true;
    CHECK(!x4_epd_present(frame,previous,true)); fail_write = false;
    CHECK(!x4_epd_present(frame,previous,true));
    CHECK(x4_epd_init(&io)); stuck_busy = true; slept = 0;
    CHECK(!x4_epd_present(frame,previous,true)); CHECK(slept >= 15000 && slept < 15100);
    puts("PASS: SSD1677 addressing, full/partial sequences, baseline sync, sleep, SPI failure, BUSY timeout");
}
