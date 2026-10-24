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

#if CHGAME_DEBUG
bool debugCommand(char cmd, const char *args) {
    if (cmd == 'U') { mystic::framePerf.report(); return true; }
    if (cmd == 'V') {
        char line[320];
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
            "MYBL state=%u level=%u x=%d y=%d sy=%d balloons=%u coins=%u total=%u key=%u score=%u spikes=%u maxheight=%u fans=%u walkers=%u saved=%u sound=%u camx=%d camy=%d view=%u\n",
            gameState, level+1, kid.pos.x, kid.pos.y, kid.speed.y,
            kid.balloons, coinsActive, totalCoins, key.haveKey, (unsigned)scorePlayer,
            ns, maxHeight, nf, nw, progress.level+1, audio::on(), cam.pos.x, cam.pos.y, mystic::VIEW_H);
        dbg::print(line);
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
    gfx_setPaletteEntry(0, gfx_rgb(17, 17, 34));
    gfx_setPaletteEntry(1, gfx_rgb(221, 238, 255));
    chgame.setFrameRate(CHMB_FPS);
    mystic::loadProgress();
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
    if (chgame.justPressed(SELECT_BUTTON)) mystic::setSound(!audio::on());
    // Resume on the following frame so START never advances a paused world.
    const bool start = chgame.justPressed(START_BUTTON);
    const bool resume = start && gameState == STATE_GAME_PAUSE;
    if (start && gameState == STATE_GAME_PLAYING) gameState = STATE_GAME_PAUSE;
    if (gameState < STATE_GAME_NEXT_LEVEL && chgame.everyXFrames(10))
        sparkleFrames = (sparkleFrames + 1) % 5;
    if (gameState != STATE_GAME_PAUSE) arduboy.clear();
    const uint8_t drawnState = gameState;
    mainGameLoop[drawnState](); // native-width function pointer, never AVR pgm_read_word
    audio::update();
#if CHGAME_DEBUG
    const uint32_t perfGame = micros();
#endif
    gfx_wait();
    if ((gameState == STATE_GAME_NEXT_LEVEL && canPressButton) ||
        gameState == STATE_GAME_OVER || chgame.justPressed(SELECT_BUTTON) ||
        gameState == STATE_MENU_MAIN)
        mystic::storeProgress();
    dbg::markRenderStart();
#if CHGAME_DEBUG
    const uint32_t perfWait = micros();
#endif
    arduboy.render(drawnState == STATE_GAME_PLAYING || drawnState == STATE_GAME_PAUSE);
#if CHGAME_DEBUG
    const uint32_t perfExpand = micros();
#endif
#if CHGAME_DEBUG
    const uint32_t perfChrome = perfExpand;
#endif
    dbg::markRenderEnd();
    gfx_flushAsync();
#if CHGAME_DEBUG
    mystic::framePerf.record(perfStart, perfGame, perfWait, perfExpand, perfChrome, micros());
#endif
    if (resume) gameState = STATE_GAME_PLAYING;
}
