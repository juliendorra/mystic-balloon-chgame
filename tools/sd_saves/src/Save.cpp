// Personal games use CHSave explicitly. Old SDK save calls are disabled
// here; the SDK-only overlay selects CompactSdk.cpp separately.
#include "Save.h"
namespace save {
Status status(){return NoCard;}
bool available(){return false;}
const void *read(uint32_t,uint8_t,uint16_t,uint8_t*){return nullptr;}
void *buffer(){return nullptr;}
bool write(uint32_t,uint8_t,uint16_t,uint8_t){return false;}
}
