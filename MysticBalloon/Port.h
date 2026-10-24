// Native CHGame services for Mystic Balloon's original Arduboy game code.
#pragma once
#include <CHGame.h>
#include <string.h>

namespace mystic {
constexpr int VIEW_Y = 0;
constexpr int VIEW_W = 128;
constexpr int VIEW_H = 128;
constexpr int MENU_H = 64;
constexpr int MENU_Y = (VIEW_H - MENU_H) / 2;
constexpr uint32_t SAVE_MAGIC = save::magic("MYBL");
struct Progress {
    uint32_t score = 0, highScore = 0;
    uint8_t level = 0, coins = 0, highCoins = 0, sound = 1;
};
static_assert(sizeof(Progress) == 12, "save format must be identical on host and device");
extern Progress progress;
void loadProgress();
void storeProgress();
void setSound(bool on);

struct PortAudio {
    static bool enabled() { return audio::on(); }
    void on() { setSound(true); }
    void off() { setSound(false); }
    void saveOnOff() { progress.sound = enabled(); }
};

struct PortDisplay {
    PortAudio audio;
    uint8_t buffer[VIEW_W * VIEW_H / 8] = {};
    bool pressed(uint8_t b) const { return chgame.pressed(b); }
    bool justPressed(uint8_t b) const { return chgame.justPressed(b); }
    bool everyXFrames(uint16_t n) const { return chgame.everyXFrames(n); }
    void clear() { memset(buffer, 0, sizeof buffer); }
    void render(bool fullScreen = true) const;
};
extern PortDisplay display;

// Sprite formats are the original page-packed Arduboy bytes. In PlusMask
// each image byte is followed by its opacity mask, including across frames.
struct PortSprites {
    enum Mode : uint8_t { Self, Overwrite, Erase, PlusMask };
    static void draw(int x, int y, const uint8_t *sprite, uint8_t frame, Mode mode);
    void drawSelfMasked(int x, int y, const uint8_t *s, uint8_t f) { draw(x,y,s,f,Self); }
    void drawOverwrite(int x, int y, const uint8_t *s, uint8_t f) { draw(x,y,s,f,Overwrite); }
    void drawErase(int x, int y, const uint8_t *s, uint8_t f) { draw(x,y,s,f,Erase); }
    void drawPlusMask(int x, int y, const uint8_t *s, uint8_t f) { draw(x,y,s,f,PlusMask); }
};
struct PortSound {
    void tone(uint16_t hz, uint16_t ms) { audio::blip(hz, ms); }
};
} // namespace mystic
