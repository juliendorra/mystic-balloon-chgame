#!/usr/bin/env python3
"""Render four SNES-inspired color studies and every sprite animation frame."""
from pathlib import Path
import json
import subprocess
import sys
from PIL import Image,ImageDraw,ImageFont
from review_font import load_font

GAME=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(GAME/'tools/art'))
from build_art import COLOR_VARIANTS,VARIANT_NAMES
from verify_sim import build_runner

SLUGS=['mushroom','pastel','jungle','dreamland']
INSPIRED=['Super Mario World','Yoshi\'s Island','Donkey Kong Country','Kirby Super Star']
FONT=load_font(14)
SMALL=load_font(11)


def main():
    out=GAME/'out/color-study';out.mkdir(parents=True,exist_ok=True)
    exe=build_runner(GAME/'tools/art/color_preview.cpp',out/'color_preview')
    lines=subprocess.check_output([str(exe),str(out)],text=True).splitlines()
    rows=[]
    for line in lines:
        name,frame,w,h,x,y=line.split('|')
        rows.append(dict(name=name,frame=int(frame),w=int(w),h=int(h),x=int(x),y=int(y)))
    (out/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
    def image(row,palette):
        raw=(out/f"{row['name']}_{row['frame']}.bin").read_bytes()
        pixels=[c for b in raw for c in (b&15,b>>4)]
        rgb=[(((c>>8)&15)*17,((c>>4)&15)*17,(c&15)*17) for c in palette]
        im=Image.new('RGB',(128,128));im.putdata([rgb[c] for c in pixels])
        return im.crop((row['x'],row['y'],row['x']+row['w'],row['y']+row['h']))
    levels=[r for r in rows if r['name']=='level']
    scale=2;cw=280;rh=286;header=80
    study=Image.new('RGB',(cw*4,header+rh*4),'#151626');d=ImageDraw.Draw(study)
    for i,(palette,name,source) in enumerate(zip(COLOR_VARIANTS,VARIANT_NAMES,INSPIRED)):
        x=i*cw
        d.text((x+12,10),f'{i+1}. {name}',font=FONT,fill='#fff3d9')
        d.text((x+12,33),source,font=SMALL,fill='#b8bfd3')
        for c in range(16):
            v=palette[c];rgb=(((v>>8)&15)*17,((v>>4)&15)*17,(v&15)*17)
            d.rectangle((x+12+c*16,55,x+26+c*16,66),fill=rgb)
        for j,row in enumerate(levels):
            y=header+j*rh
            d.text((x+12,y),f"LEVEL {row['frame']:02}",font=SMALL,fill='#d2d9e8')
            study.paste(image(row,palette).resize((256,256),Image.Resampling.NEAREST),(x+12,y+18))
    study.save(GAME/'docs/color-palette-study.png')
    # Every original visual array, including unused menu art and all animation frames.
    assets=[r for r in rows if r['name'] not in ('level','splash')]
    groups=[]
    for row in assets:
        if not groups or groups[-1][0]['name']!=row['name']:groups.append([])
        groups[-1].append(row)
    width=768;columns=[66,66];placements=[]
    for group in groups:
        w=group[0]['w']*3;h=group[0]['h']*3
        span=w>360 or len(group)>max(3,360//(w+10))
        if span:
            x=16;y=max(columns);cols=max(1,(width-32)//(w+10))
            height=24+((len(group)+cols-1)//cols)*(h+20)+16
            columns=[y+height,y+height]
        else:
            col=0 if columns[0]<=columns[1] else 1
            x=16+col*376;y=columns[col];cols=max(1,360//(w+10))
            height=24+((len(group)+cols-1)//cols)*(h+20)+16
            columns[col]+=height
        placements.append((group,x,y,cols))
    atlas_height=max(columns)
    atlases=[]
    for p,(palette,name,slug) in enumerate(zip(COLOR_VARIANTS,VARIANT_NAMES,SLUGS)):
        atlas=Image.new('RGB',(width,atlas_height),'#151626');ad=ImageDraw.Draw(atlas)
        ad.text((16,12),f'{p+1}. {name} - complete sprite sheet',font=FONT,fill='#fff3d9')
        ad.text((16,36),'All source frames; player frames 0-11 assembled with their original masks.',font=SMALL,fill='#aeb8cf')
        for group,left,y,cols in placements:
            ad.text((left,y),f"{group[0]['name']}  {group[0]['w']}x{group[0]['h']}  [{len(group)} frames]",font=SMALL,fill='#ddd7be')
            for k,row in enumerate(group):
                x=left+(k%cols)*(row['w']*3+10);py=y+22+(k//cols)*(row['h']*3+20)
                atlas.paste(image(row,palette).resize((row['w']*3,row['h']*3),Image.Resampling.NEAREST),(x,py))
                ad.text((x,py+row['h']*3+2),str(row['frame']),font=SMALL,fill='#8b9ab2')
        atlas.save(GAME/f'docs/sprites-{slug}.png');atlases.append(atlas)
    # Compact side-by-side overview; the separate sheets retain 3x pixels.
    overview=Image.new('RGB',(width*4,atlas_height))
    for i,atlas in enumerate(atlases):overview.paste(atlas,(i*width,0))
    overview.save(GAME/'docs/sprites-four-palettes.png')
    print(f'PASS {len(assets)} sprite frames in {len(groups)} groups, four levels x four palettes')
    print(f'Full sheets: {width}x{atlas_height}; comparison: {study.width}x{study.height}')

if __name__=='__main__':main()
