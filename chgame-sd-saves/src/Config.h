// SPDX-License-Identifier: MIT
#pragma once
// Maximum sectors per record. Each file holds an ID sector and two records.
#ifndef CHSAVE_MAX_SECTORS_PER_RECORD
#ifdef CHSAVE_MAX_SLOT_SECTORS
#define CHSAVE_MAX_SECTORS_PER_RECORD CHSAVE_MAX_SLOT_SECTORS
#else
#define CHSAVE_MAX_SECTORS_PER_RECORD 1
#endif
#endif

// Keep the previous flag usable by existing builds.
#ifndef CHSAVE_MAX_SLOT_SECTORS
#define CHSAVE_MAX_SLOT_SECTORS CHSAVE_MAX_SECTORS_PER_RECORD
#elif CHSAVE_MAX_SLOT_SECTORS != CHSAVE_MAX_SECTORS_PER_RECORD
#error "CHSAVE_MAX_SLOT_SECTORS and CHSAVE_MAX_SECTORS_PER_RECORD disagree"
#endif

static_assert(CHSAVE_MAX_SECTORS_PER_RECORD >= 1 && CHSAVE_MAX_SECTORS_PER_RECORD <= 128,
              "CHSAVE_MAX_SECTORS_PER_RECORD must be between 1 and 128");
