// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

namespace chsave { namespace block {
// SPI1/LCD bus must be idle before these calls.
bool init();
bool read(uint32_t lba, uint8_t *sector);
bool write(uint32_t lba, const uint8_t *sector);
} }
