// SPDX-License-Identifier: MIT
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Oz")
#endif
#include "Card.h"
#include "Format.h"
#include <FatWrite.h>
#include <SdWrite.h>

namespace chsave { namespace detail {
namespace {
constexpr char directory[] = "SAVES      ";

void filename(const Id &id, char (&name)[11]) {
    memcpy(name, id.basename(), 8);
    memcpy(name + 8, "SAV", 3);
}

Status status(int8_t rc) {
    switch (rc) {
    case fat::OK: return Status::Ready;
    case fat::E_NOTFOUND: return Status::NotFound;
    case fat::E_NOFS:
    case fat::E_EXFAT: return Status::UnsupportedFilesystem;
    case fat::E_NOSPACE: return Status::NoSpace;
    case fat::E_READ:
    case fat::E_WRITE: return Status::IoError;
    default: return Status::Damaged;
    }
}

class Card final : public Storage {
#if CHSAVE_MAX_SECTORS_PER_RECORD <= 2
    uint32_t lbas_[1u + 2u * MAX_SLOT_SECTORS] = {};
    bool lba(unsigned index, uint32_t &out, uint8_t *) const {
        if (index >= 1u + 2u * slotSectors()) return false;
        out = lbas_[index];
        return true;
    }
#else
    fat::File file_{};
    uint8_t *scratch_ = nullptr;
    bool lba(unsigned index, uint32_t &out, uint8_t *b) const {
        return !fat::sector(file_, index, out, b ? b : scratch_);
    }
#endif
#if CHSAVE_MAX_SECTORS_PER_RECORD != 1
    uint16_t sectors_ = 1;
#endif
public:
    constexpr Card() = default;
    Status open(const Id &id, uint8_t *scratch) override {
        if (!sd::init()) return Status::NoCard;
        int8_t rc = fat::mountWritable(scratch);
        if (rc) return status(rc);
        fat::File root, dir, file;
        fat::root(root);  // mountWritable already verified the volume.
        rc = fat::findExact(root, directory, true, dir, scratch);
        if (rc) return status(rc);
        char name[11]; filename(id, name);
        rc = fat::findExact(dir, name, false, file, scratch);
        if (rc) return status(rc);
#if CHSAVE_MAX_SECTORS_PER_RECORD == 1
        return status(fat::mapThreeBlocks(file, lbas_, scratch));
#else
        if (file.size < 1536 || file.size > 512u + MAX_SLOT_SECTORS * 1024u ||
            (file.size - 512) % 1024) return Status::Damaged;
        sectors_ = (file.size - 512) / 1024;
#if CHSAVE_MAX_SECTORS_PER_RECORD == 2
        return status(fat::map(file, lbas_, 1u + 2u * MAX_SLOT_SECTORS, scratch));
#else
        file_ = file;
        scratch_ = scratch;
        return status(fat::validate(file, scratch));
#endif
#endif
    }
    Status create(const Id &id, uint16_t slots, uint8_t *scratch) override {
        InitialFile initial{id, MAX_SLOT_SECTORS == 1 ? uint16_t(1) : slots, ~0u};
        char name[11]; filename(id, name);
#if CHSAVE_MAX_SECTORS_PER_RECORD == 1
        return status(fat::createThreeBlockFile(directory, name, initializeSector, &initial, scratch));
#elif CHSAVE_MAX_SECTORS_PER_RECORD == 2
        return status(fat::createSmallFile(directory, name, 1u + 2u * slots,
                                           initializeSector, &initial, scratch));
#else
        return status(fat::createFile(directory, name, 1u + 2u * slots,
                                      initializeSector, &initial, scratch));
#endif
    }
#if CHSAVE_MAX_SECTORS_PER_RECORD != 1
    uint16_t slotSectors() const override { return sectors_; }
#endif
    bool read(unsigned index, uint8_t *sector) override {
        uint32_t physical;
        return lba(index, physical, sector) && sd::read(physical, sector);
    }
    bool write(unsigned index, const uint8_t *sector) override {
        uint32_t physical;
        return index && lba(index, physical, nullptr) && sd::write(physical, sector);
    }
};
}
Storage &card() { static Card instance; return instance; }
} }
