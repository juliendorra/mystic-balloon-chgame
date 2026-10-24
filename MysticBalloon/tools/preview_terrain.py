#!/usr/bin/env python3
"""Review platform transparency through the production renderer in all modes."""
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw, ImageFont
from art.build_art import PALETTES
from verify_sim import build_runner

GAME = Path(__file__).resolve().parents[1]


def main():
    out = GAME / "out/terrain-review"
    out.mkdir(parents=True, exist_ok=True)
    exe = build_runner(GAME / "tools/art/terrain_preview.cpp", out / "terrain_preview")
    subprocess.run([str(exe), str(out)], check=True)
    font = ImageFont.truetype("/System/Library/Fonts/Menlo.ttc", 13)
    review = Image.new("RGB", (4 * 224, 2 * 246), "#171926")
    full = Image.new("RGB", (4 * 272, 292), "#171926")
    draw, full_draw = ImageDraw.Draw(review), ImageDraw.Draw(full)
    for p, name in enumerate(["COLOR", "ORIGINAL B&W", "DMG GREEN", "AMBER"]):
        rgb = [((c >> 8 & 15) * 17, (c >> 4 & 15) * 17, (c & 15) * 17) for c in PALETTES[p]]
        for corrected in range(2):
            raw = (out / f"{p}_{corrected}.bin").read_bytes()
            im = Image.new("RGB", (128, 128))
            im.putdata([rgb[i] for b in raw for i in (b & 15, b >> 4)])
            im.save(out / f"{p}_{corrected}.png")
            x, y = p * 224 + 16, corrected * 246
            draw.text((x, y + 5), name, font=font, fill="white")
            draw.text((x, y + 24), "AFTER" if corrected else "BEFORE", font=font, fill="#bbbbcc")
            review.paste(im.crop((16, 16, 40, 40)).resize((192, 192), Image.Resampling.NEAREST), (x, y + 44))
            if corrected:
                full_draw.text((p * 272 + 8, 8), name, font=font, fill="white")
                full.paste(im.resize((256, 256), Image.Resampling.NEAREST), (p * 272 + 8, 30))
    review.save(GAME / "docs/platform-corners.png")
    full.save(GAME / "docs/platform-joins.png")
    print("Rendered transparent corners and preserved joins in Color, Original B&W, DMG and Amber")


if __name__ == "__main__":
    main()
