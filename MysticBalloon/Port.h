// Native CHGame services for Mystic Balloon's original Arduboy game code.
#pragma once
#include <CHGame.h>
#include <string.h>
#include "Tones.h"

namespace mystic {
constexpr int VIEW_Y = 0;
constexpr int VIEW_W = 128;
constexpr int VIEW_H = 128;
constexpr int MENU_H = 64;
constexpr int MENU_Y = (VIEW_H - MENU_H) / 2;
enum Palette : uint8_t { Color, Original, DMG, Amber };
enum Role : uint8_t { Text, Gold, CoinInk, KeyInk, BalloonInk, KidMask,
    KidInk, KidSuck, WalkerInk, FanInk, SpikeInk, ParticleInk, DoorInk,
    EyeInk, LifeInk, HudCoinInk, LogoInk, YellowBalloonInk, BlueBalloonInk,
    YellowLifeInk, BlueLifeInk, TerrainInk, Skip = 63 };
constexpr uint32_t SAVE_MAGIC = save::magic("MYBL");
struct Progress {
    uint32_t score = 0, highScore = 0;
    // Bit 0 is sound; bits 1..2 select the palette. Old records use 0/1.
    uint8_t level = 0, coins = 0, highCoins = 0, options = 1;
};
static_assert(sizeof(Progress) == 12, "save format must be identical on host and device");
extern Progress progress;
void loadProgress();
bool storeProgress();
void setSound(bool on);
Palette palette();
void setPalette(Palette mode);
Role spriteRole(const uint8_t *sprite, uint8_t frame, uint8_t mode);
void applyPalette();
#if CHGAME_DEBUG
void setColorVariant(uint8_t index);
#endif
void paintSprite(const uint8_t *sprite, int x, int y, uint8_t frame,
                 uint8_t style, bool original, int scale = 1);
void paintSplash(bool original);

struct PortAudio {
    static bool enabled() { return tones::enabled(); }
    void on() { setSound(true); }
    void off() { setSound(false); }
    void saveOnOff() { setSound(enabled()); }
};

struct PortDisplay {
    struct Command {
        const uint8_t *sprite;
        int8_t x, y;
        uint8_t frame, style;
    };
    static constexpr unsigned MAX_COMMANDS = 128;
    PortAudio audio;
    uint8_t buffer[VIEW_W * VIEW_H / 8] = {};
    Command commands[MAX_COMMANDS];
    uint16_t commandCount = 0, pauseCount = 0, dropped = 0;
    uint16_t terrain[9] = {};
    uint8_t terrainFrames[9][9];
    bool terrainCoverage = false;
    // At most 16 columns by three source pages under the player. Preserve
    // this before the original white clearing mask touches the mono buffer.
    uint8_t kidBackdrop[48];
    uint8_t kidX=0, kidPage=0, kidW=0, kidPages=0;
    int16_t worldX = 0, worldY = 0;
    bool world = false, title = false, intro = false, paused = false;
    Role nextRole = Skip;
    bool pressed(uint8_t b) const { return chgame.pressed(b); }
    bool justPressed(uint8_t b) const { return chgame.justPressed(b); }
    bool everyXFrames(uint16_t n) const { return uint16_t(chgame.frameCount) % n == 0; }
    void clear() {
        memset(buffer, 0, sizeof buffer);
        memset(terrain, 0, sizeof terrain);
        terrainCoverage = false;
        commandCount = dropped = 0;
        kidW = 0;
        world = title = intro = paused = false;
        nextRole = Skip;
    }
    void beginPause() {
        if (!paused) { pauseCount = commandCount; paused = true; }
        commandCount = pauseCount;
    }
    void queue(int x, int y, const uint8_t *sprite, uint8_t frame, uint8_t mode);
    void noteTerrain(unsigned x,unsigned y,uint8_t frame) {
        terrain[y] |= 1u<<x;
        terrainFrames[y][x]=frame;
        terrainCoverage=true;
    }
    void render(bool fullScreen = true) const;
    void renderBase(bool fullScreen) const;
    void renderColors(bool fullScreen) const;
    void renderPause() const;
    void restoreKidBackground() const;
    void restoreTerrainCorners() const;
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
    void drawTinted(int x, int y, const uint8_t *s, uint8_t f, Mode mode, Role role) {
        display.nextRole = role;
        draw(x,y,s,f,mode);
        display.nextRole = Skip;
    }
};
struct PortSound {
    void tone(uint16_t hz, uint16_t duration) { tones::tone(hz, duration); }
};
} // namespace mystic
