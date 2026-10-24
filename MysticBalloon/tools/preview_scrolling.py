#!/usr/bin/env python3
"""Export edge/fan fixtures and native references for device verification."""
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw
from verify_sim import build_runner
from art.build_art import PALETTES

GAME = Path(__file__).resolve().parents[1]


def main():
    out = GAME / "out/scrolling"
    out.mkdir(parents=True, exist_ok=True)
    exe = build_runner(GAME / "tools/art/scroll_preview.cpp", out / "scroll_preview")
    result = subprocess.run([str(exe), str(out)], check=True, text=True, capture_output=True)
    cases = []
    review = Image.new("RGB", (4 * 256, 3 * 280), "#171926")
    draw = ImageDraw.Draw(review)
    names = ["COLOR", "ORIGINAL", "DMG", "AMBER"]
    for line in result.stdout.splitlines():
        p, name, frame, x, y, cx, cy = line.split(",")
        p = int(p)
        cases.append(dict(palette=p, name=name, frames=int(frame), x=int(x), y=int(y),
                          camx=int(cx), camy=int(cy)))
        raw = (out / f"{p}_{name}.bin").read_bytes()
        rgb = [((c >> 8 & 15) * 17, (c >> 4 & 15) * 17, (c & 15) * 17) for c in PALETTES[p]]
        im = Image.new("RGB", (128, 128))
        im.putdata([rgb[i] for b in raw for i in (b & 15, b >> 4)])
        im.save(out / f"{p}_{name}.png")
        row = ["left", "right", "ascent"].index(name)
        draw.text((p * 256 + 8, row * 280 + 5), f"{names[p]} / {name}", fill="white")
        review.paste(im.resize((256, 256), Image.Resampling.NEAREST), (p * 256, row * 280 + 22))
    (out / "cases.json").write_text(json.dumps(cases, indent=2) + "\n")
    review.save(GAME / "docs/scrolling-palettes.png")
    print("Rendered both side boundaries and above-map ascent in all four palettes")


if __name__ == "__main__":
    main()
