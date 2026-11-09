// SPDX-License-Identifier: MIT
#pragma once
#include "Config.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

namespace chsave {

constexpr uint16_t MAX_SLOT_SECTORS = CHSAVE_MAX_SLOT_SECTORS;
constexpr uint16_t MAX_DATA = MAX_SLOT_SECTORS * 512u - 20;
constexpr size_t SCRATCH_BYTES = 1024;

enum class Status : uint8_t {
    Ready, Empty, NoCard, NeedsSetup, Incompatible, Damaged,
    IoError, UnsupportedFilesystem, InvalidArgument, TooLarge
};

constexpr uint32_t id(const char (&text)[5]) {
    return uint32_t(uint8_t(text[0])) | uint32_t(uint8_t(text[1])) << 8 |
           uint32_t(uint8_t(text[2])) << 16 | uint32_t(uint8_t(text[3])) << 24;
}

// open() locates an existing journal file. It may use the 512-byte
// scratch buffer. Neither open() nor write() may allocate or resize files.
class Storage {
public:
    virtual Status open(uint32_t gameId, uint8_t *scratch) = 0;
    virtual uint16_t slotSectors() const { return 1; }
    virtual bool read(unsigned fileSector, uint8_t *sector) = 0;
    virtual bool write(unsigned fileSector, const uint8_t *sector) = 0;
protected:
    ~Storage() = default;
};

class Journal {
public:
    // Caller owns storage and scratch. Scratch must be 4-byte aligned,
    // at least SCRATCH_BYTES long, and separate from the payload.
    Journal(Storage &storage, void *scratch, size_t scratchBytes);
    bool load(uint32_t gameId, uint8_t schema, void *data, uint16_t size,
              uint8_t *flags = nullptr);
    bool store(uint32_t gameId, uint8_t schema, const void *data, uint16_t size,
               uint8_t flags = 0);
    Status status() const { return status_; }

    template<class T>
    bool load(uint32_t gameId, uint8_t schema, T &data, uint8_t *flags = nullptr) {
        static_assert(std::is_trivially_copyable<T>::value, "save plain data only");
        static_assert(sizeof(T) <= MAX_DATA, "save payload exceeds format limit");
        T candidate{};
        if (!load(gameId, schema, &candidate, sizeof(T), flags)) return false;
        memcpy(&data, &candidate, sizeof(T));
        return true;
    }
    template<class T>
    bool store(uint32_t gameId, uint8_t schema, const T &data, uint8_t flags = 0) {
        static_assert(std::is_trivially_copyable<T>::value, "save plain data only");
        static_assert(sizeof(T) <= MAX_DATA, "save payload exceeds format limit");
        return store(gameId, schema, &data, sizeof(T), flags);
    }

private:
    Storage &storage_;
    uint8_t *scratch_;
    size_t scratchBytes_;
    Status status_ = Status::Empty;
    bool arguments(uint32_t gameId, const void *data, uint16_t size);
    bool open(uint32_t gameId);
    bool latest(uint32_t gameId, int &slot, uint32_t &generation);
    uint16_t sectors_ = 1;
    uint16_t sectors() const { return MAX_SLOT_SECTORS == 1 ? 1 : sectors_; }
};

} // namespace chsave
