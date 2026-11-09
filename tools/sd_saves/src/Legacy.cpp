#pragma GCC optimize("Oz")
#include "Save.h"
#ifndef CHSIM
extern "C" uint32_t _data_lma,_data_vma,_edata;
#endif
namespace save {
static uint32_t crc(const uint8_t *p,unsigned n) {
    uint32_t c=~0u;while(n--) {c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;
}
const void *readLegacy(uint32_t magic,uint8_t version,uint16_t size,uint8_t *flag) {
    if(size>MAX_DATA)return nullptr;
#ifdef CHSIM
    // The host journal tests provide a synthetic flash backup here.
    extern const uint8_t *legacyPages();const uint8_t *pages=legacyPages();
    if(!pages)return nullptr;
#else
    // A larger image can occupy former save pages; never interpret its code.
    uint32_t end=uint32_t(&_data_lma)+(uint32_t(&_edata)-uint32_t(&_data_vma));
    const uint8_t *pages=reinterpret_cast<const uint8_t *>(0xF500);
#endif
    const uint8_t *chosen=nullptr;uint16_t seq=0;
    const unsigned at=(8+size+3)&~3u;
    for(unsigned i=0;i<2;++i) {
#ifndef CHSIM
        if(end>0xF500+i*256)continue;
#endif
        const uint8_t *p=pages+i*256;
        if(*reinterpret_cast<const uint32_t *>(p)!=magic || p[4]!=version ||
           *reinterpret_cast<const uint32_t *>(p+at)!=crc(p,at))continue;
        const auto n=*reinterpret_cast<const uint16_t *>(p+6);
        if(!chosen || int16_t(n-seq)>0) {chosen=p;seq=n;}
    }
    if(chosen && flag)*flag=chosen[5];
    return chosen?chosen+8:nullptr;
}
}
