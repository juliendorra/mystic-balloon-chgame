#!/usr/bin/env python3
"""Compare every player/inhale pose to reviewed native pixels on CHGame.

Requires diagnose_mattes.py references and the current diagnostic build.
Uses lockstep canvases and restores the saved menu/preferences afterward.
"""
import argparse
import json
from pathlib import Path
import sys

GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GAME.parent / "CHGame/tools/chsim"))
from chdrivelib import Driver, SerialTransport, SimTransport
from fbimage import to_image
from verify_device import fields


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    target = ap.add_mutually_exclusive_group(required=True)
    target.add_argument("--port")
    target.add_argument("--sim", type=Path, help="validate the same protocol against a simulator executable")
    ap.add_argument("--out", type=Path, default=GAME / "out/device_player")
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    t = SimTransport(a.sim) if a.sim else SerialTransport(a.port)
    d = Driver(t, "MYBL ")
    result = dict(target="deterministic CHGame host" if a.sim else "physical CHGame Rev0", checks=[], cases=[])
    connected = False
    def state():
        return fields(d.query("V", "MYBL "))
    def check(value, label):
        assert value, label
        result["checks"].append(label)
        print("PASS:", label, flush=True)
    try:
        result["hello"] = d.handshake()
        expected = "MYBL " + json.loads((GAME / "chgame.json").read_text())["version"]
        check(result["hello"] == expected, "installed diagnostic identity matches source")
        d.cmd("L1");d.buttons(0);d.cmd("R");d.frames(2)
        # Other diagnostic suites may leave runtime palette overrides active.
        # Compare restoration with the saved preferences, not those overrides.
        result["initial"] = state()
        connected = True
        for p, name in enumerate(["Color", "Original B&W", "DMG", "Amber"]):
            d.cmd(f"C {p}")
            for frame in range(16):
                asset, source_frame = ("kid", frame) if frame < 12 else ("inhale", frame - 12)
                d.cmd(f"D {frame}")
                shot = d.shot()
                native = (GAME / f"out/matte-diagnosis/{p}_{asset}_{source_frame}.bin").read_bytes()
                check(shot[:8192] == native,
                      f"{name} {asset} frame {source_frame} matches all 16384 reviewed pixels")
                if p == 3:
                    indices = {c for b in shot[:8192] for c in (b & 15, b >> 4)}
                    assert indices == {0, 2, 14}
                result["cases"].append(dict(palette=p, asset=asset, frame=source_frame,
                                             matched_pixels=16384))
                if frame in (0, 5, 6, 11, 12, 14):
                    to_image(shot, 3).save(a.out / f"{p}_{asset}_{source_frame}.png")
            check(state()["dropped"] == 0, name + " retains all player commands")
        check(d.cmd("!", "FAULT") == "FAULT none", "no recorded device fault")
    finally:
        try:
            if connected:
                d.buttons(0);d.cmd("R");d.frames(2)
                result["restored"] = state()
                check(result["restored"]["state"] == 1 and
                      all(result["restored"][k] == result["initial"][k] for k in ("saved", "sound", "palette")),
                      "saved checkpoint and preferences restored at main menu")
                d.cmd("L0")
        finally:
            (a.out / "results.json").write_text(json.dumps(result, indent=2) + "\n")
            t.close()


if __name__ == "__main__":
    main()
