// The simulator's SD card (the stand-in for CHSd's src/SdSpi.cpp;
// tools/chsim/chsim.py compiles it for a sketch that includes CHSd): the file
// named by $CHSD_CARD is in the slot. A FAT image (*.img, e.g. from CHSd's
// tools/fatimg.py) is served as the card's blocks; any other file is put on
// a pretend FAT16 card of its own (VCard.h), so the game's FAT code runs in
// the simulator just as on the board. With the variable unset there is no
// card. The debug protocol's eject command pulls it out and puts it back.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "VCard.h"
#include "SdWrite.h"

void sim_advance(uint32_t us);

static FILE *s_card;
static bool s_out, s_image, s_writable, s_explicit;         // pulled out; a whole image, not a file to wrap
static vcard::Card s_vc;

// Templates and asset fixtures are copied before any write. Only an
// explicitly selected save image is modified in place.
static bool makeWritable() {
    if (s_writable) return true;
    if (s_explicit) return false;
    FILE *copy = tmpfile();
    if (!copy) return false;
    uint8_t buf[4096];
    size_t n;
    bool ok = !fseek(s_card, 0, SEEK_SET);
    while (ok && (n = fread(buf, 1, sizeof buf, s_card))) ok = fwrite(buf, 1, n, copy) == n;
    if (!ok || ferror(s_card) || fflush(copy)) {
        fclose(copy);
        return false;
    }
    fclose(s_card);
    s_card = copy;
    return s_writable = true;
}

static FILE *card() {
    if (s_out) return nullptr;
    if (s_card) return s_card;
    const char *explicitImage = getenv("CHGAME_SAVE_CARD");
    if (!explicitImage) explicitImage = getenv("CHSAVE_CARD_IMAGE");
    const char *asset = getenv("CHSD_CARD");
    const char *path = explicitImage ? explicitImage : asset ? asset : getenv("CHGAME_SAVE_TEMPLATE");
    if (!path) return nullptr;
    if (explicitImage && (s_card = fopen(path, "r+b"))) s_writable = true;
    if (!s_card) s_card = fopen(path, "rb");
    if (!s_card) return nullptr;
    size_t n = strlen(path);
    s_explicit = explicitImage != nullptr;
    s_image = s_explicit || !asset ||
              (n > 4 && (!strcmp(path + n - 4, ".img") || !strcmp(path + n - 4, ".IMG")));
    if (!s_image) {
        fseek(s_card, 0, SEEK_END);
        s_vc.setup(path, (uint32_t)ftell(s_card));
    }
    return s_card;
}

uint32_t sim_cardBlocks() {
    FILE *f = card();
    if (!f) return 0;
    if (!s_image) return s_vc.blocks();
    if (fseek(f, 0, SEEK_END)) return 0;
    long end = ftell(f);
    return end < 0 ? 0 : (uint32_t)(end / 512);
}

void sim_cardEject(bool out) { s_out = out; }

namespace sd {

bool init() { return card() != nullptr; }

// A block, taking `us` of virtual time.
static bool fetch(uint32_t lba, uint8_t *dst, uint32_t us) {
    FILE *f = card();
    if (!f || lba >= sim_cardBlocks()) return false;
    sim_advance(us);
    int64_t k = s_image ? (int64_t)lba : s_vc.read(lba, dst);
    if (k < 0) return true;
    if (k > LONG_MAX / 512) return false;
    memset(dst, 0, 512);            // (a file's last block, past its end)
    return !fseek(f, long(k * 512), SEEK_SET) && fread(dst, 1, 512, f) > 0;
}

bool read(uint32_t lba, uint8_t *dst) {
    return fetch(lba, dst, 900);    // about what a polled block read takes on the board
}

bool write(uint32_t lba, const uint8_t *src) {
    if (!card() || !s_image || lba >= sim_cardBlocks() ||
        uint64_t(lba) * 512 > LONG_MAX || !makeWritable()) return false;
    const char *cut = getenv("CHGAME_SAVE_CUT");
    unsigned long n = cut ? strtoul(cut, nullptr, 10) : 512;
    if (n > 512) n = 512;
    if (fseek(s_card, long(uint64_t(lba) * 512), SEEK_SET) ||
        fwrite(src, 1, n, s_card) != n || fflush(s_card)) return false;
#ifdef _WIN32
    return !_commit(_fileno(s_card)) && n == 512;
#else
    return !fsync(fileno(s_card)) && n == 512;
#endif
}

// One command's latency, then each block at the 24 MHz wire rate; the
// board's fn runs while the next block arrives, so only the wire is timed.
bool stream(uint32_t lba, uint32_t n, uint8_t *buf0, uint8_t *buf1, BlockFn fn, void *ctx) {
    if (!n) return true;
    sim_advance(600);
    for (uint32_t k = 0; k < n; k++) {
        uint8_t *b = (k & 1) ? buf1 : buf0;
        if (!fetch(lba + k, b, 170)) return false;
        fn(b, ctx);
    }
    return true;
}

}  // namespace sd
