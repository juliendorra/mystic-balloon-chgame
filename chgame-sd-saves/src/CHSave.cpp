// SPDX-License-Identifier: MIT
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("Oz")
#endif
#include "CHSave.h"
#include "detail/Card.h"
#include <CHGfx.h>

namespace chsave {
bool Store::load(uint32_t gameId, uint8_t schema, void *data, uint16_t size, uint8_t *flags) {
    gfx_wait();
    Journal journal(detail::card(), gfx_chunkScratch(), SCRATCH_BYTES);
    bool ok = journal.load(gameId, schema, data, size, flags);
    status_ = journal.status();
    return ok;
}
bool Store::store(uint32_t gameId, uint8_t schema, const void *data, uint16_t size, uint8_t flags) {
    gfx_wait();
    Journal journal(detail::card(), gfx_chunkScratch(), SCRATCH_BYTES);
    bool ok = journal.store(gameId, schema, data, size, flags);
    status_ = journal.status();
    return ok;
}
} // namespace chsave
