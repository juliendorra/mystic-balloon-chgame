// Compile against both the original interpreter and the CHGame audio engine.
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef ATM_REFERENCE
#include "ATMlib.h"
#include "ATMlibimpl.h"
#include "MusicData.h"
ATMsynth synth;
static unsigned count=0;
static void play(unsigned n) {
    synth.play(n==1 ? mysticTitle : n>=2 ? gameTunes[n-2] : silentSong);
    memset(osc,0,sizeof osc); count=0;
    ATM_playroutine();
}
static void step() { ATM_playroutine(); }
static void effect(unsigned fx) {
    const uint8_t *list[]={sfxMove,sfxSelect,sfxJump,sfxStep,sfxLand,sfxWind,
        sfxHurt,sfxKey,sfxCoin,sfxPop,sfxSelect,sfxSelect};
    synth.playSfx(fx==10 ? sfxCountList[count>5 ? 3 : count++>>1] : list[fx],0);
}
#else
#include "Audio.h"
using namespace mystic;
// Beep synthesis is checked separately against the actual ArduboyTones ISR.
namespace mystic { namespace tones {
void begin(bool) {} void end() {} void setEnabled(bool) {}
bool playing() { return false; } void tone(uint16_t,uint16_t) {}
} }
static FILE *pcm=nullptr;
static unsigned sampleFraction=0;
static void play(unsigned n) { audio::begin(true,audio::Atm); audio::playSong(n); sampleFraction=0; }
static void step() {
    const auto tick=audio::ticks();
    while (audio::ticks()==tick) {
        audio::millisecond();
        sampleFraction+=31250;
        while (sampleFraction>=1000) {
            sampleFraction-=1000;
            const uint8_t b=audio::sample();
            assert(b>0 && b<255);
            if (pcm) fwrite(&b,1,1,pcm);
        }
    }
}
static void effect(unsigned fx) { audio::tone(200,100,audio::Effect(fx)); }
#endif

static void row(unsigned song,unsigned tick) {
    printf("T %u %u",song,tick);
    for (unsigned ch=0;ch<3;++ch) {
#ifdef ATM_REFERENCE
        const unsigned note=(atmSfxMask & (1u<<ch)) ? sfxV.note : channel[ch].note;
        printf(" %u %u",note,osc[ch].vol);
#else
        const auto v=audio::voice(ch);
        printf(" %u %u",v.note,v.volume);
#endif
    }
    putchar('\n');
}

int main(int argc,char **argv) {
    unsigned stack=0;
    for (unsigned song=0;song<7;++song) {
        play(song);
#ifndef ATM_REFERENCE
        char path[1024];snprintf(path,sizeof path,"%s/%u.pcm",argv[1],song);
        if (song) { pcm=fopen(path,"wb");assert(pcm); }
#endif
        for (unsigned t=0;t<2049;++t) {
            if (t) step();
            row(song,t);
#ifdef ATM_REFERENCE
            for (unsigned ch=0;ch<4;++ch)
                if (channel[ch].stackIndex>stack) stack=channel[ch].stackIndex;
#endif
        }
#ifndef ATM_REFERENCE
        if (pcm) fclose(pcm);pcm=nullptr;
#endif
    }
    for (unsigned fx=0;fx<12;++fx) {
        play(2);
        for (unsigned t=0;t<80;++t) {
            if (t==3 || t==4 || (fx==10 && t%8==0)) effect(fx);
            step();row(10+fx,t);
        }
    }
#ifdef ATM_REFERENCE
    fprintf(stderr,"Reference maximum tracker stack depth: %u\n",stack);
    assert(stack<=2);
#else
    play(1);step();
    const auto before=audio::ticks();
    audio::suspend();
    for (unsigned i=0;i<1000;++i) { audio::millisecond();assert(audio::sample()==128); }
    assert(audio::ticks()==before);
    audio::resume();step();assert(audio::ticks()>before);
    audio::setEnabled(false);
    for (unsigned i=0;i<1000;++i) { audio::millisecond();assert(audio::sample()==128); }
    audio::setEnabled(true);audio::update(7,3);assert(audio::song()==5);
    audio::setMode(audio::Beeps);assert(audio::saveReady());
    audio::setMode(audio::Atm);audio::update(1,0);assert(audio::song()==1);
    fprintf(stderr,"PASS: mixer bounds, PCM mute, flash suspension, resume and mode ownership\n");
#endif
}
