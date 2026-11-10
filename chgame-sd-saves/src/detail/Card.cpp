// SPDX-License-Identifier: MIT
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Oz")
#endif
#include "Card.h"
#include "Block.h"
#include "Fat.h"

namespace chsave { namespace detail {
namespace {
class Card : public Storage {
    uint32_t cluster_ = 0;
    uint16_t sectors_ = 1;
    uint8_t *scratch_ = nullptr;
public:
    Status open(const Id &gameId, uint8_t *scratch) override {
        if (!block::init()) return Status::NoCard;
        int error = fat::mount(scratch);
        if (error) return error == fat::E_READ ? Status::IoError : Status::UnsupportedFilesystem;
        scratch_ = scratch;
        error = fat::saveFile(gameId.basename(), cluster_, sectors_, scratch);
        if (!error) return Status::Ready;
        if (error == fat::E_NOTFOUND) return Status::NotFound;
        return error == fat::E_READ ? Status::IoError : Status::Damaged;
    }
    Status create(const Id &id, uint16_t sectors, uint8_t *scratch) override {
        return fat::createFile(id, sectors, scratch);
    }
    uint16_t slotSectors() const override { return sectors_; }
    bool read(unsigned fileSector, uint8_t *sector) override {
        uint32_t lba;
        return fileSector < 1u + 2u * sectors_ && !fat::sector(cluster_, fileSector, lba, sector) &&
               block::read(lba, sector);
    }
    bool write(unsigned fileSector, const uint8_t *sector) override {
        uint32_t lba;
        return fileSector && fileSector < 1u + 2u * sectors_ && !fat::sector(cluster_, fileSector, lba, scratch_) &&
               block::write(lba, sector);
    }
};
}
Storage &card() { static Card instance; return instance; }
} }
