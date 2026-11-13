#!/usr/bin/env python3
"""Provision isolated SD saves, select CHSave and CHSd, and run CHGame tools."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace

ROOT=Path(__file__).resolve().parents[1]
CONFIG=json.loads((ROOT/'save-config.json').read_text())
SDK_ROOT=Path(os.environ.get('CHGAME_ROOT',str(ROOT/'CHGame'))).resolve()
if not (SDK_ROOT/'tools/chgame.py').is_file() and (ROOT/'.deps/CHGame/tools/chgame.py').is_file():SDK_ROOT=ROOT/'.deps/CHGame'
SDK=SDK_ROOT/'platform/board/arduino/CHGame/libraries'
PUBLIC=ROOT/'chgame-sd-saves'
CHSD=PUBLIC/'extras/CHSd'

def shared_prepare():
    spec=importlib.util.spec_from_file_location('chsave_prepare',PUBLIC/'tools/prepare.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def catalog():
    shared=shared_prepare()
    return {shared.save_path(CONFIG['id']):{'game':CONFIG['game'],'id':CONFIG['id']}}

def validate(raw,ident):
    shared_prepare().validate(raw,ident)

def simulator_environment():
    env = {'CHSIM_CHSD': str(CHSD), 'CHSIM_EXTRA_LIBRARIES': str(PUBLIC)}
    # Older SDK simulators only link CHSd on a direct include and have no
    # extra-library hook. A compiler response file declares the same sources.
    sdk_sim = SDK_ROOT/'tools/chsim/chsim.py'
    if 'CHSIM_EXTRA_LIBRARIES' not in sdk_sim.read_text():
        cache = ROOT/'.cache/sd-saves'
        cache.mkdir(parents=True, exist_ok=True)
        response = cache/'libraries.rsp'
        roots = (PUBLIC/'src', CHSD/'src')
        arguments = [f'-I{path}' for path in roots]
        arguments += [str(source) for path in roots for source in sorted(path.rglob('*.cpp'))]
        arguments.append(str(CHSD/'host/sd_host.cpp'))
        response.write_text('\n'.join(json.dumps(arg) for arg in arguments)+'\n')
        if any(char.isspace() for char in str(response)):
            short = Path(tempfile.mkdtemp(prefix='chsave-sim-', dir='/tmp'))/'libraries.rsp'
            short.symlink_to(response)
            response = short
        env['CHSIM_FLAGS'] = f"{os.environ.get('CHSIM_FLAGS', '')} @{response}"
    return env

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
    os.environ['PYTHONPATH']=os.pathsep.join([str(SDK_ROOT/'tools'),str(SDK_ROOT/'tools/chsim'),str(SDK/'CHSd/tools'),os.environ.get('PYTHONPATH','')])
    sys.path[:0]=[str(SDK_ROOT/'tools'),str(SDK_ROOT/'tools/chsim')]
    os.environ.update(simulator_environment())
    if not os.environ.get('CHGAME_SAVE_CARD') and not os.environ.get('CHGAME_SAVE_TEMPLATE'):
        template=(ROOT/'.cache/sd-saves')/('sim-'+hashlib.sha256(json.dumps(catalog(),sort_keys=True).encode()).hexdigest()[:12]+'.img')
        if not template.exists():
            template.parent.mkdir(parents=True, exist_ok=True)
            simulation_image(template)
        os.environ['CHGAME_SAVE_TEMPLATE']=str(template)
    import device,gamecfg
    def compile_with_libraries(command, **kwargs):
        if command[:2] == ['arduino-cli', 'compile']:
            command = command.copy()
            index = command.index(str(SDK/'CHSd'))
            command[index] = str(CHSD)
            command[-1:-1] = ['--library', str(PUBLIC)]
        return subprocess.run(command, **kwargs)

    device.LIBRARIES=SDK
    device.subprocess=SimpleNamespace(run=compile_with_libraries)
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
        result=build(stage,debug,flags if debug else f"{flags} -Oz -fno-caller-saves")
        target=source/'build'/('debug' if debug else 'release');target.mkdir(parents=True,exist_ok=True)
        for item in result.iterdir():
            if item.is_file():shutil.copy2(item,target/item.name)
        return target
    device.build=build_with_sd
    # SD saves do not reserve flash pages.
    load=gamecfg.load
    def config(sketch):
        value=load(sketch);value.SAVE_PAGES=0;return value
    gamecfg.load=config
    import check
    def run_check(command, **kwargs):
        if len(command) > 1 and command[1] == str(SDK_ROOT/'tools/device.py'):
            command = [command[0], str(Path(__file__).resolve()), 'run', *command[2:]]
        return subprocess.run(command, **kwargs)
    check.subprocess=SimpleNamespace(run=run_check)
    import chgame
    raise SystemExit(chgame.main())

def main():
    if len(sys.argv)>1 and sys.argv[1]=='run':
        sys.argv.pop(1);run();return
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    sub.add_parser('list')
    p=sub.add_parser('sim-image');p.add_argument('path',type=Path)
    args=parser.parse_args()
    if args.command=='list':print(json.dumps(catalog(),indent=2))
    elif args.command=='sim-image':simulation_image(args.path)
if __name__=='__main__':main()
