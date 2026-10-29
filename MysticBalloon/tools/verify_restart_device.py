#!/usr/bin/env python3
"""Exercise 1.4.1 menus and restart on a temporary NEW run.

Requires a CHMB_MENU_DIAGNOSTICS build. Back up MYBLSAVE.DAT first and
restore that backup after testing; this appends one run and changes prefs.
Existing runs are never loaded for gameplay or restarted.
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
 def state():return {k:int(v) for k,v in (p.split('=') for p in d.query('V','MYBL ').split()[1:])}
 def tap(b):d.buttons(b);d.frames(1);d.buttons(0);d.frames(1)
 def check(ok,label):
  assert ok,label
  report['checks'].append(label);print('PASS:',label,flush=True)
 def until(n):
  end=time.monotonic()+4
  while time.monotonic()<end:
   d.frames(2);s=state()
   if s['state']==n:return s
   time.sleep(.025)
  raise AssertionError(s)
 def reset():d.cmd('R');d.frames(2)
 def conf():tap(8);tap(8);tap(1);return until(5)
 def confirm():
  tap(1);until(1)
  # CONF commits after its confirmation effect ends. R deliberately
  # discards pending diagnostic changes, so do not reload before that.
  time.sleep(.65);d.frames(4)
 try:
  report['hello']=d.handshake();d.cmd('L1');d.buttons(0);reset();before=state();report['before']=before
  check(before['media']==1 and before['slots']<8,'SD saves are available with room for one temporary run')
  tap(2);tap(3);check(state()['state']==1 and state()['slots']==before['slots'],'B and simultaneous A+B never accept a main-menu item')
  tap(1);s=state();check(s['state']==3 and s['choice']==before['slot']+1,'A opens PLAY with the last played run preselected')
  to_image(d.shot(),3).save(a.out/'saved-runs-before.png');tap(2);check(state()['state']==1,'B backs out of saved runs')
  conf();tap(128);tap(2);s=until(1)
  check(all(s[k]==before[k] for k in ('sound','palette','audio')),'B cancels CONF previews')
  reset();conf();tap(128);confirm();reset()
  check(state()['sound']!=before['sound'],'A confirms CONF changes and they survive reload')
  conf();tap(128);confirm();reset()
  check(all(state()[k]==before[k] for k in ('sound','palette','audio')),'original preferences restored before gameplay checks')
  tap(1)
  for _ in range(state()['choice']):tap(4)
  tap(1);created=until(6);check(created['slots']==before['slots']+1 and created['level']==1,'A on NEW appends an independent run')
  d.frames(30);tap(2);check(state()['state']==1,'B backs out of the level-start prompt')
  tap(1);tap(1);until(6);d.frames(30);tap(1);until(7)
  sys.path.insert(0,str(GAME/'tools/art'));from build_art import arrays
  data=arrays()['level1'];i=72;target=None
  while data[i]!=255:
   kind=data[i]&224;y=data[i]&31;x=data[i+1]&31;i+=2
   if kind==160:target=(x*16+2,y*16);break
   if kind==96:i+=1
  assert target
  d.cmd(f'J {target[0]} {target[1]} 0');d.frames(1);time.sleep(.35);d.frames(2)
  check(state()['coins']==5 and state()['score']==200,'normal coin collision records mid-level progress')
  tap(64);until(8);tap(8);tap(2);check(state()['state']==7 and state()['coins']==5,'B cancels the highlighted RESTART LEVEL action')
  tap(64);paused=until(8);report['paused']=paused
  tap(8);to_image(d.shot(),3).save(a.out/'pause-restart.png');tap(8);to_image(d.shot(),3).save(a.out/'pause-save-quit.png')
  tap(1);until(1);reset();tap(1);tap(1);loaded=until(8);report['loaded']=loaded
  check(all(loaded[k]==paused[k] for k in ('level','x','y','score','total','coins','key','balloons')) and loaded['entry']==0,'SAVE & QUIT and A LOAD retain the exact mid-level world and entry score')
  tap(8);tap(1);restarted=until(6);report['restarted']=restarted
  check(restarted['level']==1 and restarted['score']==0 and restarted['total']==0 and restarted['slots']==created['slots'],'A on RESTART LEVEL rolls back only the current attempt')
  d.frames(30);tap(1);fresh=until(7);check(fresh['coins']==6 and fresh['balloons']==3 and not fresh['key'],'restarted level has all coins, its key reset, and three balloons')
  tap(64);tap(8);tap(8);tap(3);check(state()['state']==7 and state()['slots']==created['slots'],'simultaneous A+B backs out instead of saving and quitting')
  d.cmd('L0');time.sleep(.2);d.cmd('P','PERF');start=time.monotonic();time.sleep(1);d.cmd('L1');elapsed=time.monotonic()-start
  perf={k:int(v) for k,v in (p.split('=') for p in d.cmd('P','PERF').split()[1:])};report['performance']=dict(seconds=elapsed,**perf)
  check(perf['late']==0 and perf['stk']<1900,'restarted gameplay has zero late frames and fits the 2 KiB stack')
  report['fault']=d.cmd('!','FAULT');check(report['fault']=='FAULT none','no recorded device fault')
  reset();d.cmd('L0')
 finally:
  (a.out/'results.json').write_text(json.dumps(report,indent=2)+'\n');t.close()
if __name__=='__main__':main()
