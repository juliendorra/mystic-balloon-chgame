"""Use the expanded, native-color splash artwork as the cartridge picture."""
from pathlib import Path
from collections import Counter
import sys
from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parent/'art'))
from build_art import DEFAULT_COLORS as COLORS, scene


def draw():
    color,_=scene()
    image=Image.new('RGB',(128,128))
    image.putdata([(((COLORS[i]>>8)&15)*17,((COLORS[i]>>4)&15)*17,(COLORS[i]&15)*17)
                   for i in color])
    # The launcher reserves five of its sixteen slots for its own UI. Keep
    # the eleven dominant splash colors and merge only rare shading colors.
    dominant=[COLORS[i] for i,_ in Counter(color).most_common(11)]
    rgb=[(((c>>8)&15)*17,((c>>4)&15)*17,(c&15)*17) for c in dominant]
    palette=Image.new('P',(1,1))
    palette.putpalette([v for c in rgb for v in c]+list(rgb[0])*(256-len(rgb)))
    return image.quantize(palette=palette,dither=Image.Dither.NONE).convert('RGB')
