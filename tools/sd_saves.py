#!/usr/bin/env python3
"""Provision isolated SD saves, generate the SDK overlay, and run CHGame tools."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib
try:
    import fcntl
except ImportError:
    fcntl=None

ROOT=Path(__file__).resolve().parents[1]
CONFIG=json.loads((ROOT/'save-config.json').read_text())
SDK_ROOT=Path(os.environ.get('CHGAME_ROOT',str(ROOT/'CHGame'))).resolve()
if not (SDK_ROOT/'tools/chgame.py').is_file() and (ROOT/'.deps/CHGame/tools/chgame.py').is_file():SDK_ROOT=ROOT/'.deps/CHGame'
SDK=SDK_ROOT/'platform/board/arduino/CHGame/libraries'
SOURCE=ROOT/'tools/sd_saves/src'
PUBLIC=ROOT/'chgame-sd-saves'
OVERLAY=ROOT/'.cache/sd-saves/libraries'

def shared_prepare():
    spec=importlib.util.spec_from_file_location('chsave_prepare',PUBLIC/'tools/prepare.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def catalog():
    shared=shared_prepare()
    return {shared.save_path(CONFIG['id']):{'game':CONFIG['game'],'id':CONFIG['id']}}

def validate(raw,ident):
    shared_prepare().validate(raw,ident)

def apply_sdk_patch():
    pass # Standalone games do not patch the SDK examples.

def overlay():
    OVERLAY.parent.mkdir(parents=True,exist_ok=True)
    with (OVERLAY.parent/'overlay.lock').open('a') as lock:
        if fcntl:fcntl.flock(lock,fcntl.LOCK_EX)
        return _overlay()

def allocation_fat(text):
    # Keep the mounted geometry once; the allocator shares this reader.
    text='''#if __has_include(<chgame/Allocate.h>)
#include <chgame/Allocate.h>
#define CHGAME_AUTO_SAVES 1
#endif
'''+text
    text=text.replace('};\nVol v;', '''#ifdef CHGAME_AUTO_SAVES
    uint32_t copies, info, backupInfo;
#endif
};
Vol v;''',1)
    text=text.replace('    v.end = n + 2;', '''    v.end = n + 2;
#ifdef CHGAME_AUTO_SAVES
    chsave::detail::allocationHints(v,b,base,rsvd,nf,!rootSecs);
#endif''',1)
    return text

def _overlay():
    apply_sdk_patch()
    # Hash every input; rebuild after either the SDK or our implementation changes.
    sources=list((SDK/'CHGame/src').rglob('*'))+list(SOURCE.glob('*'))
    sources += [SDK/'CHGame/library.properties',Path(__file__)]
    sources += list((SDK/'CHSd/src').glob('Fat.*'))
    sources += list((ROOT/'vendor/sd-spi').glob('SdSpi.*'))
    sources += list((PUBLIC/'src').rglob('*'))+[PUBLIC/'library.properties']
    digest=hashlib.sha256()
    for path in sorted(p for p in sources if p.is_file()):digest.update(str(path).encode());digest.update(path.read_bytes())
    fingerprint=digest.hexdigest();stamp=OVERLAY/'fingerprint'
    if stamp.exists() and stamp.read_text()==fingerprint:return OVERLAY
    OVERLAY.mkdir(parents=True,exist_ok=True)
    dest=OVERLAY/'CHGame'
    if dest.exists():shutil.rmtree(dest)
    shutil.copytree(SDK/'CHGame/src',dest/'src')
    shutil.copy2(SDK/'CHGame/library.properties',dest/'library.properties')
    for path in SOURCE.glob('*'):
        if path.name not in ('LegacyOnly.cpp','CompactSdk.cpp'):shutil.copy2(path,dest/'src/chgame'/path.name)
    # Bundle the standalone library unchanged, with a board transport bridge
    # to reuse the SDK asset driver's SPI state and host card fixtures.
    shutil.copytree(PUBLIC/'src',dest/'src/chsave')
    for name in ('LICENSE','NOTICE'):shutil.copy2(PUBLIC/name,dest/'src/chsave'/name)
    (dest/'src/CHSave.h').write_text('#pragma once\n#include "chsave/CHSave.h"\n')
    (dest/'src/chsave/detail/Card.cpp').unlink()
    (dest/'src/chsave/detail/WorkspaceStorage.cpp').write_text('''// Workspace adapter: share CHSd with asset streaming instead of linking
// a second FAT parser. The journal implementation is the public library.
#include "Card.h"
#include "../../chgame/Card.h"
namespace chsave {namespace detail {
namespace {
class WorkspaceCard final : public Storage {
public:
    Status open(const Id &id,uint8_t *scratch) override {
        switch(save_card::open(id.basename(),scratch)) {
        case save::Ready:return Status::Ready;
        case save::NoCard:return Status::NoCard;
        case save::NeedsSetup:return Status::NotFound;
        case save::Damaged:return Status::Damaged;
        default:return Status::IoError;
        }
    }
    Status create(const Id &id,uint16_t sectors,uint8_t *scratch) override {return save_card::create(id,sectors,scratch);}
    bool read(unsigned slot,uint8_t *p) override {return save_card::read(slot,p);}
    bool write(unsigned slot,const uint8_t *p) override {return save_card::write(slot,p);}
};
}
Storage &card(){static WorkspaceCard instance;return instance;}
}}
''')
    private=dest/'src/chgame/card';private.mkdir()
    for origin,old,new in ((SDK/'CHSd/src','fat','save_fat'),(ROOT/'vendor/sd-spi','mybl_sd','save_sd')):
        pattern='Fat.*' if old=='fat' else 'SdSpi.*'
        for path in origin.glob(pattern):
            if path.name=='SdSpi.cpp':continue
            text=path.read_text()
            if path.name=='Fat.cpp':text=allocation_fat(text)+'\n'+(SOURCE/'FatSave.inc').read_text()
            text=text.replace('namespace '+old,'namespace '+new).replace(old+'::',new+'::')
            # FAT uses the private SPI driver, isolated from streamed game assets.
            if old=='fat':text=text.replace('sd::','save_sd::')
            if path.suffix=='.cpp':text='#ifdef CHSIM\n'+text+'\n#endif\n'
            (private/path.name).write_text(text)
    # Reuse the board's SD/FAT code for saves and game assets. The only
    # addition is Mystic Balloon's already-proven single-sector writer.
    link=OVERLAY/'CHGfx'
    if not link.exists():link.symlink_to(SDK/'CHGfx',target_is_directory=True)
    card=OVERLAY/'CHSd'
    if card.is_symlink():card.unlink()
    elif card.exists():shutil.rmtree(card)
    shutil.copytree(SDK/'CHSd',card,ignore=shutil.ignore_patterns('build','out','tools','test','tests'))
    for name in ('SdSpi.cpp','SdSpi.h'):
        text=(ROOT/'vendor/sd-spi'/name).read_text().replace('mybl_sd','sd')
        (card/'src'/name).write_text(text)
    fat_source=card/'src/Fat.cpp';fat_source.write_text(allocation_fat(fat_source.read_text()).replace('#pragma GCC optimize("Os", "no-ipa-sra")','#pragma GCC optimize("Oz", "ipa-sra")')+'\n'+(SOURCE/'FatSave.inc').read_text())
    props=dest/'library.properties';props.write_text(props.read_text().replace('depends=CHGfx','depends=CHGfx,CHSd'))
    # Header docs must describe the selected backend.
    header=dest/'src/CHGame.h';header.write_text(header.read_text().replace('a record in flash that survives power cycles','an isolated SD journal that survives power cycles'))
    (dest/'src/chgame/Legacy.cpp').unlink(missing_ok=True)
    stamp.write_text(fingerprint)
    return OVERLAY

def simulation_image(path):
    sys.path.insert(0,str(SDK/'CHSd/tools'))
    from fatimg import build_image
    shared=shared_prepare()
    build_image(path,{name:shared.blank(item['id']) for name,item in catalog().items()},fs='fat16',spc=1,clusters=5000,mbr=True)

def run():
    for key,relative in {
        'ARDUINO_CONFIG_FILE':'arduino-cli.yaml','ARDUINO_DIRECTORIES_DATA':'.arduino15',
        'ARDUINO_DIRECTORIES_DOWNLOADS':'.arduino15/staging','ARDUINO_DIRECTORIES_USER':'sketchbook',
        'ARDUINO_BUILD_CACHE_PATH':'.arduino15/cache','TMPDIR':'.cache/tmp',
    }.items():os.environ.setdefault(key,str(ROOT/relative))
    (ROOT/'.cache/tmp').mkdir(parents=True,exist_ok=True)
    libraries=overlay()
    os.environ['PYTHONPATH']=os.pathsep.join([str(SDK_ROOT/'tools'),str(SDK_ROOT/'tools/chsim'),str(SDK/'CHSd/tools'),os.environ.get('PYTHONPATH','')])
    sys.path[:0]=[str(SDK_ROOT/'tools'),str(SDK_ROOT/'tools/chsim')]
    os.environ['CHSIM_CHGAME']=str(libraries/'CHGame/src')
    if not os.environ.get('CHGAME_SAVE_CARD') and not os.environ.get('CHGAME_SAVE_TEMPLATE'):
        template=OVERLAY.parent/('sim-'+hashlib.sha256(json.dumps(catalog(),sort_keys=True).encode()).hexdigest()[:12]+'.img')
        if not template.exists():simulation_image(template)
        os.environ['CHGAME_SAVE_TEMPLATE']=str(template)
    import device,gamecfg,hosttests
    device.LIBRARIES=libraries
    build=device.build
    def build_with_sd(sketch,debug=False,flags=''):
        source=Path(sketch).resolve();name=next(source.glob('*.ino')).stem
        stage=ROOT/'.cache/source-build'/name
        if stage.exists():shutil.rmtree(stage)
        stage.mkdir(parents=True)
        for item in source.iterdir():
            if item.is_file() and item.suffix in ('.ino','.cpp','.c','.h','.hpp','.json','.S'):
                shutil.copy2(item,stage/item.name)
        if (source/'src').exists():shutil.copytree(source/'src',stage/'src')
        for item in (source/'tools').rglob('*.py'):
            if any(part in ('build','out','__pycache__') for part in item.relative_to(source).parts):continue
            target=stage/item.relative_to(source);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(item,target)
        result=build(stage,debug,flags)
        target=source/'build'/('debug' if debug else 'release');target.mkdir(parents=True,exist_ok=True)
        for item in result.iterdir():
            if item.is_file():shutil.copy2(item,target/item.name)
        return target
    device.build=build_with_sd
    hosttests.LIB=libraries/'CHGame/src'
    hosttests.INCLUDES['lib']=hosttests.LIB
    # SD saves do not reserve flash pages.
    load=gamecfg.load
    def config(sketch):
        value=load(sketch);value.SAVE_PAGES=0;return value
    gamecfg.load=config
    import chgame
    raise SystemExit(chgame.main())

def main():
    if len(sys.argv)>1 and sys.argv[1]=='run':
        sys.argv.pop(1);run();return
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    sub.add_parser('overlay');sub.add_parser('list')
    p=sub.add_parser('sim-image');p.add_argument('path',type=Path)
    args=parser.parse_args()
    if args.command=='overlay':print(overlay())
    elif args.command=='list':print(json.dumps(catalog(),indent=2))
    elif args.command=='sim-image':simulation_image(args.path)
if __name__=='__main__':main()
