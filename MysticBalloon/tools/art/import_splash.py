#!/usr/bin/env python3
"""Convert the approved drawing to 128x128 semantic palette indices, without dithering."""
from pathlib import Path
from PIL import Image
import json
from build_art import COLOR_VARIANTS
from pixelpatch import apply
from polish_splash import refine

DIRECTORY=Path(__file__).resolve().parent/'splash'


def main():
    # The drawing was made in Pastel Island. Indices then recolor consistently
    # with every game palette; changing the default must not re-index the art.
    colors=COLOR_VARIANTS[1]
    rgb=[((c>>8&15)*17,(c>>4&15)*17,(c&15)*17) for c in colors]
    palette=Image.new('P',(1,1))
    palette.putpalette([v for color in rgb for v in color]+list(rgb[0])*240)
    image=Image.open(DIRECTORY/'continuous-source.png').convert('RGB')
    image=image.resize((128,128),Image.Resampling.NEAREST)
    image=image.quantize(palette=palette,dither=Image.Dither.NONE)
    # The palette has duplicate padding entries; retain only semantic indices.
    image.putdata([index if index<16 else 0 for index in image.tobytes()])
    image.save(DIRECTORY/'continuous-base.png')
    image,report=apply(image,json.loads((DIRECTORY/'eyes.json').read_text()))
    image=refine(image)
    image.save(DIRECTORY/'continuous-128.png')
    print('Imported continuous splash: 128x128, 16 semantic colors, no dithering')
    print('Hand-drawn eye patch:',json.dumps(report))
    print('Whole-scene pixel refinement: eight colors, explicit silhouettes and layer order')


if __name__=='__main__':main()
