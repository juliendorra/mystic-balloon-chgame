// Share the public library's first-save allocator with the asset filesystem.
#if __has_include(<chsave/detail/Allocate.h>)
#include <chsave/detail/Allocate.h>
#define CHGAME_AUTO_SAVES 1
#endif
namespace fat {
static int8_t saveSectors(uint32_t c,unsigned count,uint32_t *sectors,uint8_t *b) {
    unsigned index=0;uint32_t cached=~0u;
    while(index<count) {
        if(c<2 || c>=v.end)return E_CHAIN;
        uint32_t lba=clusLba(c);
        for(unsigned i=0;i<(1u<<v.shift) && index<count;++i)sectors[index++]=lba+i;
        int32_t n=next(c,b,cached);if(n<0)return n;
        if(index==count)return n==0?OK:E_CHAIN;
        if(n<2)return E_CHAIN;
        c=n;
    }
    return E_CHAIN;
}
// The bundled SDK games still use their four-character root filenames.
static int8_t saveExact(const char *name,unsigned count,uint32_t *sectors,uint8_t *b) {
    uint32_t dir=v.rootSecs?0:v.root,sec=0,cached=~0u;
    for(unsigned guard=4096;guard--;) {
        uint32_t lba;
        if(!dir) {
            if(sec==v.rootSecs)return E_NOTFOUND;
            lba=v.root+sec;
        } else {
            if(sec>>v.shift) {
                int32_t n=next(dir,b,cached);if(n<2)return n<0?n:E_NOTFOUND;
                dir=n;sec=0;
            }
            lba=clusLba(dir)+sec;
        }
        ++sec;
        if(!sd::read(lba,b))return E_READ;
        for(unsigned off=0;off<512;off+=32) {
            const uint8_t *d=b+off;
            if(!d[0])return E_NOTFOUND;
            bool match=true;
            for(unsigned i=0;i<11;++i)if(d[i]!=uint8_t(name[i]))match=false;
            if(!match)continue;
            if((d[11]&0x1f) || u32at(d+28)!=count*512)return E_CHAIN;
            uint32_t c=u16at(d+26) | (v.rootSecs?0:u16at(d+20)<<16);
            return saveSectors(c,count,sectors,b);
        }
    }
    return E_CHAIN;
}
int8_t saveFile(uint32_t magic,uint32_t sectors[2],uint8_t *b) {
    char name[11];
    for(unsigned i=0;i<4;++i){name[i]=magic;magic>>=8;}
    for(unsigned i=4;i<8;++i)name[i]=' ';
    name[8]='S';name[9]='A';name[10]='V';
    return saveExact(name,2,sectors,b);
}
#ifdef CHGAME_AUTO_SAVES
namespace {
struct SaveBlocks {
    static bool read(uint32_t lba,uint8_t *p){return sd::read(lba,p);}
    static bool write(uint32_t lba,const uint8_t *p){return sd::write(lba,p);}
};
using SaveStorage=chsave::detail::Allocator<SaveBlocks,decltype(v)>;
uint32_t saveParent;
}
int8_t saveNamespacedFile(const char *base,uint32_t sectors[3],uint8_t *b) {
    uint32_t c,bytes;
    auto status=SaveStorage(v,b).open(base,saveParent,c,bytes);
    if(status==chsave::Status::NotFound)return E_NOTFOUND;
    if(status==chsave::Status::IoError)return E_READ;
    if(status!=chsave::Status::Ready || bytes!=1536)return E_CHAIN;
    return saveSectors(c,3,sectors,b);
}
chsave::Status createNamespacedFile(const chsave::Id &id,uint16_t sectors,uint8_t *b) {
    return SaveStorage(v,b).create(id,sectors,saveParent);
}
#endif
}
