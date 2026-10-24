#!/usr/bin/env python3
"""Check real-device side walls, negative camera rendering and balloon ascent.

Run preview_scrolling.py and install the current debug build first. Diagnostic
fixtures change only RAM; R reloads the original flash checkpoint at the end.
"""
import argparse
import json
from pathlib import Path
import sys
import time

GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GAME.parent / "CHGame/tools/chsim"))
from chdrivelib import Driver, SerialTransport
from fbimage import to_image
from verify_device import fields


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True)
    ap.add_argument("--out", type=Path, default=GAME / "out/device_scrolling")
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    t = SerialTransport(a.port)
    d = Driver(t, "MYBL ")
    result = dict(target="physical CHGame Rev0", checks=[], performance=[], cases=[])
    references = json.loads((GAME / "out/scrolling/cases.json").read_text())
    connected = False

    def state():
        return fields(d.query("V", "MYBL "))

    def check(value, label):
        assert value, label
        result["checks"].append(label)
        print("PASS:", label, flush=True)

    def measure(label):
        d.cmd("L0")
        time.sleep(.3)
        d.query("U", "TIME "); d.cmd("P", "PERF")
        start = time.monotonic()
        time.sleep(2)
        d.cmd("L1")
        elapsed = time.monotonic() - start
        stages = fields(d.query("U", "TIME "))
        perf = fields(d.cmd("P", "PERF"))
        fps = perf["frames"] / elapsed
        result["performance"].append(dict(label=label, fps=round(fps, 2), seconds=elapsed,
                                          stages=stages, perf=perf, state=state()))
        print(f"{label}: {fps:.2f} fps, max work {stages['max']} us, late={perf['late']}", flush=True)
        check(fps >= 57 and perf["late"] == 0 and stages["max"] < 16667,
              label + " sustains 60 fps with zero late frames")
        check(perf["stk"] < 1900 and state()["dropped"] == 0,
              label + " fits stack and retains every drawing command")

    try:
        result["hello"] = d.handshake()
        expected = "MYBL " + json.loads((GAME / "chgame.json").read_text())["version"]
        check(result["hello"] == expected, "diagnostic version matches current source")
        connected = True
        d.cmd("L1"); d.buttons(0)
        result["initial"] = state()
        d.cmd("O 1")
        for ref in references:
            p, name = ref["palette"], ref["name"]
            d.buttons(0); d.frames(1)
            d.cmd(f"C {p}"); d.cmd("G 1")
            pose = dict(left="0 16 0", right="372 16 0", ascent="112 32 96")[name]
            d.cmd("J " + pose); d.buttons(2)
            if name == "ascent":
                targets = sorted(set(range(5, 151, 5)) | {61})
            else:
                targets = [1]
            previous = 0
            for frame in targets:
                d.frames(frame - previous)
                previous = frame
                s = state()
                assert s["state"] == 7 and s["balloons"] == 3 and s["dropped"] == 0
                assert 24 <= s["y"] - s["camy"] <= 112
                if frame != ref["frames"]:
                    continue
                check(all(s[k] == ref[k] for k in ("x", "y", "camx", "camy")),
                      f"palette {p} {name} camera and physics match simulator")
                shot = d.shot()
                to_image(shot, 3).save(a.out / f"{p}_{name}.png")
                native = (GAME / f"out/scrolling/{p}_{name}.bin").read_bytes()
                compared = 0
                for y in range(8, 128):
                    for x in range(128):
                        wx, wy = x + s["camx"], y + s["camy"]
                        # Random wind particles can overshoot row zero by
                        # six pixels. Above that band, the sky and player
                        # are deterministic; the side walls always are too.
                        if not (wx < 0 or wx >= 384 or (name == "ascent" and wy < -8)):
                            continue
                        i, shift = y * 64 + x // 2, (x & 1) * 4
                        assert (shot[i] >> shift) & 15 == (native[i] >> shift) & 15
                        compared += 1
                check(compared > 5000, f"palette {p} {name} native pixels match reviewed wall/sky/player")
                result["cases"].append(dict(palette=p, name=name, compared_pixels=compared, state=s))
            if name == "ascent":
                check(state()["y"] < 0 and state()["camy"] < 0,
                      f"palette {p} tracks balloon flight above map row zero")
            if p == 0 or name == "ascent":
                measure(f"palette {p} {name}")
        check(d.cmd("!", "FAULT") == "FAULT none", "no recorded device fault")
    finally:
        try:
            if connected:
                d.buttons(0); d.cmd("L1"); d.cmd("R"); d.frames(2)
                result["restored"] = state()
                check(result["restored"]["state"] == 1 and
                      all(result["restored"][k] == result["initial"][k] for k in ("saved", "sound", "palette")),
                      "original flash checkpoint and preferences restored; back at main menu")
                d.cmd("L0")
        finally:
            (a.out / "results.json").write_text(json.dumps(result, indent=2) + "\n")
            t.close()


if __name__ == "__main__":
    main()
