// SPDX-License-Identifier: MIT
#pragma once
#include "Fat.h"

namespace fat {
// These calls use the same aligned 512-byte scratch buffer as mount().
// Exact short-name lookup for callers that may write the result. Unlike
// match(), a conflicting entry or invalid cluster is E_CHAIN, not absent.
int8_t findExact(const File &dir, const char name[11], bool folder, File &out, uint8_t *scratch);

// Check that the chain ends at the advertised size. map() additionally
// fills one LBA per file block, up to the caller's capacity.
int8_t validate(const File &file, uint8_t *scratch);
int8_t map(const File &file, uint32_t *lbas, uint32_t capacity, uint8_t *scratch);
// Validate an exact three-block file and return its block locations.
int8_t mapThreeBlocks(const File &file, uint32_t (&lbas)[3], uint8_t *scratch);
int8_t sector(const File &file, uint32_t index, uint32_t &lba, uint8_t *scratch);
}
