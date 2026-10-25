#include <Arduino.h>
#include "Tones.h"

namespace mystic { namespace tones {
static bool outputEnabled = false;

#ifdef CHSIM
// The simulator stays silent, but a test can observe every requested tone.
void (*trace)(uint16_t, uint16_t) = nullptr;
static uint32_t endAt = 0;
static bool active = false, indefinite = false;
void begin(bool on) { outputEnabled = on; active = false; }
void setEnabled(bool on) { outputEnabled = on; }
void tone(uint16_t hz, uint16_t duration) {
    const uint16_t frequency = hz ? hz : 250;
    const uint32_t count = uint32_t(duration) * frequency >> 9;
    endAt = micros() + (count + 1) * (1000000u / frequency) / 2;
    indefinite = !duration; active = true;
    if (trace) trace(hz, duration);
}
bool playing() { return active && (indefinite || int32_t(micros() - endAt) < 0); }
#else
extern "C" volatile uint32_t CFGHR_tmpB;
static volatile int32_t toggles = 0;
static volatile bool silent = true;

static void output(bool on) {
    // PB10 is the piezo pin. Like Arduboy2Audio, mute disconnects its output
    // while the tone's timer continues; unmute never restarts its duration.
    CFGHR_tmpB = (CFGHR_tmpB & ~(15u << 8)) | ((on ? 3u : 4u) << 8);
    GPIOB->CFGHR = CFGHR_tmpB;
}

static void stop() {
    TIM1->DMAINTENR = 0;
    TIM1->CTLR1 = 0;
    GPIOB->BCR = 1u << 10;
}

void begin(bool on) {
    __disable_irq();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOB | RCC_APB2Periph_TIM1;
    stop();
    TIM1->CTLR2 = 0; TIM1->SMCFGR = 0; TIM1->CCER = 0;
    TIM1->CHCTLR1 = 0; TIM1->CHCTLR2 = 0; TIM1->RPTCR = 0;
    TIM1->PSC = 23; // 48 MHz / 24 = 2 MHz, matching AVR timer 3 at 16 MHz / 8.
    TIM1->CNT = 0; TIM1->SWEVGR = 1; TIM1->INTFR = 0;
    toggles = 0; silent = true;
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    setEnabled(on);
    __enable_irq();
}

void setEnabled(bool on) { outputEnabled = on; output(on); }
bool playing() { return (TIM1->CTLR1 & 1) != 0; }

void tone(uint16_t hz, uint16_t duration) {
    __disable_irq();
    TIM1->CTLR1 = 0; TIM1->DMAINTENR = 0;
    silent = !hz || !outputEnabled;
    if (!hz) GPIOB->BCR = 1u << 10;
    const uint16_t frequency = hz ? hz : 250; // ArduboyTones' silent clock.
    // The game uses 80..425 Hz. These are the original timer's integer
    // divider and toggle-count rules, including its final stop interrupt.
    TIM1->ATRLR = 1000000u / frequency - 1;
    toggles = duration ? int32_t(uint32_t(duration) * frequency >> 9) : -1;
    // Retain the counter and pin phase when a new tone interrupts the old
    // one, as ArduboyTones does. The prescaler was latched once in begin().
    TIM1->INTFR = 0;
    TIM1->DMAINTENR = 1;
    TIM1->CTLR1 = 1;
    __enable_irq();
}

void interrupt() {
    TIM1->INTFR = 0;
    if (toggles) {
        if (!silent) GPIOB->OUTDR ^= 1u << 10;
        if (toggles > 0) --toggles;
    } else stop();
}
#endif

bool enabled() { return outputEnabled; }
} }

#ifndef CHSIM
extern "C" {
#ifdef __riscv
void TIM1_UP_IRQHandler() __attribute__((interrupt("WCH-Interrupt-fast")));
#endif
void TIM1_UP_IRQHandler() { mystic::tones::interrupt(); }
}
#endif
