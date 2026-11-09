#include "Saves.h"
#include "SaveCard.h"
#include <stddef.h>
#include <CHSave.h>
#ifdef CHSIM
#include <stdio.h>
#include <stdlib.h>
#endif

namespace mystic { namespace saves {
Status status=Flash;
uint8_t count=0,active=NO_RUN;
Action action=None;
static bool usingCard=false,legacy=false;
static chsave::Status sharedStatus=chsave::Status::NoCard;
#ifdef CHSIM
// The unit harness uses a raw journal file. Full CHSIM uses the library's
// FAT disk-image backend, just like the other games.
class TestJournal : public chsave::Storage {
public:
    chsave::Status open(uint32_t,uint8_t *) override {
        const char *path=getenv("CHMB_SAVE_CARD");
        FILE *f=path?fopen(path,"rb"):nullptr;
        if(!f)return chsave::Status::NoCard;
        fseek(f,0,SEEK_END);long size=ftell(f);fclose(f);
        return size==1024?chsave::Status::Ready:chsave::Status::Damaged;
    }
    bool read(unsigned slot,uint8_t *out) override {
        const char *path=getenv("CHMB_SAVE_CARD");FILE *f=path?fopen(path,"rb"):nullptr;
        if(!f || slot>1){if(f)fclose(f);return false;}
        bool ok=!fseek(f,slot*512,SEEK_SET) && fread(out,1,512,f)==512;
        fclose(f);return ok;
    }
    bool write(unsigned slot,const uint8_t *in) override {
        const char *path=getenv("CHMB_SAVE_CARD");FILE *f=path?fopen(path,"r+b"):nullptr;
        if(!f || slot>1){if(f)fclose(f);return false;}
        int cut=card::testCutBytes();unsigned bytes=cut<0?512:unsigned(cut);
        bool ok=!fseek(f,slot*512,SEEK_SET) && fwrite(in,1,bytes,f)==bytes;
        ok=!fclose(f) && ok;return ok && bytes==512;
    }
};
static TestJournal testJournal;
static uint32_t owners[2]={0xFFFFFFFFu,0xFFFFFFFFu};
void testFlashOwners(uint32_t a,uint32_t b) { owners[0]=a;owners[1]=b; }
#endif
static bool sharedLoad(Store &out) {
    gfx_wait();
#ifdef CHSIM
    if(getenv("CHMB_SAVE_CARD")) {
        chsave::Journal journal(testJournal,gfx_chunkScratch(),1024);
        bool ok=journal.load(SAVE_MAGIC,VERSION,&out,sizeof out,nullptr);
        sharedStatus=journal.status();return ok;
    }
#endif
    chsave::Store journal;
    bool ok=journal.load(SAVE_MAGIC,VERSION,&out,sizeof out,nullptr);
    sharedStatus=journal.status();return ok;
}
static bool sharedStore(const Store &value) {
    gfx_wait();
#ifdef CHSIM
    if(getenv("CHMB_SAVE_CARD")) {
        chsave::Journal journal(testJournal,gfx_chunkScratch(),1024);
        bool ok=journal.store(SAVE_MAGIC,VERSION,&value,sizeof value,0);
        sharedStatus=journal.status();return ok;
    }
#endif
    chsave::Store journal;
    bool ok=journal.store(SAVE_MAGIC,VERSION,&value,sizeof value,0);
    sharedStatus=journal.status();return ok;
}
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
static void importSeed(Store &out) {
    if (!out.generation && !out.count) {
        Store old;
        if (fromFlash(old)) { out=old;out.generation=0;legacy=true; }
    }
}
bool read(Store &out) {
    out=Store{};
    if(!usingCard)return fromFlash(out);
    if(sharedLoad(out)) {
        if(valid(out,MAX_RUNS))return true;
        status=Damaged;return false;
    }
    if(sharedStatus==chsave::Status::Empty) {
        out=Store{};importSeed(out);return true;
    }
    status=sharedStatus==chsave::Status::Damaged?Damaged:WriteFailed;
    return false;
}
bool write(Store &s) {
    if (!usingCard || !valid(s,capacity()) || status==NeedsSetup || status==Damaged || status==ForeignFlash) return false;
    const uint32_t next=s.generation+1;s.generation=next;
    bool ok=sharedStore(s);
    if (!ok) { status=WriteFailed;return false; }
    status=Sd;legacy=false;
    count=s.count;active=s.active;return true;
}
void begin() {
    usingCard=false;legacy=false;count=0;active=NO_RUN;action=None;
#ifdef CHSIM
    card::testCutAfter(-1);
#endif
    Store s;
    bool loaded=sharedLoad(s);
    usingCard=loaded || sharedStatus==chsave::Status::Empty;
    status=usingCard?Sd:sharedStatus==chsave::Status::NeedsSetup?NeedsSetup:
        sharedStatus==chsave::Status::Damaged?Damaged:ForeignFlash;
    if(usingCard && !loaded) {
        // Older SD files are imported by host provisioning. Flash remains
        // a read-only fallback for the first start of an empty journal.
        s=Store{};importSeed(s);loaded=true;
    }
    if(!usingCard)loaded=fromFlash(s);
    if(loaded && !valid(s,capacity()))status=Damaged;
    if (loaded && status!=Damaged && valid(s,capacity())) {
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
