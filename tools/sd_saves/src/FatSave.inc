// Exact root lookups for provisioned journals. No allocation or FAT writes.
namespace fat {
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
            unsigned index=0;cached=~0u;
            while(index<count) {
                if(c<2 || c>=v.end)return E_CHAIN;
                lba=clusLba(c);
                for(unsigned i=0;i<(1u<<v.shift) && index<count;++i)sectors[index++]=lba+i;
                int32_t n=next(c,b,cached);if(n<0)return n;
                if(index==count)return n==0?OK:E_CHAIN;
                if(n<2)return E_CHAIN;
                c=n;
            }
            return E_CHAIN;
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
int8_t saveNamespacedFile(const char *base,uint32_t sectors[3],uint8_t *b) {
    char name[11];for(unsigned i=0;i<8;++i)name[i]=base[i];
    name[8]='S';name[9]='A';name[10]='V';
    return saveExact(name,3,sectors,b);
}
}
