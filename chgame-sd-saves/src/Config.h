// SPDX-License-Identifier: MIT
#pragma once
// Optional flash-saving profile. File capacity is discovered at runtime;
// this only sets the largest slot the compiled firmware can handle.
#ifndef CHSAVE_MAX_SLOT_SECTORS
#define CHSAVE_MAX_SLOT_SECTORS 128
#endif
static_assert(CHSAVE_MAX_SLOT_SECTORS >= 1 && CHSAVE_MAX_SLOT_SECTORS <= 128,
              "CHSAVE_MAX_SLOT_SECTORS must be between 1 and 128");
