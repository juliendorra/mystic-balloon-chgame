// SPDX-License-Identifier: MIT
#pragma once
#include "../chsave/Journal.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("Oz", "ipa-sra")
#endif

namespace chsave { namespace detail {
constexpr char SAVE_DIRECTORY[] = "SAVES      ";

inline void journalName(char (&name)[11], const char *basename) {
    memcpy(name, basename, 8);
    memcpy(name + 8, "SAV", 3);
}

inline uint32_t crcUpdate(uint32_t crc, const uint8_t *p, unsigned size) {
    while (size--) {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return crc;
}

// Geometry comes from an already mounted, validated FAT16/FAT32 volume.
struct AllocationVolume { uint32_t fat, data, root, end, rootSecs, shift, copies, info, backupInfo; };
template<class Volume>
void allocationHints(Volume &v, const uint8_t *b, uint32_t base, uint32_t reserved, uint32_t copies, bool fat32) {
    v.copies = copies; v.info = v.backupInfo = 0;
    if (!fat32) return;
    if (b[40] & 0x80) v.copies = 0; // non-mirrored FAT32 is unsupported
    uint32_t info = b[48] | uint32_t(b[49]) << 8;
    uint32_t backup = b[50] | uint32_t(b[51]) << 8;
    if (info && info < reserved) {
        v.info = base + info;
        if (backup && backup + info < reserved) v.backupInfo = base + backup + info;
    }
}

template<class Blocks, class Volume = AllocationVolume> class Allocator {
    const Volume &v;
    uint8_t *b;
    uint32_t stride = 0, copies = 0, cached = ~0u;
    static uint32_t get16(const uint8_t *p) {
#if defined(__GNUC__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        uint16_t value; memcpy(&value, __builtin_assume_aligned(p, 2), 2); return value;
#else
        return p[0] | uint32_t(p[1]) << 8;
#endif
    }
    static uint32_t get32(const uint8_t *p) { return get16(p) | get16(p + 2) << 16; }
    static void put16(uint8_t *p, uint32_t n) {
#if defined(__GNUC__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        uint16_t value = n; memcpy(__builtin_assume_aligned(p, 2), &value, 2);
#else
        p[0] = n; p[1] = n >> 8;
#endif
    }
    static void put32(uint8_t *p, uint32_t n) { put16(p, n); put16(p + 2, n >> 16); }
    uint32_t lba(uint32_t c) const { return v.data + ((c - 2) << v.shift); }
    bool endOfChain(uint32_t entry) const { return entry >= (v.rootSecs ? 0xfff8u : 0x0ffffff8u); }
    bool read(uint32_t sector) {
        if (cached == sector) return true;
        cached = ~0u;
        if (!Blocks::read(sector, b)) return false;
        cached = sector;
        return true;
    }
    bool write(uint32_t sector) {
        uint32_t expected = crcUpdate(~0u, b, 512);
        cached = ~0u;
        return Blocks::write(sector, b) && read(sector) && crcUpdate(~0u, b, 512) == expected;
    }
    uint32_t entryOffset(uint32_t c) const { return c << (v.rootSecs ? 1 : 2); }
    int32_t get(uint32_t c) {
        uint32_t off = entryOffset(c);
        if (!read(v.fat + (off >> 9))) return -1;
        return v.rootSecs ? get16(b + (off & 511)) : get32(b + (off & 511)) & 0x0fffffffu;
    }
    bool set(uint32_t c, uint32_t value) {
        uint32_t off = entryOffset(c);
        int32_t previous = get(c);
        if (previous < 0) return false;
        // Check all copies before changing any of them.
        for (unsigned i = 1; i < copies; ++i) {
            if (!read(v.fat + i * stride + (off >> 9))) return false;
            uint32_t n = v.rootSecs ? get16(b + (off & 511)) : get32(b + (off & 511)) & 0x0fffffffu;
            if (n != uint32_t(previous)) return false;
        }
        for (unsigned i = 0; i < copies; ++i) {
            uint32_t sector = v.fat + i * stride + (off >> 9);
            if (!read(sector)) return false;
            uint8_t *p = b + (off & 511);
            if (v.rootSecs) put16(p, value);
            else put32(p, (get32(p) & 0xf0000000u) | (value & 0x0fffffffu));
            if (!write(sector)) return false;
        }
        return true;
    }
    Status geometry() {
        copies = v.copies;
        if (!copies) return Status::UnsupportedFilesystem;
        stride = (v.data - v.fat - v.rootSecs) / copies;
        if (!v.rootSecs) {
            // Free-space counts are hints. Invalidate primary and backup
            // FSInfo before allocating so operating systems rescan the FAT.
            for (unsigned i = 0; i < 2; ++i) {
                uint32_t sector = i ? v.backupInfo : v.info;
                if (!sector) continue;
                if (!read(sector)) return Status::IoError;
                if (get32(b) != 0x41615252u || get32(b + 484) != 0x61417272u || get32(b + 508) != 0xaa550000u) continue;
                put32(b + 488, ~0u); put32(b + 492, ~0u);
                if (!write(sector)) return Status::IoError;
            }
        }
        return Status::Ready;
    }
    int32_t allocate(unsigned count) {
        if (MAX_SLOT_SECTORS == 1) {
            if (count - 1 >= 3) return 0;
            uint32_t free[3]; unsigned found = 0;
            for (uint32_t c = 2; c < v.end && found < count; ++c) {
                int32_t n = get(c); if (n < 0) return -1;
                if (!n) free[found++] = c;
            }
            if (found != count) return 0;
            for (unsigned i = 0; i < count; ++i)
                if (!set(free[i], i + 1 < count ? free[i + 1] : 0x0fffffffu)) return -1;
            return free[0];
        }
        // Check availability before making FAT changes.
        unsigned free = 0;
        for (uint32_t c = 2; c < v.end && free < count; ++c) {
            int32_t n = get(c); if (n < 0) return -1;
            if (!n) ++free;
        }
        if (free < count) return 0;
        uint32_t first = 0, previous = 0;
        for (uint32_t c = 2; count && c < v.end; ++c) {
            int32_t n = get(c); if (n < 0) return -1;
            if (n) continue;
            if (!set(c, 0x0fffffffu) || (previous && !set(previous, c))) return -1;
            if (!first) first = c;
            previous = c; --count;
        }
        return first;
    }
    // Look up an exact short name, or reserve an entry if name is null.
#if defined(__GNUC__) && !defined(__clang__)
    __attribute__((noinline, noclone))
#endif
    Status locate(uint32_t parent, const char *name, uint32_t &sector, unsigned &offset) {
        const bool fixedRoot = !parent && v.rootSecs;
        const unsigned clusterSectors = 1u << v.shift;
        uint32_t dir = parent ? parent : v.root, sec = 0, link = 0;
        for (unsigned guard = 4096; guard--;) {
            if (fixedRoot) {
                if (sec == v.rootSecs) return name ? Status::NotFound : Status::NoSpace;
                sector = dir + sec++;
            } else {
                if (sec == clusterSectors) {
                    if (endOfChain(link)) {
                        if (name) return Status::NotFound;
                        int32_t added = allocate(1);
                        if (added <= 0) return added ? Status::IoError : Status::NoSpace;
                        if (!initializeDirectory(added, false)) return Status::IoError;
                        if (!set(dir, added)) return Status::IoError;
                        link = added;
                    }
                    dir = link; sec = 0;
                }
                if (!sec) {
                    int32_t next = get(dir);
                    if (next < 0) return Status::IoError;
                    link = next;
                    if (!endOfChain(link) && (link < 2 || link >= v.end)) return Status::Damaged;
                }
                sector = lba(dir) + sec++;
            }
            if (!read(sector)) return Status::IoError;
            for (offset = 0; offset < 512; offset += 32) {
                uint8_t first = b[offset];
                if (name) {
                    if (!first) return Status::NotFound;
                    if (!memcmp(b + offset, name, 11)) return Status::Ready;
                    continue;
                }
                if (first == 0xe5) return Status::Ready;
                if (first) continue;
                // The next entry must terminate the directory before this
                // end marker is consumed by publishing the new entry.
                if (offset < 480) {
                    if (b[offset + 32]) {
                        b[offset + 32] = 0;
                        if (!write(sector)) return Status::IoError;
                    }
                } else {
                    uint32_t following = 0;
                    if (fixedRoot) {
                        if (sec < v.rootSecs) following = dir + sec;
                    } else if (sec < clusterSectors) following = lba(dir) + sec;
                    else if (!endOfChain(link)) following = lba(link);
                    if (following) {
                        if (!read(following)) return Status::IoError;
                        if (b[0]) {
                            b[0] = 0;
                            if (!write(following)) return Status::IoError;
                        }
                    }
                }
                return Status::Ready;
            }
        }
        return Status::Damaged;
    }
    void fillEntry(uint8_t *entry, const char *name, uint8_t attr, uint32_t cluster, uint32_t bytes) {
        memset(entry, 0, 32);
        memcpy(entry, name, 11);
        entry[11] = attr;
        put16(entry + 16, 0x21); put16(entry + 18, 0x21); put16(entry + 24, 0x21); // 1980-01-01
        put16(entry + 20, v.rootSecs ? 0 : cluster >> 16);
        put16(entry + 26, cluster); put32(entry + 28, bytes);
    }
    bool initializeDirectory(uint32_t cluster, bool firstCluster) {
        for (unsigned i = 0; i < 1u << v.shift; ++i) {
            memset(b, 0, 512);
            if (firstCluster && !i) {
                fillEntry(b, ".          ", 0x10, cluster, 0);
                fillEntry(b + 32, "..         ", 0x10, 0, 0);
            }
            if (!write(lba(cluster) + i)) return false;
        }
        return true;
    }
    Status publish(uint32_t sector, unsigned offset, const char *name, uint8_t attr, uint32_t cluster, uint32_t bytes) {
        if (!read(sector)) return Status::IoError;
        fillEntry(b + offset, name, attr, cluster, bytes);
        // Publish only after the contents and FAT copies have been verified.
        b[offset] = 0xe5;
        if (!write(sector)) return Status::IoError;
        b[offset] = name[0];
        return write(sector) ? Status::Ready : Status::IoError;
    }
    Status makeSaveDirectory(uint32_t &parent) {
        uint32_t sector; unsigned offset;
        Status status = locate(0, nullptr, sector, offset);
        if (status != Status::Ready) return status;
        int32_t head = allocate(1);
        if (head <= 0) return head ? Status::IoError : Status::NoSpace;
        if (!initializeDirectory(head, true)) return Status::IoError;
        parent = head;
        return publish(sector, offset, SAVE_DIRECTORY, 0x10, head, 0);
    }
public:
    Allocator(const Volume &volume, uint8_t *scratch) : v(volume), b(scratch) {}
    // An exact lookup also detects a file blocking the save directory or a
    // directory blocking a journal. Such entries must never be replaced.
    Status find(const char *name, uint32_t parent, uint32_t &cluster, uint32_t &bytes) {
        uint32_t sector; unsigned offset;
        Status status = locate(parent, name, sector, offset);
        if (status != Status::Ready) return status;
        const uint8_t *entry = b + offset;
        const uint8_t attr = entry[11] & 0x1f;
        if (attr != (parent ? 0 : 0x10)) return Status::Damaged;
        cluster = get16(entry + 26) | (v.rootSecs ? 0 : get16(entry + 20) << 16);
        if (cluster < 2 || cluster >= v.end) return Status::Damaged;
        bytes = get32(entry + 28);
        return Status::Ready;
    }
    Status open(const char *basename, uint32_t &parent, uint32_t &cluster, uint32_t &bytes) {
        parent = 0;
        Status status = find(SAVE_DIRECTORY, 0, parent, bytes);
        if (status != Status::Ready) return status;
        char name[11]; journalName(name, basename);
        return find(name, parent, cluster, bytes);
    }
    Status create(const Id &id, uint16_t sectors, uint32_t parent) {
        if (MAX_SLOT_SECTORS == 1) sectors = 1;
        Status status = geometry();
        if (status != Status::Ready) return status;
        if (!parent) {
            status = makeSaveDirectory(parent);
            if (status != Status::Ready) return status;
        }
        uint32_t dirSector; unsigned dirOffset;
        status = locate(parent, nullptr, dirSector, dirOffset);
        if (status != Status::Ready) return status;
        unsigned fileSectors = 1u + 2u * sectors;
        int32_t head = allocate((fileSectors + (1u << v.shift) - 1) >> v.shift);
        if (head <= 0) return head ? Status::IoError : Status::NoSpace;
        uint32_t cluster = head, crc = ~0u;
        for (unsigned i = 0; i < fileSectors; ++i) {
            if (i && !(i & ((1u << v.shift) - 1))) {
                int32_t n = get(cluster);
                if (n < 2 || uint32_t(n) >= v.end) return Status::IoError;
                cluster = n;
            }
            if (!i) {
                memset(b, 0, 512); memcpy(b, "CHSAVE2", 8);
                put16(b + 8, id.size()); put16(b + 10, sectors);
                memcpy(b + 16, id.text(), id.size());
                put32(b + 508, ~crcUpdate(~0u, b, 508));
            } else if (i <= sectors) {
                // A valid zero-length record makes the first save retryable
                // if power is lost while writing its first actual payload.
                memset(b, 0, 512);
                if (i == 1) put32(b, 0x33565343u);
                crc = crcUpdate(crc, b, i == sectors ? 508 : 512);
                if (i == sectors) put32(b + 508, ~crc);
            } else memset(b, 0xff, 512);
            if (!write(lba(cluster) + (i & ((1u << v.shift) - 1)))) return Status::IoError;
        }
        char name[11]; journalName(name, id.basename());
        return publish(dirSector, dirOffset, name, 0x20, head, fileSectors * 512u);
    }
};
} }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
