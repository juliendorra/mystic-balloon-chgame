// Specialized exact root lookup for a preallocated two-sector save file.
// Uses CHSd's existing validated volume/chain reader; no allocator or writer.
namespace fat {
int8_t saveFile(uint32_t magic,uint32_t sectors[2],uint8_t *b) {
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
            if(u32at(d)!=magic || le32(d+4)!=0x20202020u ||
               d[8]!='S' || d[9]!='A' || d[10]!='V')continue;
            if((d[11]&0x1f) || u32at(d+28)!=1024)return E_CHAIN;
            uint32_t c=u16at(d+26) | (v.rootSecs?0:u16at(d+20)<<16);
            if(c<2 || c>=v.end)return E_CHAIN;
            sectors[0]=clusLba(c);cached=~0u;
            int32_t n=next(c,b,cached);if(n<0)return n;
            if(v.shift) {
                if(n)return E_CHAIN;
                sectors[1]=sectors[0]+1;
            } else {
                if(n<2)return E_CHAIN;
                sectors[1]=clusLba(n);
                if(next(n,b,cached)!=0)return E_CHAIN;
            }
            return OK;
        }
    }
    return E_CHAIN;
}
}
