// SPDX-License-Identifier: MIT
// FAT allocation and verified metadata updates, adapted from CHSave.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Oz", "ipa-sra")
#endif
#include "FatWrite.h"
#include "FatInternal.h"
#include "SdWrite.h"

namespace fat {
using namespace detail;

namespace {
#if defined(__GNUC__)
__attribute__((noinline))
#endif
uint32_t checksum(const uint8_t *p) {
    uint32_t crc = ~0u;
    for (unsigned n = 0; n < 512; ++n) {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return crc;
}

class Allocator {
    uint8_t *b;
    uint32_t stride, copies, hints[2] = {}, cached = ~0u;
    static void put16(uint8_t *p, uint32_t n) {
#if defined(__GNUC__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        uint16_t value = n; memcpy(__builtin_assume_aligned(p, 2), &value, 2);
#else
        p[0] = n; p[1] = n >> 8;
#endif
    }
    static void put32(uint8_t *p, uint32_t n) {
#if defined(__GNUC__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        memcpy(__builtin_assume_aligned(p, 4), &n, 4);
#else
        put16(p, n); put16(p + 2, n >> 16);
#endif
    }
    bool read(uint32_t sector) {
        if (cached == sector) return true;
        cached = ~0u;
        if (!sd::read(sector, b)) return false;
        cached = sector;
        return true;
    }
    bool write(uint32_t sector) {
        uint32_t expected = checksum(b);
        cached = ~0u;
        return sd::write(sector, b) && read(sector) && checksum(b) == expected;
    }
    int32_t get(uint32_t c) { return entry(c, b, cached); }
    bool set(uint32_t c, uint32_t value) {
        const uint32_t offset = entryOffset(c);
        const uint32_t first = v.fat + (offset >> 9);
        uint8_t *entry = b + (offset & 511);
        if (!read(first)) return false;
        const uint32_t previous = fatEntry(entry);
        // Check all copies before changing any of them.
        for (unsigned copy = 1; copy < copies; ++copy) {
            if (!read(first + copy * stride) || fatEntry(entry) != previous) return false;
        }
        for (unsigned copy = 0, sector = first; copy < copies; ++copy, sector += stride) {
            if (!read(sector)) return false;
            if (v.rootSecs) put16(entry, value);
            else put32(entry, (u32at(entry) & 0xf0000000u) | value);
            if (!write(sector)) return false;
        }
        return true;
    }
    int8_t invalidateHints() {
        for (uint32_t &sector : hints) {
            if (!sector) continue;
            if (!read(sector)) return E_READ;
            if (u32at(b) == 0x41615252u && u32at(b + 484) == 0x61417272u &&
                u32at(b + 508) == 0xaa550000u) {
                put32(b + 488, ~0u); put32(b + 492, ~0u);
                if (!write(sector)) return E_WRITE;
            }
            sector = 0;
        }
        return OK;
    }
    // Keep one scanner for both passes of the general allocator.
#if defined(__GNUC__)
    __attribute__((noinline))
#endif
    int32_t findFree(uint32_t c) {
        for (; valid(c); ++c) {
            int32_t n = get(c);
            if (n < 0) return n;
            if (!n) return c;
        }
        return E_NOSPACE;
    }
    int8_t allocateSmall(unsigned count, uint32_t *clusters) {
        // Cache the free clusters so the file writer can skip the FAT walk.
        uint32_t *out = clusters;
        uint32_t *end = clusters + count;
        for (uint32_t c = 2; out != end && valid(c); ++c) {
            int32_t n = get(c);
            if (n < 0) return n;
            if (!n) *out++ = c;
        }
        if (out != end) return E_NOSPACE;
        int8_t rc = invalidateHints();
        if (rc) return rc;
        for (uint32_t link = 0x0fffffffu; out != clusters;) {
            uint32_t cluster = *--out;
            if (!set(cluster, link)) return E_WRITE;
            link = cluster;
        }
        return OK;
    }
    int32_t allocate(unsigned count) {
        // Preflight available space, then link only enough free clusters.
        // The chain is not visible in a directory until initialized.
        int32_t first = findFree(2);
        if (first < 0) return first;
        int32_t c = first;
        for (unsigned needed = 1; needed < count; ++needed) {
            c = findFree(c + 1);
            if (c < 0) return c;
        }
        int8_t rc = invalidateHints();
        if (rc) return rc;
        c = first;
        while (--count) {
            int32_t next = findFree(c + 1);
            if (next < 0) return next == E_NOSPACE ? E_WRITE : next;
            if (!set(c, next)) return E_WRITE;
            c = next;
        }
        return set(c, 0x0fffffffu) ? first : E_WRITE;
    }
    template<bool Compact>
    int8_t allocateDirectoryCluster(uint32_t &cluster) {
        // Use the same allocator for directories and files in each profile.
        if (Compact) return allocateSmall(1, &cluster);
        int32_t first = allocate(1);
        if (first < 0) return first;
        cluster = first;
        return OK;
    }
    template<bool Compact>
#if defined(__GNUC__) && !defined(__clang__)
    __attribute__((noinline, noclone))
#endif
    int8_t reserve(uint32_t parent, uint32_t &sector, unsigned &offset) {
        const bool fixedRoot = !parent;
        unsigned remaining = fixedRoot ? v.rootSecs : 0;
        uint32_t nextSector = v.root;
        uint32_t cluster = 0, link = parent;
        for (unsigned guard = 4096; guard--;) {
            if (!remaining) {
                if (fixedRoot) return E_NOSPACE;
                if (cluster && endOfChain(link)) {
                    uint32_t added;
                    int8_t rc = allocateDirectoryCluster<Compact>(added);
                    if (rc) return rc;
                    if (!initializeDirectory(added, false) || !set(cluster, added)) return E_WRITE;
                    link = added;
                }
                cluster = link;
                if (!valid(cluster)) return E_CHAIN;
                int32_t next = get(cluster);
                if (next < 0) return next;
                link = next;
                if (!endOfChain(link) && !valid(link)) return E_CHAIN;
                nextSector = clusLba(cluster);
                remaining = 1u << v.shift;
            }
            sector = nextSector++;
            --remaining;
            if (!read(sector)) return E_READ;
            for (offset = 0; offset < 512; offset += 32) {
                uint8_t first = b[offset];
                if (first == 0xe5) return OK;
                if (first) continue;
                // The next entry must terminate the directory before this
                // end marker is consumed by publishing the new entry.
                uint32_t following = sector;
                unsigned endOffset = offset + 32;
                if (endOffset == 512) {
                    if (remaining) following = nextSector;
                    else if (!fixedRoot && !endOfChain(link)) following = clusLba(link);
                    else return OK;
                    if (!read(following)) return E_READ;
                    endOffset = 0;
                }
                if (b[endOffset]) {
                    b[endOffset] = 0;
                    if (!write(following)) return E_WRITE;
                }
                return OK;
            }
        }
        return E_CHAIN;
    }
    void fillEntry(uint8_t *entry, const char *name, uint8_t attr, uint32_t cluster, uint32_t bytes) {
        memset(entry, 0, 32);
        memcpy(entry, name, 11);
        entry[11] = attr;
        put16(entry + 16, 0x21); put16(entry + 18, 0x21); put16(entry + 24, 0x21); // 1980-01-01
        put16(entry + 20, cluster >> 16);
        put16(entry + 26, cluster); put32(entry + 28, bytes);
    }
    bool initializeDirectory(uint32_t cluster, bool firstCluster) {
        uint32_t sector = clusLba(cluster);
        const uint32_t end = sector + (1u << v.shift);
        for (; sector < end; ++sector) {
            memset(b, 0, 512);
            if (firstCluster) {
                fillEntry(b, ".          ", 0x10, cluster, 0);
                fillEntry(b + 32, "..         ", 0x10, 0, 0);
                firstCluster = false;
            }
            if (!write(sector)) return false;
        }
        return true;
    }
    int8_t publish(uint32_t sector, unsigned offset, const char *name, uint8_t attr, uint32_t cluster, uint32_t bytes) {
        if (!read(sector)) return E_READ;
        fillEntry(b + offset, name, attr, cluster, bytes);
        // Publish only after the contents and FAT copies have been verified.
        b[offset] = 0xe5;
        if (!write(sector)) return E_WRITE;
        b[offset] = name[0];
        return write(sector) ? OK : E_WRITE;
    }
    template<bool Compact>
    int8_t makeDirectory(const char *name, uint32_t &parent) {
        uint32_t sector; unsigned offset;
        int8_t status = reserve<Compact>(v.rootSecs ? 0 : v.root, sector, offset);
        if (status != OK) return status;
        status = allocateDirectoryCluster<Compact>(parent);
        if (status != OK) return status;
        if (!initializeDirectory(parent, true)) return E_WRITE;
        return publish(sector, offset, name, 0x10, parent, 0);
    }
public:
    explicit Allocator(uint8_t *scratch) : b(scratch), copies(b[16]) {
        // Capture the write geometry while the buffer still holds the BPB.
        stride = (v.data - v.fat - v.rootSecs) / copies;
        if (!v.rootSecs) {
            uint32_t reserved = u16at(b + 14), base = v.fat - reserved;
            uint32_t primary = u16at(b + 48), backup = u16at(b + 50);
            if (primary && primary < reserved) {
                hints[0] = base + primary;
                if (backup && backup + primary < reserved) hints[1] = hints[0] + backup;
            }
        }
    }
    template<unsigned CachedClusters>
    int8_t create(const char *folder, const char *name, uint32_t blocks,
                  InitSector init, void *context) {
        constexpr bool Compact = CachedClusters != 0;
        File dir{v.rootSecs ? 0 : v.root, 0}, f;
        int8_t rc;
        if (folder) {
            rc = findExact(dir, folder, true, f, b);
            cached = ~0u;
            if (rc == E_NOTFOUND) {
                if ((rc = makeDirectory<Compact>(folder, dir.cluster))) return rc;
            } else {
                if (rc) return rc;
                dir = f;
            }
        }
        rc = findExact(dir, name, false, f, b);
        cached = ~0u;
        if (rc != E_NOTFOUND) return rc ? rc : E_EXISTS;
        uint32_t dirSector; unsigned dirOffset;
        if ((rc = reserve<Compact>(dir.cluster, dirSector, dirOffset))) return rc;
        const uint32_t mask = (1u << v.shift) - 1;
        const unsigned count = (blocks + mask) >> v.shift;
        uint32_t clusters[Compact ? CachedClusters : 1], head;
        if (Compact) {
            if ((rc = allocateSmall(count, clusters))) return rc;
            head = clusters[0];
        } else {
            int32_t first = allocate(count);
            if (first < 0) return first;
            head = first;
        }
        uint32_t cluster = head;
        for (uint32_t i = 0; i < blocks; ++i) {
            if (Compact) {
                cluster = clusters[i >> v.shift];
            } else if (i && !(i & mask)) {
                int32_t n = get(cluster);
                if (n < 0) return n;
                if (!valid(n)) return E_CHAIN;
                cluster = n;
            }
            memset(b, 0, 512);
            if (init) init(i, b, context);
            if (!write(clusLba(cluster) + (i & mask))) return E_WRITE;
        }
        return publish(dirSector, dirOffset, name, 0x20, head, blocks * 512u);
    }
};

template<bool Compact>
bool validName(const char *name) {
    if (!name || *name == ' ') return false;
    for (unsigned i = 0; i < 11; ++i) {
        char c = name[i];
        if (!(c == ' ' || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              (!Compact && c && strchr("!#$%&'()-@^_`{}~", c)))) return false;
    }
    return true;
}
}

int8_t createFile(const char *folder, const char name[11], uint32_t blocks,
                  InitSector init, void *context, uint8_t *scratch) {
    if (!validName<false>(name) || (folder && !validName<false>(folder)) ||
        !blocks || blocks > UINT32_MAX / 512) return E_CHAIN;
    int8_t rc = mountWritable(scratch);
    return rc ? rc : Allocator(scratch).create<0>(folder, name, blocks, init, context);
}

int8_t createThreeBlockFile(const char *folder, const char name[11],
                           InitSector init, void *context, uint8_t *scratch) {
    if (!validName<true>(name) || !validName<true>(folder)) return E_CHAIN;
    int8_t rc = mountWritable(scratch);
    return rc ? rc : Allocator(scratch).create<3>(folder, name, 3, init, context);
}

int8_t createSmallFile(const char *folder, const char name[11], uint32_t blocks,
                       InitSector init, void *context, uint8_t *scratch) {
    if (!blocks || blocks > 5 || !validName<true>(name) || !validName<true>(folder))
        return E_CHAIN;
    int8_t rc = mountWritable(scratch);
    return rc ? rc : Allocator(scratch).create<5>(folder, name, blocks, init, context);
}
}  // namespace fat
