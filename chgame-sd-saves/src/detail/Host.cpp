// SPDX-License-Identifier: MIT
#if defined(CHSIM) && !defined(CHSAVE_HOST)
#include "Block.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace chsave { namespace block {
namespace {
FILE *image(const char *mode) {
    const char *path = getenv("CHSAVE_CARD_IMAGE");
    return path ? fopen(path, mode) : nullptr;
}
bool seek(FILE *file, uint32_t lba) {
    return uint64_t(lba) * 512 <= LONG_MAX && !fseek(file, long(uint64_t(lba) * 512), SEEK_SET);
}
}
bool init() {
    FILE *file = image("rb");
    if (!file) return false;
    fclose(file);
    return true;
}
bool read(uint32_t lba, uint8_t *sector) {
    FILE *file = image("rb");
    if (!file) return false;
    bool ok = seek(file, lba) && fread(sector, 1, 512, file) == 512;
    fclose(file);
    return ok;
}
bool write(uint32_t lba, const uint8_t *sector) {
    FILE *file = image("r+b");
    if (!file) return false;
    bool ok = seek(file, lba) && fwrite(sector, 1, 512, file) == 512 && !fflush(file);
#ifdef _WIN32
    ok = ok && !_commit(_fileno(file));
#else
    ok = ok && !fsync(fileno(file));
#endif
    return !fclose(file) && ok;
}
} }
#endif
