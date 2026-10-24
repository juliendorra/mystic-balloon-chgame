#include "Port.h"

namespace mystic {
Progress progress;
static Progress stored;
PortDisplay display;
// The native audio service requires a non-null table even for direct blips.
AUDIO_STEPS(toneAnchor) = { AUDIO_REST(2) };
static const audio::Effect toneEffects[] = { AUDIO_EFFECT(toneAnchor,0) };

void loadProgress() {
    Progress p;
    if (save::load(SAVE_MAGIC, 1, p) && p.level < 39 && p.coins <= 232 &&
        p.highCoins <= 232 && p.options <= 7 && p.score <= 999999 && p.highScore <= 999999)
        progress = p;
    else
        progress = Progress{};
    stored = progress;
    audio::begin(toneEffects, 1, (progress.options & 1) != 0);
}

bool storeProgress() {
    // Commit checkpoints, records and confirmed preferences only when their
    // bytes changed. Settings previews never call this function.
    if (!save::available()) return false;
    if (!memcmp(&progress, &stored, sizeof progress)) return true;
    if (!save::store(SAVE_MAGIC, 1, progress)) return false;
    stored = progress;
    return true;
}

void setSound(bool on) {
    progress.options = (progress.options & ~1u) | on;
    audio::setOn(on);
}

Palette palette() { return Palette((progress.options >> 1) & 3); }

void setPalette(Palette mode) {
    progress.options = (progress.options & 1) | (uint8_t(mode) << 1);
}

void PortDisplay::queue(int x, int y, const uint8_t *s, uint8_t frame, uint8_t mode) {
    const Role role = nextRole==Skip ? spriteRole(s, frame, mode) : nextRole;
    if (role == Skip) return;
    if (commandCount == MAX_COMMANDS) { ++dropped; return; }
    commands[commandCount++] = {s, int8_t(x), int8_t(y), frame, uint8_t((role << 2) | mode)};
}

RAMFUNC(mysticSprites) void PortSprites::draw(int x, int y, const uint8_t *s, uint8_t frame, Mode mode) {
    const int w = s[0], h = s[1], stride = mode == PlusMask ? 2 : 1;
    if (x >= VIEW_W || y >= VIEW_H || x + w <= 0 || y + h <= 0) return;
    display.queue(x, y, s, frame, mode);
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

void PortDisplay::render(bool fullScreen) const {
    renderBase(fullScreen);
    renderColors(fullScreen);
}

RAMFUNC(mysticExpand) void PortDisplay::renderBase(bool fullScreen) const {
    // Read each pair of source bytes once and expand its eight rows directly
    // into packed destination bytes. The hot transpose runs from SRAM.
    const bool original = palette() == Original;
    const int height = fullScreen ? VIEW_H : MENU_H;
    const int top = (fullScreen || title || intro) ? VIEW_Y : MENU_Y;
    if (title) {
        paintSplash(original);
    } else if (intro) {
        memset(gfx_fb, 0, GFX_FB_BYTES);
    } else {
      if (!fullScreen) {
        memset(gfx_fb, 0, MENU_Y*GFX_FB_STRIDE);
        memset(gfx_fb+(MENU_Y+MENU_H)*GFX_FB_STRIDE, 0, MENU_Y*GFX_FB_STRIDE);
      }
      const unsigned shade = world && !original ? 1 : 0;
      for (int page=0; page<height/8; ++page) {
        const uint8_t *src = buffer + page*VIEW_W;
        uint8_t *dst = gfx_fb + (top+page*8)*GFX_FB_STRIDE;
        for (int x=0; x<VIEW_W; x+=2, ++dst) {
            const unsigned a=src[x], b=src[x+1];
#define MB_EXPAND(row) dst[row*GFX_FB_STRIDE] = ((((a>>row)&1) | (((b>>row)&1)<<4))<<shade)
            MB_EXPAND(0); MB_EXPAND(1); MB_EXPAND(2); MB_EXPAND(3);
            MB_EXPAND(4); MB_EXPAND(5); MB_EXPAND(6); MB_EXPAND(7);
#undef MB_EXPAND
        }
      }
      if (world && !original) {
        // Background uses 0/2; adding three changes terrain to 3/5.
        // Tint whole packed spans instead of looking up a tile per pixel.
        for (int ty=0; ty<9; ++ty) for (int tx=0; tx<9; ++tx) {
            if (!(terrain[ty] & (1u<<tx))) continue;
            const int left=max(0,tx*16-(worldX&15));
            const int right=min(VIEW_W,tx*16-(worldX&15)+16);
            const int first=max(0,ty*16-(worldY&15));
            const int last=min(VIEW_H,ty*16-(worldY&15)+16);
            for (int y=first; y<last; ++y) {
                uint8_t *dst=gfx_fb+y*GFX_FB_STRIDE+left/2;
                int count=right-left;
                if (left&1) { *dst++ += 0x30; --count; }
                while (count>=2) { *dst++ += 0x33; count-=2; }
                if (count) *dst += 3;
            }
        }
      }
    }
}

void PortDisplay::renderColors(bool fullScreen) const {
    const bool original=palette()==Original;
    const int top=(fullScreen || title || intro) ? VIEW_Y : MENU_Y;
    if (world && !original) {
    }
    if (!original || title || intro) {
        for (unsigned i=0; i<commandCount; ++i) {
            const Command &c=commands[i];
            if (intro) paintSprite(c.sprite, 4, 8, c.frame, c.style, original, 2);
            else paintSprite(c.sprite, c.x, c.y+top, c.frame, c.style, original);
        }
    }
}



void PortDisplay::renderPause() const {
    // The native world is frozen too. The opaque pause panel replaces its
    // previous contents, so replay only the UI after the preceding DMA ends.
    const bool original=palette()==Original;
    for (unsigned i=pauseCount; i<commandCount; ++i) {
        const Command &c=commands[i];
        paintSprite(c.sprite, c.x, c.y, c.frame, c.style, original);
    }
}
} // namespace mystic
