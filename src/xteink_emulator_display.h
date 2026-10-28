// SPDX-License-Identifier: Zlib
#ifndef XTEINK_EMULATOR_DISPLAY_H
#define XTEINK_EMULATOR_DISPLAY_H
#include "xteink_device.h"
#include <SDL.h>
#include <stdexcept>
#include <array>
#include <algorithm>
#include <cstring>

// Native portrait preview of the selected profile's packed panel frame.
class XteinkEmulatorDisplay {
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    std::array<uint8_t, XTEINK_WIDTH*XTEINK_HEIGHT/8> frame_;
    unsigned int brightness_ = 0, warmth_ = 0;
    void close() {
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }
public:
    explicit XteinkEmulatorDisplay(const char *title) {
        frame_.fill(0xff);
        try {
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) throw std::runtime_error(SDL_GetError());
            window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                XTEINK_WIDTH, XTEINK_HEIGHT, SDL_WINDOW_SHOWN);
            if (!window_) throw std::runtime_error(SDL_GetError());
            renderer_ = SDL_CreateRenderer(window_, -1, 0);
            if (!renderer_) throw std::runtime_error(SDL_GetError());
            texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                XTEINK_WIDTH, XTEINK_HEIGHT);
            if (!texture_) throw std::runtime_error(SDL_GetError());
        } catch (...) { close(); throw; }
    }
    ~XteinkEmulatorDisplay() { close(); }
    XteinkEmulatorDisplay(const XteinkEmulatorDisplay&) = delete;
    XteinkEmulatorDisplay& operator=(const XteinkEmulatorDisplay&) = delete;
    SDL_Renderer *renderer() const { return renderer_; }
    bool frontlight(unsigned int brightness, unsigned int warmth) {
        brightness_ = brightness;
        warmth_ = warmth;
        return draw(nullptr);
    }
    bool draw(const uint8_t *frame) {
        if (frame) std::memcpy(frame_.data(), frame, frame_.size());
        const unsigned int cool = brightness_*(100-warmth_)/100;
        const unsigned int warm = brightness_-cool;
        // A visual preview of the two LEDs, not a calibrated panel color model.
        const int paper[] = {232+(int)(cool*23+warm*23)/100,
                            220+(int)(cool*35+warm*5)/100,
                            200+((int)(cool*55)-(int)(warm*50))/100};
        void *raw = nullptr;
        int pitch = 0;
        if (SDL_LockTexture(texture_, nullptr, &raw, &pitch) != 0) return false;
        for (int y = 0; y < XTEINK_HEIGHT; y++) {
            auto *row = static_cast<uint8_t *>(raw) + y*pitch;
            for (int x = 0; x < XTEINK_WIDTH; x++) {
                unsigned int bit = (XTEINK_WIDTH - 1 - x)*XTEINK_HEIGHT + y;
                bool white = (frame_[bit/8] & (0x80u >> (bit%8))) != 0;
                row[3*x] = white ? paper[0] : 24;
                row[3*x+1] = white ? paper[1] : 26;
                row[3*x+2] = white ? paper[2] : 28;
            }
        }
        SDL_UnlockTexture(texture_);
        return SDL_RenderClear(renderer_) == 0 && SDL_RenderCopy(renderer_, texture_, nullptr, nullptr) == 0;
    }
};
#endif
