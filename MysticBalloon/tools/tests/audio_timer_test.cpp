// Run the real ArduboyTones source and the real CHGame timer/ISR side by side.
#include <Arduino.h>
#include <ArduboyTones.h>
#include "../../Tones.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>

uint8_t PORTC=0, DDRC=0, TIMSK3=0, TCCR3A=0, TCCR3B=0;
uint16_t OCR3A=0, TCNT3=0;
TimerRegisters timerRegisters;
GpioRegisters gpioRegisters;
ClockRegisters clockRegisters;
extern "C" { volatile uint32_t CFGHR_tmpB=0; void TIM1_UP_IRQHandler(); }
void TIMER3_COMPA_vect();
static bool audible=true;
static bool enabled() { return audible; }
static ArduboyTones reference(enabled);

static void mute(bool on) { audible=on; mystic::tones::setEnabled(on); }
static void reset() {
    reference.noTone(); TCNT3=0;
    mystic::tones::begin(true); mute(true);
}
static void compare() {
    assert(mystic::tones::playing()==reference.playing());
    assert(bool(TIM1->CTLR1&1)==bool(TCCR3B));
    assert(bool(TIM1->DMAINTENR&1)==bool(TIMSK3&_BV(OCIE3A)));
    assert(bool(GPIOB->OUTDR&(1u<<10))==bool(PORTC&_BV(PORTC6)));
}
static void play(unsigned hz,unsigned duration) {
    reference.tone(hz,duration); mystic::tones::tone(hz,duration);
    assert(TIM1->PSC==23 && TIM1->ATRLR==OCR3A);
    assert(TIM1->CNT==TCNT3); // Replacing a note retains timer phase.
    compare();
}
static void tick() {
    if (TCCR3B) TIMER3_COMPA_vect();
    if (TIM1->CTLR1&1) TIM1_UP_IRQHandler();
    compare();
}
int main() {
    unsigned cases=0;
    // Include all 20 wind frequencies and every bonus-note frequency,
    // crossed with every duration used by the game (including rests).
    for (unsigned hz : {0u,80u,131u,139u,145u,150u,152u,172u,188u,200u,
                       213u,255u,300u,320u,321u,322u,323u,324u,325u,326u,
                       327u,328u,329u,330u,331u,332u,333u,334u,335u,336u,
                       337u,338u,339u,400u,420u,425u})
      for (unsigned duration : {20u,30u,100u,150u,200u}) {
        reset(); play(hz,duration);
        unsigned n=0;
        while (TCCR3B) { tick(); assert(++n<1000); }
        printf("TIMER %u %u %u %u\n",hz,duration,OCR3A+1,n);
        // Same behavior when requested while muted; enabling does not
        // turn that silent request into an old, restarted effect.
        reset(); mute(false); play(hz,duration); tick(); mute(true);
        while (TCCR3B) tick();
        ++cases;
      }
    reset(); play(420,200);
    for (unsigned i=0;i<13;++i) tick();
    // A replacement changes the note immediately, without a queue or a
    // 1 kHz start delay, and without zeroing a partially elapsed period.
    TCNT3=TIM1->CNT=123;
    play(131,150);
    for (unsigned i=0;i<7;++i) tick();
    mute(false); for (unsigned i=0;i<3;++i) tick(); mute(true);
    play(425,20); while (TCCR3B) tick();
    reset(); play(80,0); for (unsigned i=0;i<100;++i) tick();
    play(200,100); while (TCCR3B) tick();
    printf("PASS: %u timer cases, pin edges, exact stops, mute/unmute, interruption, phase retention and indefinite tones\n",cases);
}
