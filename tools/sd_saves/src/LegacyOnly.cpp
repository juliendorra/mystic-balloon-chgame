// Mystic Balloon has its own larger SD journal. Keep the SDK interface
// read-only there, so a missing card can never enable shared flash writes.
#include "Save.h"
namespace save {
Status status(){return NoCard;}
bool available(){return false;}
const void *read(uint32_t m,uint8_t v,uint16_t s,uint8_t *f){return readLegacy(m,v,s,f);}
void *buffer(){return nullptr;}
bool write(uint32_t,uint8_t,uint16_t,uint8_t){return false;}
}
