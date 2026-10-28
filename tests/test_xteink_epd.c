#include "xteink_epd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint8_t old[60000], current[60000], frame[52272], registers[256][16];
static size_t lengths[256], offset;
static uint8_t active;
static bool selected, stuck, fail_data, fail_begin;
static uint64_t now, busy_until;
static unsigned int refreshes, window_before_refresh;
static bool begin(uint8_t command)
{
    CHECK(!selected && now>=busy_until);
    if (fail_begin) return false;
    selected=true; active=command; offset=0; lengths[command]=0;
    if (command==0x04 || command==0x02) busy_until=now+5000;
    if (command==0x12) { busy_until=now+200000; refreshes++; window_before_refresh=lengths[0x90]; }
    return true;
}
static bool data(const uint8_t *bytes, size_t size)
{
    CHECK(selected);
    if (fail_data) return false;
    uint8_t *dest=active==0x10 ? old : active==0x13 ? current : registers[active];
    size_t limit=(active==0x10 || active==0x13) ? sizeof(old) : sizeof(registers[0]);
    if (offset+size<=limit) memcpy(dest+offset,bytes,size);
    else { CHECK(active>=0x20 && active<=0x24 && size==42); memcpy(dest,bytes,limit); }
    offset+=size; lengths[active]=offset;
    return true;
}
static void end(void) { selected=false; }
static bool busy(void) { return stuck || now<busy_until; }
static void reset(bool high) { (void)high; busy_until=now; }
static void sleep_ms(unsigned int ms) { now+=(uint64_t)ms*1000; }
static uint64_t time_us(void) { return now; }
static const XteinkEpdIO io={begin,data,end,busy,reset,sleep_ms,time_us};
static void verify_plane(XteinkPanel panel, const uint8_t *plane)
{
    unsigned int width=panel<XTEINK_UC8179 ? 792 : 800;
    unsigned int height=panel<XTEINK_UC8179 ? 528 : 480;
    unsigned int rows=panel<XTEINK_UC8179 ? 528 : 600, stride=width/8;
    for (unsigned int y=0; y<rows; y++) for (unsigned int x=0; x<stride; x++) {
        uint8_t expected=255;
        if (panel==XTEINK_UC8279_X4) { if (y>=120) expected=frame[(y-120)*stride+x]; }
        else if (y<height) expected=frame[(height-1-y)*stride+x];
        CHECK(plane[y*stride+x]==expected);
    }
}
int main(void)
{
    for (unsigned int i=0; i<sizeof(frame); i++) frame[i]=(i*53+i/97)&255;
    for (XteinkPanel panel=XTEINK_UC8253; panel<=XTEINK_UC8279_X4; panel++) for (int classic=0; classic<2; classic++) {
        XteinkEpd e;
        memset(lengths,0,sizeof(lengths));
        CHECK(xteink_epd_init(&e,&io,panel,classic));
        CHECK(!selected && !xteink_epd_begin(&e,frame,false));
        if (panel==XTEINK_UC8279_X4) CHECK((lengths[0x30]==0)==(classic!=0));
        unsigned int full_before=refreshes;
        CHECK(xteink_epd_begin(&e,frame,true));
        CHECK(xteink_epd_busy(&e) && !xteink_epd_begin(&e,frame,true));
        CHECK(e.pending==frame && e.ready && !selected);
        CHECK(xteink_epd_finish(&e) && !e.pending && e.baseline && !e.powered);
        CHECK(refreshes==full_before+(panel==XTEINK_UC8253 ? 2 : 1));
        size_t bytes=panel<XTEINK_UC8179 ? 52272 : 60000;
        CHECK(lengths[0x10]==bytes && lengths[0x13]==bytes);
        verify_plane(panel,old); verify_plane(panel,current);
        if (panel<XTEINK_UC8179) for (int c=0x20; c<=0x24; c++) CHECK(lengths[c]==42);
        frame[23]^=0xff;
        unsigned int before=refreshes;
        CHECK(xteink_epd_begin(&e,frame,false));
        if (panel==XTEINK_UC8253) CHECK(registers[0x21][0]==0xaa); // Wake uses the scrub bank.
        CHECK(refreshes==before+1);
        if (panel==XTEINK_UC8279_X4) {
            CHECK(window_before_refresh==9 && registers[0x90][5]==120 && registers[0x90][6]==2 && registers[0x90][7]==87);
            CHECK(registers[0x00][0]==0x17 && registers[0xe5][0]==0x5a);
        }
        if (panel==XTEINK_UC8279_X3) CHECK(window_before_refresh==9 && registers[0x90][3]==0x17 && registers[0x90][7]==0x0f);
        CHECK(xteink_epd_finish(&e) && e.powered);
        verify_plane(panel,old); verify_plane(panel,current);
        if (panel==XTEINK_UC8253) {
            CHECK(xteink_epd_begin(&e,frame,false) && registers[0x21][0]==0x20);
            CHECK(xteink_epd_finish(&e));
        }
        now+=2000000; CHECK(xteink_epd_idle(&e) && !e.powered);
        CHECK(xteink_epd_sleep(&e) && !e.ready && registers[0x07][0]==0xa5);
        CHECK(!xteink_epd_begin(&e,frame,true));
        CHECK(xteink_epd_init(&e,&io,panel,classic));
        fail_data=true; CHECK(!xteink_epd_begin(&e,frame,true) && !e.ready && !selected); fail_data=false;
        CHECK(xteink_epd_init(&e,&io,panel,classic));
        fail_begin=true; CHECK(!xteink_epd_begin(&e,frame,true) && !e.ready && !selected); fail_begin=false;
        CHECK(xteink_epd_init(&e,&io,panel,classic) && xteink_epd_begin(&e,frame,true));
        stuck=true; now+=15000000;
        CHECK(!xteink_epd_busy(&e) && !e.ready && !xteink_epd_finish(&e));
        stuck=false;
    }
    puts("PASS: four UltraChip variants, RAM orientation/padding, old-plane sync, LUTs, partial windows, idle/sleep, SPI failures, BUSY timeout");
}
