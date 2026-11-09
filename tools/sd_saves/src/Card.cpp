#pragma GCC optimize("Oz")
#include "Card.h"
#if defined(CHSIM) && !defined(CHGAME_SAVE_SHARED_TEST)
#include "card/Fat.h"
#include "card/SdSpi.h"
#else
#include <Fat.h>
#include <SdSpi.h>
namespace save_fat=fat;
namespace save_sd=sd;
#endif
#if defined(CHSIM) && !defined(CHGAME_SAVE_SHARED_TEST)
namespace save_fat {int8_t saveFile(uint32_t,uint32_t*,uint8_t*);}
#else
namespace fat {int8_t saveFile(uint32_t,uint32_t*,uint8_t*);}
#endif
namespace save_card {
static uint32_t sectors[2];
save::Status open(uint32_t magic,uint8_t *scratch) {
    if(!save_sd::init())return save::NoCard;
    if(save_fat::mount(scratch))return save::Failed;
    int found=save_fat::saveFile(magic,sectors,scratch);
    if(found)return found==save_fat::E_NOTFOUND?save::NeedsSetup:save::Damaged;
    return save::Ready;
}
bool read(unsigned slot,uint8_t *dst) {return slot<2 && save_sd::read(sectors[slot],dst);}
bool write(unsigned slot,const uint8_t *src) {return slot<2 && save_sd::write(sectors[slot],src);}
}
