// SPDX-License-Identifier: MIT
// Copyright (c) 2026 FreeInk
// Register sequences adapted from FreeInk; see LICENSES/FreeInk-MIT.txt and THIRD_PARTY.md.
#include "xteink_epd.h"
#include "xteink_luts.h"
#include <string.h>

static bool failed(XteinkEpd *e)
{
    e->io.end();
    e->ready=e->baseline=e->powered=false;
    e->pending=NULL;
    return false;
}
static bool command(XteinkEpd *e, uint8_t c, const uint8_t *data, size_t size)
{
    if (!e->io.begin(c) || (size && !e->io.data(data,size))) return failed(e);
    e->io.end();
    return true;
}
#define CMD(e,c,...) command(e,c,(const uint8_t[]){__VA_ARGS__},sizeof((const uint8_t[]){__VA_ARGS__}))
static bool cmd(XteinkEpd *e, uint8_t c) { return command(e,c,NULL,0); }
static bool wait_until(XteinkEpd *e, uint64_t start)
{
    e->io.sleep_ms(2);
    while (e->io.busy()) {
        if (e->io.time_us()-start >= 15000000) return failed(e);
        e->io.sleep_ms(2);
    }
    return e->io.time_us()-start < 15000000 || failed(e);
}
static bool power(XteinkEpd *e, bool on)
{
    if (e->powered == on) return true;
    if (!cmd(e,on ? 0x04 : 0x02) || !wait_until(e,e->io.time_us())) return false;
    e->powered=on;
    return true;
}
static bool window(XteinkEpd *e, unsigned int offset)
{
    unsigned int x=e->width-1, y=offset+e->height-1;
    return cmd(e,0x91) && CMD(e,0x90,0,0,x>>8,x,offset>>8,offset,y>>8,y,1);
}
static bool plane(XteinkEpd *e, uint8_t c, const uint8_t *frame, bool invert)
{
    unsigned int stride=e->width/8, rows=e->panel>=XTEINK_UC8179 ? 600 : e->height;
    unsigned int offset=e->panel==XTEINK_UC8279_X4 ? 120 : 0;
    uint8_t block[100];
    if (!e->io.begin(c)) return failed(e);
    for (unsigned int y=0; y<rows; y++) {
        if (!frame || y<offset || y>=offset+e->height) memset(block,0xff,stride);
        else {
            unsigned int row=e->panel==XTEINK_UC8279_X4 ? y-offset : e->height-1-y;
            const uint8_t *src=frame+row*stride;
            for (unsigned int x=0; x<stride; x++) block[x]=invert ? (uint8_t)~src[x] : src[x];
        }
        if (!e->io.data(block,stride)) return failed(e);
    }
    e->io.end();
    return true;
}
static bool lut(XteinkEpd *e, bool full, bool half)
{
    for (int i=0; i<5; i++) {
        const uint8_t *data;
        if (e->panel==XTEINK_UC8253) data=(full ? x3_full : half ? x3_half : x3_normal)[i];
        else data=(full ? x3_8279_gc : x3_8279_du)[i]+1;
        if (!command(e,0x20+i,data,42)) return false;
    }
    return true;
}
static bool refresh(XteinkEpd *e)
{
    if (!cmd(e,0x12)) return false;
    e->started=e->io.time_us();
    // Allow active-low controllers to assert BUSY before the first asynchronous poll.
    for (int i=0; i<50 && !e->io.busy(); i++) e->io.sleep_ms(1);
    return true;
}
bool xteink_epd_init(XteinkEpd *e, const XteinkEpdIO *io, XteinkPanel panel, bool classic)
{
    if (!e || !io || !io->begin || !io->data || !io->end || !io->busy ||
        !io->reset || !io->sleep_ms || !io->time_us || panel<XTEINK_UC8253 || panel>XTEINK_UC8279_X4) return false;
    *e=(XteinkEpd){.io=*io,.panel=panel,.classic=classic};
    e->width=panel<XTEINK_UC8179 ? 792 : 800;
    e->height=panel<XTEINK_UC8179 ? 528 : 480;
    io->reset(true); io->sleep_ms(10); io->reset(false); io->sleep_ms(50);
    io->reset(true); io->sleep_ms(50);
    if (!wait_until(e,io->time_us())) return false;
    if (panel==XTEINK_UC8279_X3) {
        if (!CMD(e,0x00,0x3f,0x4a) || !window(e,0) || !CMD(e,0x03,0x20) ||
            !CMD(e,0x01,0x43,0,0x78,0x78,0x17) || !CMD(e,0x82,0x24) ||
            !CMD(e,0x06,0x25,0x25,0x3c) || !CMD(e,0x30,0x0f) || !CMD(e,0xe1,2)) return false;
    } else {
        bool uc8279=panel==XTEINK_UC8279_X4;
        if (!CMD(e,0x00,uc8279 ? 0x37 : 0x3f,uc8279 ? 0x4d : 0x0a) ||
            !CMD(e,0x61,e->width>>8,e->width,0x02,0x58) ||
            !CMD(e,0x65,0,0,0,0) || !CMD(e,0x03,0x20)) return false;
        if (panel==XTEINK_UC8253 && (!CMD(e,0x01,7,0x17,0x3f,0x3f,0x17) ||
            !CMD(e,0x82,0x24))) return false;
        if (!uc8279 && !CMD(e,0x06,0x25,0x25,0x3c,panel==XTEINK_UC8253 ? 0x37 : 0x25)) return false;
        if (panel==XTEINK_UC8253 && !CMD(e,0x30,9)) return false;
        if (uc8279 && !classic && !CMD(e,0x30,0x0e)) return false;
        if (!CMD(e,0xe1,2)) return false;
    }
    e->ready=true;
    return true;
}
bool xteink_epd_begin(XteinkEpd *e, const uint8_t *frame, bool full)
{
    if (!e->ready || e->pending || !frame || (!full && !e->baseline)) return false;
    if (!wait_until(e,e->io.time_us())) return false;
    e->full=full;
    if (e->panel==XTEINK_UC8279_X3 && !window(e,0)) return false;
    if (e->panel<XTEINK_UC8179) {
        if ((e->panel==XTEINK_UC8253 && full) || !e->baseline) {
            if (!plane(e,0x10,NULL,false) || !cmd(e,0x11)) return false;
        }
        if (!plane(e,0x13,frame,false) || !cmd(e,0x11)) return false;
        if (e->panel==XTEINK_UC8253) {
            if (!CMD(e,0x50,full ? 0x29 : 0xa9,7)) return false;
        } else if (!CMD(e,0x50,e->baseline ? 0xd7 : 0x97)) return false;
        if (!lut(e,full,!e->powered) || !power(e,true)) return false;
    } else {
        if (!plane(e,0x13,frame,false)) return false;
        if (full && !plane(e,0x10,NULL,false)) return false;
        if (e->panel==XTEINK_UC8179) {
            if (!CMD(e,0x50,0x29,7)) return false;
        } else if (!CMD(e,0x50,full ? 0x97 : 0xd7)) return false;
        if (!CMD(e,0xe0,2) || !CMD(e,0xe5,full ? 0x1e : 0x5a)) return false;
        if (e->panel==XTEINK_UC8179 && !CMD(e,0x00,0x1f,0x0a)) return false;
        if (!full && (!CMD(e,0x03,0x20) || !CMD(e,0xe1,2))) return false;
        if (!power(e,true)) return false;
        if (!full) {
            if (e->panel==XTEINK_UC8279_X4) { if (!window(e,120)) return false; }
            else if (!cmd(e,0x91)) return false;
        }
        if (e->panel==XTEINK_UC8279_X4 && !CMD(e,0x00,0x17,0x4d)) return false;
    }
    if (!refresh(e)) return false;
    e->pending=frame;
    return true;
}
bool xteink_epd_busy(XteinkEpd *e)
{
    if (!e->pending) return false;
    if (e->io.time_us()-e->started>=15000000) return failed(e);
    return e->io.busy();
}
bool xteink_epd_finish(XteinkEpd *e)
{
    if (!e->ready) return false;
    if (!e->pending) return true;
    if (!wait_until(e,e->started)) return false;
    if (e->panel==XTEINK_UC8253 && e->full) {
        // Condition the full image before accepting the next differential update.
        e->io.sleep_ms(200);
        if (!CMD(e,0x50,0xa9,7) || !lut(e,false,false) || !window(e,0) ||
            !plane(e,0x13,e->pending,false) || !cmd(e,0x11) || !cmd(e,0x92) ||
            !refresh(e) || !wait_until(e,e->started)) return false;
    }
    if (e->panel>=XTEINK_UC8179 && !e->full && !cmd(e,0x92)) return false;
    if (e->panel==XTEINK_UC8179 && !CMD(e,0x50,0xa9,7)) return false;
    if (e->panel==XTEINK_UC8279_X3 && !CMD(e,0x50,0xd7)) return false;
    if (!plane(e,0x10,e->pending,false)) return false;
    if (e->panel<XTEINK_UC8179 && !cmd(e,0x11)) return false;
    if (e->panel==XTEINK_UC8279_X3 && !cmd(e,0x92)) return false;
    e->pending=NULL; e->baseline=true; e->activity=e->io.time_us();
    return !e->full || power(e,false);
}
bool xteink_epd_idle(XteinkEpd *e)
{
    if (!e->ready) return false;
    return e->pending || e->io.time_us()-e->activity<2000000 || power(e,false);
}
bool xteink_epd_sleep(XteinkEpd *e)
{
    if (!e->ready) return false;
    bool ok=xteink_epd_finish(e) && power(e,false) && CMD(e,0x07,0xa5);
    e->ready=e->baseline=false;
    return ok;
}
