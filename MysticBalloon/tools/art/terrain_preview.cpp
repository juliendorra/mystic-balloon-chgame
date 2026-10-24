// Production world pipeline: isolated tile, horizontal join and 2x2 platform.
#include <assert.h>
#include <stdio.h>
#include "../../MysticBalloon.ino"
#define main unused_chsim_main
#include <main.cpp>
#undef main

int main(int argc,char **argv) {
    assert(argc==2);setup();
    const unsigned cells[][3]={{2,2,0},{2,5,1},{3,5,5},{4,5,4},
                              {6,2,9},{7,2,12},{6,3,3},{7,3,6}};
    for (unsigned p=0;p<4;++p) for (unsigned corrected=0;corrected<2;++corrected) {
        mystic::setPalette(mystic::Palette(p));arduboy.clear();arduboy.world=true;
        arduboy.worldX=arduboy.worldY=12;
        for (int x=0;x<=8;++x) for (int y=0;y<=8;++y)
            sprites.drawSelfMasked(x*16-3,y*16-3,tileSetTwo,16);
        for (const auto &c : cells) {
            arduboy.noteTerrain(c[0],c[1],c[2]);
            sprites.drawOverwrite(c[0]*16-12,c[1]*16-12,tileSetTwo,c[2]);
        }
        arduboy.terrainCoverage=corrected;
        arduboy.render(true);
        char file[512];snprintf(file,sizeof file,"%s/%u_%u.bin",argv[1],p,corrected);
        FILE *f=fopen(file,"wb");assert(f);fwrite(gfx_fb,1,8192,f);fclose(f);
    }
    gfx_wait();return 0;
}
