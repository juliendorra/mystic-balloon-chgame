// Fault injection for the save tests; no legacy storage backend in firmware.
#include "SaveCard.h"
#ifdef CHSIM
namespace mystic { namespace card {
static int cut=-1;
void testCutAfter(int bytes) { cut=bytes; }
int testCutBytes() { return cut; }
} }
#endif
