// SPDX-License-Identifier: MIT
#pragma once
#include "FatAccess.h"

namespace fat {
// These calls use the same aligned 512-byte scratch buffer as mount().
constexpr int8_t E_NOSPACE = -15, E_WRITE = -16, E_EXISTS = -17;

// Mounts as mount() does, also checking that writes can use all FAT copies.
// Re-mount with this before writing to a different card.
int8_t mountWritable(uint8_t *scratch);

// Fills one sector of a newly allocated file. Called in increasing order,
// with scratch initially zeroed. The callback may not do SD or FAT I/O.
using InitSector = void (*)(uint32_t index, uint8_t *scratch, void *context);

// Creates a whole-block file in the given root subdirectory (or root if
// folder is null). Creates the folder if needed. Names are literal, padded
// 8.3 names. Returns E_EXISTS for an existing file. Data and FAT writes are read
// back before publishing the directory entry. An interruption may strand
// allocated clusters; this API is not a general FAT transaction layer.
int8_t createFile(const char *folder, const char name[11], uint32_t blocks,
                  InitSector init, void *context, uint8_t *scratch);

// Same write guarantees as createFile, specialized for three blocks in
// a named root subdirectory. Names use A–Z, 0–9 and spaces. Compact builds
// can omit the general allocator.
int8_t createThreeBlockFile(const char *folder, const char name[11],
                           InitSector init, void *context, uint8_t *scratch);

// Same as createThreeBlockFile for files of one to five blocks.
int8_t createSmallFile(const char *folder, const char name[11], uint32_t blocks,
                       InitSector init, void *context, uint8_t *scratch);
}
