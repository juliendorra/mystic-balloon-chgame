#!/usr/bin/env python3
"""Review the production splash and menu overlays through the real renderer."""
from pathlib import Path
import subprocess
import sys
from PIL import Image,ImageDraw,ImageFont
from review_font import load_font
from verify_sim import build_runner

GAME=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(GAME/'tools/art'))
from build_art import PALETTES


def main():
    out=GAME/'out/splash-review';out.mkdir(parents=True,exist_ok=True)
    exe=build_runner(GAME/'tools/art/splash_preview.cpp',out/'splash_preview')
    subprocess.run([str(exe),str(out)],check=True)
    font=load_font(12)
    names=['PASTEL ISLAND','ORIGINAL B&W','DMG GREEN','AMBER']
    scenes=[('art',0),('main',0),('main',1),('main',3),('config',0),('config',4),('pause',0),('pause',1)]
    review=Image.new('RGB',(4*272,36+len(scenes)*280),'#151626')
    draw=ImageDraw.Draw(review)
    for column,(name,palette) in enumerate(zip(names,PALETTES)):
        rgb=[((c>>8&15)*17,(c>>4&15)*17,(c&15)*17) for c in palette]
        draw.text((column*272+8,10),name,font=font,fill='white')
        for row,(scene,frame) in enumerate(scenes):
            raw=(out/f'{column}_{scene}_{frame}.bin').read_bytes()
            indices=[c for b in raw for c in (b&15,b>>4)]
            image=Image.new('RGB',(128,128));image.putdata([rgb[c] for c in indices])
            image.save(out/f'{name.lower().replace(" ","-")}-{scene}-{frame}.png')
            x=column*272+8;y=36+row*280
            draw.text((x,y),f'{scene} {frame}',font=font,fill='#bbbbcc')
            review.paste(image.resize((256,256),Image.Resampling.NEAREST),(x,y+17))
    review.save(GAME/'docs/splash-review.png')
    print('Rendered art and main/config selections in all four display modes')


if __name__=='__main__':main()
