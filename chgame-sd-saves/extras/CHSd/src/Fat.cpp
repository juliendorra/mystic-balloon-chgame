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
#pragma GCC optimize("Oz", "ipa-sra")
#endif
#include "FatAccess.h"
#include "FatInternal.h"
#include "FatWrite.h"
#include "SdSpi.h"

namespace fat {

namespace detail {
Volume v;

int32_t entry(uint32_t c, uint8_t *b, uint32_t &cached) {
    uint32_t off = entryOffset(c), sec = v.fat + (off >> 9);
    if (sec != cached) {
        cached = ~0u;
        if (!sd::read(sec, b)) return E_READ;
        cached = sec;
    }
    return fatEntry(b + (off & 511));
}

// Next cluster, 0 for end-of-chain, 1 for an invalid link, or E_READ.
int32_t next(uint32_t c, uint8_t *b, uint32_t &cached) {
    if (!valid(c)) return 1;
    int32_t n = entry(c, b, cached);
    if (n < 0) return n;
    if (endOfChain(n)) return 0;
    return valid(n) ? n : 1;
}
}  // namespace detail

using namespace detail;
namespace {

// Validate the BPB and keep the geometry used for both reads and writes.
template<bool Writable>
bool bpb(const uint8_t *b, uint32_t base, uint32_t limit) {
    uint32_t spc = b[13], sh = 0, nf = b[16];
    while ((1u << sh) < spc) sh++;
    uint32_t rsvd = u16at(b + 14), rootSecs = (le16(b + 17) + 15) >> 4;
    uint32_t tot = le16(b + 19), fsz = u16at(b + 22);
    if (!tot) tot = u32at(b + 32);
    if (!fsz) fsz = u32at(b + 36);
    if (le16(b + 11) != 512 || (1u << sh) != spc || !rsvd || !nf || !fsz) return false;
    if (nf > 4 || fsz >= (1u << 24)) return false;  // keeps nf * fsz and fsz * 256 in range
    uint32_t meta = rsvd + nf * fsz + rootSecs;
    if (meta >= tot || tot > limit || base > UINT32_MAX - tot) return false;
    uint32_t n = (tot - meta) >> sh;
    if (n < 4085 || (rootSecs ? n >= 65525 : n < 65525) || n >= 0x0ffffff5u) return false;
    if (fsz * (rootSecs ? 256u : 128u) < n + 2) return false;
    uint32_t root = rootSecs ? base + rsvd + nf * fsz : u32at(b + 44), active = 0;
    if (!rootSecs) {
        if (root < 2 || root >= n + 2 || root >= 0x0ffffff0u) return false;
        if (!Writable && (b[40] & 0x80)) {
            active = b[40] & 15;
            if (active >= nf) return false;
        }
    }
    v.fat = base + rsvd + active * fsz;
    v.data = base + meta;
    v.root = root;
    v.rootSecs = rootSecs;
    v.shift = sh;
    v.end = n + 2;
    return true;
}

// Finds an 11-byte short name ('?' = any character) in directory `dir` (a
// cluster, or 0 for the FAT16 root region), passing over the first `skip`
// that fit. `want` is the entry's attributes masked with ATTR_TEST: 0 for a
// plain file, ATTR_DIR for a folder; so deleted entries aside, volume
// labels, long-name parts and hidden entries never match, and neither does
// a file for a folder or the other way round, all in one test. A directory
// holds at most 65,536 entries (4,096 sectors), which bounds the walk when
// a chain loops. b is shared by the directory sectors and the FAT lookups,
// so nothing is cached here.
const uint32_t ATTR_DIR = 0x10, ATTR_TEST = ATTR_DIR | 0x08 | 0x02;   // folder, label, hidden

int32_t lookup(uint32_t dir, const uint8_t *name, File &f, uint8_t *b, uint32_t want, uint32_t skip,
               char *nameOut, bool exact = false) {
    uint32_t sec = 0, cached = ~0u;
    int32_t link = 0;
    for (uint32_t guard = 4096; guard--;) {
        uint32_t lba;
        if (!dir) {
            if (sec == v.rootSecs) return E_NOTFOUND;   // end of the FAT16 root region
            lba = v.root + sec;
        } else {
            if (sec >> v.shift) {                      // this cluster is done
                int32_t n = exact ? link : next(dir, b, cached);
                if (n < 2) return n < 0 ? n : (int32_t)E_NOTFOUND;
                dir = (uint32_t)n;
                sec = 0;
            }
            if (!valid(dir)) return exact ? E_CHAIN : E_NOTFOUND;
            if (exact && !sec) {
                link = next(dir, b, cached);
                if (link < 0) return link;
                if (link == 1) return E_CHAIN;
            }
            lba = clusLba(dir) + sec;
        }
        sec++;
        if (!sd::read(lba, b)) return E_READ;
        cached = ~0u;
        for (const uint8_t *d = b; d < b + 512; d += 32) {
            if (!d[0]) return E_NOTFOUND;                   // end-of-directory mark
            if (d[0] == 0xE5 || (!exact && (d[11] & ATTR_TEST) != want)) continue;   // deleted, or not the kind wanted
            if (exact) {
                if (memcmp(d, name, 11)) continue;
            } else {
                uint32_t i = 0;
                while (i < 11 && (d[i] == name[i] || name[i] == '?')) i++;
                if (i < 11) continue;
            }
            if (exact && (d[11] & 0x1f) != want) return E_CHAIN;
            if (skip) { skip--; continue; }
            if (nameOut) for (uint32_t i = 0; i < 11; i++) nameOut[i] = (char)d[i];
            // The high cluster word only exists on FAT32 (a fixed root
            // region means FAT16); FAT16 ignores it, as the PC tools do.
            f.cluster = u16at(d + 26) | (v.rootSecs ? 0u : u16at(d + 20) << 16);
            f.size = u32at(d + 28);
            return exact && !valid(f.cluster) ? E_CHAIN : OK;
        }
    }
    return exact ? E_CHAIN : E_NOTFOUND;
}

template<bool Writable>
int8_t mountVolume(uint8_t *b) {
    v.end = 0;
    uint32_t base = 0, limit = UINT32_MAX;
    for (uint32_t pass = 0;; pass++) {
        if (!sd::read(base, b)) return E_READ;
        if (u16at(b + 510) != 0xAA55) return E_NOFS;
        if (le32(b + 3) == 0x41465845u) return E_EXFAT;       // OEM name "EXFAT   "
        if ((b[0] == 0xEB || b[0] == 0xE9) && bpb<Writable>(b, base, limit)) {
            if (Writable && !v.rootSecs && (b[40] & 0x80)) {
                v.end = 0;
                return E_NOFS;
            }
            return OK;
        }
        if (pass) return E_NOFS;
        // LBA 0 is not a usable boot sector: read it as an MBR and take the
        // first FAT partition. With none, a type 07 one (exFAT, or NTFS)
        // gets its own error: the card needs reformatting as FAT32.
        bool other = false;
        for (const uint8_t *p = b + 446; p < b + 510; p += 16) {
            uint32_t t = p[4];
            if (t < 16 && (0x5852u >> t & 1)) { base = le32(p + 8); limit = le32(p + 12); break; }   // 01 04 06 0B 0C 0E
            other |= t == 0x07;
        }
        if (!base) return other ? E_EXFAT : E_NOFS;
    }
}
}  // namespace

int8_t mount(uint8_t *b) { return mountVolume<false>(b); }
int8_t mountWritable(uint8_t *b) { return mountVolume<true>(b); }

int8_t findExact(const File &dir, const char name[11], bool folder, File &f, uint8_t *b) {
    if (!v.end) return E_NOTFOUND;
    return lookup(dir.cluster, (const uint8_t *)name, f, b, folder ? ATTR_DIR : 0, 0, nullptr, true);
}

int8_t find(const char *name, File &f, uint8_t *b) {
    if (!v.end) return E_NOTFOUND;
    return (int8_t)lookup(v.rootSecs ? 0 : v.root, (const uint8_t *)name, f, b, 0, 0, nullptr);
}

int8_t folder(const char *name, File &dir, uint8_t *b) {
    if (!v.end) return E_NOTFOUND;
    int32_t rc = lookup(v.rootSecs ? 0 : v.root, (const uint8_t *)name, dir, b, ATTR_DIR, 0, nullptr);
    // (A folder's first cluster is never 0: that would be the root.)
    if (!rc && dir.cluster < 2) rc = E_NOTFOUND;
    return (int8_t)rc;
}

int8_t match(const File &dir, const char *pattern, uint8_t skip, File &f, char *nameOut, uint8_t *b) {
    if (!v.end) return E_NOTFOUND;
    return (int8_t)lookup(dir.cluster, (const uint8_t *)pattern, f, b, 0, skip, nameOut);
}

int8_t root(File &dir) {
    dir.cluster = v.rootSecs ? 0 : v.root;
    dir.size = 0;
    return v.end ? (int8_t)OK : (int8_t)E_NOTFOUND;
}

// lookup()'s walk, handing every visible entry to fn. Kept apart from it so
// the games, which never list, keep their code as it was.
int8_t list(const File &d, ListFn fn, void *ctx, uint8_t *b) {
    if (!v.end) return E_NOTFOUND;
    uint32_t dir = d.cluster, sec = 0;
    for (uint32_t guard = 4096; guard--;) {
        uint32_t lba;
        if (!dir) {
            if (sec == v.rootSecs) break;
            lba = v.root + sec;
        } else {
            if (sec >> v.shift) {
                uint32_t none = ~0u;
                int32_t n = next(dir, b, none);
                if (n < 2) return n < 0 ? (int8_t)n : (int8_t)OK;
                dir = (uint32_t)n;
                sec = 0;
            }
            if (!valid(dir)) return E_CHAIN;
            lba = clusLba(dir) + sec;
        }
        sec++;
        if (!sd::read(lba, b)) return E_READ;
        for (const uint8_t *e = b; e < b + 512; e += 32) {
            if (!e[0]) return OK;
            if (e[0] == 0xE5 || e[0] == '.' || (e[11] & (ATTR_TEST & ~ATTR_DIR))) continue;
            File f;
            f.cluster = u16at(e + 26) | (v.rootSecs ? 0u : u16at(e + 20) << 16);
            f.size = u32at(e + 28);
            if (!fn((const char *)e, f, (e[11] & ATTR_DIR) != 0, ctx)) return OK;
        }
    }
    return OK;
}

namespace {
inline uint32_t blocks(const File &f) { return (f.size >> 9) + ((f.size & 511) != 0); }

template<class Visit>
int8_t walk(uint32_t c, uint32_t left, uint8_t *b, Visit visit) {
    uint32_t cached = ~0u;
    const uint32_t spc = 1u << v.shift;
    while (left) {
        if (!valid(c)) return E_CHAIN;
        uint32_t k = left < spc ? left : spc;
        if (!visit(clusLba(c), k)) return E_FRAG;
        left -= k;
        int32_t nx = next(c, b, cached);
        if (nx < 0) return nx;
        if (!left) return nx ? E_CHAIN : OK;
        c = (uint32_t)nx;
    }
    return OK;
}

int8_t mapBlocks(uint32_t cluster, uint32_t count, uint32_t *lbas, uint8_t *b) {
    return walk(cluster, count, b, [&](uint32_t lba, uint32_t n) {
        while (n--) *lbas++ = lba++;
        return true;
    });
}
}

int8_t runs(const File &f, Run *out, uint8_t maxRuns, uint8_t *b) {
    if (maxRuns > 127) maxRuns = 127;
    uint32_t n = 0;
    int8_t rc = walk(f.cluster, blocks(f), b, [&](uint32_t lba, uint32_t k) {
        Run *r = out + n;
        if (n && r[-1].lba + r[-1].blocks == lba) r[-1].blocks += k;
        else {
            if (n == maxRuns) return false;
            *r = {lba, k};
            ++n;
        }
        return true;
    });
    return rc ? rc : (int8_t)n;
}

int8_t validate(const File &f, uint8_t *b) {
    return walk(f.cluster, blocks(f), b, [](uint32_t, uint32_t) { return true; });
}

int8_t map(const File &f, uint32_t *lbas, uint32_t capacity, uint8_t *b) {
    uint32_t count = blocks(f);
    if (count > capacity) return E_FRAG;
    return mapBlocks(f.cluster, count, lbas, b);
}

int8_t mapThreeBlocks(const File &f, uint32_t (&lbas)[3], uint8_t *b) {
    if (f.size != 3 * 512u) return E_CHAIN;
    return mapBlocks(f.cluster, 3, lbas, b);
}

int8_t sector(const File &f, uint32_t index, uint32_t &lba, uint8_t *b) {
    if (index >= blocks(f)) return E_CHAIN;
    uint32_t c = f.cluster, cached = ~0u;
    for (uint32_t skip = index >> v.shift; skip; --skip) {
        int32_t n = next(c, b, cached);
        if (n < 2) return n < 0 ? n : (int32_t)E_CHAIN;
        c = n;
    }
    if (!valid(c)) return E_CHAIN;
    lba = clusLba(c) + (index & ((1u << v.shift) - 1));
    return OK;
}

// (Passing the reason for a failure on would cost some 40 B of flash in
// every game that only needs to know whether the file is there.)
uint8_t open(const char *name, Run *out, uint8_t maxRuns, uint8_t *b) {
    File f;
    if (!sd::init() || mount(b) || find(name, f, b)) return 0;
    int8_t n = runs(f, out, maxRuns, b);
    return n > 0 ? (uint8_t)n : 0;
}

bool read(const Run *r, uint32_t n, uint32_t k, uint8_t *dst) {
    for (; n; n--, r++) {
        if (k < r->blocks) return sd::read(r->lba + k, dst);
        k -= r->blocks;
    }
    return false;
}

}  // namespace fat
