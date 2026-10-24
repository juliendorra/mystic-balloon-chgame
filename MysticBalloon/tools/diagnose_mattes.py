#!/usr/bin/env python3
"""Trace exterior sprite colors to the original white clearing pixels.

Diagnostic only: render the current production blitter, classify white pixels
connected to the frame edge around its black outline, and compare all modes.
"""
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw, ImageFont
from art.build_art import arrays, tint_mask, PALETTES
from verify_sim import build_runner, asset_bytes

GAME = Path(__file__).resolve().parents[1]


def bitmap(data, frame, masked=False, mask=False):
    w, h = data[:2]
    stride = 2 if masked else 1
    return {(x, y) for y in range(h) for x in range(w)
            if data[2 + (frame * w * ((h + 7) // 8) + (y // 8) * w + x) * stride + int(mask)]
            & (1 << (y & 7))}


def exterior(w, h, black):
    free = {(x, y) for y in range(h) for x in range(w)} - black
    todo = [p for p in free if p[0] in (0, w - 1) or p[1] in (0, h - 1)]
    seen = set(todo)
    while todo:
        x, y = todo.pop()
        for p in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if p in free and p not in seen:
                seen.add(p)
                todo.append(p)
    return seen


def main():
    expected = json.loads((GAME / "tools/upstream-assets.json").read_text())
    assert asset_bytes((GAME / "bitmaps.h").read_text()) == expected["arrays"]
    out = GAME / "out/matte-diagnosis"
    out.mkdir(parents=True, exist_ok=True)
    exe = build_runner(GAME / "tools/art/matte_probe.cpp", out / "matte_probe")
    subprocess.run([str(exe), str(out)], check=True)
    source = arrays()
    cases = []
    for name, sprite, frames in [("kid", "kidSprite", 12),
                                  ("inhale", "kidSpriteSuck_plus_mask", 4),
                                  ("walker", "walkerSprite", 4),
                                  ("balloon", "balloon_plus_mask", 1)]:
        data = source[sprite]
        w, h = data[:2]
        for f in range(frames):
            ink = bitmap(data, f, masked="plus_mask" in sprite)
            if name == "kid":
                mask = bitmap(data, 12 + f // 6)
                black, light = ink, mask - ink
            elif "plus_mask" in sprite:
                mask = bitmap(data, f, masked=True, mask=True)
                black, light = mask - ink, ink
            else:
                black = {(x, y) for y in range(h) for x in range(w)} - ink
                light = tint_mask(data, f)
            suspects = sorted(light & exterior(w, h, black))
            visible = []
            for p in range(4):
                raw = (out / f"{p}_{name}_{f}.bin").read_bytes()
                def pixel(x, y):
                    i = (48 + y) * 64 + (48 + x) // 2
                    return (raw[i] >> (((48 + x) & 1) * 4)) & 15
                visible.append(sum(pixel(x, y) != (1 if p == 1 else 2) for x, y in suspects))
            assert visible[1] == 0
            assert visible == [0,0,0,0]
            cases.append(dict(name=name, frame=f, exterior_light_pixels=suspects,
                              visible_exterior_pixels_by_mode=visible))
    font = ImageFont.truetype("/System/Library/Fonts/Menlo.ttc", 13)
    names = ["COLOR", "ORIGINAL B&W", "DMG GREEN", "AMBER"]
    scenes = [("kid", 0, "Standing"), ("kid", 5, "Floating pose"), ("inhale", 0, "Inhaling")]
    sheet = Image.new("RGB", (4 * 224, 3 * 252), "#171926")
    draw = ImageDraw.Draw(sheet)
    for p, palette in enumerate(PALETTES):
        rgb = [((c >> 8 & 15) * 17, (c >> 4 & 15) * 17, (c & 15) * 17) for c in palette]
        for row, (name, f, label) in enumerate(scenes):
            raw = (out / f"{p}_{name}_{f}.bin").read_bytes()
            image = Image.new("RGB", (128, 128))
            image.putdata([rgb[i] for b in raw for i in (b & 15, b >> 4)])
            crop = image.crop((44, 44, 68, 68)).resize((192, 192), Image.Resampling.NEAREST)
            image.save(out / f"{p}_{name}_{f}.png")
            x, y = p * 224 + 16, row * 252
            draw.text((x, y + 5), names[p], font=font, fill="white")
            draw.text((x, y + 24), label, font=font, fill="#bbbbcc")
            sheet.paste(crop, (x, y + 45))
    sheet.save(GAME / "docs/sprite-matte-comparison.png")
    # Exact source classification for a representative standing frame.
    trace = Image.new("RGB", (3 * 224, 200), "#171926")
    td = ImageDraw.Draw(trace)
    black = bitmap(source["kidSprite"], 0)
    mask = bitmap(source["kidSprite"], 12)
    fringe = set(map(tuple, cases[0]["exterior_light_pixels"]))
    for column, label in enumerate(["Original clearing mask", "Black outline/detail", "Exterior white = PINK"]):
        td.text((column * 224 + 5, 6), label, font=font, fill="white")
        for y in range(16):
            for x in range(12):
                color = "#222b3b"
                if column == 0 and (x, y) in mask: color = "#ffffff"
                if column == 1:
                    color = "#14141c" if (x, y) in black else "#ffffff"
                if column == 2:
                    if (x, y) in black: color = "#14141c"
                    elif (x, y) in fringe: color = "#ff5599"
                    elif (x, y) in mask: color = "#ffffff"
                px, py = column * 224 + 50 + x * 9, 38 + y * 9
                td.rectangle((px, py, px + 8, py + 8), fill=color)
    trace.save(GAME / "docs/sprite-matte-origin.png")
    report = dict(version=json.loads((GAME / "chgame.json").read_text())["version"],
                  upstream_arrays_unchanged=len(expected["arrays"]),
                  cases=cases, modes=names,
                  source_sha256=hashlib.sha256((GAME / "Color.cpp").read_bytes()).hexdigest(),
                  interpretation="Per-frame player/inhale color sprites exclude all original exterior clearing pixels; the compact backdrop restores underlying sky/terrain. Amber fills the complete body with the same light index as the head.")
    report["particles"] = []
    for p in range(4):
        raw = (out / f"{p}_particle_0.bin").read_bytes()
        index = raw[48 * 64 + 24] & 15
        assert index == (0 if p == 1 else 13)
        report["particles"].append(dict(mode=names[p], index=index,
                                         rgb444=f"{PALETTES[p][index]:03X}"))
    # Cross-check the identified pixels in previously verified physical
    # captures when they belong to this exact release; no USB mutation.
    device_record = GAME / "docs/device-performance-scrolling.json"
    if device_record.exists():
        device = json.loads(device_record.read_text())
        if device["hello"] == "MYBL " + report["version"]:
            report["physical_capture_checks"] = []
            fringe = next(c["exterior_light_pixels"] for c in cases
                          if c["name"] == "kid" and c["frame"] == 5)
            for p in range(4):
                state = next(c["state"] for c in device["cases"]
                             if c["palette"] == p and c["name"] == "ascent")
                capture = GAME / f"out/device_scrolling/{p}_ascent.png"
                if not capture.exists():
                    continue
                im = Image.open(capture).convert("RGB")
                def rgb(c):
                    return ((c >> 8 & 15) * 17, (c >> 4 & 15) * 17, (c & 15) * 17)
                sky = rgb(PALETTES[p][1 if p == 1 else 2])
                visible = 0
                for x, y in fringe:
                    actual = im.getpixel(((state["x"] - state["camx"] + x) * 3 + 1,
                                          (state["y"] - state["camy"] + y) * 3 + 1))
                    index = 1 if p == 1 else 14 if y < 8 else 6 if y < 13 else 12
                    assert actual == rgb(PALETTES[p][index])
                    visible += actual != sky
                report["physical_capture_checks"].append(dict(mode=names[p],
                    frame=5, checked_pixels=len(fringe), visible_fringe_pixels=visible))
    (GAME / "docs/sprite-matte-diagnosis.json").write_text(json.dumps(report, indent=2) + "\n")
    for name in ("kid", "inhale", "walker", "balloon"):
        counts = [len(c["exterior_light_pixels"]) for c in cases if c["name"] == name]
        print(f"{name}: {len(counts)} frames, {min(counts)}–{max(counts)} source clearing pixels; zero visible exterior tint in all four modes")
    print("PASS: 84 sprite cases plus four particle cases under UBSan; all 81 upstream arrays unchanged")
    print("Physical captures:", report.get("physical_capture_checks", []))


if __name__ == "__main__":
    main()
