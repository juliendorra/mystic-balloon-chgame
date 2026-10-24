// Flat white Arduboy sky makes clearing pixels distinguishable from body tint.
#include <assert.h>
#include <stdio.h>
#include "../../MysticBalloon.ino"
#define main unused_chsim_main
#include <main.cpp>
#undef main
using namespace mystic;
static void dumpFrame(const char *dir,unsigned p,const char*name,unsigned frame) {
 char path[512];snprintf(path,sizeof path,"%s/%u_%s_%u.bin",dir,p,name,frame);
 FILE*f=fopen(path,"wb");assert(f);fwrite(gfx_fb,1,8192,f);fclose(f);
}
int main(int argc,char**argv) {
 assert(argc==2);setup();
 for(unsigned p=0;p<4;++p) {
  setPalette(Palette(p));
  for(unsigned frame=0;frame<12;++frame) {
   arduboy.clear();memset(arduboy.buffer,255,sizeof arduboy.buffer);arduboy.world=true;
   PortSprites::draw(48,48,kidSprite,12+frame/6,PortSprites::Self);
   PortSprites::draw(48,48,kidSprite,frame,PortSprites::Erase);
   arduboy.render(true);dumpFrame(argv[1],p,"kid",frame);
  }
  for(unsigned frame=0;frame<4;++frame) {
   arduboy.clear();memset(arduboy.buffer,255,sizeof arduboy.buffer);arduboy.world=true;
   PortSprites::draw(48,48,kidSpriteSuck_plus_mask,frame,PortSprites::PlusMask);
   arduboy.render(true);dumpFrame(argv[1],p,"inhale",frame);
   arduboy.clear();memset(arduboy.buffer,255,sizeof arduboy.buffer);arduboy.world=true;
   PortSprites::draw(48,48,walkerSprite,frame,PortSprites::Overwrite);
   arduboy.render(true);dumpFrame(argv[1],p,"walker",frame);
  }
  arduboy.clear();memset(arduboy.buffer,255,sizeof arduboy.buffer);arduboy.world=true;
  PortSprites::draw(48,48,balloon_plus_mask,0,PortSprites::PlusMask);
  arduboy.render(true);dumpFrame(argv[1],p,"balloon",0);
  arduboy.clear();memset(arduboy.buffer,255,sizeof arduboy.buffer);arduboy.world=true;
  PortSprites::draw(48,48,particle,0,PortSprites::Erase);
  arduboy.render(true);dumpFrame(argv[1],p,"particle",0);
 }
}
