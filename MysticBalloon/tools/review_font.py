"""Choose an available monospace font for offline review sheets."""
from PIL import ImageFont


def load_font(size):
    for name in ("/System/Library/Fonts/Menlo.ttc", "DejaVuSansMono.ttf",
                 "LiberationMono-Regular.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default(size=size)
