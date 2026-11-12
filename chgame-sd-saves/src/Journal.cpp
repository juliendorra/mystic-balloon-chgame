// SPDX-License-Identifier: MIT
// CHSAVE2 identity descriptor and CSV3 two-slot journal.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Oz")
#endif
#include "Journal.h"
#include "detail/Format.h"
#include <string.h>

namespace chsave {
namespace {
inline uint32_t crcUpdate(uint32_t crc, const uint8_t *p, unsigned size) {
    while (size--) {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return crc;
}


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
bool blank(const uint8_t *p) {
    for (unsigned i = 0; i < 512; ++i) if (p[i] != 0xff) return false;
    return true;
}
bool newer(uint32_t a, uint32_t b) {
    return a - b - 1 < 0x7fffffffu;
}
constexpr uint32_t RECORD_TAG = 0x33565343u; // CSV3
} // namespace

void detail::initializeSector(uint32_t index, uint8_t *b, void *context) {
    auto &initial = *static_cast<InitialFile *>(context);
    const uint32_t slots = MAX_SLOT_SECTORS == 1 ? 1 : initial.sectors;
    if (index > slots) {
        memset(b, 0xff, 512);
        return;
    }
    if (!index) {
        memcpy(b, "CHSAVE2", 8);
        put16(b + 8, initial.id.size());
        put16(b + 10, slots);
        memcpy(b + 16, initial.id.text(), initial.id.size());
    } else {
        // An empty valid record makes the first interrupted save retryable.
        if (index == 1) put32(b, RECORD_TAG);
    }
    const uint32_t crc = MAX_SLOT_SECTORS == 1 || !index ? ~0u : initial.crc;
    if (!index || index == slots) put32(b + 508, ~crcUpdate(crc, b, 508));
    else initial.crc = crcUpdate(crc, b, 512);
}

Journal::Journal(Storage &storage, void *scratch, size_t scratchBytes)
    : storage_(storage), scratch_(static_cast<uint8_t *>(scratch)), scratchBytes_(scratchBytes) {}

bool Journal::arguments(const Id &gameId, const void *data, uint16_t size) {
    status_ = Status::InvalidArgument;
    if (!scratch_ || (reinterpret_cast<uintptr_t>(scratch_) & 3) ||
        scratchBytes_ < SCRATCH_BYTES || !data || uint16_t(size - 1) >= MAX_DATA) return false;
    if (!gameId.size()) return false;
    return true;
}
bool Journal::open(const Id &gameId, uint16_t createBytes) {
    status_ = storage_.open(gameId, scratch_ + 512);
    if (status_ == Status::NotFound && createBytes) {
        status_ = storage_.create(gameId, (uint32_t(createBytes) + 531) / 512, scratch_ + 512);
        if (status_ == Status::Ready) status_ = storage_.open(gameId, scratch_ + 512);
    }
    if (status_ == Status::NotFound) status_ = Status::Empty;
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
    for (unsigned i = 0; i < 2; ++i) {
        // With one sector per record, both verified records fit in scratch.
        uint8_t *b = scratch_ + (MAX_SLOT_SECTORS == 1 ? i * 512 : 512);
        uint32_t crc = ~0u, gen = 0;
        unsigned length = ~0u;
        for (unsigned block = 0; block < sectors(); ++block) {
            if (!storage_.read(1 + i * sectors() + block, b)) {
                status_ = Status::IoError;
                return false;
            }
            empty = empty && blank(b);
            if (!block) {
                if (get32(b) == RECORD_TAG && !get32(b + 12)) length = get16(b + 8);
                gen = get32(b + 4);
            }
            bool last = block + 1 == sectors();
            crc = crcUpdate(crc, b, last ? 508 : 512);
            if (last && length <= sectors() * 512u - 20 && get32(b + 508) == ~crc &&
                (slot < 0 || newer(gen, generation))) {
                slot = i;
                generation = gen;
                status_ = length ? Status::Ready : Status::Empty;
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
    if (slot < 0 || status_ == Status::Empty) { status_ = Status::Empty; return false; }
    if (MAX_SLOT_SECTORS == 1) {
        const uint8_t *record = scratch_ + slot * 512;
        if (record[10] != schema || get16(record + 8) != size) { status_ = Status::Incompatible; return false; }
        memcpy(data, record + 16, size);
        if (flags) *flags = record[11];
        return true;
    }
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
    if (!arguments(gameId, data, size) || !open(gameId, size)) return false;
    if (size > sectors() * 512u - 20) { status_ = Status::TooLarge; return false; }
    int slot;
    uint32_t generation;
    if (!latest(slot, generation)) return false;
    const uint8_t *input = static_cast<const uint8_t *>(data);
    uint8_t *b = scratch_, *check = b + 512;
    if (MAX_SLOT_SECTORS == 1) {
        if (slot >= 0) {
            const uint8_t *record = scratch_ + slot * 512;
            if (record[10] == schema && get16(record + 8) == size && record[11] == flags &&
                !memcmp(input, record + 16, size)) return true;
        }
    } else if (slot >= 0) {
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
