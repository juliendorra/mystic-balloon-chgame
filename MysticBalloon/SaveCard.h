#pragma once
#include <stdint.h>
namespace mystic { namespace card {
enum Result : uint8_t { Absent, Ready, Missing, Failed };
Result begin();
bool read(unsigned sector,uint8_t *dst);
bool write(unsigned sector,const uint8_t *src);
#ifdef CHSIM
void testCutAfter(int bytes);
#endif
} }
