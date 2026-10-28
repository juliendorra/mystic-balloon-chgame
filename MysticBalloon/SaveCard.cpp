#include "SaveCard.h"
#include <CHGame.h>
#ifdef CHSIM
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
namespace mystic { namespace card {
static int cut=-1;
void testCutAfter(int bytes) { cut=bytes; }
Result begin() {
    cut=-1;
    const char *path=getenv("CHMB_SAVE_CARD");
    if (!path || !*path) return Absent;
    FILE *f=fopen(path,"rb");if (!f) return Missing;
    fseek(f,0,SEEK_END);const auto size=ftell(f);fclose(f);
    return size==1536 ? Ready : Failed;
}
bool read(unsigned sector,uint8_t *dst) {
    const char *path=getenv("CHMB_SAVE_CARD");if (!path || sector>=3) return false;
    FILE *f=fopen(path,"rb");if (!f) return false;
    const bool ok=!fseek(f,sector*512,SEEK_SET) && fread(dst,1,512,f)==512;
    fclose(f);return ok;
}
bool write(unsigned sector,const uint8_t *src) {
    const char *path=getenv("CHMB_SAVE_CARD");if (!path || sector<1 || sector>2) return false;
    FILE *f=fopen(path,"r+b");if (!f) return false;
    const unsigned n=cut<0 ? 512 : unsigned(cut);
    const bool ok=!fseek(f,sector*512,SEEK_SET) && fwrite(src,1,n,f)==n &&
        !fflush(f) && !fsync(fileno(f)) && n==512;
    fclose(f);return ok;
}
} }
#else
#include "src/savecard/Fat.h"
#include "src/savecard/SdSpi.h"
namespace mystic { namespace card {
static mybl_fat::Run runs[3];
static uint8_t nRuns=0;
Result begin() {
    nRuns=0;
    if (!mybl_sd::init()) {
        delay(50);
        if (!mybl_sd::init()) return Absent;
    }
    auto *buf=gfx_chunkScratch();
    if (mybl_fat::mount(buf)) return Failed;
    mybl_fat::File file;
    const int found=mybl_fat::find("MYBLSAVEDAT",file,buf);
    if (found) return found==mybl_fat::E_NOTFOUND ? Missing : Failed;
    if (file.size!=1536) return Failed;
    const int n=mybl_fat::runs(file,runs,3,buf);
    if (n<1) return Failed;
    nRuns=n;return Ready;
}
bool read(unsigned sector,uint8_t *dst) {
    return sector<3 && mybl_fat::read(runs,nRuns,sector,dst);
}
bool write(unsigned sector,const uint8_t *src) {
    if (sector<1 || sector>2) return false;
    for (unsigned i=0;i<nRuns;++i) {
        if (sector<runs[i].blocks) return mybl_sd::write(runs[i].lba+sector,src);
        sector-=runs[i].blocks;
    }
    return false;
}
} }
#endif
