#!/usr/bin/env python3
"""Check live CONF audio controls on an installed diagnostic, retaining saves."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GAME.parent / 'CHGame/tools/chsim'))
from chdrivelib import Driver, SerialTransport
from fbimage import to_image


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', required=True)
    ap.add_argument('--out', type=Path, default=GAME / 'out/audio-menu-device')
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    transport = SerialTransport(args.port)
    d = Driver(transport, 'MYBL ')
    report = dict(checks=[], modes=[], firmware_sha256=hashlib.sha256(
        (GAME / 'build/debug/MysticBalloon.ino.bin').read_bytes()).hexdigest())

    def state():
        return {k: int(v) for k, v in (p.split('=') for p in d.query('V', 'MYBL ').split()[1:])}

    def tap(button):
        d.buttons(button); d.frames(1); d.buttons(0); d.frames(1)

    def check(value, label):
        assert value, label
        report['checks'].append(label)
        print('PASS:', label, flush=True)

    def reload():
        d.cmd('R'); d.frames(2)
        return state()

    def open_audio():
        current = state()
        tap(8); tap(8); tap(1)
        for _ in range(4 - current['palette']):
            tap(8)
        check(state()['state'] == 5 and state()['palette'] == current['palette'],
              'sound rows restore the confirmed palette')

    def confirm():
        tap(1)
        time.sleep(.65); d.frames(3)
        check(state()['state'] == 1, 'A saves and returns to main menu')

    try:
        report['hello'] = d.handshake()
        check(report['hello'] == 'MYBL ' + json.loads((GAME / 'chgame.json').read_text())['version'],
              'installed diagnostic has the current release identity')
        d.cmd('L1'); d.buttons(0); reload()
        tap(1); tap(1)
        before = state()
        keys = ('level', 'score', 'total', 'saved', 'sound', 'palette', 'audio')
        baseline = {k: before[k] for k in keys}
        report['checkpoint_before'] = baseline
        check(before['state'] == 6, 'normal CONT loads the existing checkpoint')
        reload(); open_audio()
        tap(16)
        check(state()['state'] == 5 and state()['audio'] != baseline['audio'],
              'Left changes audio mode immediately without leaving CONF')
        tap(8); tap(32)
        check(state()['sound'] != baseline['sound'] and state()['palette'] == baseline['palette'],
              'Right changes sound immediately without changing the confirmed palette')
        tap(2)
        check(all(state()[k] == baseline[k] for k in ('sound', 'palette', 'audio')),
              'B restores sound, audio mode and palette together')
        reload()

        for wanted in (1, 0):
            open_audio()
            if state()['audio'] != wanted:
                tap(32)
            check(state()['state'] == 5 and state()['audio'] == wanted,
                  f'audio row displays and previews mode {wanted}')
            tap(8)
            if not state()['sound']:
                tap(16)
            d.frames(80)
            to_image(d.shot(), 3).save(args.out / f'conf-mode-{wanted}-on.png')
            first = state(); time.sleep(.2); last = state()
            if wanted == 0:
                check(last['song'] == 1 and last['samples'] > first['samples'] + 5000 and
                      last['ticks'] > first['ticks'], 'ATM preview runs title music and the physical PCM sampler')
            else:
                check(last['samples'] == first['samples'] == 0, 'beep mode releases the PCM sampler')
            tap(32)
            first = state(); time.sleep(.2); d.frames(80); last = state()
            check(last['sound'] == 0 and last['samples'] == first['samples'] and
                  last['ticks'] == first['ticks'], f'muting mode {wanted} stops the preview')
            to_image(d.shot(), 3).save(args.out / f'conf-mode-{wanted}-off.png')
            tap(16); confirm()
            loaded = reload()
            check(loaded['audio'] == wanted and loaded['sound'] == 1 and
                  loaded['palette'] == baseline['palette'], f'mode {wanted} and sound on survive flash reload')
            report['modes'].append(dict(mode=wanted, persisted_after_reload=True, sampler_active=wanted == 0))

        open_audio()
        if state()['audio'] != baseline['audio']:
            tap(16)
        tap(8)
        if state()['sound'] != baseline['sound']:
            tap(32)
        confirm(); reload(); tap(1); tap(1)
        final = {k: state()[k] for k in keys}
        check(final == baseline, 'original checkpoint and every preference retained after all tests')
        report['checkpoint_after'] = final
        check(state()['dropped'] == 0 and d.cmd('!', 'FAULT') == 'FAULT none', 'no dropped menu sprites or device faults')
        reload(); d.cmd('L0')
    finally:
        (args.out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        transport.close()


if __name__ == '__main__':
    main()
