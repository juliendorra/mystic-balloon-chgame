#!/usr/bin/env python3
"""Record the same README gameplay through each actual display mode."""
from pathlib import Path
import json
import sys

from PIL import Image, ImageDraw, ImageSequence

GAME = Path(__file__).resolve().parents[1]
SDK_TOOLS = GAME.parent / "CHGame/tools"
sys.path.insert(0, str(SDK_TOOLS))
sys.path.insert(0, str(SDK_TOOLS / "chsim"))
from chsim import build
from chdrivelib import Driver, SimTransport
from readme_gif import join, LIMIT
from review_font import load_font
from verify_device import fields

MODES = [("color", "Color / Pastel Island"), ("original", "Original B&W"),
         ("dmg", "Game Boy DMG"), ("amber", "Amber")]
STATE_FIELDS = ("state", "level", "x", "y", "sy", "balloons", "coins",
                "total", "key", "score", "camx", "camy", "view")


class RecordingDriver(Driver):
    def __init__(self, transport, palette):
        super().__init__(transport, "MYBL ")
        self.palette = palette
        self.states = []

    def shot(self):
        if getattr(self, "rec", None) is not None:
            state = fields(self.query("V", "MYBL "))
            assert state["palette"] == self.palette and state["dropped"] == 0
            self.states.append({key: state[key] for key in STATE_FIELDS})
        return super().shot()


def timeline(path):
    with Image.open(path) as image:
        return [(frame.convert("RGB").copy(), frame.info["duration"])
                for frame in ImageSequence.Iterator(image)]


def main():
    source = (GAME / "tools/scripts/gameplay.txt").read_text()
    assert "rec start 4" in source
    out = GAME / "out/palette-previews"
    out.mkdir(parents=True, exist_ok=True)
    executable = build(GAME)
    reference_states = None
    results, reels = [], []
    for palette, (slug, name) in enumerate(MODES):
        work = out / slug
        work.mkdir(exist_ok=True)
        script = work / "gameplay.txt"
        script.write_text(source.replace("rec start 4", f"say C {palette}\nrec start 4", 1))
        transport = SimTransport(executable)
        driver = RecordingDriver(transport, palette)
        try:
            driver.run(script, work)
        finally:
            code = transport.close()
        assert code == 0 and not transport.bugs, transport.bugs
        if reference_states is None:
            reference_states = driver.states
        else:
            assert driver.states == reference_states, f"{name} gameplay diverged"
        clips = [work / filename for filename in ("01_title.gif", "02_play.gif", "03_world.gif")]
        candidate = work / "gameplay.gif"
        frames, seconds, colors = join(clips, candidate)
        assert candidate.stat().st_size < LIMIT
        reel = timeline(candidate)
        assert len(reel) == 60 and sum(duration for _, duration in reel) == 6630
        destination = GAME / "docs" / ("gameplay.gif" if palette == 0 else f"gameplay-{slug}.gif")
        if palette == 0:
            existing = timeline(destination)
            assert [(im.tobytes(), ms) for im, ms in reel] == [(im.tobytes(), ms) for im, ms in existing], \
                "Color capture must match the existing README preview exactly"
        else:
            destination.write_bytes(candidate.read_bytes())
        assert all(image.size == (256, 256) for image, _ in reel)
        reels.append(reel)
        results.append(dict(mode=name, palette=palette, file=destination.relative_to(GAME).as_posix(),
                            frames=frames, seconds=seconds, max_colors=colors,
                            bytes=destination.stat().st_size, matched_game_states=len(driver.states)))
        print(f"{name}: {frames} frames, {seconds:.2f} s, {destination.stat().st_size:,} B")

    review = Image.new("RGB", (4 * 256, 4 * 282), "#171926")
    draw = ImageDraw.Draw(review)
    for column, ((slug, name), reel) in enumerate(zip(MODES, reels)):
        for row, frame in enumerate((0, 14, 32, 49)):
            draw.text((column * 256 + 4, row * 282 + 4), f"{name} / {frame}",
                      fill="white", font=load_font(12))
            review.paste(reel[frame][0], (column * 256, row * 282 + 24))
    review.save(out / "review.png")
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("PASS: identical gameplay states, scene cuts and timing in all four actual render modes")


if __name__ == "__main__":
    main()
