#include "xteink_peripherals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint8_t status, point[4], voltage[2];
static bool fail_read, fail_write;
static int acknowledgements;
static bool read_reg(uint8_t addr, uint16_t reg, unsigned int n, uint8_t *out, size_t size)
{
    if (fail_read) return false;
    if (addr==0x5d) {
        CHECK(n==2);
        if (reg==0x814e) { CHECK(size==1); *out=status; }
        else { CHECK(reg==0x8150 && size==4); memcpy(out,point,4); }
    } else { CHECK(n==1 && size==2 && ((addr==0x55 && reg==8) || (addr==0x63 && reg==2))); memcpy(out,voltage,2); }
    return true;
}
static bool write_reg(uint8_t addr, uint16_t reg, unsigned int n, const uint8_t *out, size_t size)
{
    CHECK(addr==0x5d && reg==0x814e && n==2 && size==1 && *out==0);
    acknowledgements++; return !fail_write;
}
int main(void)
{
    XteinkI2c io={read_reg,write_reg}; XteinkTouch touch={0};
    status=0x91; point[0]=0xdf; point[1]=1; point[2]=0x1f; point[3]=3;
    CHECK(xteink_gt911_poll(&io,&touch) && touch.down && touch.home && touch.x==479 && touch.y==799);
    status=0x10; CHECK(xteink_gt911_poll(&io,&touch) && touch.down && touch.home && acknowledgements==1);
    status=0x80; CHECK(xteink_gt911_poll(&io,&touch) && !touch.down && !touch.home && acknowledgements==2);
    status=0x81; point[0]=0xe0; CHECK(!xteink_gt911_poll(&io,&touch) && !touch.down && acknowledgements==3);
    status=0x8f; CHECK(!xteink_gt911_poll(&io,&touch) && !touch.down && acknowledgements==4);
    status=0x81; point[0]=10; point[1]=0; point[2]=20; point[3]=0;
    fail_write=true; CHECK(!xteink_gt911_poll(&io,&touch) && !touch.down); fail_write=false;
    CHECK(xteink_gt911_poll(&io,&touch) && touch.down);
    fail_read=true; CHECK(!xteink_gt911_poll(&io,&touch) && !touch.down); CHECK(xteink_battery_mv(&io,true)==-1); fail_read=false;
    voltage[0]=0x74; voltage[1]=0x0e; CHECK(xteink_battery_mv(&io,true)==3700);
    voltage[0]=0x2e; voltage[1]=0x40; CHECK(xteink_battery_mv(&io,false)==3700);
    voltage[0]=voltage[1]=0xff; CHECK(xteink_battery_mv(&io,true)==-1 && xteink_battery_mv(&io,false)==-1);
    for (unsigned int brightness=0; brightness<=100; brightness++) for (unsigned int warmth=0; warmth<=100; warmth++) {
        unsigned int cool,warm; xteink_frontlight_duty(brightness,warmth,&cool,&warm);
        CHECK(cool+warm==(brightness*1023+50)/100 && cool<=1023 && warm<=1023);
        if (!brightness) CHECK(!cool && !warm);
        if (!warmth) CHECK(!warm);
        if (warmth==100) CHECK(!cool);
    }
    puts("PASS: GT911 readiness, coordinates, release/home, malformed/failing samples; gauge byte order and frontlight power budget");
}
