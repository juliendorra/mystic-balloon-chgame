// Deterministic edge/fan fixtures through the actual frame loop and physics.
#include <assert.h>
#include <stdio.h>
#include "../../MysticBalloon.ino"
#define main unused_chsim_main
#include <main.cpp>
#undef main

static void tick() {
    chgame.injected=B_BUTTON;chgame.lockstep=1;loop();gfx_wait();
}

static void prepare(unsigned scenario) {
    chgame.injected=0;chgame.pollButtons();
    assert(debugCommand('G',"1"));
    const char *poses[]={"0 16 0","372 16 0","112 32 96"};
    assert(debugCommand('J',poses[scenario]));
}

static void dump(const char *directory,unsigned p,const char *name,unsigned frame) {
    char file[512];snprintf(file,sizeof file,"%s/%u_%s.bin",directory,p,name);
    FILE *f=fopen(file,"wb");assert(f);
    assert(fwrite(gfx_fb,1,8192,f)==8192);fclose(f);
    printf("%u,%s,%u,%d,%d,%d,%d\n",p,name,frame,kid.pos.x,kid.pos.y,cam.pos.x,cam.pos.y);
}

int main(int argc,char **argv) {
    assert(argc==2);setup();
    for (unsigned p=0;p<4;++p) {
        mystic::setPalette(mystic::Palette(p));
        for (unsigned scenario=0;scenario<3;++scenario) {
            prepare(scenario);
            for (unsigned frame=1;frame<=(scenario==2 ? 150u : 1u);++frame) {
                tick();
                if (scenario<2 || frame==61)
                    dump(argv[1],p,scenario==0 ? "left" : scenario==1 ? "right" : "ascent",frame);
            }
        }
    }
    return 0;
}
