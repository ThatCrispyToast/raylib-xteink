#include "x4_epd.h"
#include "raylib_x4.h"

static X4EpdIO bus;
static bool ready;

static bool wait_ready(void)
{
    // BUSY is active high. Always give the controller time to assert it.
    bus.sleep_ms(2);
    for (unsigned int elapsed = 0; elapsed < 15000; elapsed += 5) {
        if (!bus.busy()) return true;
        bus.sleep_ms(5);
    }
    ready = false;
    return false;
}

static bool command(uint8_t cmd, const uint8_t *data, size_t size)
{
    bool ok = bus.write(false, &cmd, 1) && (!size || bus.write(true, data, size));
    if (!ok) ready = false;
    return ok;
}

static bool byte(uint8_t cmd, uint8_t value) { return command(cmd, &value, 1); }

static bool ram_window(void)
{
    const uint8_t x[] = {0, 0, 0x1f, 3}; // 0..799 pixels, SSD1677 uses 16-bit X addresses.
    const uint8_t y[] = {0xdf, 1, 0, 0}; // 479..0
    const uint8_t xc[] = {0, 0}, yc[] = {0xdf, 1};
    return byte(0x11, 0x01) && command(0x44, x, sizeof(x)) && command(0x45, y, sizeof(y)) &&
        command(0x4e, xc, sizeof(xc)) && command(0x4f, yc, sizeof(yc));
}

bool x4_epd_init(const X4EpdIO *io)
{
    if (!io || !io->write || !io->busy || !io->reset || !io->sleep_ms) return false;
    bus = *io; ready = false;
    bus.reset(true); bus.sleep_ms(20);
    bus.reset(false); bus.sleep_ms(2);
    bus.reset(true); bus.sleep_ms(20);
    if (!wait_ready() || !command(0x12, NULL, 0) || !wait_ready()) return false;
    const uint8_t booster[] = {0xae, 0xc7, 0xc3, 0xc0, 0x40};
    const uint8_t driver[] = {0xdf, 0x01, 0x02};
    ready = byte(0x18, 0x80) && command(0x0c, booster, sizeof(booster)) &&
        command(0x01, driver, sizeof(driver)) && byte(0x3c, 0x01);
    return ready;
}

bool x4_epd_present(const uint8_t *frame, const uint8_t *previous, bool full)
{
    if (!ready || !frame || (!full && !previous) || !wait_ready()) return false;
    if (!ram_window() || !command(0x26, full ? frame : previous, X4_FRAME_BYTES) ||
        !ram_window() || !command(0x24, frame, X4_FRAME_BYTES)) return false;
    if (!byte(0x21, full ? 0x40 : 0x00) || !byte(0x22, full ? 0xf7 : 0xfc) ||
        !command(0x20, NULL, 0) || !wait_ready()) return false;
    // Explicitly seed the next differential update after the waveform completes.
    if (!ram_window() || !command(0x26, frame, X4_FRAME_BYTES)) return false;
    // Partial waveforms leave the high-voltage rails on; power them down during idle.
    return full || (byte(0x22, 0x83) && command(0x20, NULL, 0) && wait_ready());
}

bool x4_epd_sleep(void)
{
    if (!ready) return false;
    bool ok = wait_ready() && byte(0x22, 0x83) && command(0x20, NULL, 0) &&
        wait_ready() && byte(0x10, 0x01);
    ready = false;
    return ok;
}
