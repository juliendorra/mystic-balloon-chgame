// Register stand-ins for executing both actual tone drivers on the host.
#pragma once
#include <stdint.h>
#define F_CPU 16000000UL
using boolean = bool;
extern uint8_t PORTC, DDRC, TIMSK3, TCCR3A, TCCR3B;
extern uint16_t OCR3A, TCNT3;
enum { PORTC6=6, PORTC7=7, OCIE3A=1, WGM32=3, CS30=0, CS31=1 };
#define _BV(b) (1u << (b))
#define bitSet(r,b) ((r) |= _BV(b))
#define bitClear(r,b) ((r) &= ~_BV(b))
#define bitRead(r,b) (((r) >> (b)) & 1)
#define bitWrite(r,b,v) ((v) ? bitSet(r,b) : bitClear(r,b))
#define ISR(name) void name()
#define pgm_read_word(p) (*(p))
struct TimerRegisters {
    uint32_t CTLR1=0,CTLR2=0,SMCFGR=0,DMAINTENR=0,CCER=0,
             CHCTLR1=0,CHCTLR2=0,RPTCR=0,PSC=0,CNT=0,SWEVGR=0,INTFR=0,ATRLR=0;
};
struct ClearRegister {
    uint32_t *out;
    void operator=(uint32_t bits) { *out &= ~bits; }
};
struct GpioRegisters {
    uint32_t OUTDR=0,CFGHR=0;
    ClearRegister BCR{&OUTDR};
};
struct ClockRegisters { uint32_t APB2PCENR=0; };
extern TimerRegisters timerRegisters;
extern GpioRegisters gpioRegisters;
extern ClockRegisters clockRegisters;
#define TIM1 (&timerRegisters)
#define GPIOB (&gpioRegisters)
#define RCC (&clockRegisters)
enum { RCC_APB2Periph_GPIOB=1, RCC_APB2Periph_TIM1=2, TIM1_UP_IRQn=35 };
inline void NVIC_EnableIRQ(int) {}
inline void NVIC_DisableIRQ(int) {}
inline void NVIC_ClearPendingIRQ(int) {}
inline void __disable_irq() {}
inline void __enable_irq() {}
