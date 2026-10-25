#pragma once
#include <stdint.h>
void audioTrace(uint16_t, uint16_t);
struct ArduboyTones {
    explicit ArduboyTones(bool (*)()) {}
    void tone(uint16_t hz, uint16_t duration) { audioTrace(hz,duration); }
};
