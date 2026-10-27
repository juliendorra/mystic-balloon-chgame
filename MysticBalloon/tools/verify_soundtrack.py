#!/usr/bin/env python3
"""Compare original/new ATM interpreter traces; render the real mixer to WAV."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import wave
from import_soundtrack import score_header

GAME=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(GAME.parent/'CHGame/tools/chsim'))
from chsim import find_cxx
OUT=GAME/'out/soundtrack'
OUT.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(GAME/'tools/import_soundtrack.py'),'--check'],check=True)
spec=json.loads((GAME/'tools/soundtrack-upstream.json').read_text())
for name,expected in spec['effect_calls'].items():
    source=re.sub(r'/\*.*?\*/|//[^\n]*','',(GAME/name).read_text(),flags=re.S)
    effects=re.findall(r'sound\.tone\(.*?,\s*mystic::audio::(\w+)\s*\);',source)
    assert effects==expected,(name,effects,expected)
print('PASS: original gameplay effect identities match every jump, step, land, wind, hurt, key, coin and pop trigger')
stage=OUT/'reference'
stage.mkdir(exist_ok=True)
vendor=GAME/'vendor/ATMlib'
# The reference is the untouched vendor interpreter with AVR peripheral
# stand-ins, not the generated CHGame port. Its original tuning table stays.
header=(vendor/'ATMlib.h').read_text().split('#define ATM__PHASE(N)',1)[0]+'\n#endif\n'
(stage/'ATMlib.h').write_text(header)
(stage/'ATMlibimpl.h').write_bytes((vendor/'ATMlibimpl.h').read_bytes())
(stage/'ATMcmds.h').write_bytes((vendor/'ATMcmds.h').read_bytes())
upstream=GAME/'vendor/MysticBalloon'
(stage/'ATMconfig.h').write_bytes((upstream/'ATMconfig.h').read_bytes())
(stage/'MusicData.h').write_text(score_header((upstream/'music.h').read_text()).replace('"atm/ATMcmds.h"','"ATMcmds.h"'))
(stage/'Arduino.h').write_text('''#pragma once
#include <stdint.h>
#include <string.h>
using byte=uint8_t; using word=uint16_t;
#define PROGMEM
inline uint8_t pgm_read_byte(const void *p) { return *(const uint8_t*)p; }
inline uint16_t pgm_read_word(const void *p) { const auto b=(const uint8_t*)p; return b[0]|(uint16_t(b[1])<<8); }
inline uint8_t TCCR4A,TCCR4B,TCCR4C,OCR4A,OCR4C,OCR4D;
#define ATMLIB_CONSTRUCT_ISR(x) uint16_t cia=625,cia_count=1; uint8_t TIMSK4=0;
''')
source=GAME/'tools/tests/soundtrack_test.cpp'
flags=find_cxx()+['-std=c++17','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all']
commands=[flags+['-DATM_REFERENCE',f'-I{stage}',str(source),'-o',str(OUT/'reference_test')],
          flags+['-DCHMB_AUDIO_TEST',f'-I{GAME}',str(source),str(GAME/'Audio.cpp'),'-o',str(OUT/'native_test')]]
traces=[]
for cmd,name in zip(commands,['reference','native']):
    subprocess.run(cmd,check=True)
    result=subprocess.run([str(OUT/f'{name}_test'),str(OUT)],capture_output=True,text=True,check=True)
    (OUT/f'{name}.txt').write_text(result.stdout)
    print(result.stderr.strip())
    traces.append(result.stdout)
assert traces[0]==traces[1],'Original/native score or SFX trace mismatch; see out/soundtrack/*.txt'
waves=[]
for n in range(1,7):
    pcm=(OUT/f'{n}.pcm').read_bytes()
    assert len(pcm)>31250*30
    assert min(pcm)>0 and max(pcm)<255 and max(pcm)-min(pcm)>40
    path=OUT/f'{n}.wav'
    with wave.open(str(path),'wb') as w:
        w.setnchannels(1);w.setsampwidth(1);w.setframerate(31250);w.writeframes(pcm)
    waves.append(dict(song=n,samples=len(pcm),min=min(pcm),max=max(pcm),sha256=hashlib.sha256(pcm).hexdigest()))
report=dict(events=len(traces[0].splitlines()),original_native_traces_identical=True,
            songs=7,effects=14,trace_ticks_per_song=2049,sample_rate=31250,wavs=waves,
            limits=['Digital engine and host register checks; hardware timing and speaker output require device measurements.'])
(OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(f"PASS: {report['events']} identical original/native tracker states; six non-clipping WAVs")
