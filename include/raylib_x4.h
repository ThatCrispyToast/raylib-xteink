#ifndef RAYLIB_X4_H
#define RAYLIB_X4_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define X4_WIDTH 480
#define X4_HEIGHT 800
#define X4_PANEL_WIDTH 800
#define X4_PANEL_HEIGHT 480
#define X4_FRAME_BYTES 48000
#define X4_GRAY_BYTES 96000

typedef struct X4Stats {
    unsigned int frames, full_refreshes, partial_refreshes, unchanged_frames, errors;
    size_t framebuffer_bytes;
    double last_convert_seconds, last_present_seconds;
    unsigned int coalesced_frames; // Pending snapshots replaced by a newer frame.
} X4Stats;

typedef struct X4Status {
    int front_adc, side_adc, battery_mv; // -1 means unavailable; voltage uses ADC calibration.
    unsigned int buttons;              // Bits: Back, Confirm, Left, Right, Up, Down, Power.
    size_t free_heap, minimum_heap, largest_block;
    unsigned int dropped_input_presses; // Queue overflow; zero during normal use.
    bool storage_mounted;
} X4Status;

// Request a full cleaning refresh on the next EndDrawing(), even if unchanged.
void X4RefreshFull(void);
// Full refresh after this many partial updates. Zero means full refresh every time.
void X4SetRefreshInterval(unsigned int partial_updates);
// Opt in after InitWindow. Ordinary updates queue the newest complete frame.
// Startup, explicit cleans, and CloseWindow still wait for panel completion.
// Changing the mode applies to the next EndDrawing(); use WaitDisplay to drain now.
void X4SetAsyncRefresh(bool enabled);
// Drain pending updates. Use before acting on a confirmation or entering sleep.
bool X4WaitDisplay(void);
// Query only. EndDrawing/PollInputEvents service completion and queued frames.
bool X4IsDisplayBusy(void);
X4Stats X4GetStats(void);
X4Status X4GetStatus(void);
// Latest completed panel frame: 800x480, MSB first, 1 = white.
// NULL while an async frame is in flight; wait before capturing.
const uint8_t *X4GetPanelFrame(void);
// Emulator: current virtual SD directory. Hardware: /sd. NULL if not mounted.
const char *X4GetStoragePath(void);

#ifdef __cplusplus
}
#endif
#endif
