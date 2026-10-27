#!/usr/bin/env python3
"""Check palette preview and checkpoint controls on an installed debug release.

Uses normal CONT to preserve the existing checkpoint. Run preview_splash.py
first for independent native panel references, then upload the debug build.
"""
import argparse
import json
from pathlib import Path
import sys
import time

GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GAME.parent / "CHGame/tools/chsim"))
from chdrivelib import Driver, SerialTransport
from fbimage import to_image, palette_rgb


def fields(line):
    return {k: int(v) for k, v in (part.split("=") for part in line.split()[1:])}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True)
    ap.add_argument("--out", type=Path, default=GAME / "out/device_controls")
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    transport = SerialTransport(a.port)
    d = Driver(transport, "MYBL ")
    evidence = {"target": "physical CHGame Rev0", "checks": [], "performance": []}
    original_sound = None

    def state():
        return fields(d.query("V", "MYBL "))

    def check(value, label):
        assert value, label
        evidence["checks"].append(label)
        print("PASS:", label, flush=True)

    def tap(mask):
        d.buttons(mask)
        d.frames(1)
        d.buttons(0)
        d.frames(1)

    def measure(label, seconds):
        d.cmd("L0")
        time.sleep(.3)
        d.query("U", "TIME ")
        d.cmd("P", "PERF")
        start = time.monotonic()
        time.sleep(seconds)
        d.cmd("L1")
        elapsed = time.monotonic() - start
        stages = fields(d.query("U", "TIME "))
        perf = fields(d.cmd("P", "PERF"))
        fps = perf["frames"] / elapsed
        evidence["performance"].append(dict(label=label, seconds=elapsed,
            fps=round(fps, 2), stages=stages, perf=perf))
        print(f"{label}: {fps:.2f} fps, max work {stages['max']} us", flush=True)
        check(fps >= 57 and perf["late"] == 0 and stages["max"] < 16667,
              label + " sustains 60 fps with zero late frames")
        check(perf["stk"] < 1900, label + " fits the 2 KiB stack")

    def panel(palette, selection):
        shot = d.shot()
        expected = (GAME / f"out/splash-review/{palette}_pause_{selection}.bin").read_bytes()
        for y in range(36, 92):
            for x in range(20, 108):
                i, shift = y * 64 + x // 2, (x & 1) * 4
                assert (shot[i] >> shift) & 15 == (expected[i] >> shift) & 15
        to_image(shot, 3).save(a.out / f"pause-{palette}-{selection}.png")
        check(state()["dropped"] == 0,
              f"palette {palette} pause option {selection} matches reviewed pixels with no dropped commands")

    try:
        evidence["hello"] = d.handshake()
        expected_version = json.loads((GAME / "chgame.json").read_text())["version"]
        check(evidence["hello"] == "MYBL " + expected_version,
              "installed diagnostic identity is MYBL " + expected_version)
        d.cmd("L1")
        d.buttons(0)
        initial = state()
        original_sound, original_palette = initial["sound"], initial["palette"]
        d.cmd("O 1")
        if initial["state"] == 0:
            tap(2)
        check(state()["state"] == 1, "boot reaches main menu")
        tap(8); tap(8); tap(2)
        check(state()["state"] == 5, "CONF opens through normal controls")
        for _ in range(original_palette):
            tap(4)
        for p, name, rgb in [(0, "color", (68, 51, 85)), (1, "original", (0, 0, 0)),
                             (2, "dmg", (17, 51, 17)), (3, "amber", (34, 17, 0))]:
            if p:
                tap(8)
            shot = d.shot()
            check(state()["state"] == 5 and state()["palette"] == p and
                  palette_rgb(shot[8192:])[0] == rgb,
                  name + " is previewed immediately by cursor movement")
            to_image(shot, 3).save(a.out / f"preview-{name}.png")
        tap(1)
        check(state()["state"] == 1 and state()["palette"] == original_palette,
              "A cancels the preview and restores the previous palette")
        tap(2)
        for _ in range(3 - original_palette):
            tap(8)
        tap(2)
        check(state()["state"] == 1 and state()["palette"] == 3, "B confirms the highlighted palette")
        tap(2); tap(4)
        check(state()["palette"] == 2, "moving up previews DMG")
        tap(1)
        check(state()["palette"] == 3, "cancel restores the newly confirmed palette")
        tap(2)
        for _ in range(3 - original_palette):
            tap(4)
        tap(2)
        check(state()["palette"] == original_palette, "original palette restored before saving progress")
        to_image(d.shot(), 3).save(a.out / "title.png")
        measure("128x128 title with ATM music", 2)
        tap(4); tap(4); tap(2); tap(2)
        bank = state()
        check(bank["state"] == 6, "CONT loads the existing level checkpoint")
        evidence["checkpoint"] = {k: bank[k] for k in ("level", "score", "total")}
        d.frames(30); tap(2)
        check(state()["state"] == 7, "saved level starts normally")
        tap(64)
        check(state()["state"] == 8, "START opens pause")
        panel(original_palette, 0)
        before, image = state(), d.shot()
        d.frames(60)
        frozen = state()
        check(all(frozen[k] == v for k, v in before.items() if k not in ("ticks", "samples")) and d.shot() == image,
              "paused physics, camera and whole frame remain frozen")
        tap(8)
        for p, name in enumerate(["Color", "Original B&W", "DMG", "Amber"]):
            d.cmd(f"C {p}")
            d.frames(2)
            panel(p, 1)
            measure(name + " pause menu", 2)
        d.cmd(f"C {original_palette}")
        tap(1)
        check(state()["state"] == 7, "A resumes from SAVE & QUIT without leaving")
        tap(64); panel(original_palette, 0); tap(8); tap(2)
        deadline = time.monotonic() + 2
        while state()["state"] == 8 and time.monotonic() < deadline:
            time.sleep(.03)
            d.frames(1)
        check(state()["state"] == 1 and state()["saved"] == bank["level"],
              "SAVE & QUIT returns to menu with the current checkpoint")
        to_image(d.shot(), 3).save(a.out / "saved-main.png")
        tap(2); tap(2)
        continued = state()
        check(continued["state"] == 6 and all(continued[k] == bank[k] for k in ("level", "score", "total"))
              and continued["balloons"] == 3,
              "normal CONT reloads saved level, score and coins with three balloons")
        d.frames(30); tap(2); tap(64)
        check(d.cmd("!", "FAULT") == "FAULT none", "no recorded device fault")
    finally:
        (a.out / "results.json").write_text(json.dumps(evidence, indent=2) + "\n")
        try:
            d.buttons(0)
            if original_sound is not None:
                d.cmd(f"O {original_sound}")
            d.cmd("L0")
        finally:
            transport.close()


if __name__ == "__main__":
    main()
