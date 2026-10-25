/* Mystic Balloon, Arduboy 1.7.2 by TEAM a.r.g., ported to CHGame.
 * MIT; see LICENSE and NOTICE. Original game: the headers beside this file.
 * CHGame hardware, sprite adapter and flash saves: Port.h / Port.cpp.
 */
#include <CHGame.h>
#include "config.h"
#define GAME_ID 34
#include "globals.h"
#include "menu.h"
#include "game.h"
#include "inputs.h"
#include "player.h"
#include "enemies.h"
#include "elements.h"
#include "levels.h"
#include "Perf.h"

using FunctionPointer = void (*)();
const FunctionPointer mainGameLoop[] = {
    stateMenuIntro, stateMenuMain, stateMenuHelp, stateMenuPlaySelect,
    stateMenuInfo, stateMenuSoundfx, stateGameNextLevel, stateGamePlaying,
    stateGamePause, stateGameOver, stateMenuPlayContinue, stateMenuPlayNew,
};
static_assert(sizeof levels / sizeof levels[0] == TOTAL_LEVELS, "all 39 levels required");
static bool soundSaveRequested = false;

namespace mystic {
Role spriteRole(const uint8_t *s, uint8_t frame, uint8_t mode) {
    if (s==tileSetTwo) return Skip;
    if (s==T_arg) return LogoInk;
    if (s==kidSprite) return mode==PortSprites::Erase ? KidInk : KidMask;
    if (s==kidSpriteSuck_plus_mask) return KidSuck;
    if (s==balloon_plus_mask) return BalloonInk;
    if (s==walkerSprite) return WalkerInk;
    if (s==sprSpikes) return SpikeInk;
    if (s==fan) return FanInk;
    if (s==particle) return ParticleInk;
    if (s==elements) return frame<4 ? CoinInk : KeyInk;
    if (s==elementsHUD && frame==10) return LifeInk;
    if (s==elementsHUD && frame==11) return HudCoinInk;
    if (s==door) return DoorInk;
    if (s==stars || s==badgeMysticBalloon) return Gold;
    if (s==leftGuyLeftEye || s==leftGuyRightEye || s==rightGuyEyes) return EyeInk;
    return Text;
}
}

#if CHGAME_DEBUG
bool debugCommand(char cmd, const char *args) {
    if (cmd == 'U') { mystic::framePerf.report(); return true; }
    if (cmd == 'O') { mystic::tones::setEnabled(dbg::parseNum(args,10)!=0); return true; }
    if (cmd == 'C' || cmd == 'T') {
        const unsigned n=dbg::parseNum(args,10);
        if (n>3) return false;
        mystic::setPalette(cmd=='T' ? mystic::Color : mystic::Palette(n));
        if (cmd=='T') mystic::setColorVariant(n);
        gfx_wait();
        mystic::applyPalette();
        arduboy.render(gameState==STATE_GAME_PLAYING || gameState==STATE_GAME_PAUSE);
        gfx_flushAsync();
        return true;
    }
    if (cmd == 'V') {
        char line[352];
        unsigned ns = 0, nf = 0, nw = 0, maxHeight = 0;
        for (unsigned i = 0; i < MAX_PER_TYPE; ++i) {
            if ((spikes[i].characteristics >> 2) & 1) {
                ++ns;
                if (spikes[i].pos.height > (int)maxHeight) maxHeight = spikes[i].pos.height;
            }
            nf += fans[i].active;
            nw += walkers[i].active;
        }
        snprintf(line, sizeof line,
            "MYBL state=%u level=%u x=%d y=%d sy=%d balloons=%u coins=%u total=%u key=%u score=%u spikes=%u maxheight=%u fans=%u walkers=%u saved=%u sound=%u camx=%d camy=%d view=%u palette=%u draws=%u dropped=%u\n",
            gameState, level+1, kid.pos.x, kid.pos.y, kid.speed.y,
            kid.balloons, coinsActive, totalCoins, key.haveKey, (unsigned)scorePlayer,
            ns, maxHeight, nf, nw, progress.level+1, mystic::tones::enabled(), cam.pos.x, cam.pos.y, mystic::VIEW_H,
            mystic::palette(), arduboy.commandCount, arduboy.dropped);
        dbg::print(line);
        return true;
    }
    if (cmd == 'D') {
        // Static native pixel reference for all twelve poses and four inhale
        // frames. Lockstep keeps this diagnostic canvas until the next frame.
        const unsigned f=dbg::parseNum(args,10);
        if (f>=16) return false;
        gfx_wait();arduboy.clear();
        memset(arduboy.buffer,255,sizeof arduboy.buffer);arduboy.world=true;
        if (f<12) {
            sprites.drawSelfMasked(48,48,kidSprite,12+f/6);
            sprites.drawErase(48,48,kidSprite,f);
        } else sprites.drawPlusMask(48,48,kidSpriteSuck_plus_mask,f-12);
        mystic::applyPalette();arduboy.render(true);gfx_flushAsync();
        return true;
    }
    if (cmd == 'G') {
        const unsigned n = dbg::parseNum(args, 10);
        if (n < 1 || n > TOTAL_LEVELS) return false;
        stateMenuPlayNew();
        level = n - 1;
        setKid();
        enemiesInit();
        levelLoad(levels[level]);
        kid.pos = kid.actualpos >> FIXED_POINT;
        resetCamera();
        gameState = STATE_GAME_PLAYING;
        return true;
    }
    if (cmd == 'J') {
        // Reproducible edge/fan launch in the current level; normal physics
        // runs on subsequent frames. Diagnostic only, never writes a save.
        const int x=int32_t(dbg::parseNum(args,10));
        const int y=int32_t(dbg::parseNum(args,10));
        const int vy=int32_t(dbg::parseNum(args,10));
        if (x<0 || x>LEVEL_WIDTH-12 || y<0 || y>LEVEL_HEIGHT-16 ||
            vy < -MAX_YSPEED || vy > MAX_YSPEED) return false;
        kid.pos=vec2(x,y);kid.actualpos=kid.pos<<FIXED_POINT;
        kid.speed=vec2(0,vy);kid.isImune=false;kid.isActive=true;
        kid.isWalking=kid.isJumping=kid.isSucking=false;
        kid.isBalloon=kid.isLanding=true;kid.balloonOffset=0;
        resetCamera();gameState=STATE_GAME_PLAYING;
        return true;
    }
    if (cmd == 'R') {
        // Discard diagnostic level/position changes and reload saved options.
        mystic::loadProgress();
        gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_PLAY;cont=0;
        return true;
    }
    return false;
}
#endif

void setup() {
    chgame.boot();
    dbg::begin("MYBL " CHMB_VERSION);
#if CHGAME_DEBUG
    dbg::hook = debugCommand;
#endif
    gfx_begin(GFX_DIV2, GFX_12BPP);
    chgame.setFrameRate(CHMB_FPS);
    mystic::loadProgress();
    mystic::applyPalette();
    gfx_clear(0);
}

void loop() {
    dbg::poll();
    if (!chgame.nextFrame()) { dbg::waitInput(); return; }
#if CHGAME_DEBUG
    const uint32_t perfStart = micros();
#endif
    dbg::markUpdateStart();
    chgame.pollButtons();
    if (chgame.justPressed(SELECT_BUTTON)) {
        mystic::setSound(!mystic::tones::enabled());
        if (gameState!=STATE_MENU_SOUNDFX) soundSaveRequested=true;
    }
    // Resume on the following frame so START never advances a paused world.
    const bool start = chgame.justPressed(START_BUTTON);
    const bool resume = start && gameState == STATE_GAME_PAUSE;
    if (start && gameState == STATE_GAME_PLAYING) gameState = STATE_GAME_PAUSE;
    if (gameState < STATE_GAME_NEXT_LEVEL && chgame.everyXFrames(10))
        sparkleFrames = (sparkleFrames + 1) % 5;
    if (gameState != STATE_GAME_PAUSE) arduboy.clear();
    else {
        if (!arduboy.paused) { pauseSelection=0; pauseSaveFailed=false; }
        arduboy.beginPause();
    }
    const uint8_t drawnState = gameState;
    mainGameLoop[drawnState](); // native-width function pointer, never AVR pgm_read_word
#if CHGAME_DEBUG
    const uint32_t perfGame = micros();
#endif
    gfx_wait();
    // Flash programming masks interrupts. Let the short tone finish first
    // so a checkpoint/preferences write cannot stretch or drop half-waves.
    if (pauseSaveRequested && !mystic::tones::playing()) {
        pauseSaveRequested=false;
        pauseSaveFailed=!mystic::storeProgress();
        if (!pauseSaveFailed) {
            soundSaveRequested=false;
            gameState=STATE_MENU_MAIN;
            menuSelection=STATE_MENU_PLAY;
            cont=0;
        }
    } else if (!pauseSaveRequested && !mystic::tones::playing() &&
        ((gameState == STATE_GAME_NEXT_LEVEL && canPressButton) ||
        gameState == STATE_GAME_OVER ||
        (soundSaveRequested && drawnState!=STATE_MENU_SOUNDFX) ||
        gameState == STATE_MENU_MAIN))
        if (mystic::storeProgress()) soundSaveRequested=false;
    dbg::markRenderStart();
    mystic::applyPalette();
#if CHGAME_DEBUG
    const uint32_t perfWait = micros();
#endif
    const bool fullScreen=drawnState==STATE_GAME_PLAYING || drawnState==STATE_GAME_PAUSE;
    const bool pauseOverlay=drawnState==STATE_GAME_PAUSE && arduboy.world;
    if (!pauseOverlay) arduboy.renderBase(fullScreen);
#if CHGAME_DEBUG
    const uint32_t perfExpand = micros();
#endif
    if (pauseOverlay) arduboy.renderPause();
    else arduboy.renderColors(fullScreen);
#if CHGAME_DEBUG
    const uint32_t perfChrome = micros();
#endif
    dbg::markRenderEnd();
    gfx_flushAsync();
#if CHGAME_DEBUG
    mystic::framePerf.record(perfStart, perfGame, perfWait, perfExpand, perfChrome, micros());
#endif
    if (resume && gameState==STATE_GAME_PAUSE) gameState = STATE_GAME_PLAYING;
}
