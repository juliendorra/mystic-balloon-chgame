#pragma once
#include <stdint.h>
// Each game owns <four-character game ID>.SAV on SD. Provision with
// tools/sd_saves.py prepare --card PATH. Calls borrow CHGfx's idle scratch;
// read() uses scratch bytes 512..1023 and buffer() uses 0..511. A read
// pointer survives buffer(), as SDK games need, until write() or a flush. Flash is read-only.
namespace save {
static const uint16_t MAX_DATA=244;
enum Status : uint8_t { NoCard, NeedsSetup, Ready, Damaged, Failed };
Status status();
bool available();
constexpr uint32_t magic(const char (&s)[5]) {
    return uint32_t(uint8_t(s[0])) | uint32_t(uint8_t(s[1]))<<8 |
           uint32_t(uint8_t(s[2]))<<16 | uint32_t(uint8_t(s[3]))<<24;
}
__attribute__((cold)) const void *read(uint32_t magic,uint8_t version,uint16_t size,uint8_t *flag=nullptr);
// Read-only compatibility source for games with their own SD journal.
__attribute__((cold)) const void *readLegacy(uint32_t magic,uint8_t version,uint16_t size,uint8_t *flag=nullptr);
__attribute__((cold)) void *buffer();
__attribute__((cold)) bool write(uint32_t magic,uint8_t version,uint16_t size,uint8_t flag=0);
template<class T> bool load(uint32_t magic,uint8_t version,T &data,uint8_t *flag=nullptr) {
    static_assert(sizeof(T)<=MAX_DATA,"save payload too large");
    const T *p=static_cast<const T *>(read(magic,version,sizeof(T),flag));
    if(!p)return false;data=*p;return true;
}
template<class T> bool store(uint32_t magic,uint8_t version,const T &data,uint8_t flag=0) {
    static_assert(sizeof(T)<=MAX_DATA,"save payload too large");
    *static_cast<T *>(buffer())=data;return write(magic,version,sizeof(T),flag);
}
}
