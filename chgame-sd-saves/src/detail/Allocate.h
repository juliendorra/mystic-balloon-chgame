// SPDX-License-Identifier: MIT
#pragma once
#include "../Journal.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("Oz", "ipa-sra")
#endif

namespace chsave { namespace detail {
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
    Status directory(uint32_t &sector, unsigned &offset) {
        uint32_t dir = v.root, sec = 0;
        for (unsigned guard = 4096; guard--;) {
            if (v.rootSecs) {
                if (sec == v.rootSecs) return Status::NoSpace;
                sector = v.root + sec++;
            } else {
                if (sec == 1u << v.shift) {
                    int32_t n = get(dir); if (n < 0) return Status::IoError;
                    if (uint32_t(n) >= 0x0ffffff8u) {
                        n = allocate(1);
                        if (n <= 0) return n ? Status::IoError : Status::NoSpace;
                        for (unsigned i = 0; i < 1u << v.shift; ++i) {
                            memset(b, 0, 512);
                            if (!write(lba(n) + i)) return Status::IoError;
                        }
                        if (!set(dir, n)) return Status::IoError;
                    }
                    if (n < 2 || uint32_t(n) >= v.end) return Status::Damaged;
                    dir = n; sec = 0;
                }
                sector = lba(dir) + sec++;
            }
            if (!read(sector)) return Status::IoError;
            for (offset = 0; offset < 512; offset += 32) {
                if (b[offset] && b[offset] != 0xe5) continue;
                if (!b[offset] && offset == 480) {
                    uint32_t following = 0;
                    if (v.rootSecs) {
                        if (sec < v.rootSecs) following = v.root + sec;
                    } else if (sec < 1u << v.shift) following = lba(dir) + sec;
                    else {
                        int32_t n = get(dir);
                        if (n < 0) return Status::IoError;
                        if (n >= 2 && uint32_t(n) < v.end) following = lba(n);
                        else if (uint32_t(n) < 0x0ffffff8u) return Status::Damaged;
                    }
                    if (following) {
                        if (!read(following)) return Status::IoError;
                        b[0] = 0;
                        if (!write(following)) return Status::IoError;
                    }
                    if (!read(sector)) return Status::IoError;
                }
                return Status::Ready;
            }
        }
        return Status::Damaged;
    }
public:
    Allocator(const Volume &volume, uint8_t *scratch) : v(volume), b(scratch) {}
    Status create(const Id &id, uint16_t sectors) {
        if (MAX_SLOT_SECTORS == 1) sectors = 1;
        Status status = geometry();
        if (status != Status::Ready) return status;
        uint32_t dirSector; unsigned dirOffset;
        status = directory(dirSector, dirOffset);
        if (status != Status::Ready) return status;
        // Save the following terminator before consuming an end marker.
        if (!b[dirOffset] && dirOffset < 480) {
            b[dirOffset + 32] = 0;
            if (!write(dirSector)) return Status::IoError;
        }
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
        if (!read(dirSector)) return Status::IoError;
        uint8_t *entry = b + dirOffset;
        memset(entry, 0, 32); memcpy(entry, id.basename(), 8); memcpy(entry + 8, "SAV", 3);
        entry[0] = 0xe5; entry[11] = 0x20;
        put16(entry + 16, 0x21); put16(entry + 18, 0x21); put16(entry + 24, 0x21); // 1980-01-01, no RTC
        put16(entry + 20, v.rootSecs ? 0 : uint32_t(head) >> 16);
        put16(entry + 26, head); put32(entry + 28, fileSectors * 512u);
        // Publish only after data and all FAT copies have been verified.
        // The final sector write changes one byte of a complete deleted entry.
        if (!write(dirSector)) return Status::IoError;
        b[dirOffset] = id.basename()[0];
        return write(dirSector) ? Status::Ready : Status::IoError;
    }
};
} }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
