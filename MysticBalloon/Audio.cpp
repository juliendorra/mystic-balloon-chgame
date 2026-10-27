#include "Audio.h"
#include "Tones.h"
#include "src/atm/ATMlib.h"
#include "src/atm/ATMlibimpl.h"
#include "src/MusicData.h"
#if defined(__riscv) && !defined(CHSIM)
#include <Arduino.h>
#include <chgame/RamFunc.h>
#define MYBL_AUDIO_DEVICE 1
#else
#define MYBL_AUDIO_DEVICE 0
#define RAMFUNC(name)
#endif
#ifdef CHSIM
#include <Arduino.h>
#endif

namespace {
ATMsynth tracker;
volatile bool on = false;
volatile mystic::audio::Mode selectedMode = mystic::audio::Atm;
volatile bool ready = false, frozen = false;
uint8_t currentSong = 255, previousState = 255, previousLevel = 255, bonusIndex = 0;
volatile uint32_t tickCounter = 0;
#if CHGAME_DEBUG
volatile uint32_t sampleCounter = 0;
#endif
uint32_t clockFraction = 0;
uint16_t phases[3] = {};
struct Mix { uint16_t increment[3]; uint8_t volume[3]; };
Mix controls[2] = {};
volatile uint8_t front = 0;
#ifdef CHSIM
uint32_t hostTime = 0;
#endif

void publish() {
    const unsigned next = front ^ 1;
    for (unsigned i=0; i<3; ++i) {
        controls[next].increment[i] = osc[i].freq;
        controls[next].volume[i] = osc[i].vol;
    }
    asm volatile("" ::: "memory");
    front = next;
}

void play(unsigned n) {
    ready = false;
#if MYBL_AUDIO_DEVICE
    __disable_irq();
#endif
    tracker.play(n==1 ? mysticTitle : n>=2 && n<7 ? gameTunes[n-2] : silentSong);
    memset(osc,0,sizeof osc);
    memset(phases,0,sizeof phases);
    clockFraction=0; tickCounter=0;
    ATM_playroutine();
    publish();
    currentSong=n;
    ready=true;
#if MYBL_AUDIO_DEVICE
    __enable_irq();
#endif
}

#if MYBL_AUDIO_DEVICE
extern "C" volatile uint32_t CFGHR_tmpB;
void stopHardware() {
    TIM3->DMAINTENR=0; TIM3->CTLR1=0; TIM3->INTFR=0;
    NVIC_DisableIRQ(TIM3_IRQn);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    TIM1->CTLR1=0; TIM1->DMAINTENR=0; TIM1->CCER=0;
    GPIOB->BCR=1u<<10;
    CFGHR_tmpB=(CFGHR_tmpB & ~(15u<<8)) | (4u<<8);
    GPIOB->CFGHR=CFGHR_tmpB;
}
void startHardware() {
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB | RCC_APB2Periph_TIM1;
    RCC->APB1PCENR |= RCC_APB1Periph_TIM3;
    AFIO->PCFR1=(AFIO->PCFR1 & ~(7u<<15)) | (1u<<15);
    CFGHR_tmpB=(CFGHR_tmpB & ~(15u<<8)) | (11u<<8);
    GPIOB->CFGHR=CFGHR_tmpB;
    TIM1->CTLR1=0; TIM1->DMAINTENR=0; TIM1->CCER=0;
    TIM1->PSC=0; TIM1->ATRLR=255; TIM1->CNT=0;
    TIM1->CHCTLR1=0x6800; TIM1->CH2CVR=128;
    TIM1->RPTCR=0; TIM1->BDTR=0x8000; TIM1->CCER=0x10;
    TIM1->SWEVGR=1; TIM1->CTLR1=0x81; // 187.5 kHz PWM on PB10.
    TIM3->CTLR1=0; TIM3->DMAINTENR=0;
    TIM3->PSC=0; TIM3->ATRLR=1535; TIM3->CNT=0;
    TIM3->SWEVGR=1; TIM3->INTFR=0; TIM3->DMAINTENR=1;
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    NVIC_SetPriority(TIM3_IRQn,0);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CTLR1=1; // Exactly 31,250 PCM samples per second.
}
#endif
}

namespace mystic { namespace audio {
void begin(bool enabled, Mode m) {
    ready=false;
#if MYBL_AUDIO_DEVICE
    // Initialize/stop the exact-tone driver before touching its TIM1/PB10.
    tones::begin(false);
    tones::end();
    stopHardware();
#endif
    on=enabled; selectedMode=m; frozen=false;
    currentSong=previousState=previousLevel=255; bonusIndex=0;
    clockFraction=tickCounter=0;
#if CHGAME_DEBUG
    sampleCounter=0;
#endif
#ifdef CHSIM
    hostTime=millis();
#endif
    if (m==Beeps) tones::begin(enabled);
    else {
        play(0);
#if MYBL_AUDIO_DEVICE
        if (on) startHardware();
#endif
    }
}
void setMode(Mode m) { if (m!=selectedMode) begin(on,m); }
Mode mode() { return selectedMode; }
bool enabled() { return on; }
void setEnabled(bool value) {
    if (on==value) return;
    on=value;
    if (selectedMode==Beeps) tones::setEnabled(value);
    else {
        ready=false;
#if MYBL_AUDIO_DEVICE
        stopHardware();
#endif
        play(0); currentSong=255;
#if MYBL_AUDIO_DEVICE
        if (on) startHardware();
#endif
    }
}

void update(uint8_t state, uint8_t level) {
#ifdef CHSIM
    const uint32_t now=millis(), elapsed=now-hostTime;
    hostTime=now;
    for (uint32_t i=0;i<elapsed;++i) millisecond();
#endif
    if (state==6 && previousState!=6) bonusIndex=0;
    previousState=state;
    if (selectedMode!=Atm) return;
    unsigned want=0;
    if (on) {
        if (state<6) want=1;
        else if (state==7 || state==8)
            want=currentSong>=2 && currentSong<7 && level==previousLevel ? currentSong : 2+(level%TUNE_COUNT);
    }
    if (want!=currentSong) play(want);
    previousLevel=level;
}

void tone(uint16_t hz, uint16_t duration, Effect effect) {
    if (selectedMode==Beeps) { tones::tone(hz,duration); return; }
    if (!on || !ready) return;
    const uint8_t *track=sfxSelect;
    switch (effect) {
        case Move: track=sfxMove; break;
        case Select: case CountEnd: track=sfxSelect; break;
        case Jump: track=sfxJump; break;
        case Step: track=sfxStep; break;
        case Land: track=sfxLand; break;
        case Wind: track=sfxWind; break;
        case Hurt: track=sfxHurt; break;
        case Key: track=sfxKey; break;
        case Coin: track=sfxCoin; break;
        case Pop: track=sfxPop; break;
        case Count: track=sfxCountList[bonusIndex>5 ? 3 : bonusIndex>>1]; ++bonusIndex; break;
    }
#if MYBL_AUDIO_DEVICE
    __disable_irq();
#endif
    tracker.playSfx(track,0);
#if MYBL_AUDIO_DEVICE
    __enable_irq();
#endif
}
bool effectPlaying() {
    // The tracker clears this from SysTick while the foreground checks saves.
    return ready && *static_cast<volatile uint8_t *>(&atmSfxMask)!=0;
}
bool saveReady() { return selectedMode==Beeps ? !tones::playing() : !effectPlaying(); }
void suspend() {
    frozen=true;
#if MYBL_AUDIO_DEVICE
    if (selectedMode==Atm && on) { TIM3->CTLR1=0; TIM1->CH2CVR=128; }
#endif
}
void resume() {
#if MYBL_AUDIO_DEVICE
    if (selectedMode==Atm && on) { TIM3->INTFR=0; TIM3->CNT=0; TIM3->CTLR1=1; }
#endif
#ifdef CHSIM
    hostTime=millis();
#endif
    frozen=false;
}
void millisecond() {
    if (!on || selectedMode!=Atm || !ready || frozen || !TIMSK4) return;
    // Preserve ATMlib's integer AVR divider rather than substituting BPM.
    // SysTick bounds tracker scheduling jitter to one millisecond; the
    // higher-priority PCM interrupt keeps running through tracker work.
    clockFraction+=15625;
    const uint32_t period=uint32_t(cia)*1000;
    if (clockFraction>=period) {
        clockFraction-=period;
        ATM_playroutine(); ++tickCounter;
        publish();
    }
}
RAMFUNC(mysticSample) uint8_t sample() {
    if (!ready || !on || frozen) return 128;
#if CHGAME_DEBUG
    ++sampleCounter;
#endif
    const Mix &m=controls[front];
    for (unsigned i=0;i<3;++i) phases[i]+=m.increment[i];
    const unsigned p0=phases[0]>>8, p1=phases[1]>>8, p2=phases[2]>>8;
    // Original ATM waveforms: 25% pulse, square, half-amplitude saw.
    // The NES demo's triangle replacement does not belong in this score.
    int mix=p0>=192 ? -int(m.volume[0]) : int(m.volume[0]);
    mix+=p1&128 ? -int(m.volume[1]) : int(m.volume[1]);
    mix+=((int(p2)-128)*int(m.volume[2]))>>8;
    return uint8_t(128+mix);
}
Voice voice(unsigned ch) {
    if (ch>=3) return {};
    return {controls[front].increment[ch],controls[front].volume[ch],
            (atmSfxMask & (1u<<ch)) ? sfxV.note : channel[ch].note};
}
unsigned song() { return currentSong; }
uint32_t ticks() { return tickCounter; }
uint32_t samples() {
#if CHGAME_DEBUG
    return sampleCounter;
#else
    return 0;
#endif
}
#if defined(CHSIM) || defined(CHMB_AUDIO_TEST)
void playSong(unsigned n) { play(n<7 ? n : 0); }
#endif
} }

#if MYBL_AUDIO_DEVICE
extern "C" void osSystickHandler() { mystic::audio::millisecond(); }
extern "C" void TIM3_IRQHandler() __attribute__((interrupt("WCH-Interrupt-fast")));
extern "C" RAMFUNC(mysticAudioIRQ) void TIM3_IRQHandler() {
    TIM3->INTFR=~1u;
    TIM1->CH2CVR=mystic::audio::sample();
}
#endif
