#include "Port.h"
#include "SplashArt.h"

namespace mystic {
#if CHGAME_DEBUG
static uint8_t colorVariant = art::defaultColorVariant;
void setColorVariant(uint8_t index) { colorVariant=index&3; }
#else
static constexpr uint8_t colorVariant = art::defaultColorVariant;
#endif
void applyPalette() {
    static int previous = -1;
    const unsigned mode=palette();
    const unsigned key=mode*4+colorVariant;
    if (previous == int(key)) return;
    previous=key;
    for (unsigned i=0; i<16; ++i) {
        const unsigned c=mode==Color ? art::colorPalettes[colorVariant][i] : art::palettes[mode][i];
        gfx_setPaletteEntry(i,gfx_rgb(((c>>8)&15)*17,((c>>4)&15)*17,(c&15)*17));
    }
}

void paintSplash(bool original) {
    const uint8_t *src=original ? art::splashMono : art::splashColor;
    uint8_t *dst=gfx_fb;
    while (dst<gfx_fb+GFX_FB_BYTES) {
        const unsigned code=*src++, count=(code&127)+1;
        if (code&128) { memset(dst,*src++,count); }
        else { memcpy(dst,src,count); src+=count; }
        dst+=count;
    }
}

static inline __attribute__((always_inline)) void rowColors(unsigned role, unsigned frame,
        int y, bool original, uint8_t &ink, uint8_t &paper) {
    static const uint8_t inks[] = {1,8,8,8,6,14,1,14,10,12,6,1,10,1,1,1,13,8,12,1,1,5};
    static const uint8_t papers[] = {0,0,0,0,0,0,0,0,0,0,0,13,0,0,6,8,0,0,0,8,12,3};
    ink=original ? 1 : inks[role<sizeof inks ? role : 0];
    paper=original ? 0 : papers[role<sizeof papers ? role : 0];
    if (original) return;
    if (role==KidMask || role==KidSuck)
        ink=y<8 ? 14 : y<13 ? 6 : 12;
    else if (role==FanInk) ink=y<5 ? 13 : 12;
    else if (role==DoorInk) ink=frame ? 8 : 10;
    else if (role==LogoInk) ink=y<25 ? 13 : 8;
    else if (role==TerrainInk && frame==16) { ink=2; paper=0; }
}

static const uint8_t *tintFor(unsigned role) {
    if (role==CoinInk || role==KeyInk) return art::elementsTint;
    if (role==FanInk) return art::fanTint;
    if (role==SpikeInk) return art::sprSpikesTint;
    if (role==WalkerInk) return art::walkerSpriteTint;
    if (role==DoorInk) return art::doorTint;
    return nullptr;
}



// Only the static introduction needs scaling; keep this out of the game hot loop.
static void paintScaled(const uint8_t *s,int x,int y,unsigned frame,unsigned style,bool original,int scale) {
    const unsigned role=style>>2,mode=style&3;
    const int w=s[0],h=s[1],stride=mode==PortSprites::PlusMask ? 2 : 1;
    s+=2+frame*w*((h+7)/8)*stride;
    for (int sy=0;sy<h;++sy) {
        uint8_t ink,paper;rowColors(role,frame,sy,original,ink,paper);
        for (int sx=0;sx<w;++sx) {
            const uint8_t *src=s+((sy>>3)*w+sx)*stride;
            const unsigned bit=1u<<(sy&7);
            const bool on=*src&bit;
            const bool mask=mode==PortSprites::Overwrite ||
                (mode==PortSprites::PlusMask ? (src[1]&bit)!=0 : on);
            if (!mask && !on) continue;
            const unsigned c=on && mode!=PortSprites::Erase ? ink : paper;
            for (int dy=0;dy<scale;++dy) for (int dx=0;dx<scale;++dx) {
                const int px=x+sx*scale+dx,py=y+sy*scale+dy;
                if (px<0 || px>=128 || py<0 || py>=128) continue;
                uint8_t &dst=gfx_fb[py*64+px/2];
                const unsigned shift=(px&1)*4;
                dst=(dst&~(15u<<shift)) | (c<<shift);
            }
        }
    }
}

static RAMFUNC(mysticColor) void paintPacked(const uint8_t *s,int x,int y,uint8_t frame,
                                           unsigned role,unsigned mode,bool original) {
    const int w=s[0],h=s[1],stride=mode==PortSprites::PlusMask ? 2 : 1;
    if (x>=128 || y>=128 || x+w<=0 || y+h<=0) return;
    const int x0=max(0,-x),x1=min(w,128-x);
    const uint8_t *tint=original ? nullptr : tintFor(role);
    if (tint) tint+=frame*w*((h+7)/8);
    s+=2+frame*w*((h+7)/8)*stride;
    const bool balloon=!original && (role==BalloonInk || role==YellowBalloonInk || role==BlueBalloonInk);
    for (int page=0;page<(h+7)/8;++page) {
        const int py=y+page*8,first=max(0,-py),last=min(8,min(h-page*8,128-py));
        if (first>=last) continue;
        uint8_t inks[8],papers[8];
        for (int r=first;r<last;++r) rowColors(role,frame,page*8+r,original,inks[r],papers[r]);
        for (int sx=x0;sx<x1;) {
            const int dx=x+sx;
            const bool pair=!(dx&1) && sx+1<x1;
            const uint8_t *src=s+(page*w+sx)*stride;
            unsigned a=src[0],b=pair ? src[stride] : 0;
            unsigned ma=mode==PortSprites::Overwrite ? 255 : mode==PortSprites::PlusMask ? src[1]|a : a;
            unsigned mb=!pair ? 0 : mode==PortSprites::Overwrite ? 255 : mode==PortSprites::PlusMask ? src[stride+1]|b : b;
            if (tint) {
                ma &= ~a | tint[page*w+sx];
                if (pair) mb &= ~b | tint[page*w+sx+1];
            }
            if (mode==PortSprites::Erase) a=b=0;
            a>>=first;b>>=first;ma>>=first;mb>>=first;
            const unsigned shift=(dx&1)*4;
            uint8_t *dst=gfx_fb+(py+first)*64+dx/2;
            for (int r=first;r<last;++r,dst+=64,a>>=1,b>>=1,ma>>=1,mb>>=1) {
                unsigned mask=0,bits=0;
                const bool highlight=balloon && page==0 && r>=2 && r<=3;
                if (ma&1) {
                    unsigned c=(a&1) ? inks[r] : papers[r];
                    if (highlight && sx==2 && (a&1)) c=1;
                    mask=15u<<shift;bits=c<<shift;
                }
                if (mb&1) {
                    unsigned c=(b&1) ? inks[r] : papers[r];
                    if (highlight && sx+1==2 && (b&1)) c=1;
                    mask|=240;bits|=c<<4;
                }
                if (mask) *dst=(*dst&~mask)|bits;
            }
            sx+=pair ? 2 : 1;
        }
    }
}

void paintSprite(const uint8_t *s,int x,int y,uint8_t frame,
                 uint8_t style,bool original,int scale) {
    unsigned role=style>>2,mode=style&3;
    if (scale!=1) paintScaled(s,x,y,frame,(role<<2)|mode,original,scale);
    else paintPacked(s,x,y,frame,role,mode,original);
}
} // namespace mystic
