// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <string.h>

namespace fat { namespace detail {
struct Volume {
    uint32_t fat;      // first FAT (or active FAT when mirroring is disabled)
    uint32_t data;     // LBA of cluster 2
    uint32_t root;     // FAT16 root LBA or FAT32 root cluster
    uint32_t end;      // exclusive cluster bound; 0 means not mounted
    uint32_t rootSecs; // FAT16 root length; 0 means FAT32
    uint32_t shift;    // log2(sectors per cluster)
};
extern Volume v;
inline uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
inline uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }
#if defined(__GNUC__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
inline uint32_t u16at(const uint8_t *p) {
    uint16_t n; memcpy(&n, __builtin_assume_aligned(p, 2), 2); return n;
}
inline uint32_t u32at(const uint8_t *p) {
    uint32_t n; memcpy(&n, __builtin_assume_aligned(p, 4), 4); return n;
}
#else
inline uint32_t u16at(const uint8_t *p) { return le16(p); }
inline uint32_t u32at(const uint8_t *p) { return le32(p); }
#endif
inline bool valid(uint32_t c) {
    return c >= 2 && c < v.end && c < (v.rootSecs ? 0xfff0u : 0x0ffffff0u);
}
inline uint32_t clusLba(uint32_t c) { return v.data + ((c - 2) << v.shift); }
inline bool endOfChain(uint32_t value) { return value >= (v.rootSecs ? 0xfff8u : 0x0ffffff8u); }
inline uint32_t entryOffset(uint32_t c) { return c << (v.rootSecs ? 1 : 2); }
inline uint32_t fatEntry(const uint8_t *p) { return v.rootSecs ? u16at(p) : u32at(p) & 0x0fffffffu; }
// Read a FAT entry whose cluster has already passed valid().
int32_t entry(uint32_t c, uint8_t *b, uint32_t &cached);
int32_t next(uint32_t c, uint8_t *b, uint32_t &cached);
} }
