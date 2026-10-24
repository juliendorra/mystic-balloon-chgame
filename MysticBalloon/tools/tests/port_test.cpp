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

static void tap(uint8_t button) { tick(button); tick(); }

static void startLevel(unsigned n, uint32_t entryScore=0, uint8_t entryCoins=0) {
    stateMenuPlayNew();
    level = n;
    scorePlayer = entryScore;
    totalCoins = entryCoins;
    setKid();
    enemiesInit();
    levelLoad(levels[level]);
    kid.pos = kid.actualpos >> FIXED_POINT;
    resetCamera();
    gameState = STATE_GAME_PLAYING;
}

static void spritesTest() {
    mystic::setPalette(mystic::Original);
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
            arduboy.clear();
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
    const int cameras[][2]={{0,0},{256,256},{0,256},{256,0},{23,71},{255,255},
        {-58,-40},{314,-40},{-63,161},{329,255},{-1,-1},{-16,-16},
        {23,-95},{255,-128},{-59,256},{315,256}};
    const auto cell=[](int v) { return v>=0 ? v/16 : (v-15)/16; };
    const auto mod16=[](int v) { return (v%16+16)%16; };
    for (unsigned n : {0u,12u,38u}) {
      startLevel(n);
      for (const auto &c : cameras) {
        cam.pos=vec2(c[0],c[1]);
        arduboy.clear();
        drawGrid();
        for (int y=0; y<128; ++y) for (int x=0; x<128; ++x) {
          const int wx=x+cam.pos.x, wy=y+cam.pos.y;
          const unsigned tile=gridGetTile(cell(wx),cell(wy));
          unsigned expected=tile==16 ? bitmapPixel(tileSetTwo,16,mod16(x+(cam.pos.x>>2)),mod16(y+(cam.pos.y>>2)))
                                    : bitmapPixel(tileSetTwo,tile,mod16(wx),mod16(wy));
          if (wx>=levelExit.x && wx<levelExit.x+16 && wy>=levelExit.y && wy<levelExit.y+16)
            expected=bitmapPixel(door,key.haveKey,wx-levelExit.x,wy-levelExit.y);
          const unsigned got=(arduboy.buffer[(y/8)*128+x] >> (y%8))&1;
          assert(got==expected);
        }
        mystic::setPalette(mystic::Color);
        arduboy.renderBase(true);
        for (int y=0; y<128; ++y) for (int x=0; x<128; ++x) {
          const bool solid=gridGetSolid(cell(x+cam.pos.x),cell(y+cam.pos.y));
          const unsigned bit=(arduboy.buffer[(y/8)*128+x]>>(y%8))&1;
          const unsigned expected=(solid ? 3 : 0)+bit*2;
          assert(((gfx_fb[y*64+x/2]>>((x&1)*4))&15)==expected);
        }
        mystic::setPalette(mystic::Original);
      }
    }
    for (const auto &p : cameras) {
      kid.pos=vec2(p[0],p[1]);
      resetCamera();
      assert(cam.pos.x>=0 && cam.pos.x<=CAMERA_MAX_X && cam.pos.y>=0 && cam.pos.y<=CAMERA_MAX_Y);
    }
    kid.pos=vec2(192,192);
    resetCamera();
    assert(cam.pos.x==134 && cam.pos.y==136);
    kid.pos=vec2(0,0); resetCamera(); assert(cam.pos.x==0 && cam.pos.y==0);
    kid.pos=vec2(372,368); resetCamera(); assert(cam.pos.x==256 && cam.pos.y==256);
    puts("PASS: full viewport and packed terrain color at 48 camera positions, negative parallax, implicit walls, doors and bottom cap");
}

static void levelsTest() {
    mystic::setPalette(mystic::Original);
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
            assert(cam.pos.y<=CAMERA_MAX_Y);
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



static void colorsTest() {
    using namespace mystic;
    for (unsigned n=0; n<TOTAL_LEVELS; ++n) {
        startLevel(n);
        setPalette(Color);
        tick(B_BUTTON);
        assert(!arduboy.dropped && arduboy.commandCount<=PortDisplay::MAX_COMMANDS);
        const Players before=kid;
        const Camera camera=cam;
        unsigned long colorHash=0;
        uint8_t colorPixels[8192];
        for (Palette p : {Color,Original,DMG,Amber}) {
            setPalette(p);
            applyPalette();
            arduboy.render(true);
            unsigned seen=0;
            unsigned long hash=0;
            for (int y=0; y<128; ++y) for (int x=0; x<128; ++x) {
                const unsigned c=(gfx_fb[y*64+x/2]>>((x&1)*4))&15;
                seen |= 1u<<c;
                hash=hash*31+c;
                if (p==Original) assert(c==((arduboy.buffer[(y/8)*128+x]>>(y%8))&1));
                if (p==Amber) {
                    const unsigned prev=(colorPixels[y*64+x/2]>>((x&1)*4))&15;
                    assert(c==prev || (c==14 && (prev==6 || prev==12)));
                }
            }
            if (p==Original) assert(seen==3);
            else {
                assert(__builtin_popcount(seen)>=4);
                if (p==Color) { colorHash=hash;memcpy(colorPixels,gfx_fb,sizeof colorPixels); }
                else if (p==DMG) assert(hash==colorHash);
            }
            assert(!memcmp(&kid,&before,sizeof kid) && !memcmp(&cam,&camera,sizeof cam));
        }
    }
    setPalette(Color);
    arduboy.clear();
    gameState=STATE_MENU_MAIN;
    stateMenuMain();
    arduboy.render(false);
    unsigned top=0,bottom=0;
    for (int y=0; y<32; ++y) for (int x=0; x<64; ++x) top+=gfx_fb[y*64+x]!=0;
    for (int y=104; y<128; ++y) for (int x=0; x<64; ++x) bottom+=gfx_fb[y*64+x]!=0;
    assert(top>100 && bottom>100); // Both formerly blank bands contain illustration.
    gameState=STATE_MENU_SOUNDFX;
    tick();
    for (unsigned i=0; i<4; ++i) {
        settingsSelection=i;
        tick(B_BUTTON);
        assert(gameState==STATE_MENU_MAIN && palette()==Palette(i));
        tick();
        const bool sound=audio::on();
        setSound(!sound);
        assert(palette()==Palette(i));
        setSound(sound);
        assert(palette()==Palette(i));
        storeProgress();
        progress=Progress{};
        loadProgress();
        assert(palette()==Palette(i) && audio::on()==sound);
        gameState=STATE_MENU_SOUNDFX;
    }
    // Schema-1 records from the monochrome release have only the sound bit.
    Progress legacy;
    legacy.options=0;
    assert(save::store(SAVE_MAGIC,1,legacy));
    loadProgress();
    assert(palette()==Color && !audio::on());
    setSound(true);
    setPalette(Color);
    puts("PASS: all 39 levels in four palettes, unchanged geometry/physics, full splash, menu selection and compatible saved preferences");
}

static void colorBlitTest() {
    using namespace mystic;
    uint8_t sprite[2+2*9*2*2] = {9,11};
    for (unsigned i=2; i<sizeof sprite; ++i) sprite[i]=uint8_t(i*73+41);
    unsigned cases=0;
    for (unsigned mode=0; mode<4; ++mode) for (int scale : {1,2})
      for (const auto &pos : {vec2(-3,-5),vec2(3,7),vec2(119,121),vec2(127,127)}) {
        uint8_t expected[8192];
        memset(expected,0x33,sizeof expected);
        memset(gfx_fb,0x33,sizeof expected);
        const unsigned stride=mode==PortSprites::PlusMask ? 2 : 1;
        for (int sy=0; sy<11; ++sy) for (int sx=0; sx<9; ++sx) {
            const unsigned at=2+((sy/8)*9+sx)*stride, bit=1u<<(sy%8);
            const bool on=sprite[at]&bit;
            const bool mask=mode==PortSprites::Overwrite ||
                (mode==PortSprites::PlusMask ? (sprite[at+1]&bit)!=0 : on);
            for (int dy=0; dy<scale; ++dy) for (int dx=0; dx<scale; ++dx) {
                const int x=pos.x+sx*scale+dx,y=pos.y+sy*scale+dy;
                if (x<0 || x>=128 || y<0 || y>=128 || (!mask && !on)) continue;
                const unsigned value=on && mode!=PortSprites::Erase ? 1 : 0;
                const unsigned atPixel=y*64+x/2,shift=(x%2)*4;
                expected[atPixel]=(expected[atPixel]&~(15u<<shift)) | (value<<shift);
            }
        }
        paintSprite(sprite,pos.x,pos.y,0,(Text<<2)|mode,true,scale);
        assert(!memcmp(expected,gfx_fb,sizeof expected));
        ++cases;
      }
    memset(gfx_fb,0x22,8192);
    paintSprite(elements,20,20,0,(CoinInk<<2)|PortSprites::Overwrite,false);
    assert((gfx_fb[20*64+10]&15)==2); // White matte keeps the world colour.
    unsigned gold=0;
    for (int y=20; y<36; ++y) for (int x=20; x<30; ++x)
        gold+=((gfx_fb[y*64+x/2]>>((x&1)*4))&15)==8;
    assert(gold>0);
    printf("PASS: %u native color-blit clipping/masking/scale references and transparent coin matte\n",cases);
}



static void settingsPreviewTest() {
    using namespace mystic;
    gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_SOUNDFX;
    setPalette(Color);setSound(true);tick();
    const Progress checkpoint=progress;
    const void *record=save::read(SAVE_MAGIC,1,sizeof(Progress));
    tap(B_BUTTON);
    assert(gameState==STATE_MENU_SOUNDFX && settingsSelection==0);
    for (unsigned p=1;p<4;++p) {
        tap(DOWN_BUTTON);
        assert(settingsSelection==p && palette()==Palette(p));
        assert(save::read(SAVE_MAGIC,1,sizeof(Progress))==record);
    }
    tap(DOWN_BUTTON); // Sound row keeps the last preview on screen.
    assert(settingsSelection==4 && palette()==Amber);
    tap(SELECT_BUTTON); // Muting cannot accidentally persist a preview.
    assert(!audio::on() && save::read(SAVE_MAGIC,1,sizeof(Progress))==record);
    tap(A_BUTTON);
    Progress loaded;
    assert(gameState==STATE_MENU_MAIN && palette()==Color);
    assert(save::load(SAVE_MAGIC,1,loaded) && loaded.level==checkpoint.level &&
           loaded.score==checkpoint.score && loaded.coins==checkpoint.coins && loaded.options==0);
    setSound(true);tick();tap(B_BUTTON);tap(DOWN_BUTTON);tap(B_BUTTON);
    assert(gameState==STATE_MENU_MAIN && palette()==Original);
    assert(save::load(SAVE_MAGIC,1,loaded) && ((loaded.options>>1)&3)==Original);
    tap(B_BUTTON);tap(UP_BUTTON); // Preview Color from committed Original.
    assert(palette()==Color);
    tap(UP_BUTTON); // Wrap to SOUND, then confirm just the sound change.
    assert(settingsSelection==4);
    tap(B_BUTTON);
    assert(gameState==STATE_MENU_MAIN && palette()==Original && !audio::on());
    assert(save::load(SAVE_MAGIC,1,loaded) && loaded.options==2);
    setPalette(Color);setSound(true);tick();
    puts("PASS: live palette previews, B confirms, A cancels, SOUND restores committed palette, previews never write flash");
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

static void pauseSaveQuitTest() {
    using namespace mystic;
    startLevel(5,1200,12);tick();
    assert(storeProgress());
    kid.pos=coins[0].pos;checkCollisions();
    assert(scorePlayer==1400 && totalCoins==13 && coinsCollected==1);
    tap(START_BUTTON);tap(DOWN_BUTTON);
    assert(gameState==STATE_GAME_PAUSE && pauseSelection==1);
    const Players frozen=kid;const Camera frozenCam=cam;
    const unsigned commands=arduboy.commandCount;
    for (unsigned i=0;i<20;++i) tick();
    assert(!memcmp(&kid,&frozen,sizeof kid) && !memcmp(&cam,&frozenCam,sizeof cam));
    assert(arduboy.commandCount==commands && !arduboy.dropped);
    tick(A_BUTTON); // Back resumes even with SAVE & QUIT selected.
    assert(gameState==STATE_GAME_PLAYING && !memcmp(&kid,&frozen,sizeof kid));
    tick();tap(START_BUTTON);
    assert(pauseSelection==0); // Every new pause starts safely on RESUME.
    tap(DOWN_BUTTON);
    const Players beforeQuit=kid;
    tick(B_BUTTON);
    assert(gameState==STATE_MENU_MAIN && !pauseSaveFailed && !pauseSaveRequested);
    assert(!memcmp(&kid,&beforeQuit,sizeof kid));
    for (unsigned i=0;i<5;++i) tick(B_BUTTON);
    assert(gameState==STATE_MENU_MAIN && arduboy.title && !arduboy.world);
    tick();
    Progress loaded;
    assert(save::load(SAVE_MAGIC,1,loaded) && loaded.level==5 && loaded.score==1200 && loaded.coins==12);
    assert(loaded.highScore>=1400);
    progress=Progress{};loadProgress(); // Reload from the actual simulated flash.
    assert(progress.level==5 && progress.score==1200 && progress.coins==12);
    tap(B_BUTTON); // PLAY.
    assert(gameState==STATE_MENU_PLAY && cont==0);
    tap(B_BUTTON); // CONT.
    assert(gameState==STATE_GAME_NEXT_LEVEL && level==5 && scorePlayer==1200 && totalCoins==12 && kid.balloons==3);
    chgame.frameCount=0;stateGameNextLevel();
    tick();tap(B_BUTTON);
    assert(gameState==STATE_GAME_PLAYING && scorePlayer==1200 && totalCoins==12 && coinsActive==6);
    kid.pos=coins[0].pos;checkCollisions();
    assert(scorePlayer==1400 && totalCoins==13); // No duplicated mid-level reward.
    puts("PASS: pause selection freezes the world; SAVE & QUIT persists checkpoint and high score; CONT reloads without duplicate coins");
}

static void pauseRenderTest() {
    using namespace mystic;
    for (unsigned p=0; p<4; ++p) {
        setPalette(Palette(p));startLevel(5);tick();
        uint8_t world[8192],overlay[8192];
        memcpy(world,gfx_fb,sizeof world);
        tap(START_BUTTON);
        const unsigned commands=arduboy.commandCount;
        for (unsigned option : {0u,1u,0u}) {
            if (pauseSelection!=option) tap(DOWN_BUTTON);
            assert(arduboy.commandCount==commands && !arduboy.dropped);
            memcpy(overlay,gfx_fb,sizeof overlay);
            arduboy.render(true); // Independent full-world replay reference.
            assert(!memcmp(overlay,gfx_fb,sizeof overlay));
            for (unsigned y=0;y<128;++y) for (unsigned x=0;x<64;++x)
                if (y<36 || y>=92 || x<10 || x>=54)
                    assert(gfx_fb[y*64+x]==world[y*64+x]);
        }
        tap(A_BUTTON);
    }
    setPalette(Color);
    puts("PASS: pause-only replay matches full rendering in all four palettes; switching options preserves the frozen world");
}

static void brightBalloonsTest() {
    using namespace mystic;
    startLevel(0);setPalette(Color);
    kid.pos=vec2(100,300);kid.isActive=true;kid.isBalloon=true;kid.balloons=3;
    kid.direction=0;kid.balloonOffset=0;resetCamera();
    arduboy.clear();drawGrid();drawKid();drawHUD();arduboy.render(true);
    const int x=kid.pos.x-cam.pos.x,y=kid.pos.y-cam.pos.y;
    unsigned colors[16]={},skin=0;
    for (int py=y-12;py<y;++py) for (int px=x;px<x+18;++px)
        ++colors[(gfx_fb[py*64+px/2]>>((px&1)*4))&15];
    for (int py=y;py<y+8;++py) for (int px=x;px<x+12;++px)
        skin+=((gfx_fb[py*64+px/2]>>((px&1)*4))&15)==14;
    assert(colors[6]>4 && colors[8]>4 && colors[12]>4 && skin>10);
    puts("PASS: distinct red/yellow/blue balloons and a light peach face against the sky");
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
    assert(mystic::palette()==mystic::Color);
    assert(gfx_paletteOut(0)==gfx_rgb(68,51,85)); // Pastel Island outline.
    assert(gfx_paletteOut(2)==gfx_rgb(153,221,238)); // Pastel Island sky.
    spritesTest();
    viewportTest();
    levelsTest();
    colorsTest();
    settingsPreviewTest();
    colorBlitTest();
    brightBalloonsTest();
    pauseTest();
    pauseRenderTest();
    pauseSaveQuitTest();
    progressionTest();
    hazardsTest();
    gfx_wait();
    return 0;
}
