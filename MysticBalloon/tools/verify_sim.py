#!/usr/bin/env python3
"""Run the actual port under UBSan and check upstream asset fingerprints.

Set CHMB_SANITIZERS=address,undefined on hosts with a working ASan runtime.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys

GAME = Path(__file__).resolve().parents[1]
ROOT = GAME.parent
sys.path.insert(0, str(ROOT / "CHGame/tools/chsim"))
from chsim import compile_exe


def asset_bytes(text):
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    arrays = {}
    for name, values in re.findall(r"(\w+)\s*\[\s*\]\s*(?:PROGMEM\s*)?=\s*\{([^}]+)\}", text):
        if name == "levels":
            continue
        values = re.sub(r"\bB([01]{8})\b", r"0b\1", values)
        data = bytes(int(v.strip(), 0) if v.strip().startswith(("0x", "0b")) else int(v.strip())
                     for v in values.split(",") if v.strip())
        arrays[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    return arrays


def build_runner(source, output, extra_includes=()):
    host = ROOT / "CHGame/tools/chsim/host"
    libraries = ROOT / "CHGame/platform/board/arduino/CHGame/libraries"
    gfx = libraries / "CHGfx/src"
    lib = libraries / "CHGame/src"
    out = output.parent
    out.mkdir(parents=True, exist_ok=True)
    sanitizers = os.environ.get("CHMB_SANITIZERS", "undefined")
    os.environ["CHSIM_FLAGS"] = f"-fsanitize={sanitizers} -fno-sanitize-recover=all"
    sources = [source, GAME / "Port.cpp", GAME / "Color.cpp", GAME / "Tones.cpp", GAME / "Audio.cpp", host / "chgfx_host.cpp"]
    sources += sorted(p for p in gfx.glob("*.cpp") if p.name != "CHGfx.cpp")
    sources += sorted(p for p in lib.rglob("*.cpp") if p.name != "Save.cpp")
    # Physical flash is word-aligned. The upstream simulator declares its
    # pretend flash as a byte array, then casts it to a Header. Give only
    # this host stand-in the physical device's alignment for UBSan.
    save_source = (lib / "chgame/Save.cpp").read_text()
    declaration = "static uint8_t simFlash[2][PAGE];"
    assert declaration in save_source
    aligned_save = out / "Save_aligned.cpp"
    aligned_save.write_text(save_source.replace(declaration, "alignas(4) " + declaration))
    sources.append(aligned_save)
    return compile_exe(sources, output, includes=[*extra_includes, GAME, host, gfx, lib, lib / "chgame"])


def main():
    expected = json.loads((GAME / "tools/upstream-assets.json").read_text())
    assert asset_bytes((GAME / "bitmaps.h").read_text()) == expected["arrays"], "upstream graphics/levels changed"
    print(f"PASS: all {len(expected['arrays'])} original bitmap/level arrays match upstream", flush=True)
    exe = build_runner(GAME / "tools/tests/port_test.cpp", GAME / "out/tests/port_test")
    subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
