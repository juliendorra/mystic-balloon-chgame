// Render the actual title/menu code for a 128x128 artwork and legibility review.
#include <assert.h>
#include <stdio.h>
#include "../../MysticBalloon.ino"
#define main unused_chsim_main
#include <main.cpp>
#undef main

static void dump(const char *directory,unsigned palette,const char *name,unsigned frame) {
    char file[512];
    snprintf(file,sizeof file,"%s/%u_%s_%u.bin",directory,palette,name,frame);
    FILE *f=fopen(file,"wb");assert(f);
    assert(fwrite(gfx_fb,1,GFX_FB_BYTES,f)==GFX_FB_BYTES);fclose(f);
}

int main(int argc,char **argv) {
    assert(argc==2);setup();
    for (unsigned palette=0;palette<4;++palette) {
        mystic::setPalette(mystic::Palette(palette));mystic::applyPalette();
        mystic::paintSplash(palette==mystic::Original);dump(argv[1],palette,"art",0);
        paintHelpMenu();dump(argv[1],palette,"help",0);
        for (unsigned selection=0;selection<4;++selection) {
            arduboy.clear();gameState=STATE_MENU_MAIN;menuSelection=selection+2;
            stateMenuMain();arduboy.render(false);dump(argv[1],palette,"main",selection);
        }
        for (unsigned selection=0;selection<6;++selection) {
            arduboy.clear();gameState=STATE_MENU_SOUNDFX;settingsSelection=selection;
            stateMenuSoundfx();arduboy.render(false);dump(argv[1],palette,"config",selection);
        }
        mystic::saves::Store saves;saves.prefs=progress;saves.count=3;saves.active=0;
        saves.runs[0].level=15;saves.runs[0].score=16090;saves.runs[0].total=57;
        saves.runs[1].objects=mystic::saves::MID_LEVEL|31;
        saves.runs[2].level=22;saves.runs[2].objects=mystic::saves::MID_LEVEL|mystic::saves::HAS_KEY|4;
        assert(mystic::saves::write(saves));mystic::saves::status=mystic::saves::Sd;
        for (unsigned selection=0;selection<4;++selection) {
            arduboy.clear();gameState=STATE_MENU_PLAY;cont=selection;
            stateMenuPlaySelect();arduboy.render(false);paintSaveMenu();dump(argv[1],palette,"play",selection);
        }
        stateMenuPlayNew();level=5;setKid();enemiesInit();levelLoad(levels[level]);
        kid.pos=kid.actualpos>>FIXED_POINT;resetCamera();
        arduboy.clear();drawGrid();enemiesUpdate();drawKid();drawHUD();
        gameState=STATE_GAME_PAUSE;
        for (unsigned selection=0;selection<3;++selection) {
            arduboy.beginPause();pauseSelection=selection;
            stateGamePause();arduboy.render(true);dump(argv[1],palette,"pause",selection);
        }
        arduboy.clear();gameState=STATE_GAME_NEXT_LEVEL;chgame.frameCount=1;
        canPressButton=pressKeyIsVisible=true;coinsCollected=balloonsLeft=0;
        stateGameNextLevel();arduboy.render(false);dump(argv[1],palette,"next",0);
        arduboy.clear();gameState=STATE_GAME_OVER;
        stateGameOver();arduboy.render(false);dump(argv[1],palette,"over",0);
    }
    gfx_wait();return 0;
}
