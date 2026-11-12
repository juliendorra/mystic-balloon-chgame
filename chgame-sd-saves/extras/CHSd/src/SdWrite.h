// SPDX-License-Identifier: MIT
#pragma once
#include "SdSpi.h"

namespace sd {
// Write one 512-byte block and wait for the card to accept it. The same
// SPI1 bus rule as read() applies. Read back data that needs verification.
bool write(uint32_t lba, const uint8_t *src);
}
