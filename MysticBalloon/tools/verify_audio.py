#!/usr/bin/env python3
"""Compare original sound events and execute both original/native tone ISRs.

Use --fetch once to download the pinned public references. Later runs are
offline and verify every reference against tools/audio-upstream.json.
"""
from pathlib import Path
import argparse
from collections import Counter
import hashlib
import json
import re
import subprocess
import sys
import urllib.request

from verify_sim import build_runner, GAME, ROOT
sys.path.insert(0, str(ROOT / "CHGame/tools/chsim"))
from chsim import find_cxx


def clean(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def calls(text):
    return [re.sub(r"\s+", "", re.sub(r",\s*mystic::audio::\w+\s*$", "", v)) for v in
            re.findall(r"sound\.tone\((.*?)\);", clean(text), re.S)]


def references(root, manifest, fetch):
    for group, spec in manifest.items():
        for name, expected in spec["files"].items():
            path = root / group / Path(name).name
            if not path.exists() and fetch:
                url = f"https://raw.githubusercontent.com/{spec['repository']}/{spec['commit']}/{name}"
                path.parent.mkdir(parents=True, exist_ok=True)
                request = urllib.request.Request(url, headers={"User-Agent": "CHGame-audio-audit"})
                path.write_bytes(urllib.request.urlopen(request, timeout=30).read())
            if not path.exists():
                raise SystemExit(f"Missing {path}; run with --fetch once.")
            assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, path


def run(exe):
    return subprocess.run([str(exe)], text=True, capture_output=True, check=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--upstream", type=Path, default=ROOT / ".cache/mystic-audio-upstream")
    args = parser.parse_args()
    manifest = json.loads((GAME / "tools/audio-upstream.json").read_text())
    references(args.upstream, manifest, args.fetch)
    out = GAME / "out/audio"
    out.mkdir(parents=True, exist_ok=True)
    original = args.upstream / "game"
    inventory = {}
    assert "eachFrameMillis = 1000 / rate;" in (args.upstream / "arduboy/Arduboy2.cpp").read_text()
    fps=int(re.search(r"#define CHMB_FPS (\d+)", (GAME / "config.h").read_text())[1])
    for name in ("menu.h", "game.h", "inputs.h", "player.h", "levels.h"):
        old, new = calls((original / name).read_text()), calls((GAME / name).read_text())
        if name != "menu.h":
            assert old == new, f"Sound requests changed in {name}"
        inventory[name] = {"original": old, "port": new}

    # Execute the actual reference library, not a reimplementation of its
    # duration formula. Only its AVR registers and GPIO are stand-ins.
    tests = GAME / "tools/tests"
    timer_exe = out / "audio_timer_test"
    command = find_cxx() + ["-std=gnu++17", "-O1", "-w", "-fsanitize=undefined",
                           "-fno-sanitize-recover=all"]
    command += [f"-I{p}" for p in (tests / "audio_host", args.upstream / "tones", GAME)]
    command += [str(p) for p in (tests / "audio_timer_test.cpp", GAME / "Tones.cpp",
                                args.upstream / "tones/ArduboyTones.cpp")]
    subprocess.run(command + ["-o", str(timer_exe)], check=True)
    timer_output = run(timer_exe)
    (out / "timer.txt").write_text(timer_output)
    timer_cases = []
    for hz, duration, divider, interrupts in re.findall(r"TIMER (\d+) (\d+) (\d+) (\d+)", timer_output):
        hz, duration, divider, interrupts = map(int, (hz, duration, divider, interrupts))
        timer_cases.append(dict(hz=hz, duration_1024=duration, half_period_ticks=divider,
                                stop_interrupt=interrupts, stop_us=interrupts * divider / 2,
                                old_blip_hz=((hz + 10) // 20) * 20))
    assert len(timer_cases) == 180
    print(timer_output.splitlines()[-1], flush=True)

    # Stage byte-for-byte original game headers, changing only Arduino's
    # Bxxxxxxxx binary spelling to a standard C++ binary literal.
    stage = out / "reference-game"
    stage.mkdir(exist_ok=True)
    for p in original.glob("*.h"):
        (stage / p.name).write_text(re.sub(r"\bB([01]{8})\b", r"0b\1", p.read_text()))
    wrapper = out / "reference_events.cpp"
    wrapper.write_text('#define CHMB_AUDIO_REFERENCE 1\n#include "' +
                       str(tests / "audio_events_test.cpp") + '"\n')
    source = tests / "audio_events_test.cpp"
    ref_exe = build_runner(wrapper, out / "reference_events",
                           extra_includes=[tests / "audio_trace_host", stage])
    port_exe = build_runner(source, out / "port_events")
    traces = []
    for label, exe in (("original", ref_exe), ("port", port_exe)):
        output = run(exe)
        (out / f"{label}-events.txt").write_text(output)
        traces.append([line for line in output.splitlines() if line.startswith("SND ")])
    assert traces[0] == traces[1], "Original/port event trace mismatch; inspect out/audio/*-events.txt"
    counts = Counter(line.split()[1] for line in traces[0])
    for scenario in ("jump", "footsteps", "landing", "head-bump", "inhale-wind", "hurt", "key",
                     "six-coins-same-frame", "inhale-walker-left", "inhale-walker-right",
                     "walker-hurt", "spike-hurt", "fall-life", "fall-game-over", "fan-up", "fan-right", "fan-left",
                     "same-frame-overrides", "intro-auto", "config-enable", "config-back", "next-level-enter", "same-frame-menu"):
        assert counts[scenario] > 0, f"Fixture did not reach {scenario}"
    assert counts["six-coins-same-frame"] == 6
    assert counts["hurt"] == 3
    assert counts["same-frame-overrides"] == 4
    assert counts["pause-silent"] == 0
    assert counts["game-over-silent"] == 0
    assert len([s for s in counts if s.startswith("bonus-")]) == 29
    for scenario, interval in (("footsteps",16),("fan-up",2),("fan-right",2),("fan-left",2)):
        frames = [int(line.split()[2]) for line in traces[0] if line.split()[1] == scenario]
        assert all(b-a == interval for a,b in zip(frames,frames[1:])), scenario
    report = dict(port_version=json.loads((GAME / "chgame.json").read_text())["version"],
                  reference_commits={k:v["commit"] for k,v in manifest.items()},
                  source_sha256={name:hashlib.sha256((GAME/name).read_bytes()).hexdigest()
                                 for name in ("Tones.cpp","Tones.h","Port.cpp","Port.h","MysticBalloon.ino","menu.h","config.h","chgame.json",
                                              "tools/tests/audio_events_test.cpp","tools/tests/audio_timer_test.cpp")},
                  source_calls=inventory, timer_cases=timer_cases,
                  event_count=len(traces[0]), event_scenarios=dict(sorted(counts.items())),
                  event_traces_identical=True, all_timer_comparisons_passed=True,
                  cadence=dict(original_frame_ms=1000//60,port_frame_us=1000000//fps,
                               note="Original Arduboy2 truncates 1000/60; port retains its existing 60 fps pacing."),
                  limits=["Register/ISR execution and deterministic handler fixtures; no electrical or acoustic capture.",
                          "The original game does not pin its Arduboy library versions; reference commits are explicit.",
                          "Random wind uses a common seeded host RNG for event comparison; physical platform RNG streams differ.",
                          "Expanded CONF controls and early intro skip are CHGame additions; equivalent enable/back actions are compared."])
    (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"PASS: {len(traces[0])} identical tone events across {len(counts)} scenarios; pitches, durations, frame timing and same-frame order", flush=True)


if __name__ == "__main__":
    main()
