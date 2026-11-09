#include "CHSave.h"
#include "detail/Block.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

alignas(4) static uint8_t scratch[1024];
uint8_t *gfx_chunkScratch() { return scratch; }
void gfx_wait() {}

#ifdef CHSAVE_HOST
namespace chsave { namespace block {
bool init() {
    const char *path = getenv("CHSAVE_CARD_IMAGE");
    FILE *file = path ? fopen(path, "rb") : nullptr;
    if (!file) return false;
    fclose(file);
    return true;
}
bool read(uint32_t lba, uint8_t *dst) {
    const char *fail = getenv("CHSAVE_FAIL_READ");
    static unsigned count = 0;
    if (fail && ++count == strtoul(fail, nullptr, 10)) return false;
    const char *path = getenv("CHSAVE_CARD_IMAGE");
    FILE *file = path ? fopen(path, "rb") : nullptr;
    if (!file) return false;
    bool ok = !fseek(file, uint64_t(lba) * 512, SEEK_SET) && fread(dst, 1, 512, file) == 512;
    fclose(file);
    return ok;
}
bool write(uint32_t lba, const uint8_t *src) {
    const char *path = getenv("CHSAVE_CARD_IMAGE");
    FILE *file = path ? fopen(path, "r+b") : nullptr;
    if (!file) return false;
    const char *cut = getenv("CHSAVE_CUT");
    static unsigned written = 0;
    unsigned remaining = cut ? strtoul(cut, nullptr, 10) : 131072;
    unsigned size = remaining > written ? remaining - written : 0;
    if (size > 512) size = 512;
    written += size;
    bool ok = !fseek(file, uint64_t(lba) * 512, SEEK_SET) && fwrite(src, 1, size, file) == size;
    if (getenv("CHSAVE_CORRUPT_WRITE") && size == 512) {
        assert(!fseek(file, uint64_t(lba) * 512 + 20, SEEK_SET));
        fputc(src[20] ^ 1, file);
    }
    ok = !fclose(file) && ok;
    return ok && size == 512;
}
} }
#endif

struct Data { uint32_t score; uint8_t prefs[32]; };
int main(int argc, char **argv) {
    assert(argc >= 3);
    const auto gameId = chsave::id(argv[2]);
    if (!strcmp(argv[1], "filename")) { printf("%s.SAV\n", gameId.basename()); return 0; }
    chsave::Store saves;
    Data data{999, {}};
    uint8_t flags = 99;
    bool ok;
    if (!strncmp(argv[1], "large-", 6)) {
        assert(argc >= 5);
        unsigned size = strtoul(argv[3], nullptr, 10);
        unsigned seed = strtoul(argv[4], nullptr, 10);
        std::vector<uint8_t> bytes(size, 0);
        if (!strcmp(argv[1], "large-store")) {
            for (unsigned i = 0; i < size; ++i) bytes[i] = (i * 17 + seed) & 255;
            ok = saves.store(gameId, 1, bytes.data(), size, 7);
        } else {
            ok = saves.load(gameId, 1, bytes.data(), size, &flags);
            if (ok) for (unsigned i = 0; i < size; ++i) assert(bytes[i] == ((i * 17 + seed) & 255));
        }
        printf("%u %u\n", ok, unsigned(saves.status()));
        return 0;
    }
    if (!strcmp(argv[1], "store")) {
        assert(argc >= 4);
        data.score = strtoul(argv[3], nullptr, 10);
        memset(data.prefs, data.score & 255, sizeof data.prefs);
        ok = saves.store(gameId, argc > 4 ? atoi(argv[4]) : 1, data, 7);
    } else {
        ok = saves.load(gameId, argc > 3 ? atoi(argv[3]) : 1, data, &flags);
        if (ok) for (auto byte : data.prefs) assert(byte == (data.score & 255));
        else assert(data.score == 999 && flags == 99);
    }
    printf("%u %u %u %u\n", ok, data.score, flags, unsigned(saves.status()));
}
