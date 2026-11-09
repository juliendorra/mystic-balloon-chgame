#pragma GCC optimize("Oz")
#include "Save.h"
#include "Card.h"
#include <CHGfx.h>
#include <string.h>
namespace save {
namespace {
// Fixed-width, little-endian header. CRC covers all 508 preceding bytes.
struct Header {uint32_t magic,generation;uint16_t size;uint8_t version,flag;uint32_t tag;};
static_assert(sizeof(Header)==16,"journal header layout");
Status state=NoCard;
uint32_t crc(const uint8_t *p,unsigned n) {
    uint32_t c=~0u;while(n--) {c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;
}
bool blank(const uint8_t *p) {for(unsigned i=0;i<512;++i)if(p[i]!=255)return false;return true;}
bool valid(const uint8_t *p,uint32_t magic) {
    const auto &h=*reinterpret_cast<const Header *>(p);
    return h.magic==magic && h.tag==0x31565343 && h.size<=MAX_DATA &&
        *reinterpret_cast<const uint32_t *>(p+508)==crc(p,508);
}
// Scan both slots, including on every write: a game may switch magic IDs.
// A failed read never counts as an empty slot. Only two erased slots seed.
bool latest(uint32_t magic,uint8_t *scratch,int &index,Header &head) {
    bool have=false,empty=true;index=-1;
    for(unsigned i=0;i<2;++i) {
        if(!save_card::read(i,scratch)) {state=Failed;return false;}
        empty=empty && blank(scratch);
        if(valid(scratch,magic)) {
            const auto &h=*reinterpret_cast<const Header *>(scratch);
            if(!have || int32_t(h.generation-head.generation)>0) {head=h;index=i;have=true;}
        }
    }
    if(!have && !empty) {state=Damaged;return false;}
    return true;
}
}
Status status() {return state;}
bool available() {return state==Ready;}
const void *read(uint32_t magic,uint8_t version,uint16_t size,uint8_t *flag) {
    if(size>MAX_DATA)return nullptr;
    gfx_wait();auto *b=gfx_chunkScratch();
    state=save_card::open(magic,b+512);
    if(state!=Ready)return state==NoCard || state==NeedsSetup?readLegacy(magic,version,size,flag):nullptr;
    Header h{};int index;
    if(!latest(magic,b+512,index,h))return nullptr;
    if(index>=0) {
        if(h.version!=version || h.size!=size)return nullptr;
        if(!save_card::read(index,b+512)) {state=Failed;return nullptr;}
        if(!valid(b+512,magic)) {state=Damaged;return nullptr;}
        if(flag)*flag=h.flag;return b+512+sizeof(Header);
    }
    uint8_t oldFlag=0;const void *old=readLegacy(magic,version,size,&oldFlag);
    if(!old)return nullptr;
    memcpy(buffer(),old,size);
    if(!write(magic,version,size,oldFlag))return nullptr;
    if(flag)*flag=oldFlag;return b+512+sizeof(Header);
}
void *buffer() {gfx_wait();auto *b=gfx_chunkScratch();memset(b,0,512);return b+sizeof(Header);}
bool write(uint32_t magic,uint8_t version,uint16_t size,uint8_t flag) {
    if(size>MAX_DATA)return false;
    gfx_wait();auto *b=gfx_chunkScratch();
    state=save_card::open(magic,b+512);if(state!=Ready)return false;
    Header h{};int index;
    if(!latest(magic,b+512,index,h))return false;
    if(index>=0) {
        // Avoid wear when the current payload and flags are unchanged.
        if(!save_card::read(index,b+512)) {state=Failed;return false;}
        if(h.version==version && h.size==size && h.flag==flag && !memcmp(b+16,b+528,size))return true;
    }
    *reinterpret_cast<Header *>(b)=Header{magic,h.generation+1,size,version,flag,0x31565343};
    memset(b+16+size,0,508-16-size);
    *reinterpret_cast<uint32_t *>(b+508)=crc(b,508);
    unsigned target=index<0?0:1-index;
    if(!save_card::write(target,b) || !save_card::read(target,b+512) || memcmp(b,b+512,512)) {state=Failed;return false;}
    return true;
}
}
