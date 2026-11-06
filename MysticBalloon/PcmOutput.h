/// @file PcmOutput.h
/// @brief Bounded PCM output for the Rev0 buzzer's 8-bit PWM timer.
#pragma once
#include <stdint.h>

/// @brief Output conditioning for signed PCM mixed at 31,250 samples/second.
namespace chgame_pcm {

/// @brief Remove waveform DC, raise quiet mixes and compress loud peaks.
/// @details This leaves score pitches, instruments and relative voice levels
/// intact. The approximately 10 Hz DC tracker removes the bias of asymmetric
/// pulse waves before 2x gain. Above magnitude 80, a 4:1 knee reduces peaks;
/// a final bound of 120 leaves PWM headroom. Master volume follows compression
/// so quieter settings still get quieter. No allocation or extra timer is used.
/// Call only from the sample producer; each independent mixer needs its own
/// instance. This is digital conditioning, not a model of speaker acoustics.
class Output {
    int32_t dcQ16 = 0;
public:
    /// @brief Forget the DC estimate and return neutral PWM immediately.
    /// @return 128, the silent midpoint of an unsigned 8-bit PCM stream.
    __attribute__((always_inline)) inline uint8_t silence() { dcQ16 = 0; return 128; }

    /// @brief Condition one signed mixer sample.
    /// @param sample Signed sum of voices, in the range -512 to 512.
    /// @param volume Master volume, 0 (silent) through 4 (full).
    /// @return Unsigned PWM duty, bounded to 8 through 248 at full volume.
    __attribute__((always_inline)) inline uint8_t write(int sample, unsigned volume = 4) {
        if (!volume) return silence();
        if (volume > 4) volume = 4;
        const int32_t target = sample * 65536;
        dcQ16 += (target - dcQ16) >> 9;
        int value = (target - dcQ16) / 32768;  // DC removal and 2x gain.
        if (value > 80) value = 80 + (value - 80) / 4;
        else if (value < -80) value = -80 + (value + 80) / 4;
        if (value > 120) value = 120;
        else if (value < -120) value = -120;
        return uint8_t(128 + value * int(volume) / 4);
    }
};
}  // namespace chgame_pcm
