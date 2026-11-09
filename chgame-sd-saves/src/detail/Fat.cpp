// SPDX-License-Identifier: MIT
// Modified from CHSd (Copyright 2026 bateske): save-only lookup, private
// namespace, byte-wise decoding, and stricter volume/chain validation.
// Fat.cpp - see Fat.h. From HypeRunner's src/sd/Fat.cpp (MIT): clean-room
// from the Microsoft FAT specification and the MBR layout, by way of the
// cuts CHWords and CHCrossword made of it. Files are looked for in the root
// directory or in a folder of it. This file runs on the PC as well: the
// simulator's card and the host tests go through it, so what is tested is
// what the board runs.
//
// Size notes (-Os, RV32): word-sized locals avoid the masking and sign
// extension that uint8_t/int8_t locals cost. Fields at odd offsets (some BPB
// ones) are read with byte loads; aligned ones straight from the 4-byte
// aligned buffer (little-endian, like FAT).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Oz")
#endif
#include "Fat.h"
#include "Block.h"
#include "../Config.h"

namespace chsave { namespace fat {

namespace {

struct Vol {
    uint32_t fat;            // LBA of the first FAT
    uint32_t data;           // LBA of cluster 2
    uint32_t root;           // FAT16: LBA of the fixed root region; FAT32: root cluster
    uint32_t end;            // one past the highest cluster number; 0 = not mounted
    uint32_t rootSecs;       // FAT16 root region sectors; 0 means FAT32
    uint32_t shift;          // log2(sectors per cluster)
};
Vol v;

uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }
inline uint32_t u16at(const uint8_t *p) { return le16(p); }   // p 2-byte aligned
inline uint32_t u32at(const uint8_t *p) { return le32(p); }   // p 4-byte aligned

uint32_t clusLba(uint32_t c) { return v.data + ((c - 2) << v.shift); }

// FAT entry of cluster c: the next cluster (>= 2), 0 at the end of the
// chain, 1 for a free, bad or out-of-range link, or a negative sd error.
// `cached` names the FAT sector already in b, so a chain walk reads each
// FAT sector once.
int32_t next(uint32_t c, uint8_t *b, uint32_t &cached) {
    const bool f32 = !v.rootSecs;
    uint32_t off = c << (f32 ? 2 : 1), sec = v.fat + (off >> 9);
    if (sec != cached) {
        if (!block::read(sec, b)) return E_READ;
        cached = sec;
    }
    const uint8_t *p = b + (off & 511);
    uint32_t n = f32 ? u32at(p) & 0x0FFFFFFFu : u16at(p);
    if (n >= (f32 ? 0x0FFFFFF8u : 0xFFF8u)) return 0;
    return n >= 2 && n < v.end ? (int32_t)n : 1;
}

// Validates the BPB in b (the volume starts at LBA base) and fills v. Only
// the first FAT is read: mirroring is assumed, as every formatter sets it.
bool bpb(const uint8_t *b, uint32_t base) {
    uint32_t spc = b[13], sh = 0, nf = b[16];
    while ((1u << sh) < spc) sh++;
    uint32_t rsvd = u16at(b + 14), rootSecs = (le16(b + 17) + 15) >> 4;
    uint32_t tot = le16(b + 19), fsz = u16at(b + 22);
    if (!tot) tot = u32at(b + 32);
    if (!fsz) fsz = u32at(b + 36);
    if (spc > 128 || le16(b + 11) != 512 || (1u << sh) != spc || !rsvd || !nf || !fsz) return false;
    if (nf > 4 || fsz >= (1u << 24)) return false;  // keeps nf * fsz and fsz * 256 in range
    uint32_t meta = rsvd + nf * fsz + rootSecs;
    if (meta >= tot || base > UINT32_MAX - tot) return false;
    // FAT12 (under 4,085 clusters) is refused. FAT32 is told by its empty
    // fixed root region, which on a valid volume agrees with the spec's
    // cluster-count rule (65,525 and up); a volume where they disagree is
    // malformed, and what it yields fails the chain and pack checks.
    uint32_t n = (tot - meta) >> sh;                 // clusters
    if (n < 4085 || (rootSecs ? n >= 65525 : n < 65525) || n >= 0x0ffffff5u) return false;
    if (!rootSecs && (le16(b + 40) & 0x80)) return false; // non-mirrored FAT32 not supported
    // The FAT must hold an entry per cluster (else a chain reads past it),
    // and a FAT32 root must be a real cluster.
    if (fsz * (rootSecs ? 256u : 128u) < n + 2) return false;
    if (!rootSecs && (u32at(b + 44) < 2 || u32at(b + 44) >= n + 2)) return false;
    v.fat = base + rsvd;
    v.data = base + meta;
    v.root = rootSecs ? v.fat + nf * fsz : u32at(b + 44);
    v.rootSecs = rootSecs;
    v.shift = sh;
    v.end = n + 2;
    return true;
}

} // namespace

int8_t mount(uint8_t *b) {
    v.end = 0;
    uint32_t base = 0;
    for (uint32_t pass = 0;; pass++) {
        if (!block::read(base, b)) return E_READ;
        if (u16at(b + 510) != 0xAA55) return E_NOFS;
        if (le32(b + 3) == 0x41465845u) return E_EXFAT;       // OEM name "EXFAT   "
        if ((b[0] == 0xEB || b[0] == 0xE9) && bpb(b, base)) return OK;
        if (pass) return E_NOFS;
        // LBA 0 is not a usable boot sector: read it as an MBR and take the
        // first FAT partition. With none, a type 07 one (exFAT, or NTFS)
        // gets its own error: the card needs reformatting as FAT32.
        bool other = false;
        for (const uint8_t *p = b + 446; p < b + 510; p += 16) {
            uint32_t t = p[4];
            if (t < 16 && (0x5852u >> t & 1)) { base = le32(p + 8); break; }   // 01 04 06 0B 0C 0E
            other |= t == 0x07;
        }
        if (!base) return other ? E_EXFAT : E_NOFS;
    }
}

int8_t saveFile(uint32_t magic, uint32_t &cluster, uint16_t &slotSectors, uint8_t *b) {
    if (!v.end) return E_NOFS;
    uint32_t dir = v.rootSecs ? 0 : v.root, sec = 0;
    for (unsigned guard = 4096; guard--;) {
        uint32_t lba;
        if (!dir) {
            if (sec == v.rootSecs) return E_NOTFOUND;
            lba = v.root + sec;
        } else {
            if (sec >> v.shift) {
                uint32_t cached = ~0u;
                int32_t n = next(dir, b, cached);
                if (n < 2) return n < 0 ? n : int32_t(n ? E_CHAIN : E_NOTFOUND);
                dir = n;
                sec = 0;
            }
            lba = clusLba(dir) + sec;
        }
        ++sec;
        if (!block::read(lba, b)) return E_READ;
        for (unsigned off = 0; off < 512; off += 32) {
            const uint8_t *d = b + off;
            if (!d[0]) return E_NOTFOUND;
            if (u32at(d) != magic || le32(d + 4) != 0x20202020u ||
                d[8] != 'S' || d[9] != 'A' || d[10] != 'V') continue;
            uint32_t bytes = u32at(d + 28);
            if ((d[11] & 0x1f) || bytes < 1024 || bytes > CHSAVE_MAX_SLOT_SECTORS * 1024u || bytes % 1024) return E_CHAIN;
            uint32_t c = u16at(d + 26) | (v.rootSecs ? 0 : u16at(d + 20) << 16);
            cluster = c;
            slotSectors = bytes / 1024;
            unsigned clusters = ((bytes / 512) + (1u << v.shift) - 1) >> v.shift;
            uint32_t cached = ~0u;
            while (clusters--) {
                if (c < 2 || c >= v.end) return E_CHAIN;
                int32_t n = next(c, b, cached);
                if (n < 0) return n;
                if (!clusters) return n == 0 ? OK : E_CHAIN;
                if (n < 2) return E_CHAIN;
                c = n;
            }
            return E_CHAIN;
        }
    }
    return E_CHAIN;
}

int8_t sector(uint32_t cluster, unsigned fileSector, uint32_t &lba, uint8_t *b) {
    uint32_t cached = ~0u;
    for (unsigned skip = fileSector >> v.shift; skip; --skip) {
        int32_t n = next(cluster, b, cached);
        if (n < 2) return n < 0 ? n : int32_t(E_CHAIN);
        cluster = n;
    }
    if (cluster < 2 || cluster >= v.end) return E_CHAIN;
    lba = clusLba(cluster) + (fileSector & ((1u << v.shift) - 1));
    return OK;
}
} } // namespace chsave::fat
