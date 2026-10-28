#pragma once
#include "Saves.h"
extern byte toneindex;
extern byte pauseSelection;
extern bool pauseSaveFailed;

uint8_t runSaveKind=0,frozenPhase=0;

mystic::saves::Run captureRun(bool mid) {
  using namespace mystic::saves;
  Run r;
  r.level=min(unsigned(level),38u);r.score=scorePlayer;r.total=totalCoins;
  r.objects=r.level==22 ? 60 : 63;
  if (level>=TOTAL_LEVELS) { r.kidFlags=512;r.objects=0;return r; }
  if (!mid) return r;
  r.objects=MID_LEVEL | (key.haveKey ? HAS_KEY : 0);
  r.x=kid.actualpos.x;r.y=kid.actualpos.y;r.vx=kid.speed.x;r.vy=kid.speed.y;
  r.kidFlags=kid.isActive | (kid.isImune<<1) | (kid.direction<<2) |
      (kid.isWalking<<3) | (kid.isJumping<<4) | (kid.isLanding<<5) |
      (kid.isBalloon<<6) | (kid.jumpLetGo<<7) | (kid.isSucking<<8);
  if (!kid.balloons) r.kidFlags|=1024;
  r.immune=kid.imuneTimer;r.jump=kid.jumpTimer;
  r.balloons=kid.balloons | (kid.balloonOffset<<2);
  r.phase=gameState==STATE_GAME_PAUSE ? frozenPhase : chgame.frameCount%40;
  for (unsigned i=0;i<6;++i) {
    r.coinX[i]=coins[i].pos.x;r.walkerX[i]=walkers[i].pos.x;r.hp[i]=walkers[i].HP;
    if (coins[i].active) r.objects|=1u<<i;
    if (walkers[i].active) r.walkers|=1u<<i;
    if (walkers[i].direction>0) r.walkers|=1u<<(i+6);
    if (walkers[i].hurt) r.hurt|=1u<<i;
  }
  return r;
}

void restoreRun(const mystic::saves::Run &r) {
  using namespace mystic::saves;
  level=r.level;scorePlayer=r.score;totalCoins=r.total;
  coinsCollected=balloonsLeft=globalCounter=0;toneindex=0;
  canPressButton=false;scoreIsVisible=false;nextLevelIsVisible=true;pressKeyIsVisible=false;
  if (r.kidFlags&1536) {
    if (r.kidFlags&512) level=TOTAL_LEVELS;
    gameState=STATE_GAME_OVER;return;
  }
  if (!(r.objects&MID_LEVEL)) { kid.balloons=3;gameState=STATE_GAME_NEXT_LEVEL;return; }
  setKid();enemiesInit();levelLoad(levels[level]);
  kid.actualpos=vec2(r.x,r.y);kid.pos=kid.actualpos>>FIXED_POINT;kid.speed=vec2(r.vx,r.vy);
  kid.isActive=r.kidFlags&1;kid.isImune=r.kidFlags&2;kid.direction=r.kidFlags&4;
  kid.isWalking=r.kidFlags&8;kid.isJumping=r.kidFlags&16;kid.isLanding=r.kidFlags&32;
  kid.isBalloon=r.kidFlags&64;kid.jumpLetGo=r.kidFlags&128;kid.isSucking=r.kidFlags&256;
  kid.imuneTimer=r.immune;kid.jumpTimer=r.jump;kid.balloons=r.balloons&3;kid.balloonOffset=r.balloons>>2;
  coinsActive=0;
  for (unsigned i=0;i<6;++i) {
    coins[i].active=r.objects&(1u<<i);coins[i].pos.x=r.coinX[i];coinsActive+=coins[i].active;
    walkers[i].active=r.walkers&(1u<<i);walkers[i].direction=r.walkers&(1u<<(i+6)) ? 1 : -1;
    walkers[i].pos.x=r.walkerX[i];walkers[i].HP=r.hp[i];walkers[i].hurt=r.hurt&(1u<<i);
  }
  coinsCollected=(level==22 ? 4 : 6)-coinsActive;
  key.haveKey=r.objects&HAS_KEY;key.active=!key.haveKey;
  frozenPhase=r.phase;chgame.frameCount=chgame.frameCount/40*40+r.phase;
  resetCamera();arduboy.clear();drawGrid();enemiesUpdate(false);drawKid();drawHUD();
  // Enter pause here, so the next update keeps the restored animation phase
  // instead of treating this as a new pause and immediately saving it again.
  arduboy.beginPause();pauseSelection=0;pauseSaveFailed=false;
  gameState=STATE_GAME_PAUSE;
}
