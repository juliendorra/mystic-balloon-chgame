#!/usr/bin/env python3
"""Optional actual-firmware tests using a TIM3-correct CHGame headless emulator."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tempfile

GAME=Path(__file__).resolve().parents[1]
ROOT=GAME.parent

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--emulator',type=Path,required=True)
    ap.add_argument('--build',type=Path,default=GAME/'build/debug')
    ap.add_argument('--nm',type=Path,default=ROOT/'.toolchains/current/native/bin/riscv-none-elf-nm')
    args=ap.parse_args()
    out=GAME/'out/soundtrack/emulator'
    out.mkdir(parents=True,exist_ok=True)
    binary=args.build/'MysticBalloon.ino.bin'
    symbols=subprocess.check_output([str(args.nm),'-S',str(args.build/'MysticBalloon.ino.elf')],text=True)
    addresses={}
    for key in ['gameState','selectedMode','tickCounter','phases']:
        matches=[int(line.split()[0],16) for line in symbols.splitlines()
                 if (line.split()[-1]=='gameState' if key=='gameState' else line.split()[-1].endswith(key+'E'))]
        assert len(matches)==1,(key,matches)
        addresses[key]=matches[0]
    start=min(addresses.values())&~3
    count=(max(addresses.values())+8-start+3)//4
    cases=[]
    def run(name,program,seconds,buttons,save=False):
        cmd=[str(args.emulator.resolve()),str(program),str(seconds),str(out/f'{name}.ppm'),
             '--mem',f'{start:x}',str(count)]
        for button in buttons: cmd+=['--press',button]
        # The headless tool only LOADS a save when --save is specified too.
        if save: cmd+=['--save']
        result=subprocess.run(cmd,text=True,capture_output=True,check=True)
        log=result.stdout+result.stderr
        (out/f'{name}.txt').write_text(log)
        assert 'FAULT:' not in log,log
        words={int(a,16):int(v) for a,v in re.findall(r'mem ([0-9a-f]+) (\d+)',log)}
        def read(addr,n):
            return bytes((words[(p&~3)] >> (8*(p&3)))&255 for p in range(addr,addr+n))
        case=dict(case=name,seconds=seconds,frames=int(re.search(r'frames written (\d+)',log)[1]),
                  state=read(addresses['gameState'],1)[0],mode=read(addresses['selectedMode'],1)[0],
                  ticks=int.from_bytes(read(addresses['tickCounter'],4),'little'),
                  phases=list(read(addresses['phases'],6)),faults=False)
        cases.append(case)
        print('PASS:',name,'state',case['state'],'mode',case['mode'],'ticks',case['ticks'],flush=True)
        return case

    with tempfile.TemporaryDirectory(dir=out,prefix='fixture-') as temp:
        directory=Path(temp)
        gameplay=directory/'gameplay.bin';shutil.copyfile(binary,gameplay)
        buttons=['b@1.2:0.1','b@1.6:0.1','down@1.9:0.1','b@2.3:0.1',
                 'b@2.9:0.1','right@3.2:1','b@3.5:1']
        first=run('gameplay-8',gameplay,8,buttons)
        last=run('gameplay-12',gameplay,12,buttons)
        assert first['state']==last['state']==7 and first['mode']==last['mode']==0
        assert last['ticks']>first['ticks'] and first['phases']!=last['phases']
        fps=(last['frames']-first['frames'])/4
        assert 59.5<=fps<=60.5,fps
        mode_binary=directory/'modes.bin';shutil.copyfile(binary,mode_binary)
        buttons=['b@1.2:0.1','down@1.6:0.1','down@1.9:0.1','b@2.3:0.1',
                 'down@2.7:0.1','down@3.0:0.1','down@3.3:0.1','down@3.6:0.1',
                 'right@4.0:0.1','b@4.4:0.1']
        for name,seconds,presses,mode in [('atm-to-beeps',8,buttons,1),
                ('saved-beeps-reboot',3,[],1),('beeps-to-atm',8,buttons,0)]:
            case=run(name,mode_binary,seconds,presses,save=True)
            assert case['mode']==mode,case
            assert (case['ticks']>0)==(mode==0),case
    report=dict(firmware_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                emulator_sha256=hashlib.sha256(args.emulator.read_bytes()).hexdigest(),
                gameplay_window_fps=fps,cases=cases,
                limits=['Actual diagnostic RISC-V firmware in the native emulator; no physical board or acoustic measurement.'])
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS: {fps:.2f} fps measured gameplay window, PCM phases advance, CONF switches both ways, saved beeps survive reboot')

if __name__=='__main__': main()
