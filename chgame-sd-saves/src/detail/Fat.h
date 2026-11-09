// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

namespace chsave { namespace fat {
enum Error : int8_t {
    OK = 0, E_READ = -1, E_NOFS = -10, E_EXFAT = -11,
    E_NOTFOUND = -12, E_CHAIN = -14
};
int8_t mount(uint8_t *scratch);
int8_t saveFile(uint32_t gameId, uint32_t &cluster, uint16_t &slotSectors, uint8_t *scratch);
int8_t sector(uint32_t cluster, unsigned fileSector, uint32_t &lba, uint8_t *scratch);
} }
