#ifndef _ATMLIB_H_
#define _ATMLIB_H_
#include <stddef.h>
#include <inttypes.h>
#include <Arduino.h>
#include "ATMcmds.h"
#if __has_include("ATMconfig.h")
  #include "ATMconfig.h"
#endif

#ifndef ATM_WAVE_PULSE
#define ATM_WAVE_PULSE  0
#define ATM_WAVE_SQUARE 1
#define ATM_WAVE_NOISE  2
#define ATM_WAVE_SAW    3
#define ATM_WAVE_OFF    255
#endif
#ifndef ATM_WAVE_OFF
#define ATM_WAVE_OFF    255
#endif
#ifndef ATM_WAVE_CH0
#define ATM_WAVE_CH0 ATM_WAVE_PULSE
#endif
#ifndef ATM_WAVE_CH1
#define ATM_WAVE_CH1 ATM_WAVE_SQUARE
#endif
#ifndef ATM_WAVE_CH2
#define ATM_WAVE_CH2 ATM_WAVE_SAW
#endif
#ifndef ATM_WAVE_CH3
#define ATM_WAVE_CH3 ATM_WAVE_NOISE
#endif
#ifndef ATM_ALT_WIRING
#define ATM_ALT_WIRING 0
#endif

// Depth of the call stack of every channel (GOTO / REPEAT nesting). 7 = the old fixed size, 4 bytes of RAM per level per channel.
// A song needs no more than its deepest nesting (the generators in extras report it).
#ifndef ATM_STACK_DEPTH
#define ATM_STACK_DEPTH 7
#endif

// No ATMconfig.h: every effect, function, and octave stays in, as before.
#ifndef ATM_FX_VOLUME
#define ATM_FX_VOLUME 1
#endif
#ifndef ATM_FX_VOL_SLIDE
#define ATM_FX_VOL_SLIDE 1
#endif
#ifndef ATM_FX_FREQ_SLIDE
#define ATM_FX_FREQ_SLIDE 1
#endif
#ifndef ATM_FX_ARP
#define ATM_FX_ARP 1
#endif
#ifndef ATM_FX_NOISE
#define ATM_FX_NOISE 1
#endif
#ifndef ATM_FX_TRANSPOSE
#define ATM_FX_TRANSPOSE 1
#endif
#ifndef ATM_FX_TREMOLO
#define ATM_FX_TREMOLO 1
#endif
#ifndef ATM_FX_VIBRATO
#define ATM_FX_VIBRATO 1
#endif
#ifndef ATM_FX_GLISSANDO
#define ATM_FX_GLISSANDO 1
#endif
#ifndef ATM_FX_NOTE_CUT
#define ATM_FX_NOTE_CUT 1
#endif
#ifndef ATM_FX_CUE
#define ATM_FX_CUE 1
#endif
#ifndef ATM_FX_TEMPO
#define ATM_FX_TEMPO 1
#endif
#ifndef ATM_FX_GOTO_ADV
#define ATM_FX_GOTO_ADV 1
#endif
#ifndef ATM_FX_STOP
#define ATM_FX_STOP 1
#endif

#ifndef ATM_FUNC_STOP
#define ATM_FUNC_STOP 1
#endif
#ifndef ATM_FUNC_PAUSE
#define ATM_FUNC_PAUSE 1
#endif
#ifndef ATM_FUNC_RESUME
#define ATM_FUNC_RESUME 1
#endif
#ifndef ATM_FUNC_PLAYPAUSE
#define ATM_FUNC_PLAYPAUSE 1
#endif
#ifndef ATM_FUNC_MUTE
#define ATM_FUNC_MUTE 1
#endif
#ifndef ATM_FUNC_UNMUTE
#define ATM_FUNC_UNMUTE 1
#endif
#ifndef ATM_FUNC_SFX
#define ATM_FUNC_SFX 1
#endif
#ifndef ATM_FUNC_TEMPO
#define ATM_FUNC_TEMPO 1
#endif
#ifndef ATM_FUNC_CUE
#define ATM_FUNC_CUE 1
#endif

#ifndef ATM_OCTAVE_2
#define ATM_OCTAVE_2 1
#endif
#ifndef ATM_OCTAVE_3
#define ATM_OCTAVE_3 1
#endif
#ifndef ATM_OCTAVE_4
#define ATM_OCTAVE_4 1
#endif
#ifndef ATM_OCTAVE_5
#define ATM_OCTAVE_5 1
#endif
#ifndef ATM_OCTAVE_6
#define ATM_OCTAVE_6 1
#endif
#ifndef ATM_OCTAVE_7
#define ATM_OCTAVE_7 1
#endif

#if ATM_FUNC_PLAYPAUSE && (!ATM_FUNC_PAUSE || !ATM_FUNC_RESUME)
#error ATM_FUNC_PLAYPAUSE needs ATM_FUNC_PAUSE and ATM_FUNC_RESUME
#endif

#define CH_ZERO             0
#define CH_ONE              1
#define CH_TWO              2
#define CH_THREE            3

extern byte trackCount;
extern const word *trackList;
extern const byte *trackBase;
extern uint8_t pcm;

extern bool half;

class ATMsynth {

  public:
    ATMsynth() {};

    void play(const byte *song);
#if ATM_FUNC_PLAYPAUSE
    void playPause();
#endif
#if ATM_FUNC_PAUSE
    void pause();
#endif
#if ATM_FUNC_RESUME
    void resume();
#endif
#if ATM_FUNC_STOP
    void stop();
#endif
#if ATM_FUNC_MUTE
    void muteChannel(byte ch);
#endif
#if ATM_FUNC_UNMUTE
    void unMuteChannel(byte ch);
#endif

#if ATM_FUNC_SFX
    // Play a raw ATM_SFX_TRACK on one channel (music on other channels keeps going)
    void playSfx(const byte *track, byte ch);
#endif
#if ATM_FUNC_TEMPO
    // Set the tempo
    void setTempo(byte t);
#endif

#if ATM_FUNC_CUE
    // Last ATM_CUE byte from the song/SFX (0 = none since last read)
    uint8_t check();
    // 1 if last cue equals id (does not clear)
    uint8_t check(uint8_t id);
#endif
};

typedef struct {
  uint8_t  vol;
  uint16_t freq;
  uint16_t phase;
} osc_t;

typedef osc_t Oscillator;

extern osc_t osc[4];

uint16_t read_vle(const byte **pp);
static inline const byte *getTrackPointer(byte track);

extern void ATM_playroutine() asm("ATM_playroutine");

#define ATM__PHASE(N) \
                "lds  r18,                   osc+" #N "*%[mul]+%[fre]  " "\n\t" \
                "lds  r0,                    osc+" #N "*%[mul]+%[pha]  " "\n\t" \
                "add  r0,                    r18                  " "\n\t" \
                "sts  osc+" #N "*%[mul]+%[pha],   r0                   " "\n\t" \
                "lds  r18,                   osc+" #N "*%[mul]+%[fre]+1" "\n\t" \
                "lds  r1,                    osc+" #N "*%[mul]+%[pha]+1" "\n\t" \
                "adc  r1,                    r18                  " "\n\t" \
                "sts  osc+" #N "*%[mul]+%[pha]+1, r1                   " "\n\t"

#define ATM__MIX_PULSE(N) \
                "lds  r1,                    osc+" #N "*%[mul]+%[pha]+1" "\n\t" \
                "mov  r18,                   r1                   " "\n\t" \
                "lsl  r18                                         " "\n\t" \
                "and  r18,                   r1                   " "\n\t" \
                "lds  r27,                   osc+" #N "*%[mul]+%[vol]  " "\n\t" \
                "sbrc r18,                   7                    " "\n\t" \
                "neg  r27                                         " "\n\t" \
                "add  r26,                   r27                  " "\n\t"

#define ATM__MIX_SQUARE(N) \
                "lds  r1,                    osc+" #N "*%[mul]+%[pha]+1" "\n\t" \
                "lds  r27,                   osc+" #N "*%[mul]+%[vol]  " "\n\t" \
                "sbrc r1,                    7                    " "\n\t" \
                "neg  r27                                         " "\n\t" \
                "add  r26,                   r27                  " "\n\t"

#define ATM__MIX_NOISE(N) \
                "lds  r27,                   osc+" #N "*%[mul]+%[vol]  " "\n\t" \
                "sbrc r25,                   7                    " "\n\t" \
                "neg  r27                                         " "\n\t" \
                "add  r26,                   r27                  " "\n\t"

#define ATM__MIX_SAW(N) \
                "lds  r18,                   osc+" #N "*%[mul]+%[pha]+1" "\n\t" \
                "subi r18,                   128                  " "\n\t" \
                "lds  r27,                   osc+" #N "*%[mul]+%[vol]  " "\n\t" \
                "muls r18,                   r27                  " "\n\t" \
                "add  r26,                   r1                   " "\n\t"


#if (ATM_WAVE_CH0 == ATM_WAVE_NOISE) || (ATM_WAVE_CH1 == ATM_WAVE_NOISE) || (ATM_WAVE_CH2 == ATM_WAVE_NOISE) || (ATM_WAVE_CH3 == ATM_WAVE_NOISE)
#define ATM__NEED_NOISE 1
#else
#define ATM__NEED_NOISE 0
#endif

#if ATM_WAVE_CH0 == ATM_WAVE_OFF
#define ATM__CH0
#elif ATM_WAVE_CH0 == ATM_WAVE_PULSE
#define ATM__CH0 ATM__PHASE(0) ATM__MIX_PULSE(0)
#elif ATM_WAVE_CH0 == ATM_WAVE_SQUARE
#define ATM__CH0 ATM__PHASE(0) ATM__MIX_SQUARE(0)
#elif ATM_WAVE_CH0 == ATM_WAVE_SAW
#define ATM__CH0 ATM__PHASE(0) ATM__MIX_SAW(0)
#else
#define ATM__CH0 ATM__MIX_NOISE(0)
#endif

#if ATM_WAVE_CH1 == ATM_WAVE_OFF
#define ATM__CH1
#elif ATM_WAVE_CH1 == ATM_WAVE_PULSE
#define ATM__CH1 ATM__PHASE(1) ATM__MIX_PULSE(1)
#elif ATM_WAVE_CH1 == ATM_WAVE_SQUARE
#define ATM__CH1 ATM__PHASE(1) ATM__MIX_SQUARE(1)
#elif ATM_WAVE_CH1 == ATM_WAVE_SAW
#define ATM__CH1 ATM__PHASE(1) ATM__MIX_SAW(1)
#else
#define ATM__CH1 ATM__MIX_NOISE(1)
#endif

#if ATM_WAVE_CH2 == ATM_WAVE_OFF
#define ATM__CH2
#elif ATM_WAVE_CH2 == ATM_WAVE_PULSE
#define ATM__CH2 ATM__PHASE(2) ATM__MIX_PULSE(2)
#elif ATM_WAVE_CH2 == ATM_WAVE_SQUARE
#define ATM__CH2 ATM__PHASE(2) ATM__MIX_SQUARE(2)
#elif ATM_WAVE_CH2 == ATM_WAVE_SAW
#define ATM__CH2 ATM__PHASE(2) ATM__MIX_SAW(2)
#else
#define ATM__CH2 ATM__MIX_NOISE(2)
#endif

#if ATM_WAVE_CH3 == ATM_WAVE_OFF
#define ATM__CH3
#elif ATM_WAVE_CH3 == ATM_WAVE_PULSE
#define ATM__CH3 ATM__PHASE(3) ATM__MIX_PULSE(3)
#elif ATM_WAVE_CH3 == ATM_WAVE_SQUARE
#define ATM__CH3 ATM__PHASE(3) ATM__MIX_SQUARE(3)
#elif ATM_WAVE_CH3 == ATM_WAVE_SAW
#define ATM__CH3 ATM__PHASE(3) ATM__MIX_SAW(3)
#else
#define ATM__CH3 ATM__MIX_NOISE(3)
#endif

#if ATM__NEED_NOISE
#define ATM__LFSR \
                "ldi  r27,                   1                    " "\n\t" \
                "lds  r0,                    osc+3*%[mul]+%[fre]  " "\n\t" \
                "lds  r1,                    osc+3*%[mul]+%[fre]+1" "\n\t" \
                "add  r0,                    r0                   " "\n\t" \
                "adc  r1,                    r1                   " "\n\t" \
                "sbrc r1,                    7                    " "\n\t" \
                "eor  r0,                    r27                  " "\n\t" \
                "sbrc r1,                    6                    " "\n\t" \
                "eor  r0,                    r27                  " "\n\t" \
                "sts  osc+3*%[mul]+%[fre],   r0                   " "\n\t" \
                "sts  osc+3*%[mul]+%[fre]+1, r1                   " "\n\t" \
                "mov  r25,                   r1                   " "\n\t"
#define ATM__PUSH_N "push r25                                         " "\n\t"
#define ATM__POP_N  "pop  r25                                         " "\n\t"
#else
#define ATM__LFSR
#define ATM__PUSH_N
#define ATM__POP_N
#endif

#if ATM_ALT_WIRING
#define ATM__PWM_STORE \
                "sts  %[reg],                r26                  " "\n\t" \
                "sts  %[reg2],               r26                  " "\n\t"
#define ATM__PWM_REG2 [reg2] "M" _SFR_MEM_ADDR(OCR4D),
#else
#define ATM__PWM_STORE \
                "sts  %[reg],                r26                  " "\n\t"
#define ATM__PWM_REG2
#endif

#define ATMLIB_CONSTRUCT_ISR(TARGET_REGISTER) \
uint16_t __attribute__((used)) cia, __attribute__((used)) cia_count; \
ISR(TIMER4_OVF_vect, ISR_NAKED) { \
  asm volatile( \
                "push r2                                          " "\n\t" \
                "in   r2,                    __SREG__             " "\n\t" \
                "push r18                                         " "\n\t" \
                "lds  r18,                   half                 " "\n\t" \
                "com  r18                                         " "\n\t" \
                "sts  half,                  r18                  " "\n\t" \
                "breq 1f                                          " "\n\t" \
                "rjmp 3f                                          " "\n\t" \
                "1:                                               " "\n\t" \
                "push r27                                         " "\n\t" \
                "push r26                                         " "\n\t" \
                ATM__PUSH_N \
                "push r0                                          " "\n\t" \
                "push r1                                          " "\n\t" \
                \
                ATM__LFSR \
                "clr  r26                                         " "\n\t" \
                ATM__CH0 \
                ATM__CH1 \
                ATM__CH2 \
                ATM__CH3 \
                \
                "lds  r27,                   pcm                  " "\n\t" \
                "add  r26,                   r27                  " "\n\t" \
                "sts  %[reg],                r26                  " "\n\t" \
                \
                "lds  r27,                   cia_count+1          " "\n\t" \
                "lds  r26,                   cia_count            " "\n\t" \
                "sbiw r26,                   1                    " "\n\t" \
                "breq call_playroutine                            " "\n\t" \
                "sts  cia_count+1,           r27                  " "\n\t" \
                "sts  cia_count,             r26                  " "\n\t" \
                "rjmp 2f                                          " "\n\t" \
                \
                "call_playroutine:                                " "\n\t" \
                \
                "lds  r27, cia+1                                  " "\n\t" \
                "lds  r26, cia                                    " "\n\t" \
                "sts  cia_count+1,           r27                  " "\n\t" \
                "sts  cia_count,             r26                  " "\n\t" \
                \
                "sei                                              " "\n\t" \
                "push r19                                         " "\n\t" \
                "push r20                                         " "\n\t" \
                "push r21                                         " "\n\t" \
                "push r22                                         " "\n\t" \
                "push r23                                         " "\n\t" \
                "push r24                                         " "\n\t" \
                "push r25                                         " "\n\t" \
                "push r30                                         " "\n\t" \
                "push r31                                         " "\n\t" \
                \
                "clr  r1                                          " "\n\t" \
                "call ATM_playroutine                             " "\n\t" \
                \
                "pop  r31                                         " "\n\t" \
                "pop  r30                                         " "\n\t" \
                "pop  r25                                         " "\n\t" \
                "pop  r24                                         " "\n\t" \
                "pop  r23                                         " "\n\t" \
                "pop  r22                                         " "\n\t" \
                "pop  r21                                         " "\n\t" \
                "pop  r20                                         " "\n\t" \
                "pop  r19                                         " "\n\t" \
                "2:                                               " "\n\t" \
                "pop  r1                                          " "\n\t" \
                "pop  r0                                          " "\n\t" \
                ATM__POP_N \
                "pop  r26                                         " "\n\t" \
                "pop  r27                                         " "\n\t" \
                "3:                                               " "\n\t" \
                "pop  r18                                         " "\n\t" \
                "out  __SREG__,              r2                   " "\n\t" \
                "pop  r2                                          " "\n\t" \
                "reti                                             " "\n\t" \
                : \
                : [reg]  "M" _SFR_MEM_ADDR(TARGET_REGISTER), \
                  ATM__PWM_REG2 \
                  [mul]  "M" (sizeof(Oscillator)), \
                  [pha]  "M" (offsetof(Oscillator, phase)), \
                  [fre]  "M" (offsetof(Oscillator, freq)), \
                  [vol]  "M" (offsetof(Oscillator, vol)) \
              ); \
}

#ifndef ATMLIBIMPL_ONCE
#define ATMLIBIMPL_ONCE
#include "ATMlibimpl.h"
#endif

#endif
