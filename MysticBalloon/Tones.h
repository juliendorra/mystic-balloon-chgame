#pragma once
#include <stdint.h>

namespace mystic { namespace tones {
// ArduboyTones-compatible single voice. Durations are in 1/1024 seconds,
// counted as half-wave toggles, rather than the shared 20 Hz/2 ms effects.
void begin(bool enabled);
void setEnabled(bool enabled);
bool enabled();
bool playing();
void tone(uint16_t hz, uint16_t duration);
#ifdef CHSIM
extern void (*trace)(uint16_t hz, uint16_t duration);
#endif
} }
