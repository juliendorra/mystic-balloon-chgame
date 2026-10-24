"""Visual-menu picture using TEAM a.r.g.'s original title-screen sprites."""
from pathlib import Path
import re
import sys
from PIL import Image

GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GAME.parent / "CHGame/tools"))
import pixkit as pk


def draw():
    source = (GAME / "bitmaps.h").read_text()
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    colours = [(17, 17, 34), (221, 238, 255), (255, 204, 136), (85, 136, 170)]
    fb = pk.FB(0)

    def sprite(name, x, y, frame=0):
        body = re.search(rf"\b{name}\[\]\s*=\s*\{{([^}}]+)\}}", source).group(1)
        data = [int(v.strip(), 0) if v.strip().startswith(("0x", "0b")) else int(v.strip())
                for v in body.split(",") if v.strip()]
        w, h = data[:2]
        offset = 2 + frame * w * ((h + 7) // 8)
        for sy in range(h):
            for sx in range(w):
                if data[offset + (sy // 8) * w + sx] & (1 << (sy % 8)):
                    fb.pixel(x + sx, y + sy, 1)

    fb.text35x2(9, 5, "MYSTIC BALLOON", 2)
    fb.text35(36, 20, "TEAM A.R.G.", 3)
    fb.hline(0, 30, 128, 3)
    for i in range(4):
        sprite("titleScreen", i * 32, 32, i)
    sprite("badgeMysticBalloon", 85, 77)
    sprite("stars", 79, 75)
    sprite("leftGuyLeftEye", 9, 41)
    sprite("leftGuyRightEye", 15, 45)
    sprite("rightGuyEyes", 109, 66)
    fb.hline(0, 97, 128, 3)
    fb.text35(25, 104, "39 LEVELS TO EXPLORE", 1)
    fb.text35(27, 115, "ARDUBOY TO CHGAME", 3)
    out = Image.new("RGB", (128, 128))
    out.putdata([colours[i] for i in fb.p])
    return out
