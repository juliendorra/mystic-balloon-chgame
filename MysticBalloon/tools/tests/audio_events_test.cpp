// Compile the same fixtures against the pinned original headers and the port.
#include <assert.h>
#include <initializer_list>
#ifdef CHMB_AUDIO_REFERENCE
#define GAME_ID 34
#include "globals.h"
#include "menu.h"
#include "game.h"
#include "inputs.h"
#include "player.h"
#include "enemies.h"
#include "elements.h"
#include "levels.h"
#include "Port.h"
namespace mystic { Role spriteRole(const uint8_t *,uint8_t,uint8_t) { return Skip; } }
void setup() {}
void loop() {}
#else
#include "../../MysticBalloon.ino"
#endif
#define main unused_chsim_main
#include <main.cpp>
#undef main

static const char *scenario="initial";
void audioTrace(uint16_t hz,uint16_t duration) {
    printf("SND %s %u %u %u %u\n",scenario,unsigned(chgame.frameCount),hz,duration,arduboy.audio.enabled());
}
static void frame(unsigned n,uint8_t buttons=0) {
    chgame.frameCount=n;
    chgame.injected=0; chgame.pollButtons();
    chgame.injected=buttons; chgame.pollButtons();
    arduboy.clear();
}
static void reset(const char *name) {
    scenario=name; frame(0); randomSeed(1);
    level=0; scorePlayer=totalCoins=coinsCollected=balloonsLeft=0;
    globalCounter=toneindex=0; canPressButton=false;
    setKid(); enemiesInit(); key.active=key.haveKey=false;
    kid.pos=vec2(100,160); kid.actualpos=kid.pos<<FIXED_POINT;
    kid.isImune=false; kid.frame=0; levelExit=vec2(1000,1000);
    gameState=STATE_GAME_PLAYING;
}
static vec2 floorPosition() {
    for (int y=0;y<23;++y) for (int x=1;x<23;++x)
        if (!gridGetSolid(x,y) && gridGetSolid(x,y+1)) return vec2(x*16,y*16);
    assert(false); return vec2(0,0);
}
int main() {
    chgame.boot(); gfx_begin(GFX_DIV2,GFX_12BPP);
#ifndef CHMB_AUDIO_REFERENCE
    mystic::audio::begin(true,mystic::audio::Beeps); mystic::tones::trace=audioTrace;
#endif
    reset("intro-auto");
    for (unsigned f=1;f<=251;++f) { frame(f); stateMenuIntro(); }
    reset("intro-late-skip"); globalCounter=200; frame(201,A_BUTTON); stateMenuIntro();
    reset("main-menu"); menuSelection=3;
    frame(1,DOWN_BUTTON); stateMenuMain(); frame(2,UP_BUTTON); stateMenuMain();
    frame(3,B_BUTTON); stateMenuMain();
    reset("help-back"); frame(1,A_BUTTON); stateMenuHelp();
    reset("info-back"); frame(1,B_BUTTON); stateMenuInfo();
    reset("play-menu");
    frame(1,DOWN_BUTTON); stateMenuPlaySelect(); frame(2,UP_BUTTON); stateMenuPlaySelect();
    frame(3,B_BUTTON); stateMenuPlaySelect(); frame(4,A_BUTTON); stateMenuPlaySelect();
    reset("config-enable");
#ifdef CHMB_AUDIO_REFERENCE
    arduboy.audio.off(); frame(1,DOWN_BUTTON);
#else
    mystic::audio::setEnabled(false); settingsSelection=5;
    settingsOriginalPalette=mystic::Color; settingsOriginalOptions=9; frame(1,RIGHT_BUTTON);
#endif
    stateMenuSoundfx();
    reset("config-back"); frame(1,A_BUTTON); stateMenuSoundfx();
    reset("next-level-enter"); canPressButton=true; toneindex=10;
    frame(1,B_BUTTON); stateGameNextLevel();
    reset("same-frame-menu"); menuSelection=3;
    frame(1,UP_BUTTON|DOWN_BUTTON|B_BUTTON); stateMenuMain();
    reset("jump"); frame(1,B_BUTTON); checkInputs();
    reset("footsteps"); const vec2 floor=floorPosition();
    for (unsigned f=0;f<128;++f) {
        frame(f); kid.pos=floor; kid.actualpos=kid.pos<<FIXED_POINT;
        kid.isWalking=true; kid.speed=vec2(0,0); checkKid();
    }
    reset("landing"); kid.pos=floor; kid.actualpos=kid.pos<<FIXED_POINT;
    kid.isLanding=true; kid.speed.y=-1; frame(1); checkKid();
    reset("head-bump");
    bool found=false;
    for (int y=1;y<22 && !found;++y) for (int x=1;x<23 && !found;++x)
        if (gridGetSolid(x,y) && !gridGetSolid(x,y+1) && !gridGetSolid(x,y+2)) {
            kid.pos=vec2(x*16,(y+1)*16); kid.actualpos=kid.pos<<FIXED_POINT;
            kid.speed.y=40; frame(1); checkKid(); found=true;
        }
    assert(found);
    reset("inhale-wind");
    for (unsigned f=0;f<256;++f) { frame(f); kid.isSucking=true; checkKid(); }
    reset("hurt");
    for (unsigned f=1;f<=4;++f) { frame(f); kidHurt(); }
    reset("key"); key.active=true; key.pos=kid.pos; frame(1); checkCollisions();
    reset("six-coins-same-frame"); coinsActive=6;
    for (auto &coin:coins) { coin.active=true; coin.pos=kid.pos; }
    frame(1); checkCollisions();
    for (unsigned direction : {0u,1u}) {
        reset(direction ? "inhale-walker-left" : "inhale-walker-right");
        kid.isSucking=true; kid.direction=direction;
        walkers[0].active=true; walkers[0].HP=0;
        walkers[0].pos=vec2(direction ? 92 : 116,160);
        frame(1); checkCollisions(); assert(!walkers[0].active);
    }
    reset("walker-hurt"); walkers[0].active=true; walkers[0].pos=vec2(97,160);
    frame(1); checkCollisions();
    reset("spike-hurt"); spikes[0].characteristics=4;
    spikes[0].pos={100,160,8,8}; frame(1); checkCollisions();
    for (unsigned lives : {1u,3u}) {
        reset(lives==1 ? "fall-game-over" : "fall-life"); kid.balloons=lives;
        cam.pos=vec2(0,0);
#ifdef CHMB_AUDIO_REFERENCE
        const unsigned bottom=64+CAMERA_OFFSET*3;
#else
        const unsigned bottom=mystic::VIEW_H+CAMERA_OFFSET*3;
#endif
        kid.pos=vec2(100,bottom); frame(1); drawKid();
        kid.pos.y=bottom+1; frame(2); drawKid();
    }
    for (unsigned direction : {0u,1u,2u}) {
        reset(direction==0 ? "fan-up" : direction==1 ? "fan-right" : "fan-left");
        fans[0].active=true; fans[0].height=32; fans[0].dir=direction;
        fans[0].pos=direction==0 ? vec2(100,176) : direction==1 ? vec2(84,160) : vec2(116,160);
        kid.isBalloon=true;
        for (unsigned f=0;f<64;++f) { frame(f); checkCollisions(); }
    }
    reset("same-frame-overrides"); key.active=true; key.pos=kid.pos;
    coins[0].active=true; coins[0].pos=kid.pos; coinsActive=1;
    spikes[0].characteristics=4; spikes[0].pos={100,160,8,8};
    frame(2); windNoise(); checkCollisions();
    for (unsigned c=0;c<=6;++c) for (unsigned b=0;b<=3;++b) {
        char name[40]; snprintf(name,sizeof name,"bonus-%u-coins-%u-balloons",c,b);
        reset(name); level=1; coinsCollected=c; balloonsLeft=b;
        for (unsigned f=1;f<=220;++f) { frame(f); stateGameNextLevel(); }
    }
    reset("bonus-cross-wrap"); coinsCollected=6; balloonsLeft=3; level=1;
    for (unsigned f=65515;f<65740;++f) { frame(f); stateGameNextLevel(); }
    reset("pause-silent"); frame(1); stateGamePause(); frame(2,A_BUTTON); stateGamePause();
    reset("game-over-silent"); frame(1,A_BUTTON); stateGameOver();
    return 0;
}
