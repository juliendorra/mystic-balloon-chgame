// SPDX-License-Identifier: MIT
#pragma once
#include "../Id.h"

namespace chsave { namespace detail {
struct InitialFile {
    const Id &id;
    uint16_t sectors;
    uint32_t crc;
};
// CHSd calls this with a cleared block, once per sector in order.
void initializeSector(uint32_t index, uint8_t *sector, void *context);
} }
