// Exercise the real game and CHGame implementation on the deterministic host.
#include <assert.h>
#include <initializer_list>
#include "../../MysticBalloon.ino"
#define main unused_chsim_main
#include <main.cpp>
#undef main

static void tick(uint8_t buttons = 0) {
    chgame.injected = buttons;
    chgame.lockstep = 1;
    loop();
    gfx_wait();
}

static void startLevel(unsigned n) {
    stateMenuPlayNew();
    level = n;
    setKid();
    enemiesInit();
    levelLoad(levels[level]);
    kid.pos = kid.actualpos >> FIXED_POINT;
    resetCamera();
    gameState = STATE_GAME_PLAYING;
}

static void spritesTest() {
    using S = mystic::PortSprites;
    const int positions[][2] = {
        {0,0},{1,1},{2,2},{3,3},{4,4},{5,5},{6,6},{7,7},
        {-3,-7},{126,62},{-61,1},{2,-64},{128,0},{10,17},{123,53},
        {126,126},{10,127},{0,128},{2,-128}
    };
    unsigned cases=0;
    for (int w : {1,5,16,32,60}) for (int h : {1,7,8,11,16,56,64,127,128}) {
      uint8_t plain[2+2*60*16]={}, masked[2+2*60*16*2]={};
      plain[0]=masked[0]=w;
      plain[1]=masked[1]=h;
      const int frameBytes=w*((h+7)/8);
      for (int i=0; i<frameBytes*2; ++i) {
        plain[2+i] = uint8_t(i*71+37);
        masked[2+i*2] = plain[2+i];
        masked[3+i*2] = uint8_t(i*43+99);
      }
      for (int mode=0; mode<4; ++mode) for (int frame=0; frame<2; ++frame)
        for (const auto &p : positions) {
            uint8_t expected[sizeof arduboy.buffer];
            for (unsigned i=0; i<sizeof arduboy.buffer; ++i) expected[i] = arduboy.buffer[i] = uint8_t(i*31+29);
            for (int y=0; y<mystic::VIEW_H; ++y) for (int x=0; x<mystic::VIEW_W; ++x) {
                const int sx=x-p[0], sy=y-p[1];
                if (sx<0 || sx>=w || sy<0 || sy>=h) continue;
                const int at=frame*frameBytes+(sy/8)*w+sx;
                const bool ink=(plain[2+at] >> (sy%8)) & 1;
                const bool mask=(masked[3+at*2] >> (sy%8)) & 1;
                const int dst=(y/8)*128+x, bit=1 << (y%8);
                bool v=(expected[dst] & bit) != 0;
                if (mode==S::Overwrite) v=ink;
                else if (mode==S::Self) v=v || ink;
                else if (mode==S::Erase) v=v && !ink;
                else v=(v && !mask) || ink;
                if (v) expected[dst] |= bit;
                else expected[dst] &= ~bit;
            }
            S::draw(p[0],p[1], mode==S::PlusMask ? masked : plain, frame, S::Mode(mode));
            assert(!memcmp(expected,arduboy.buffer,sizeof expected));
            ++cases;
        }
    }
    printf("PASS: %u pixel-reference sprite cases (four modes, every shift, clipping, partial pages, frames)\n",cases);
    for (unsigned i=0; i<sizeof arduboy.buffer; ++i) arduboy.buffer[i]=uint8_t(i*47+13);
    gfx_wait();
    for (bool full : {true,false}) {
      gfx_clear(7);
      arduboy.render(full);
      const int top=full ? 0 : mystic::MENU_Y, height=full ? mystic::VIEW_H : mystic::MENU_H;
      for (int y=0; y<128; ++y) for (int x=0; x<128; ++x) {
        const unsigned pixel=(gfx_fb[y*64+x/2] >> ((x&1)*4)) & 15;
        const unsigned expected=y<top || y>=top+height ? 0 : (arduboy.buffer[((y-top)/8)*128+x] >> ((y-top)%8))&1;
        assert(pixel==expected);
      }
    }
    puts("PASS: all 16,384 pixels match the 128x128 playfield and centered original menus");
}

static unsigned bitmapPixel(const uint8_t *sprite, unsigned frame, unsigned x, unsigned y) {
    const unsigned w=sprite[0], frameBytes=w*((sprite[1]+7)/8);
    return (sprite[2+frame*frameBytes+(y/8)*w+x] >> (y%8))&1;
}

static void viewportTest() {
    const int cameras[][2]={{0,0},{256,256},{0,256},{256,0},{23,71},{255,255}};
    for (unsigned n : {0u,12u,38u}) {
      startLevel(n);
      for (const auto &c : cameras) {
        cam.pos=vec2(c[0],c[1]);
        arduboy.clear();
        drawGrid();
        for (int y=0; y<128; ++y) for (int x=0; x<128; ++x) {
          const int wx=x+cam.pos.x, wy=y+cam.pos.y;
          const unsigned tile=gridGetTile(wx/16,wy/16);
          unsigned expected=tile==16 ? bitmapPixel(tileSetTwo,16,(x+(cam.pos.x>>2))%16,(y+(cam.pos.y>>2))%16)
                                    : bitmapPixel(tileSetTwo,tile,wx%16,wy%16);
          if (wx>=levelExit.x && wx<levelExit.x+16 && wy>=levelExit.y && wy<levelExit.y+16)
            expected=bitmapPixel(door,key.haveKey,wx-levelExit.x,wy-levelExit.y);
          const unsigned got=(arduboy.buffer[(y/8)*128+x] >> (y%8))&1;
          assert(got==expected);
        }
      }
    }
    for (const auto &p : cameras) {
      kid.pos=vec2(p[0],p[1]);
      resetCamera();
      assert(cam.pos.x>=0 && cam.pos.x<=256 && cam.pos.y>=0 && cam.pos.y<=256);
    }
    kid.pos=vec2(192,192);
    resetCamera();
    assert(cam.pos.x==134 && cam.pos.y==136);
    kid.pos=vec2(0,0); resetCamera(); assert(cam.pos.x==0 && cam.pos.y==0);
    kid.pos=vec2(383,383); resetCamera(); assert(cam.pos.x==256 && cam.pos.y==256);
    puts("PASS: full viewport tile coverage at 18 camera positions, parallax, doors, camera centering and bounds");
}

static void levelsTest() {
    unsigned directions=0;
    for (unsigned n=0; n<TOTAL_LEVELS; ++n) {
        startLevel(n);
        assert(coinsActive>0 && coinsActive<=6 && key.active && !key.haveKey);
        assert(kid.pos.x>=0 && kid.pos.x<384 && kid.pos.y>=0 && kid.pos.y<384);
        // Decode the original record stream independently of levelLoad.
        const uint8_t *p=levels[n]+72;
        unsigned count[7]={};
        while (*p!=255) {
            const int type=*p >> 5, y=*p++ & 31, x=*p++ & 31;
            assert(type<7 && x<24 && y<24);
            ++count[type];
            if (type==0) assert(startPos.x==x*512 && startPos.y==y*512);
            if (type==1) assert(levelExit.x==x*16 && levelExit.y==y*16);
            if (type==3) ++p;
            if (type==6) assert(key.pos.x==x*16 && key.pos.y==y*16);
        }
        unsigned ns=0,nf=0,nw=0;
        for (unsigned i=0; i<MAX_PER_TYPE; ++i) {
            ns += (spikes[i].characteristics >> 2) & 1;
            nf += fans[i].active;
            nw += walkers[i].active;
            if ((spikes[i].characteristics >> 2) & 1) {
                assert(spikes[i].pos.width>0 && spikes[i].pos.height>0);
            }
            if (fans[i].active) directions |= 1 << fans[i].dir;
        }
        assert(count[0]==1 && count[1]==1 && count[5]==coinsActive && count[6]==1);
        assert(ns==count[4] && nf==count[3] && nw==count[2]);
        // Update and render each complete level, not just its record parser.
        for (unsigned f=0; f<120; ++f) {
            tick((f<60 ? RIGHT_BUTTON : LEFT_BUTTON) | (f%40<20 ? B_BUTTON : 0));
            assert(cam.pos.x>=0 && cam.pos.x<=256 && cam.pos.y>=0 && cam.pos.y<=256);
        }
        for (int y=0; y<128; ++y) for (int x=0; x<128; ++x) {
            const unsigned pixel=(gfx_fb[y*64+x/2] >> ((x&1)*4))&15;
            assert(pixel==((arduboy.buffer[(y/8)*128+x] >> (y%8))&1));
        }
    }
    assert(directions==7);
    HighRect tall={10,0,8,384}, nearBottom={11,350,2,12};
    assert(collide(tall,nearBottom));
    puts("PASS: all 39 levels, 4,680 simulated frames, all fan directions, 384-pixel collision rectangles");
}

static void pauseTest() {
    startLevel(0);
    tick(RIGHT_BUTTON);
    tick(START_BUTTON);
    assert(gameState==STATE_GAME_PAUSE);
    const Players before=kid;
    const Camera beforeCam=cam;
    uint8_t image[8192];
    memcpy(image,gfx_fb,sizeof image);
    for (int i=0; i<120; ++i) tick(RIGHT_BUTTON);
    assert(!memcmp(&before,&kid,sizeof kid) && !memcmp(&beforeCam,&cam,sizeof cam));
    assert(!memcmp(image,gfx_fb,sizeof image));
    tick(START_BUTTON);
    assert(gameState==STATE_GAME_PLAYING);
    tick();
    tick(SELECT_BUTTON);
    assert(!audio::on());
    tick();
    tick(SELECT_BUTTON);
    assert(audio::on());
    tick();
    puts("PASS: START freezes physics, camera and pixels; resume and mute work");
}

static void progressionTest() {
    startLevel(0);
    kid.isSucking=false;
    for (unsigned i=0; i<6; ++i) { kid.pos=coins[i].pos; checkCollisions(); }
    assert(coinsActive==0 && coinsCollected==6 && totalCoins==6 && scorePlayer==1500);
    kid.pos=key.pos;
    checkCollisions();
    assert(key.haveKey && !key.active);
    kid.pos=levelExit;
    chgame.injected=UP_BUTTON;
    chgame.pollButtons();
    checkCollisions();
    assert(gameState==STATE_GAME_NEXT_LEVEL && level==1);
    chgame.injected=0;
    chgame.pollButtons();
    for (unsigned f=0; f<11; ++f) { chgame.frameCount=f*20; stateGameNextLevel(); }
    assert(canPressButton && scorePlayer==1710 && progress.level==1 && progress.score==1710);
    mystic::storeProgress();
    mystic::Progress loaded;
    assert(save::load(mystic::SAVE_MAGIC,1,loaded) && loaded.level==1 && loaded.score==1710);
    const void *record=save::read(mystic::SAVE_MAGIC,1,sizeof loaded);
    mystic::storeProgress();
    assert(record==save::read(mystic::SAVE_MAGIC,1,sizeof loaded));
    progress=mystic::Progress{};
    mystic::loadProgress();
    stateMenuPlayContinue();
    assert(level==1 && totalCoins==6 && scorePlayer==1710);
    gameState=STATE_GAME_OVER;
    stateGameOver();
    mystic::storeProgress();
    assert(progress.highScore==1710 && progress.highCoins==6);
    stateMenuPlayNew();
    assert(level==0 && scorePlayer==0 && totalCoins==0 && progress.highScore==1710);
    // Finishing the last level retains the result, resets the next campaign,
    // and never indexes levels[39].
    startLevel(38);
    key.haveKey=true;
    kid.pos=levelExit;
    totalCoins=TOTAL_COINS;
    coinsCollected=6;
    scorePlayer=2000;
    chgame.injected=UP_BUTTON;
    chgame.pollButtons();
    checkCollisions();
    chgame.injected=0;
    chgame.pollButtons();
    assert(level==39);
    for (unsigned f=0; f<11; ++f) { chgame.frameCount=f*20; stateGameNextLevel(); }
    assert(gameState==STATE_GAME_OVER && scorePlayer==2210);
    stateGameOver();
    mystic::storeProgress();
    assert(progress.level==0 && progress.score==0 && progress.coins==0);
    assert(progress.highScore==2210 && progress.highCoins==TOTAL_COINS);
    puts("PASS: coins, key, exit, bonuses, checkpoint reload, continue, high score, game completion");
}

static void hazardsTest() {
    startLevel(1);
    unsigned i=0;
    while (i<MAX_PER_TYPE && !walkers[i].active) ++i;
    assert(i<MAX_PER_TYPE);
    kid.balloons=2;
    kid.pos=vec2(walkers[i].pos.x-16,walkers[i].pos.y);
    kid.direction=FACING_RIGHT;
    kid.isSucking=true;
    for (int f=0; f<60; ++f) checkCollisions();
    assert(!walkers[i].active && kid.balloons==3);
    for (unsigned dir=0; dir<3; ++dir) {
        enemiesInit();
        fansCreate(vec2(10,10),20,dir);
        kid.isBalloon=true;
        kid.speed=vec2(0,0);
        kid.pos=dir==FAN_UP ? vec2(160,128) : dir==FAN_RIGHT ? vec2(192,160) : vec2(128,160);
        checkCollisions();
        if (dir==FAN_UP) assert(kid.speed.y==FAN_POWER);
        if (dir==FAN_RIGHT) assert(kid.speed.x==FAN_POWER);
        if (dir==FAN_LEFT) assert(kid.speed.x==-FAN_POWER);
    }
    kid.balloons=3;
    kidHurt(); kidHurt(); kidHurt();
    assert(kid.balloons==0);
    kid.isActive=true;
    kid.pos=vec2(cam.pos.x+10,cam.pos.y+mystic::VIEW_H+CAMERA_OFFSET*3+1);
    drawKid();
    assert(gameState==STATE_GAME_OVER);
    puts("PASS: inhaling replenishes a balloon, three fan forces, losing all lives");
}

int main() {
    setup();
    spritesTest();
    viewportTest();
    levelsTest();
    pauseTest();
    progressionTest();
    hazardsTest();
    gfx_wait();
    return 0;
}
