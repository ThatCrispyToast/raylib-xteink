// SPDX-License-Identifier: Zlib
#include "xteink_peripherals.h"

bool xteink_gt911_poll(const XteinkI2c *io, XteinkTouch *touch)
{
    uint8_t status, point[4]={0}, ack=0;
    if (!io->read(0x5d,0x814e,2,&status,1)) { *touch=(XteinkTouch){0}; return false; }
    touch->home=(status&0x10)!=0;
    if (!(status&0x80)) return true; // No new sample; preserve a held contact.
    XteinkTouch next={.home=(status&0x10)!=0};
    unsigned int count=status&15;
    bool valid=count<=5;
    if (valid && count) {
        valid=io->read(0x5d,0x8150,2,point,4);
        next.x=point[0] | ((unsigned int)point[1]<<8);
        next.y=point[2] | ((unsigned int)point[3]<<8);
        valid=valid && next.x<480 && next.y<800;
        next.down=valid;
    }
    bool acknowledged=io->write(0x5d,0x814e,2,&ack,1);
    *touch=valid && acknowledged ? next : (XteinkTouch){0};
    return valid && acknowledged;
}
int xteink_battery_mv(const XteinkI2c *io, bool x3)
{
    uint8_t value[2];
    if (!io->read(x3 ? 0x55 : 0x63,x3 ? 0x08 : 0x02,1,value,2)) return -1;
    unsigned int raw=x3 ? value[0] | (value[1]<<8) : ((value[0]&0x3f)<<8) | value[1];
    int mv=x3 ? (int)raw : (int)((raw*5+8)>>4);
    return mv>=2000 && mv<=5000 ? mv : -1;
}
void xteink_frontlight_duty(unsigned int brightness, unsigned int warmth, unsigned int *cool, unsigned int *warm)
{
    if (brightness>100) brightness=100;
    if (warmth>100) warmth=100;
    unsigned int total=(brightness*1023+50)/100;
    *warm=(total*warmth+50)/100;
    *cool=total-*warm;
}
