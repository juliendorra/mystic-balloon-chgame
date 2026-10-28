// Exercise the real game and CHGame implementation on the deterministic host.
#include <assert.h>
#include <initializer_list>
#include "../../MysticBalloon.ino"
#define main unused_chsim_main
#include <main.cpp>
#undef main

namespace chmbtest { void clearFlash();void corruptFlash(unsigned,unsigned); }
static uint32_t saveGeneration() { mystic::saves::Store s;return mystic::saves::read(s) ? s.generation : 0; }
static bool savedProgress(mystic::Progress &p) { mystic::saves::Store s;if (!mystic::saves::read(s)) return false;p=s.prefs;return true; }

static void tick(uint8_t buttons = 0) {
    const uint32_t started=micros();
    chgame.injected = buttons;
    chgame.lockstep = 1;
    loop();
    gfx_wait();
    const uint32_t elapsed=micros()-started;
    if (elapsed<16666) delayMicroseconds(16666-elapsed);
}

static void tap(uint8_t button) { tick(button); tick(); }
static void finishTone(uint8_t buttons=0) {
    for (unsigned i=0;!mystic::audio::saveReady() && i<60;++i) tick(buttons);
    assert(mystic::audio::saveReady());
    tick(buttons); // Commit on the first frame after the last half-wave.
}

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

static bool platformCutout(unsigned tile,unsigned x,unsigned y) {
    if ((x==0 && y<=1) || (x==1 && y==0)) return !(tile&6);
    if ((x==15 && y<=1) || (x==14 && y==0)) return !(tile&3);
    if (x==0 && y==15) return !(tile&12);
    if (x==15 && y==15) return !(tile&9);
    return false;
}

static void platformCornersTest() {
    using namespace mystic;
    unsigned cases=0;
    for (unsigned f=0;f<16;++f) for (unsigned phase=0;phase<16;++phase)
      for (Palette p : {Color,Original,DMG,Amber}) {
        setPalette(p);arduboy.clear();arduboy.world=true;
        arduboy.worldX=int(phase);arduboy.worldY=-int(phase);
        const int px=(arduboy.worldX>>2)&15,py=(arduboy.worldY>>2)&15;
        for (int tx=0;tx<=8;++tx) for (int ty=0;ty<=8;++ty)
            PortSprites::draw(tx*16-px,ty*16-py,tileSetTwo,16,PortSprites::Self);
        const int left=48-(arduboy.worldX&15),top=48-(arduboy.worldY&15);
        arduboy.noteTerrain(3,3,f);
        PortSprites::draw(left,top,tileSetTwo,f,PortSprites::Overwrite);
        arduboy.render(true);
        for (int y=0;y<128;++y) for (int x=0;x<128;++x) {
            unsigned bit=bitmapPixel(tileSetTwo,16,(x+px)&15,(y+py)&15);
            const int sx=x-left,sy=y-top;
            const bool inside=sx>=0 && sx<16 && sy>=0 && sy<16;
            unsigned expected=bit*(p==Original ? 1 : 2);
            if (inside && (p==Original || !platformCutout(f,sx,sy))) {
                bit=bitmapPixel(tileSetTwo,f,sx,sy);
                expected=p==Original ? bit : 3+bit*2;
            }
            const unsigned got=(gfx_fb[y*64+x/2]>>((x&1)*4))&15;
            assert(got==expected);
        }
        ++cases;
      }
    // Integrate the corner patch with real map topology and clipped tiles.
    const vec2 cameras[]={vec2(0,0),vec2(12,12),vec2(-59,-39),vec2(315,255)};
    for (unsigned n=0;n<TOTAL_LEVELS;++n) for (const auto &c : cameras)
      for (Palette p : {Color,Original,DMG,Amber}) {
        startLevel(n);setPalette(p);cam.pos=c;levelExit=vec2(1000,1000);
        arduboy.clear();drawGrid();arduboy.render(true);
        for (int y=0;y<128;++y) for (int x=0;x<128;++x) {
            const int wx=x+c.x,wy=y+c.y;
            const unsigned f=gridGetTile(wx>>4,wy>>4);
            const bool background=f==16 || (p!=Original && platformCutout(f,wx&15,wy&15));
            const unsigned bit=background ? bitmapPixel(tileSetTwo,16,(x+(c.x>>2))&15,(y+(c.y>>2))&15)
                                          : bitmapPixel(tileSetTwo,f,wx&15,wy&15);
            const unsigned expected=p==Original ? bit : background ? bit*2 : 3+bit*2;
            assert(((gfx_fb[y*64+x/2]>>((x&1)*4))&15)==expected);
        }
        ++cases;
      }
    // The atlas uses direct native tile blits rather than the world pipeline.
    // Its alpha must preserve an arbitrary existing background as well.
    for (unsigned f=0;f<17;++f) for (Palette p : {Color,Original,DMG,Amber}) {
        setPalette(p);memset(gfx_fb,0x99,8192);
        paintSprite(tileSetTwo,47,45,f,(TerrainInk<<2)|PortSprites::Overwrite,p==Original);
        for (int y=44;y<=61;++y) for (int x=46;x<=63;++x) {
            const int sx=x-47,sy=y-45;
            unsigned expected=9;
            if (sx>=0 && sx<16 && sy>=0 && sy<16 &&
                (p==Original || f==16 || !platformCutout(f,sx,sy))) {
                const unsigned bit=bitmapPixel(tileSetTwo,f,sx,sy);
                expected=p==Original ? bit : f==16 ? bit*2 : 3+bit*2;
            }
            assert(((gfx_fb[y*64+x/2]>>((x&1)*4))&15)==expected);
        }
        ++cases;
    }
    setPalette(Color);
    printf("PASS: %u full platform composites: all 16 tile joins, 16 scroll phases, 39 maps, four palettes; transparent corners reveal exact parallax\n",cases);
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
      assert(cam.pos.x==kid.pos.x-CAMERA_X && cam.pos.y<=CAMERA_MAX_Y);
    }
    kid.pos=vec2(192,192);
    resetCamera();
    assert(cam.pos.x==134 && cam.pos.y==136);
    kid.pos=vec2(0,0); resetCamera(); assert(cam.pos.x==-58 && cam.pos.y==-56);
    kid.pos=vec2(372,368); resetCamera(); assert(cam.pos.x==314 && cam.pos.y==256);
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

static void scrollingTest() {
    using namespace mystic;
    for (unsigned n=0;n<TOTAL_LEVELS;++n) for (int side=0;side<2;++side) {
        startLevel(n);kid.pos=vec2(side ? 372 : 0,16);resetCamera();
        assert(side ? cam.pos.x+VIEW_W>LEVEL_WIDTH : cam.pos.x<0);
        arduboy.clear();drawGrid();setPalette(Color);arduboy.render(true);
        unsigned wallPixels=0;
        for (int y=0;y<128;++y) for (int x=0;x<128;++x) {
            const int wx=x+cam.pos.x;
            if (wx>=0 && wx<LEVEL_WIDTH) continue;
            const unsigned p=(gfx_fb[y*64+x/2]>>((x&1)*4))&15;
            assert(p==3 || p==5); // The collision wall is visibly terrain.
            ++wallPixels;
        }
        assert(wallPixels>=16*128);
    }
    for (unsigned p=0;p<4;++p) {
        startLevel(0);setPalette(Palette(p));
        kid.pos=vec2(112,32);kid.actualpos=kid.pos<<FIXED_POINT;
        kid.speed=vec2(0,MAX_YSPEED);kid.isImune=false;kid.isActive=true;
        kid.isBalloon=kid.isLanding=true;kid.balloonOffset=0;resetCamera();
        chgame.injected=0;chgame.pollButtons();
        int highest=32;
        for (unsigned frame=0;frame<150;++frame) {
            tick(B_BUTTON); // Real fan collision, gravity and balloon physics.
            if (kid.pos.y<highest) highest=kid.pos.y;
            const int screenY=kid.pos.y-cam.pos.y;
            assert(gameState==STATE_GAME_PLAYING && kid.balloons==3);
            assert(screenY>=24 && screenY+16<=VIEW_H);
            assert(arduboy.commandCount>0 && !arduboy.dropped);
        }
        assert(highest < -16 && cam.pos.y<0); // Actually cross the old clamp.
        for (unsigned frame=0;frame<90;++frame) {
            tick(); // Falling back into the map must keep tracking too.
            assert(kid.pos.y-cam.pos.y>=24 && kid.pos.y-cam.pos.y+16<=VIEW_H);
        }
        assert(kid.pos.y>=0 && gameState==STATE_GAME_PLAYING);
    }
    setPalette(Color);
    puts("PASS: visible side collision walls in all 39 levels; fan ascent above row zero and return stay visible in all four palettes");
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
        const bool sound=mystic::audio::enabled();
        setSound(!sound);
        assert(palette()==Palette(i));
        setSound(sound);
        assert(palette()==Palette(i));
        storeProgress();
        progress=Progress{};
        loadProgress();
        assert(palette()==Palette(i) && mystic::audio::enabled()==sound);
        gameState=STATE_MENU_SOUNDFX;
    }
    // Schema-1 records from the monochrome release have only the sound bit.
    Progress legacy;
    legacy.options=0;
    chmbtest::clearFlash();
    assert(save::store(SAVE_MAGIC,1,legacy));
    loadProgress();
    assert(palette()==Color && !mystic::audio::enabled());
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

static void playerCompositeTest() {
    using namespace mystic;
    unsigned cases=0;
    for (bool inhale : {false,true}) for (unsigned f=0;f<(inhale ? 4u : 12u);++f) {
        const uint8_t *s=inhale ? kidSpriteSuck_plus_mask : kidSprite;
        const unsigned w=s[0],h=s[1],stride=inhale ? 2 : 1;
        bool black[16][16]={},white[16][16]={},outside[16][16]={};
        for (unsigned y=0;y<h;++y) for (unsigned x=0;x<w;++x) {
            const unsigned at=2+(f*w*2+(y/8)*w+x)*stride,bit=1u<<(y%8);
            if (inhale) {
                white[y][x]=s[at]&bit;
                black[y][x]=(s[at+1]&bit) && !white[y][x];
            } else {
                black[y][x]=s[at]&bit;
                white[y][x]=(s[2+(12+f/6)*w*2+(y/8)*w+x]&bit) && !black[y][x];
            }
            outside[y][x]=!black[y][x] && (!x || !y || x==w-1 || y==h-1);
        }
        // Independent wave propagation defines the exterior around the
        // black contours; it never reads the generated color assets.
        bool changed;
        do {
            changed=false;
            for (unsigned y=0;y<h;++y) for (unsigned x=0;x<w;++x) {
                if (black[y][x] || outside[y][x]) continue;
                if ((x && outside[y][x-1]) || (x+1<w && outside[y][x+1]) ||
                    (y && outside[y-1][x]) || (y+1<h && outside[y+1][x]))
                    { outside[y][x]=true;changed=true; }
            }
        } while (changed);
        for (Palette p : {Color,Original,DMG,Amber}) for (const auto &pos :
                {vec2(-3,-5),vec2(37,41),vec2(48,48),vec2(119,121),vec2(127,127)}) {
            setPalette(p);arduboy.clear();arduboy.world=true;
            arduboy.worldX=23;arduboy.worldY=-19;
            for (unsigned i=0;i<sizeof arduboy.buffer;++i) arduboy.buffer[i]=uint8_t(i*31+19);
            for (unsigned i=0;i<9;++i) arduboy.terrain[i]=(i*73+41)&511;
            arduboy.terrainCoverage=true;
            for (unsigned ty=0;ty<9;++ty) for (unsigned tx=0;tx<9;++tx)
                arduboy.terrainFrames[ty][tx]=(ty*3+tx*5)&15;
            arduboy.render(true);
            uint8_t background[8192];memcpy(background,gfx_fb,sizeof background);
            if (inhale) PortSprites::draw(pos.x,pos.y,s,f,PortSprites::PlusMask);
            else {
                PortSprites::draw(pos.x,pos.y,s,12+f/6,PortSprites::Self);
                PortSprites::draw(pos.x,pos.y,s,f,PortSprites::Erase);
            }
            assert(arduboy.kidW*arduboy.kidPages<=sizeof arduboy.kidBackdrop);
            arduboy.render(true);
            for (int y=0;y<128;++y) for (int x=0;x<128;++x) {
                const unsigned got=(gfx_fb[y*64+x/2]>>((x&1)*4))&15;
                if (p==Original) {
                    assert(got==((arduboy.buffer[(y/8)*128+x]>>(y%8))&1));
                    continue;
                }
                const int sx=x-pos.x,sy=y-pos.y;
                unsigned expected=(background[y*64+x/2]>>((x&1)*4))&15;
                if (sx>=0 && sx<int(w) && sy>=0 && sy<int(h)) {
                    if (black[sy][sx]) expected=0;
                    else if (white[sy][sx] && !outside[sy][sx])
                        expected=p==Amber ? 14 : sy<8 ? 14 : sy<13 ? 6 : 12;
                }
                assert(got==expected);
            }
            ++cases;
        }
    }
    setPalette(Color);
    printf("PASS: %u complete player/inhale composites preserve patterned sky/terrain, clipping, original pixels and uniformly light Amber fill\n",cases);
}

static void settingsPreviewTest() {
    using namespace mystic;
    gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_SOUNDFX;
    setPalette(Color);setSound(true);tick();
    const Progress checkpoint=progress;
    const auto record=saveGeneration();
    tap(B_BUTTON);
    assert(gameState==STATE_MENU_SOUNDFX && settingsSelection==0);
    for (unsigned p=1;p<4;++p) {
        tap(DOWN_BUTTON);
        assert(settingsSelection==p && palette()==Palette(p));
        assert(saveGeneration()==record);
    }
    tap(DOWN_BUTTON); // Leaving color previews restores the confirmed palette.
    assert(settingsSelection==4 && palette()==Color);
    tap(DOWN_BUTTON);
    assert(settingsSelection==5 && palette()==Color);
    tap(UP_BUTTON);
    assert(settingsSelection==4 && palette()==Color);
    tap(SELECT_BUTTON); // Muting cannot accidentally persist a preview.
    assert(!mystic::audio::enabled() && saveGeneration()==record);
    tap(A_BUTTON);
    finishTone();
    Progress loaded;
    assert(gameState==STATE_MENU_MAIN && palette()==Color);
    assert(savedProgress(loaded) && loaded.level==checkpoint.level &&
           loaded.score==checkpoint.score && loaded.coins==checkpoint.coins && loaded.options==1);
    assert(mystic::audio::enabled());
    setSound(true);tick();tap(B_BUTTON);tap(DOWN_BUTTON);tap(B_BUTTON);
    finishTone();
    assert(gameState==STATE_MENU_MAIN && palette()==Original);
    assert(savedProgress(loaded) && ((loaded.options>>1)&3)==Original);
    tap(B_BUTTON);tap(UP_BUTTON); // Preview Color from committed Original.
    assert(palette()==Color);
    tap(UP_BUTTON); // Wrap to SOUND, then confirm just the sound change.
    assert(settingsSelection==5);
    tap(RIGHT_BUTTON);
    assert(gameState==STATE_MENU_SOUNDFX && !mystic::audio::enabled());
    tap(B_BUTTON);
    assert(gameState==STATE_MENU_MAIN && palette()==Original && !mystic::audio::enabled());
    finishTone();
    assert(savedProgress(loaded) && loaded.options==2);
    setPalette(Color);setSound(true);tick();
    puts("PASS: live palette previews, B confirms, A cancels, SOUND restores committed palette, previews never write flash");
}

static unsigned previewCount;
static uint16_t previewNotes[16];
static void previewTrace(uint16_t hz,uint16_t) {
    if (mystic::audio::enabled() && previewCount<16) previewNotes[previewCount++]=hz;
}

static void settingsAudioPreviewTest() {
    using namespace mystic;
    setPalette(DMG);setAudioMode(mystic::audio::Beeps);setSound(true);
    gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_SOUNDFX;finishTone();
    const auto record=saveGeneration();
    tap(B_BUTTON);
    previewCount=0;mystic::tones::trace=previewTrace;
    tap(DOWN_BUTTON); // Amber is temporary.
    tap(DOWN_BUTTON); // Audio row restores confirmed DMG and starts samples.
    assert(settingsSelection==4 && palette()==DMG);
    for (unsigned i=0;i<142;++i) tick();
    assert(previewCount==6);
    for (unsigned i=0;i<6;++i) assert(previewNotes[i]==(i%3==0 ? 200 : i%3==1 ? 400 : 80));
    tap(DOWN_BUTTON);tap(RIGHT_BUTTON);
    assert(settingsSelection==5 && !mystic::audio::enabled() && gameState==STATE_MENU_SOUNDFX);
    previewCount=0;
    for (unsigned i=0;i<150;++i) tick();
    assert(previewCount==0 && saveGeneration()==record);
    tap(LEFT_BUTTON);
    assert(mystic::audio::enabled() && previewCount==2 && previewNotes[0]==300 && previewNotes[1]==200);
    tap(UP_BUTTON);tap(LEFT_BUTTON);
    assert(gameState==STATE_MENU_SOUNDFX && mystic::audio::mode()==mystic::audio::Atm && palette()==DMG);
    assert(mystic::audio::song()==1 && !mystic::audio::saveReady()); // Real tracker preview effect.
    tap(DOWN_BUTTON);tap(LEFT_BUTTON);
    assert(!mystic::audio::enabled());
    const auto stopped=mystic::audio::ticks();
    for (unsigned i=0;i<80;++i) tick();
    assert(mystic::audio::ticks()==stopped);
    tap(A_BUTTON);finishTone();
    assert(gameState==STATE_MENU_MAIN && palette()==DMG && mystic::audio::enabled() &&
           mystic::audio::mode()==mystic::audio::Beeps);
    assert(saveGeneration()==record);
    mystic::tones::trace=nullptr;
    setPalette(Color);setAudioMode(mystic::audio::Atm);setSound(true);finishTone();
    puts("PASS: Left/Right previews both audio states in CONF, repeating beep/ATM samples, mute silence, A restores all options without flash writes");
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
    assert(!mystic::audio::enabled());
    tick();
    tick(SELECT_BUTTON);
    assert(mystic::audio::enabled());
    tick();
    puts("PASS: START freezes physics, camera and pixels; resume and mute work");
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



static void commandCapacityTest() {
    using Ref=mystic::PortDisplay::PackedSpriteRef;
    static_assert(sizeof(Ref)==2,"two-byte Rev0 sprite reference");
    for (uintptr_t a=0x3000;a<=0xffff;++a) {
        const auto p=reinterpret_cast<const uint8_t *>(a);
        assert(Ref::supported(p) && static_cast<const uint8_t *>(Ref(p))==p);
    }
    for (uintptr_t a=0x20000010;a<0x20003000;++a) {
        const auto p=reinterpret_cast<const uint8_t *>(a);
        assert(Ref::supported(p) && static_cast<const uint8_t *>(Ref(p))==p);
    }
    assert(!Ref::supported(reinterpret_cast<const uint8_t *>(0x20003000)));
    assert(!Ref::supported(reinterpret_cast<const uint8_t *>(0x2fff)));
    puts("PASS: packed sprite references round-trip flash and the SRAM small-constant pool, including particles and cursors");
    unsigned peak=0,views=0;
    for (unsigned n=0;n<TOTAL_LEVELS;++n) {
        startLevel(n);
        for (int y=-32;y<=288;y+=16) for (int x=-32;x<=288;x+=16) {
            cam.pos=vec2(x,y);kid.pos=vec2(x+64,y+64);
            arduboy.clear();drawGrid();enemiesUpdate();drawKid();drawHUD();
            const unsigned used=arduboy.commandCount+arduboy.dropped;
            if (used>peak) peak=used;
            ++views;
        }
    }
    printf("PASS: %u viewport positions across 39 levels; peak %u/%u sprite commands, no drops\n",
           views,peak,mystic::PortDisplay::MAX_COMMANDS);
    assert(peak<=mystic::PortDisplay::MAX_COMMANDS);
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

#include "save_cases.h"

static void helpMenuTest() {
    for (unsigned palette=0;palette<4;++palette) {
        mystic::setPalette(mystic::Palette(palette));
        gameState=STATE_MENU_MAIN;menuSelection=STATE_MENU_HELP;
        finishTone();const auto generation=saveGeneration();
        tap(B_BUTTON);assert(gameState==STATE_MENU_HELP);
        // Check actual rendered instruction rows, excluding the border.
        // A blank handler or title-only page must fail in every palette.
        for (unsigned line=0;line<11;++line) {
            unsigned ink=0;
            for (unsigned y=24+line*8;y<29+line*8;++y)
                for (unsigned x=10;x<118;++x) ink+=(gfx_fb[y*64+x/2]>>((x&1)*4)&15)!=0;
            assert(ink>=25);
        }
        assert(gfx_paletteOut(0)!=gfx_paletteOut(1));
        tap(A_BUTTON);assert(gameState==STATE_MENU_MAIN);
        tap(B_BUTTON);assert(gameState==STATE_MENU_HELP);
        tap(B_BUTTON);assert(gameState==STATE_MENU_MAIN);
        finishTone();assert(saveGeneration()==generation);
    }
    puts("PASS: HELP shows all 11 instruction rows in four palettes; A/B return without changing saves");
}

int main() {
    setup();
    assert(mystic::palette()==mystic::Color);
    assert(gfx_paletteOut(0)==gfx_rgb(68,51,85)); // Pastel Island outline.
    assert(gfx_paletteOut(2)==gfx_rgb(153,221,238)); // Pastel Island sky.
    spritesTest();
    viewportTest();
    platformCornersTest();
    levelsTest();
    scrollingTest();
    colorsTest();
    settingsPreviewTest();
    settingsAudioPreviewTest();
    colorBlitTest();
    playerCompositeTest();
    brightBalloonsTest();
    pauseTest();
    pauseRenderTest();
    hazardsTest();
    commandCapacityTest();
    saveSystemTest();
    helpMenuTest();
    gfx_wait();
    return 0;
}
