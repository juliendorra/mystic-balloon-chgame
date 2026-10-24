#!/usr/bin/env python3
"""Measure an installed Mystic Balloon diagnostic build on the real handheld."""
import argparse
import json
from pathlib import Path
import sys
import time

GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GAME.parent / "CHGame/tools/chsim"))
from chdrivelib import Driver, SerialTransport
from fbimage import to_image


def fields(line):
    return {k: int(v) for k, v in (part.split("=") for part in line.split()[1:])}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True)
    ap.add_argument("--out", type=Path, default=GAME / "out/device_validation")
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    t = SerialTransport(a.port)
    d = Driver(t, "MYBL ")
    evidence = {"target": "physical CHGame Rev0", "performance": [], "checks": []}
    original_sound = None

    def state():
        return fields(d.query("V", "MYBL "))

    def check(value, name):
        assert value, name
        evidence["checks"].append(name)
        print("PASS:", name, flush=True)

    def measure(level, buttons, seconds, label):
        d.buttons(0)
        d.cmd(f"G {level}")
        d.frames(2)  # Draw the new level before measuring steady gameplay.
        d.buttons(buttons)
        d.cmd("L0")
        time.sleep(0.25)  # Let the frame pacer recover from serial lockstep.
        d.query("U", "TIME ")
        d.cmd("P", "PERF")
        start = time.monotonic()
        time.sleep(seconds)
        d.cmd("L1")
        elapsed = time.monotonic()-start
        stages = fields(d.query("U", "TIME "))
        perf = fields(d.cmd("P", "PERF"))
        fps = perf["frames"]/elapsed
        row = dict(label=label, level=level, buttons=buttons, seconds=round(elapsed,4),
                   fps=round(fps,2), stages=stages, perf=perf, state=state())
        evidence["performance"].append(row)
        print(f"{label}: {fps:.2f} fps, work {stages['work']} us avg / {stages['max']} us max, late={perf['late']}", flush=True)
        check(fps>=57, label+" sustains 60 fps within serial measurement tolerance")
        check(perf["late"]<=2 and stages["max"]<16667, label+" stays inside the frame budget")
        check(perf["stk"]<1900, label+" fits the 2 KiB stack")
        check(row["state"]["state"]==7, label+" remains in gameplay")
        check(row["state"]["view"]==128 and row["state"]["camy"]<=256,
              label+" uses the full viewport and retains the camera bottom cap")
        check(row["state"]["dropped"]==0, label+" retains every color overlay")
        d.buttons(0)

    try:
        evidence["hello"] = d.handshake()
        d.cmd("L1")
        d.buttons(0)
        original_sound=state()["sound"]
        d.cmd("O 1")  # Runtime override; the saved sound preference is unchanged.
        check(state()["sound"]==1,"sound is enabled during performance measurements")
        check(d.cmd("!", "FAULT")=="FAULT none", "no recorded device fault")
        d.cmd("C 0")
        for level in range(1,40):
            measure(level, 32|2, 1, f"level {level:02} moving and floating")
        measure(1,32,5,"level 01 sustained walking")
        measure(6,32|2,5,"level 06 sustained jumping")
        measure(39,1,5,"level 39 sustained inhaling")
        for palette,name in [(1,"Original B&W"),(2,"Game Boy DMG"),(3,"Amber")]:
            d.cmd(f"C {palette}")
            measure(6,32|2,3,name+" sustained jumping")
        for palette,name in enumerate(["color","original","dmg","amber"]):
            d.cmd(f"C {palette}")
            d.cmd("G 1")
            d.frames(2)
            shot=d.shot()
            pixels=shot[:8192]
            indices={c for b in pixels for c in (b&15,b>>4)}
            check(state()["palette"]==palette,name+" palette selection is active")
            check(indices<={0,1} if palette==1 else len(indices)>=4,
                  name+" uses the expected original/color renderer")
            to_image(shot,3).save(a.out/(name+".png"))
        d.cmd("C 0")
        d.cmd("G 1")
        d.frames(2)
        before=state()
        d.buttons(32)
        d.frames(12)
        check(state()["x"]>before["x"],"directional input moves the player")
        d.buttons(64)
        d.frames(1)
        d.buttons(0)
        d.frames(1)
        paused=state()
        image=d.shot()
        d.frames(60)
        check(state()==paused and d.shot()==image,"pause freezes the world and framebuffer")
        d.buttons(64)
        d.frames(1)
        d.buttons(0)
        d.frames(1)
        check(state()["state"]==7,"START resumes gameplay")
        to_image(d.shot(),3).save(a.out/"device.png")
        check(d.cmd("!", "FAULT")=="FAULT none", "no device fault after the complete run")
    finally:
        (a.out/"results.json").write_text(json.dumps(evidence,indent=2)+"\n")
        try:
            d.buttons(0)
            if original_sound is not None:
                d.cmd(f"O {original_sound}")
            d.cmd("L0")
        finally:
            t.close()


if __name__ == "__main__":
    main()
