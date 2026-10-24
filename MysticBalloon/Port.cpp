#include "Port.h"

namespace mystic {
Progress progress;
static Progress stored;
PortDisplay display;

void loadProgress() {
    Progress p;
    if (save::load(SAVE_MAGIC, 1, p) && p.level < 39 && p.coins <= 232 &&
        p.highCoins <= 232 && p.sound <= 1 && p.score <= 999999 && p.highScore <= 999999)
        progress = p;
    else
        progress = Progress{};
    stored = progress;
    audio::begin(nullptr, 0, progress.sound != 0);
}

void storeProgress() {
    // The original wrote EEPROM each frame. Flash is committed only after a
    // completed tally or changed sound setting, and only when bytes changed.
    if (memcmp(&progress, &stored, sizeof progress) &&
        save::store(SAVE_MAGIC, 1, progress))
        stored = progress;
}

void setSound(bool on) {
    progress.sound = on;
    audio::setOn(on);
}

RAMFUNC(mysticSprites) void PortSprites::draw(int x, int y, const uint8_t *s, uint8_t frame, Mode mode) {
    const int w = s[0], h = s[1], stride = mode == PlusMask ? 2 : 1;
    if (x >= VIEW_W || y >= VIEW_H || x + w <= 0 || y + h <= 0) return;
    s += 2 + frame * w * ((h + 7) / 8) * stride;
    const int x0 = max(0, -x), x1 = min(w, VIEW_W - x);
    const int shift = y & 7, firstPage = y >> 3;
    // An Arduboy byte is already eight vertical pixels. Stamp it as a byte,
    // or as two shifted bytes, rather than visiting each pixel separately.
    for (int page=0; page<(h+7)/8; ++page) {
        const int row = firstPage + page;
        if (row < -1 || row >= VIEW_H/8) continue;
        const uint8_t valid = h-page*8 >= 8 ? 255 : (1u << (h-page*8))-1;
        const uint8_t *src = s + (page*w + x0)*stride;
        if (!shift) {
            if (row < 0) continue;
            uint8_t *dst = display.buffer + row*VIEW_W + x+x0;
            if (mode == Overwrite && valid == 255) {
                memcpy(dst, src, x1-x0);
                continue;
            }
            for (int sx=x0; sx<x1; ++sx, ++dst, src+=stride) {
                const uint8_t ink = *src & valid;
                const uint8_t mask = mode==PlusMask ? src[1]&valid : mode==Overwrite ? valid : ink;
                *dst = (*dst & ~mask) | (mode==Erase ? 0 : ink);
            }
        } else {
            const bool low = row>=0, high = row+1<VIEW_H/8;
            uint8_t *lo = display.buffer + ((low ? row : 0)*VIEW_W + x+x0);
            uint8_t *hi = display.buffer + ((high ? row+1 : 0)*VIEW_W + x+x0);
            for (int sx=x0; sx<x1; ++sx, ++lo, ++hi, src+=stride) {
                const unsigned ink = *src & valid;
                const unsigned mask = (mode==PlusMask ? src[1]&valid : mode==Overwrite ? valid : ink) << shift;
                const unsigned bits = mode==Erase ? 0 : ink << shift;
                if (low) *lo = (*lo & ~mask) | bits;
                if (high) *hi = (*hi & ~(mask>>8)) | (bits>>8);
            }
        }
    }
}

RAMFUNC(mysticExpand) void PortDisplay::render(bool fullScreen) const {
    // Read each pair of source bytes once and expand its eight rows directly
    // into packed destination bytes. The hot transpose runs from SRAM.
    const int height = fullScreen ? VIEW_H : MENU_H;
    const int top = fullScreen ? VIEW_Y : MENU_Y;
    if (!fullScreen) {
        memset(gfx_fb, 0, MENU_Y*GFX_FB_STRIDE);
        memset(gfx_fb+(MENU_Y+MENU_H)*GFX_FB_STRIDE, 0, MENU_Y*GFX_FB_STRIDE);
    }
    for (int page=0; page<height/8; ++page) {
        const uint8_t *src = buffer + page*VIEW_W;
        uint8_t *dst = gfx_fb + (top+page*8)*GFX_FB_STRIDE;
        for (int x=0; x<VIEW_W; x+=2, ++dst) {
            const unsigned a=src[x], b=src[x+1];
            dst[0*GFX_FB_STRIDE] = (a&1) | ((b&1)<<4);
            dst[1*GFX_FB_STRIDE] = ((a>>1)&1) | (((b>>1)&1)<<4);
            dst[2*GFX_FB_STRIDE] = ((a>>2)&1) | (((b>>2)&1)<<4);
            dst[3*GFX_FB_STRIDE] = ((a>>3)&1) | (((b>>3)&1)<<4);
            dst[4*GFX_FB_STRIDE] = ((a>>4)&1) | (((b>>4)&1)<<4);
            dst[5*GFX_FB_STRIDE] = ((a>>5)&1) | (((b>>5)&1)<<4);
            dst[6*GFX_FB_STRIDE] = ((a>>6)&1) | (((b>>6)&1)<<4);
            dst[7*GFX_FB_STRIDE] = ((a>>7)&1) | (((b>>7)&1)<<4);
        }
    }
}
} // namespace mystic
