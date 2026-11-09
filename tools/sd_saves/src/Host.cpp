#ifdef CHSIM
#ifdef CHGAME_SAVE_SHARED_TEST
#include <SdSpi.h>
#else
#include "card/SdSpi.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef CHGAME_SAVE_SHARED_TEST
namespace sd {
#else
namespace save_sd {
#endif
static const char *path() {
    const char *p=getenv("CHGAME_SAVE_CARD");if(p)return p;
    static char temporary[]="/tmp/chgame-saves-XXXXXX";static bool ready=false;
    const char *source=getenv("CHGAME_SAVE_TEMPLATE");
    if(!source)return getenv("CHSD_CARD");
    if(!ready) {
        FILE *in=fopen(source,"rb");if(!in)return nullptr;
        int fd=mkstemp(temporary);FILE *out=fd>=0?fdopen(fd,"wb"):nullptr;
        if(!out) {fclose(in);return nullptr;}
        uint8_t b[512];size_t n;bool ok=true;
        while((n=fread(b,1,512,in)))if(fwrite(b,1,n,out)!=n) {ok=false;break;}
        ok=!ferror(in) && ok;fclose(in);ok=!fclose(out) && ok;
        if(!ok) {unlink(temporary);return nullptr;}
        // The open pathname is used for each transaction; clean it at exit.
        atexit([]{unlink(temporary);});ready=true;
    }
    return temporary;
}
bool init() {const char *p=path();FILE *f=p?fopen(p,"rb"):nullptr;if(!f)return false;fclose(f);return true;}
bool read(uint32_t lba,uint8_t *dst) {
    const char *p=path();FILE *f=p?fopen(p,"rb"):nullptr;if(!f)return false;
    bool ok=!fseek(f,uint64_t(lba)*512,SEEK_SET) && fread(dst,1,512,f)==512;fclose(f);return ok;
}
bool write(uint32_t lba,const uint8_t *src) {
    const char *p=path();FILE *f=p?fopen(p,"r+b"):nullptr;if(!f)return false;
    const char *cut=getenv("CHGAME_SAVE_CUT");unsigned n=cut?strtoul(cut,nullptr,10):512;if(n>512)n=512;
    bool ok=!fseek(f,uint64_t(lba)*512,SEEK_SET) && fwrite(src,1,n,f)==n && !fflush(f) && !fsync(fileno(f)) && n==512;
    fclose(f);return ok;
}
}
namespace save {
const uint8_t *legacyPages() {
    alignas(4) static uint8_t pages[512];
    const char *p=getenv("CHGAME_LEGACY_FLASH");FILE *f=p?fopen(p,"rb"):nullptr;if(!f)return nullptr;
    bool ok=fread(pages,1,512,f)==512;fclose(f);return ok?pages:nullptr;
}
}
#endif
