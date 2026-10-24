// Export the actual game renderer for palette/complete-animation review.
#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include "../../MysticBalloon.ino"
#define main unused_chsim_main
#include <main.cpp>
#undef main

using namespace mystic;
struct Asset { const char *name; const uint8_t *data; unsigned bytes; PortSprites::Mode mode; Role role; };
#define ART(name,mode) {#name,name,sizeof name,PortSprites::mode,Skip}
const Asset assets[]={
    ART(T_arg,Self),ART(titleScreen,Self),ART(mainMenu,Overwrite),ART(soundMenu,Overwrite),
    ART(continueMenu,Overwrite),ART(selector_plus_mask,PlusMask),ART(stars,Self),
    ART(leftGuyLeftEye,Self),ART(leftGuyRightEye,Self),ART(rightGuyEyes,Self),ART(madeBy,Self),
    ART(badgeMysticBalloon,Self),ART(badgeNextLevel,Self),ART(badgeGameOver,Self),
    ART(badgeLevel,Self),ART(badgePause,Overwrite),ART(badgePressKey,Overwrite),
    ART(badgeElements,Overwrite),ART(badgeHighScore,Self),ART(badgeBorder,Self),
    ART(badgeSuper,Self),ART(kidSprite,Self),ART(kidSpriteSuck_plus_mask,PlusMask),
    ART(particle,Erase),ART(balloon_plus_mask,PlusMask),
    {"balloonYellow",balloon_plus_mask,sizeof balloon_plus_mask,PortSprites::PlusMask,YellowBalloonInk},
    {"balloonBlue",balloon_plus_mask,sizeof balloon_plus_mask,PortSprites::PlusMask,BlueBalloonInk},
    ART(walkerSprite,Overwrite),ART(fan,Overwrite),ART(sprSpikes,Overwrite),
    {"tileSetTwo",tileSetTwo,sizeof tileSetTwo,PortSprites::Overwrite,TerrainInk},
    ART(elementsHUD,Overwrite),ART(smallMask,Self),ART(numbersBig,Self),
    ART(numbersBigMask,Self),ART(numbersBigMask01,Self),ART(door,Overwrite),ART(elements,Overwrite),
    {"menuColor",art::label0,sizeof art::label0,PortSprites::Self,Text},
    {"menuOriginal",art::label1,sizeof art::label1,PortSprites::Self,Text},
    {"menuDMG",art::label2,sizeof art::label2,PortSprites::Self,Text},
    {"menuAmber",art::label3,sizeof art::label3,PortSprites::Self,Text},
    {"menuSoundOn",art::label4,sizeof art::label4,PortSprites::Self,Text},
    {"menuSoundOff",art::label5,sizeof art::label5,PortSprites::Self,Text},
    {"menuCursor",art::cursor,sizeof art::cursor,PortSprites::Self,Text},
    {"menuSelected",art::selected,sizeof art::selected,PortSprites::Self,Text},
    {"pauseResume",art::label6,sizeof art::label6,PortSprites::Self,Text},
    {"pauseSaveQuit",art::label7,sizeof art::label7,PortSprites::Self,Text},
    {"pauseRestartHint",art::label8,sizeof art::label8,PortSprites::Self,Text},
    {"pauseSaveFailed",art::label9,sizeof art::label9,PortSprites::Self,Text},
    {"pausePanel",art::pausePanel,sizeof art::pausePanel,PortSprites::Overwrite,Text},
};
#undef ART

static void dump(const char *directory,const char *name,unsigned frame,int w,int h,int cx=0,int cy=0) {
    char file[512];
    snprintf(file,sizeof file,"%s/%s_%u.bin",directory,name,frame);
    FILE *f=fopen(file,"wb");assert(f);
    assert(fwrite(gfx_fb,1,8192,f)==8192);fclose(f);
    printf("%s|%u|%d|%d|%d|%d\n",name,frame,w,h,cx,cy);
}

static bool air(int x,int y) {
    for (int ty=(y-14)/16; ty<=(y+15)/16; ++ty)
        for (int tx=(x-4)/16; tx<=(x+13)/16; ++tx)
            if (gridGetSolid(tx,ty)) return false;
    return true;
}

int main(int argc,char **argv) {
    assert(argc==2);setup();setPalette(Color);setColorVariant(0);
    for (const Asset &a:assets) {
        const unsigned w=a.data[0],h=a.data[1],stride=a.mode==PortSprites::PlusMask ? 2 : 1;
        const unsigned frames=(a.bytes-2)/(w*((h+7)/8)*stride);
        for (unsigned frame=0;frame<frames;++frame) {
            memset(gfx_fb,0,8192);
            if (a.data==kidSprite && frame<12) {
                paintSprite(kidSprite,0,0,12+frame/6,(KidMask<<2)|PortSprites::Self,false);
                paintSprite(kidSprite,0,0,frame,(KidInk<<2)|PortSprites::Erase,false);
            } else {
                const Role role=a.data==kidSprite ? Text : a.role==Skip ? spriteRole(a.data,frame,a.mode) : a.role;
                paintSprite(a.data,0,0,frame,(role<<2)|a.mode,false);
            }
            dump(argv[1],a.name,frame,w,h);
        }
    }
    const int levelsToShow[]={1,6,16,39};
    const vec2 targets[]={vec2(112,304),vec2(176,256),vec2(176,224),vec2(176,160)};
    for (int i=0;i<4;++i) {
        stateMenuPlayNew();level=levelsToShow[i]-1;setKid();enemiesInit();levelLoad(levels[level]);
        kid.pos=targets[i];
        bool found=false;
        for (int radius=0;radius<=160 && !found;radius+=8)
            for (int y=(std::max)(32,targets[i].y-radius);y<=(std::min)(344,targets[i].y+radius) && !found;y+=8)
                for (int x=(std::max)(8,targets[i].x-radius);x<=(std::min)(360,targets[i].x+radius);x+=8)
                    if (air(x,y)) {kid.pos=vec2(x,y);found=true;break;}
        assert(found);
        kid.actualpos=kid.pos<<FIXED_POINT;kid.isBalloon=true;kid.isActive=true;kid.balloons=3;
        kid.direction=0;kid.balloonOffset=0;resetCamera();coinFrame=1;fanFrame=1;
        arduboy.clear();drawGrid();enemiesUpdate();drawKid();drawHUD();arduboy.render(true);
        assert(!arduboy.dropped);
        dump(argv[1],"level",levelsToShow[i],128,128);
    }
    arduboy.clear();gameState=STATE_MENU_MAIN;stateMenuMain();arduboy.render(false);
    dump(argv[1],"splash",0,128,128);
    paintSplash(false);dump(argv[1],"splashArt",0,128,128);
    gfx_wait();return 0;
}
