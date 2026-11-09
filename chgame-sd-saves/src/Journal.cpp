// SPDX-License-Identifier: MIT
// CHSAVE2 identity descriptor and CSV3 two-slot journal.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Oz")
#endif
#include "Journal.h"
#include <string.h>

namespace chsave {
namespace {
// Header fields are aligned in the caller's checked scratch buffer.
#if defined(__GNUC__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
uint16_t get16(const uint8_t *p) {
    uint16_t value; memcpy(&value, __builtin_assume_aligned(p, 2), 2); return value;
}
uint32_t get32(const uint8_t *p) {
    uint32_t value; memcpy(&value, __builtin_assume_aligned(p, 4), 4); return value;
}
void put16(uint8_t *p, uint16_t value) { memcpy(__builtin_assume_aligned(p, 2), &value, 2); }
void put32(uint8_t *p, uint32_t value) { memcpy(__builtin_assume_aligned(p, 4), &value, 4); }
#else
uint16_t get16(const uint8_t *p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t get32(const uint8_t *p) { return uint32_t(get16(p)) | uint32_t(get16(p + 2)) << 16; }
void put16(uint8_t *p, uint16_t n) { p[0] = n; p[1] = n >> 8; }
void put32(uint8_t *p, uint32_t n) { put16(p, n); put16(p + 2, n >> 16); }
#endif
uint32_t crcUpdate(uint32_t crc, const uint8_t *p, unsigned size) {
    while (size--) {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return crc;
}
bool blank(const uint8_t *p) {
    for (unsigned i = 0; i < 512; ++i) if (p[i] != 0xff) return false;
    return true;
}
bool newer(uint32_t a, uint32_t b) {
    return a - b - 1 < 0x7fffffffu;
}
constexpr uint32_t RECORD_TAG = 0x33565343u; // CSV3
} // namespace

Journal::Journal(Storage &storage, void *scratch, size_t scratchBytes)
    : storage_(storage), scratch_(static_cast<uint8_t *>(scratch)), scratchBytes_(scratchBytes) {}

bool Journal::arguments(const Id &gameId, const void *data, uint16_t size) {
    status_ = Status::InvalidArgument;
    if (!scratch_ || (reinterpret_cast<uintptr_t>(scratch_) & 3) ||
        scratchBytes_ < SCRATCH_BYTES || !data || uint16_t(size - 1) >= MAX_DATA) return false;
    if (!gameId.size()) return false;
    return true;
}
bool Journal::open(const Id &gameId) {
    status_ = storage_.open(gameId, scratch_ + 512);
    if (status_ != Status::Ready) return false;
    sectors_ = storage_.slotSectors();
    if (!sectors_ || sectors_ > MAX_SLOT_SECTORS) {
        status_ = Status::Damaged;
        return false;
    }
    uint8_t *b = scratch_ + 512;
    if (!storage_.read(0, b)) { status_ = Status::IoError; return false; }
    if (memcmp(b, "CHSAVE2", 8) || get16(b + 8) < 1 || get16(b + 8) > 253 ||
        get16(b + 10) != sectors() || get32(b + 12) ||
        get32(b + 508) != ~crcUpdate(~0u, b, 508)) {
        status_ = Status::Damaged;
        return false;
    }
    if (get16(b + 8) != gameId.size() || memcmp(b + 16, gameId.text(), gameId.size())) {
        status_ = Status::IdCollision;
        return false;
    }
    return true;
}
bool Journal::latest(int &slot, uint32_t &generation) {
    bool empty = true;
    slot = -1;
    generation = 0;
    uint8_t *b = scratch_ + 512;
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t crc = ~0u, gen = 0;
        bool header = false;
        for (unsigned block = 0; block < sectors(); ++block) {
            if (!storage_.read(1 + i * sectors() + block, b)) {
                status_ = Status::IoError;
                return false;
            }
            empty = empty && blank(b);
            if (!block) {
                header = get32(b) == RECORD_TAG && !get32(b + 12) &&
                         get16(b + 8) <= sectors() * 512u - 20;
                gen = get32(b + 4);
            }
            bool last = block + 1 == sectors();
            crc = crcUpdate(crc, b, last ? 508 : 512);
            if (last && header && get32(b + 508) == ~crc &&
                (slot < 0 || newer(gen, generation))) {
                slot = i;
                generation = gen;
            }
        }
    }
    if (slot < 0 && !empty) { status_ = Status::Damaged; return false; }
    return true;
}

bool Journal::load(const Id &gameId, uint8_t schema, void *data, uint16_t size, uint8_t *flags) {
    if (!arguments(gameId, data, size) || !open(gameId)) return false;
    int slot;
    uint32_t generation;
    if (!latest(slot, generation)) return false;
    if (slot < 0) { status_ = Status::Empty; return false; }
    uint8_t *b = scratch_ + 512, *out = static_cast<uint8_t *>(data);
    uint32_t crc = ~0u;
    unsigned copied = 0;
    uint8_t savedFlags = 0;
    for (unsigned block = 0; block < sectors(); ++block) {
        if (!storage_.read(1 + slot * sectors() + block, b)) { status_ = Status::IoError; return false; }
        if (!block) {
            if (get32(b) != RECORD_TAG || get32(b + 4) != generation || get32(b + 12)) {
                status_ = Status::Damaged;
                return false;
            }
            if (b[10] != schema || get16(b + 8) != size) { status_ = Status::Incompatible; return false; }
            savedFlags = b[11];
        }
        bool last = block + 1 == sectors();
        crc = crcUpdate(crc, b, last ? 508 : 512);
        if (last && get32(b + 508) != ~crc) { status_ = Status::Damaged; return false; }
        unsigned offset = block ? 0 : 16;
        unsigned count = (last ? 508 : 512) - offset;
        if (count > size - copied) count = size - copied;
        memcpy(out + copied, b + offset, count);
        copied += count;
    }
    if (flags) *flags = savedFlags;
    status_ = Status::Ready;
    return true;
}

bool Journal::store(const Id &gameId, uint8_t schema, const void *data, uint16_t size, uint8_t flags) {
    if (!arguments(gameId, data, size) || !open(gameId)) return false;
    if (size > sectors() * 512u - 20) { status_ = Status::TooLarge; return false; }
    int slot;
    uint32_t generation;
    if (!latest(slot, generation)) return false;
    const uint8_t *input = static_cast<const uint8_t *>(data);
    uint8_t *b = scratch_, *check = b + 512;
    if (slot >= 0) {
        bool same = true;
        unsigned compared = 0;
        for (unsigned block = 0; same && block < sectors(); ++block) {
            if (!storage_.read(1 + slot * sectors() + block, check)) { status_ = Status::IoError; return false; }
            if (!block) same = check[10] == schema && get16(check + 8) == size && check[11] == flags;
            unsigned offset = block ? 0 : 16;
            unsigned count = (block + 1 == sectors() ? 508 : 512) - offset;
            if (count > size - compared) count = size - compared;
            same = same && !memcmp(input + compared, check + offset, count);
            compared += count;
        }
        if (same) return true;
    }
    unsigned target = slot < 0 ? 0 : 1 - slot;
    uint32_t crc = ~0u;
    unsigned copied = 0;
    for (unsigned block = 0; block < sectors(); ++block) {
        memset(b, 0, 512);
        if (!block) {
            put32(b, RECORD_TAG);
            put32(b + 4, generation + 1);
            put16(b + 8, size);
            b[10] = schema;
            b[11] = flags;
        }
        bool last = block + 1 == sectors();
        unsigned offset = block ? 0 : 16;
        unsigned count = (last ? 508 : 512) - offset;
        if (count > size - copied) count = size - copied;
        memcpy(b + offset, input + copied, count);
        copied += count;
        crc = crcUpdate(crc, b, last ? 508 : 512);
        if (last) put32(b + 508, ~crc);
        unsigned fileSector = 1 + target * sectors() + block;
        if (!storage_.write(fileSector, b) || !storage_.read(fileSector, check) || memcmp(b, check, 512)) {
            status_ = Status::IoError;
            return false;
        }
    }
    status_ = Status::Ready;
    return true;
}
} // namespace chsave
