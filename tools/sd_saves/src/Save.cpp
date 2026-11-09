// Compatibility API for SDK games; storage is implemented by CHSave.
#pragma GCC optimize("Oz")
#include "Save.h"
#include <CHSave.h>
#include <CHGfx.h>
#include <string.h>
namespace save {
namespace {
Status state = NoCard;
Status mapped(chsave::Status status) {
    switch (status) {
    case chsave::Status::Ready:
    case chsave::Status::Empty:
    case chsave::Status::Incompatible: return Ready;
    case chsave::Status::NoCard: return NoCard;
    case chsave::Status::NeedsSetup: return NeedsSetup;
    case chsave::Status::Damaged: return Damaged;
    default: return Failed;
    }
}
}
Status status() { return state; }
bool available() { return state == Ready; }
const void *read(uint32_t magic, uint8_t version, uint16_t size, uint8_t *flag) {
    if (!size || size > MAX_DATA) return nullptr;
    alignas(4) uint8_t data[MAX_DATA];
    chsave::Store saves;
    uint8_t savedFlag = 0;
    bool loaded = saves.load(magic, version, data, size, &savedFlag);
    state = mapped(saves.status());
    if (!loaded) {
        auto result = saves.status();
        if (result != chsave::Status::Empty && result != chsave::Status::NoCard &&
            result != chsave::Status::NeedsSetup) return nullptr;
        const void *old = readLegacy(magic, version, size, &savedFlag);
        if (!old) return nullptr;
        if (result != chsave::Status::Empty) {
            if (flag) *flag = savedFlag;
            return old;
        }
        memcpy(data, old, size);
        if (!saves.store(magic, version, data, size, savedFlag)) {
            state = mapped(saves.status());
            return nullptr;
        }
    }
    auto *out = gfx_chunkScratch() + 528;
    memcpy(out, data, size);
    if (flag) *flag = savedFlag;
    return out;
}
void *buffer() {
    gfx_wait();
    auto *scratch = gfx_chunkScratch();
    memset(scratch, 0, 512);
    return scratch + 16;
}
bool write(uint32_t magic, uint8_t version, uint16_t size, uint8_t flag) {
    if (!size || size > MAX_DATA) return false;
    alignas(4) uint8_t data[MAX_DATA];
    memcpy(data, gfx_chunkScratch() + 16, size);
    chsave::Store saves;
    bool ok = saves.store(magic, version, data, size, flag);
    state = mapped(saves.status());
    return ok;
}
}
