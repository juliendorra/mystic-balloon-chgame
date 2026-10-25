// Only display/storage are replaced: the original sound-trigger code runs.
#pragma once
#include <CHGame.h>
#define EEPROM_STORAGE_SPACE_START 16
#define bitRead(v,b) (((v) >> (b)) & 1)
inline char *ltoa(long v,char *s,int) { sprintf(s,"%ld",v); return s; }
inline char *itoa(int v,char *s,int) { sprintf(s,"%d",v); return s; }
struct Eeprom {
    uint8_t data[1024]={};
    uint8_t read(unsigned at) { return data[at]; }
    template<class T> void put(unsigned at,const T &v) { memcpy(data+at,&v,sizeof v); }
    template<class T> void get(unsigned at,T &v) { memcpy(&v,data+at,sizeof v); }
};
inline Eeprom EEPROM;
struct ReferenceAudio {
    static inline bool isOn=true;
    static bool enabled() { return isOn; }
    void on() { isOn=true; }
    void off() { isOn=false; }
    void saveOnOff() {}
};
struct Arduboy2Base {
    ReferenceAudio audio;
    bool pressed(uint8_t b) { return chgame.pressed(b); }
    bool justPressed(uint8_t b) { return chgame.justPressed(b); }
    bool everyXFrames(uint8_t n) { return uint16_t(chgame.frameCount) % n == 0; }
    void clear() {}
};
struct Sprites {
    void drawSelfMasked(int,int,const uint8_t *,uint8_t) {}
    void drawOverwrite(int,int,const uint8_t *,uint8_t) {}
    void drawErase(int,int,const uint8_t *,uint8_t) {}
    void drawPlusMask(int,int,const uint8_t *,uint8_t) {}
};
