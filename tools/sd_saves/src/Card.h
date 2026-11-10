#pragma once
#if __has_include(<CHSave.h>)
#include <CHSave.h>
#endif
#include <stdint.h>
#include "Save.h"
namespace save_card {
#if __has_include(<CHSave.h>)
chsave::Status create(const chsave::Id &,uint16_t,uint8_t *);
#endif
save::Status open(uint32_t magic,uint8_t *scratch);
save::Status open(const char *basename,uint8_t *scratch);
bool read(unsigned slot,uint8_t *dst);
bool write(unsigned slot,const uint8_t *src);
}
