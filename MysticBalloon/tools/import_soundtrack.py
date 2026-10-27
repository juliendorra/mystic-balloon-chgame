#!/usr/bin/env python3
"""Import the pinned Mystic Balloon ATM scores without changing their bytes."""
from pathlib import Path
import argparse
import hashlib
import json
import urllib.request

GAME=Path(__file__).resolve().parents[1]

def score_header(source):
    score=source.split('// ATMlib does not look at the Arduboy sound setting')[0]
    score=score.replace('#ifndef MUSIC_H\n#define MUSIC_H','#pragma once')
    score=score.replace('#include <ATMcmds.h>','#include "atm/ATMcmds.h"')
    silent=source.split('Song silentSong[] = {')[1].split('\n};')[0]
    return ('// Imported without changing score/effect bytes; see tools/soundtrack-upstream.json.\n'
            +score+'\nSong silentSong[] = {'+silent+'\n};\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream',type=Path,default=GAME/'vendor/MysticBalloon')
    parser.add_argument('--fetch',action='store_true')
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    spec=json.loads((GAME/'tools/soundtrack-upstream.json').read_text())
    for name,digest in spec['files'].items():
        path=args.upstream/Path(name).name
        if not path.exists() and args.fetch:
            path.parent.mkdir(parents=True,exist_ok=True)
            url=f"https://raw.githubusercontent.com/{spec['repository']}/{spec['commit']}/{name}"
            path.write_bytes(urllib.request.urlopen(url,timeout=30).read())
        if not path.exists():
            raise SystemExit(f'Missing {path}; use --fetch once.')
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digest,name
    header=score_header((args.upstream/'music.h').read_text())
    destination=GAME/'src/MusicData.h'
    if args.check:
        assert destination.read_text()==header,'Imported score changed; regenerate with import_soundtrack.py'
    else:
        destination.write_text(header)
    print('PASS: commit-pinned title, five gameplay scores and fourteen ATM effects imported unchanged')

if __name__=='__main__': main()
