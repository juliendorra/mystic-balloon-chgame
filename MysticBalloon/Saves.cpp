#include "Saves.h"
#include "SaveCard.h"
#include <stddef.h>

namespace mystic { namespace saves {
Status status=Flash;
uint8_t count=0,active=NO_RUN;
Action action=None;
static bool usingCard=false,legacy=false;
static uint8_t currentSector=0;
#ifdef CHSIM
static uint32_t owners[2]={0xFFFFFFFFu,0xFFFFFFFFu};
void testFlashOwners(uint32_t a,uint32_t b) { owners[0]=a;owners[1]=b; }
#endif
static bool flashOwned() {
    for (unsigned i=0;i<2;++i) {
#ifdef CHSIM
        const uint32_t owner=owners[i];
#else
        const uint32_t owner=*reinterpret_cast<const uint32_t *>(0xF500+i*256);
#endif
        if (owner!=0xFFFFFFFFu && owner!=SAVE_MAGIC) return false;
    }
    return true;
}
static bool flashHasSave() {
    for (unsigned i=0;i<2;++i) {
#ifdef CHSIM
        if (owners[i]==SAVE_MAGIC) return true;
#else
        if (*reinterpret_cast<const uint32_t *>(0xF500+i*256)==SAVE_MAGIC) return true;
#endif
    }
    return false;
}
uint32_t crc(const void *data,unsigned bytes) {
    auto p=static_cast<const uint8_t *>(data);uint32_t c=0xFFFFFFFFu;
    while (bytes--) { c^=*p++;for (unsigned b=0;b<8;++b) c=(c>>1)^(0xEDB88320u&(0u-(c&1))); }
    return ~c;
}
static bool validPrefs(const Progress &p) {
    return p.level<39 && p.coins<=232 && p.highCoins<=232 && p.options<=15 &&
        p.score<=999999 && p.highScore<=999999;
}
bool valid(const Store &s,unsigned limit) {
    if (!validPrefs(s.prefs) || s.count>limit || s.reserved ||
        (s.count ? s.active>=s.count : s.active!=NO_RUN)) return false;
    for (unsigned i=0;i<s.count;++i) {
        const Run &r=s.runs[i];
        if (r.level>=39 || r.total>232 || r.score>999999 || r.entryScore>r.score || (r.kidFlags&~2047u) ||
            r.vx < -54 || r.vx>54 || r.vy < -96 || r.vy>96 || r.immune>60 ||
            r.jump>11 || (r.balloons>>2)>16 || (r.walkers&0xF000) || r.phase>=40) return false;
        for (unsigned k=0;k<6;++k)
            if (r.hp[k]<-32 || r.hp[k]>30 || r.coinX[k]<-64 || r.coinX[k]>448 ||
                r.walkerX[k]<-64 || r.walkerX[k]>448) return false;
    }
    return true;
}
uint8_t capacity() { return usingCard ? MAX_RUNS : FLASH_RUNS; }
static bool fromFlash(Store &s) {
    const void *p=save::read(SAVE_MAGIC,VERSION,FLASH_BYTES);
    if (p) {
        memcpy(&s,p,FLASH_BYTES);
        if (valid(s,FLASH_RUNS)) { legacy=false;return true; }
    }
    p=save::read(SAVE_MAGIC,2,228);
    if (p) {
        memcpy(&s,p,20);
        for (unsigned i=0;i<FLASH_RUNS;++i) {
            memcpy(&s.runs[i],static_cast<const uint8_t *>(p)+20+i*52,52);
            s.runs[i].entryScore=s.runs[i].score;
        }
        if (valid(s,FLASH_RUNS)) { legacy=true;return true; }
    }
    Progress old;
    if (!save::load(SAVE_MAGIC,1,old) || !validPrefs(old)) return false;
    s=Store{};s.prefs=old;s.count=1;s.active=0;
    Run &r=s.runs[0];r.level=old.level;r.score=r.entryScore=old.score;r.total=old.coins;
    r.objects=old.level==22 ? 60 : 63;r.balloons=3;
    legacy=true;return true;
}
static bool recordValid(uint8_t *p) {
    if (*reinterpret_cast<const uint32_t *>(p)!=SAVE_MAGIC || (p[4]!=2 && p[4]!=VERSION)) return false;
    const unsigned bytes=p[4]==2 ? 436 : sizeof(Store);
    if (*reinterpret_cast<const uint32_t *>(p+8+bytes)!=crc(p,8+bytes)) return false;
    if (p[4]==2) {
        // Expand backwards in the sector scratch; no extra Store on the stack.
        for (int i=MAX_RUNS-1;i>=0;--i) {
            auto *r=reinterpret_cast<Run *>(p+28+i*sizeof(Run));
            memmove(r,p+28+i*52,52);r->entryScore=r->score;
        }
    }
    return valid(*reinterpret_cast<const Store *>(p+8),MAX_RUNS);
}
static void importSeed(Store &out) {
    if (!out.generation && !out.count) {
        Store old;
        if (fromFlash(old)) { out=old;out.generation=0;legacy=true; }
    }
}
bool read(Store &out) {
    out=Store{};
    if (!usingCard) return fromFlash(out);
    auto *a=gfx_chunkScratch(),*b=a+512;
    if (currentSector && card::read(currentSector,a) && recordValid(a)) {
        legacy=a[4]!=VERSION;out=*reinterpret_cast<const Store *>(a+8);importSeed(out);return true;
    }
    if (!card::read(1,a) || !card::read(2,b)) { status=WriteFailed;return false; }
    const bool va=recordValid(a),vb=recordValid(b);
    if (!va && !vb) { status=Damaged;return false; }
    const auto *ra=reinterpret_cast<const Store *>(a+8);
    const auto *rb=reinterpret_cast<const Store *>(b+8);
    const auto *chosen=va && vb ? (int32_t(ra->generation-rb->generation)>0 ? ra : rb) : va ? ra : rb;
    currentSector=chosen==ra ? 1 : 2;legacy=(chosen==ra ? a[4] : b[4])!=VERSION;
    out=*chosen;importSeed(out);return true;
}
bool write(Store &s) {
    if (!valid(s,capacity()) || status==NeedsSetup || status==Damaged || status==ForeignFlash) return false;
    const uint32_t next=s.generation+1;s.generation=next;
    bool ok=false;
    if (usingCard) {
        auto *p=gfx_chunkScratch();memset(p,255,512);
        *reinterpret_cast<uint32_t *>(p)=SAVE_MAGIC;p[4]=VERSION;p[5]=0;
        *reinterpret_cast<uint16_t *>(p+6)=uint16_t(next);
        memcpy(p+8,&s,sizeof s);
        *reinterpret_cast<uint32_t *>(p+8+sizeof s)=crc(p,8+sizeof s);
        const unsigned sector=1+(next&1);
        card::write(sector,p);
        ok=card::read(sector,p+512) && !memcmp(p,p+512,512);
        if (ok) currentSector=sector;
    } else if (flashOwned() && save::available()) {
        memcpy(save::buffer(),&s,FLASH_BYTES);
        audio::suspend();ok=save::write(SAVE_MAGIC,VERSION,FLASH_BYTES);audio::resume();
    }
    if (!ok) { currentSector=0;status=WriteFailed;return false; }
    status=usingCard ? Sd : Flash;legacy=false;
    count=s.count;active=s.active;return true;
}
void begin() {
    usingCard=false;legacy=false;currentSector=0;count=0;active=NO_RUN;action=None;
    const card::Result found=card::begin();
    status=found==card::Absent ? (flashOwned() ? Flash : ForeignFlash) :
        found==card::Missing ? NeedsSetup : found==card::Failed ? Damaged : Sd;
    if (status==Sd) {
        auto *p=gfx_chunkScratch();
        if (!card::read(0,p) || memcmp(p,"MYBLSD2",8) ||
            *reinterpret_cast<uint32_t *>(p+8)!=1536 || *reinterpret_cast<uint32_t *>(p+12)!=MAX_RUNS ||
            *reinterpret_cast<uint32_t *>(p+508)!=crc(p,508)) status=Damaged;
        else usingCard=true;
    }
    Store s;
    if (read(s)) {
        if (usingCard && legacy) write(s);
        progress=s.prefs;count=s.count;active=s.active;
    } else {
        progress=Progress{};
        if (!usingCard && status==Flash && flashHasSave()) status=Damaged;
    }
}
bool preferences() {
    Store s;
    const bool have=read(s);
    if (!have && usingCard) return false;
    const bool changed=s.prefs.highScore!=progress.highScore || s.prefs.highCoins!=progress.highCoins ||
        s.prefs.options!=progress.options;
    if (!changed && !legacy) return status!=WriteFailed;
    s.prefs.highScore=progress.highScore;s.prefs.highCoins=progress.highCoins;s.prefs.options=progress.options;
    return write(s);
}
bool create() {
    Store s;
    if (!read(s) && usingCard) return false;
    if (s.count>=capacity()) return false;
    s.active=s.count++;s.runs[s.active]=Run{};
    s.prefs.level=s.prefs.coins=0;s.prefs.score=0;
    s.prefs.highScore=progress.highScore;s.prefs.highCoins=progress.highCoins;s.prefs.options=progress.options;
    if (!write(s)) return false;
    progress=s.prefs;return true;
}
bool select(uint8_t index,Run &out) {
    Store s;if (!read(s) || index>=s.count) return false;
    s.active=index;
    const Run &r=s.runs[index];s.prefs.level=r.level;s.prefs.score=r.score;s.prefs.coins=r.total;
    if (!write(s)) return false;
    out=r;progress=s.prefs;return true;
}
bool checkpoint(const Run &run) {
    if (active==NO_RUN) return true; // Diagnostic visits never replace a run.
    // Death must leave the last playable world available to continue.
    if ((run.objects&MID_LEVEL) && !(run.balloons&3)) return preferences();
    Store s;if (!read(s) || active>=s.count) return false;
    const bool same=s.active==active && !memcmp(&s.runs[active],&run,sizeof run) &&
        s.prefs.highScore==progress.highScore && s.prefs.highCoins==progress.highCoins && s.prefs.options==progress.options;
    if (same && !legacy) return true;
    s.active=active;s.runs[active]=run;
    s.prefs=progress;s.prefs.level=run.level;s.prefs.score=run.score;s.prefs.coins=run.total;
    if (!write(s)) return false;
    progress=s.prefs;return true;
}
uint8_t percent(const Run &r) {
    if (r.kidFlags&512) return 100;
    const unsigned n=r.level==22 ? 4 : 6;
    const unsigned left=__builtin_popcount(unsigned(r.objects&63));
    return ((n-min(n,left))+bool(r.objects&HAS_KEY))*100/(n+1);
}
void label(const Run &r,char *text) {
    // The device's nano printf supports plain unsigned, not long formats.
    memcpy(text,"LEVEL 00 ",9);
    const unsigned level=r.level+1;
    text[6]='0'+level/10;text[7]='0'+level%10;
    fmtInt(text+9,percent(r));
    const unsigned n=strlen(text);text[n]='%';text[n+1]=0;
}
} }
