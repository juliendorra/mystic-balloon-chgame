#!/usr/bin/env python3
"""Check SD save behavior on a diagnostic build; appends two temporary runs.

Back up MYBLSAVE.DAT before running and restore it afterwards using the SD
reader. Existing runs are never selected for gameplay or overwritten.
"""
from pathlib import Path
import argparse,json,sys,time
GAME=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(GAME.parent/'CHGame/tools/chsim'))
from chdrivelib import Driver,SerialTransport
from fbimage import to_image

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--port',required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 t=SerialTransport(a.port);d=Driver(t,'MYBL ');report={'checks':[]}
 def state():return {k:int(v) for k,v in (part.split('=') for part in d.query('V','MYBL ').split()[1:])}
 def tap(b):d.buttons(b);d.frames(1);d.buttons(0);d.frames(1)
 def check(ok,text):
  assert ok,text
  report['checks'].append(text);print('PASS:',text,flush=True)
 def wait_for(predicate):
  end=time.monotonic()+3
  while time.monotonic()<end:
   d.frames(2);s=state()
   if predicate(s):return s
   time.sleep(.025)
  raise AssertionError(s)
 def reset():d.cmd('R');d.frames(2)
 def new_run():
  reset();s=state();tap(1)
  for _ in range(s['slot']+1 if s['slots'] else 0):tap(4)
  tap(1);return wait_for(lambda s:s['state']==6)
 try:
  report['hello']=d.handshake();d.cmd('L1');d.buttons(0);reset();initial=state();report['before']=initial
  check(initial['media']==1 and initial['slots']<=6,'SD is the primary backend and has space for the test runs')
  tap(1);to_image(d.shot(),3).save(a.out/'saved-runs-before.png');tap(1)
  original=wait_for(lambda s:s['state']==6);report['original']=original
  check(original['level']==16 and original['score']==16090 and original['total']==57,'original level-16 checkpoint and score migrated from flash to SD')
  created=new_run();check(created['slots']==initial['slots']+1 and created['level']==1,'NEW creates an independent level-1 run')
  # First level's first coin, read from the unchanged source data.
  sys.path.insert(0,str(GAME/'tools/art'));from build_art import arrays
  data=arrays()['level1'];i=72;coin=None
  while data[i]!=255:
   kind=data[i]&224;y=data[i]&31;x=data[i+1]&31;i+=2
   if kind==160:coin=(x*16+2,y*16);break
   if kind==96:i+=1
  assert coin
  d.frames(30);tap(1);wait_for(lambda s:s['state']==7)
  d.cmd(f'J {coin[0]} {coin[1]} 0');d.frames(1);time.sleep(.4);d.frames(4)
  collected=state();check(collected['coins']==5 and collected['score']==200,'normal coin collision advances the new run without touching the old one')
  tap(64);time.sleep(.3);d.frames(4);paused=state();image=d.shot();d.frames(20)
  stable=state();check(all(stable[k]==paused[k] for k in ('x','y','score','coins','balloons')) and d.shot()==image,'paused gameplay state and pixels stay frozen while saving')
  tap(8);tap(8);tap(1);wait_for(lambda s:s['state']==1);reset();tap(1)
  to_image(d.shot(),3).save(a.out/'saved-runs-progress.png');tap(1)
  resumed=wait_for(lambda s:s['state']==8);report['resumed']=resumed
  check(resumed['coins']==5 and resumed['score']==200 and resumed['level']==1 and resumed['balloons']==paused['balloons'],'SAVE & QUIT and reload retain mid-level coins, score and balloons')
  check(resumed['x']==paused['x'] and resumed['y']==paused['y'],'mid-level load restores the actual player position')
  tap(1);wait_for(lambda s:s['state']==7);d.cmd('L0');time.sleep(.2);d.cmd('P','PERF');start=time.monotonic();time.sleep(1);d.cmd('L1');elapsed=time.monotonic()-start
  perf={k:int(v) for k,v in (part.split('=') for part in d.cmd('P','PERF').split()[1:])};report['performance']=dict(seconds=elapsed,**perf)
  check(perf['late']==0 and perf['stk']<1900,'resumed gameplay has zero late frames and fits the 2 KiB stack')
  third=new_run();check(third['slots']==initial['slots']+2,'another NEW preserves both previous runs')
  reset();tap(1)
  # Current slot is the latest; move to the original run at row one.
  for _ in range(third['slot']):tap(4)
  tap(1);retained=wait_for(lambda s:s['state']==6)
  check(retained['level']==original['level'] and retained['score']==original['score'] and retained['total']==original['total'],'original save is still selectable and unchanged after two NEW runs')
  check(d.cmd('!','FAULT')=='FAULT none','no recorded device fault')
  reset();d.cmd('L0')
 finally:
  (a.out/'results.json').write_text(json.dumps(report,indent=2)+'\n');t.close()
if __name__=='__main__':main()
