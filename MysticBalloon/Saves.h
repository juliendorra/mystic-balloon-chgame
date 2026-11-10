#pragma once
#include "Port.h"

namespace mystic { namespace saves {
constexpr uint8_t MAX_RUNS=8, NO_RUN=255;
constexpr uint8_t VERSION=3;
// World state, not just a level number. Static map geometry and cosmetic
// particles are regenerated; moved coins, injured enemies and motion persist.
struct Run {
    uint32_t score=0;
    int16_t x=0,y=0,coinX[6]={},walkerX[6]={};
    uint16_t walkers=0,kidFlags=0;
    int8_t vx=0,vy=0,hp[6]={};
    uint8_t level=0,total=0,objects=63,immune=0,jump=0,balloons=3,hurt=0,phase=0;
    uint32_t entryScore=0;
};
static_assert(sizeof(Run)==56,"portable compact run format");
struct Store {
    uint32_t generation=0;
    Progress prefs;
    uint8_t count=0,active=NO_RUN;
    uint16_t reserved=0;
    Run runs[MAX_RUNS];
};
static_assert(sizeof(Store)==468,"save layout");
enum Status : uint8_t { NoCard, Sd, Damaged, WriteFailed, NoSpace };
enum Action : uint8_t { None, New, Load };
extern Status status;
extern uint8_t count,active;
extern Action action;
constexpr uint8_t MID_LEVEL=128, HAS_KEY=64;
uint8_t capacity();
bool read(Store &out);
bool write(Store &value);
bool create();
bool select(uint8_t index,Run &out);
bool checkpoint(const Run &run);
bool preferences();
bool valid(const Store &value,unsigned limit);
uint8_t percent(const Run &run);
void label(const Run &run,char *text);
uint32_t crc(const void *data,unsigned bytes);
void begin();
} }
