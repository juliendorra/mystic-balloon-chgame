#pragma once
#include <stdint.h>

namespace mystic { namespace audio {
enum Mode : uint8_t { Atm, Beeps };
enum Effect : uint8_t { Move, Select, Jump, Step, Land, Wind, Hurt, Key,
    Coin, Pop, Count, CountEnd };
void begin(bool enabled, Mode mode);
void setEnabled(bool enabled);
bool enabled();
void setMode(Mode mode);
Mode mode();
void tone(uint16_t hz, uint16_t duration, Effect effect);
void update(uint8_t state, uint8_t level);
bool saveReady();
void suspend();
void resume();
// The same tracker and mixer run on hardware and in the WAV/reference tests.
void millisecond();
uint8_t sample();
struct Voice { uint16_t increment; uint8_t volume, note; };
Voice voice(unsigned channel);
unsigned song(); // 0 = silence, 1 = title, 2..6 = gameplay
uint32_t ticks();
uint32_t samples(); // Diagnostic counter: actual sampler ISR calls.
bool effectPlaying();
#if defined(CHSIM) || defined(CHMB_AUDIO_TEST)
void playSong(unsigned song);
#endif
} }
