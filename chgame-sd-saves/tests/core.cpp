#include "Journal.h"
#include <cassert>
#include <cstring>

class Memory : public chsave::Storage {
public:
    uint8_t bytes[1536];
    unsigned reads = 0;
    unsigned writes = 0;
    chsave::Status open(const chsave::Id &, uint8_t *) override { return chsave::Status::Ready; }
    bool read(unsigned slot, uint8_t *out) override {
        assert(slot < 3);
        memcpy(out, bytes + slot * 512, 512);
        ++reads;
        return true;
    }
    bool write(unsigned slot, const uint8_t *in) override {
        assert(slot > 0 && slot < 3);
        memcpy(bytes + slot * 512, in, 512);
        ++writes;
        return true;
    }
};

int main() {
    Memory backend;
    memset(backend.bytes, 255, sizeof backend.bytes);
    memset(backend.bytes, 0, 512);
    memcpy(backend.bytes, "CHSAVE2", 8);
    backend.bytes[8] = 16; backend.bytes[10] = 1;
    memcpy(backend.bytes + 16, "com.example.test", 16);
    const uint8_t crc[] = {0x5c, 0x74, 0x57, 0xfc};
    memcpy(backend.bytes + 508, crc, 4);
    alignas(4) uint8_t scratch[1024];
    uint8_t data[492] = {};
    chsave::Journal saves(backend, scratch, sizeof scratch);
    constexpr auto game = chsave::id("com.example.test");
    static_assert(game.size() == 16, "ID length");
    static_assert(game.basename()[0] == '3', "base-36 hash");
    assert(!saves.load(game, 1, data));
    assert(saves.status() == chsave::Status::Empty);
    assert(saves.store(game, 1, data, 42));
    assert(backend.writes == 1);
    backend.reads = 0;
    assert(saves.store(game, 1, data, 42) && backend.writes == 1);
    if (chsave::MAX_SLOT_SECTORS == 1) assert(backend.reads == 3);
    assert(saves.store(game, 1, data, 43) && backend.writes == 2);
    uint8_t flags = 0;
    memset(data, 127, sizeof data);
    backend.reads = 0;
    assert(saves.load(game, 1, data, &flags) && flags == 43);
    if (chsave::MAX_SLOT_SECTORS == 1) assert(backend.reads == 3);
    for (auto byte : data) assert(byte == 0);
    assert(!saves.load(game, 2, data));
    assert(saves.status() == chsave::Status::Incompatible);
    assert(!saves.store(chsave::id("bad!"), 1, data));
    assert(saves.status() == chsave::Status::InvalidArgument);
    assert(!saves.store(game, 1, data, uint16_t(chsave::MAX_DATA + 1), 0));
    assert(!saves.store(game, 1, static_cast<const void *>(nullptr), 0, 0));
    chsave::Journal tooSmall(backend, scratch, 512);
    assert(!tooSmall.store(game, 1, data));
    chsave::Journal unaligned(backend, scratch + 1, sizeof scratch - 1);
    assert(!unaligned.store(game, 1, data));
}
