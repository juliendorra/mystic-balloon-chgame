#!/usr/bin/env python3
"""Pack the continuous splash drawing and generate palette/menu assets."""
from pathlib import Path
import re
import sys
from PIL import Image

GAME=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(GAME.parent/'CHGame/tools'))
import pixkit as pk

COLORS=[0x223,0xFFe,0x7BF,0x485,0x7C5,0xBE7,0xE45,0xF89,
        0xFC3,0xC72,0xA6C,0x753,0x26E,0xCEf,0xFDa,0xFFF]
COLOR_VARIANTS=[
    COLORS,
    [0x435,0xFFF,0x9DE,0x698,0x9D9,0xDFB,0xF68,0xFBD,
     0xFE7,0xC94,0xB9E,0x976,0x68E,0xEFF,0xFDC,0xFFF],
    [0x123,0xFFD,0x5BD,0x285,0x5B5,0x8D6,0xF43,0xFA8,
     0xFD3,0xB62,0x95C,0x653,0x36F,0xBFE,0xFCA,0xFFF],
    [0x325,0xFFE,0xAAF,0x46A,0x6CA,0xAFE,0xF36,0xFAB,
     0xFF5,0xB73,0xC6E,0x855,0x36F,0xDFF,0xFDA,0xFFF],
]
VARIANT_NAMES=['Mushroom Sky','Pastel Island','Jungle Jewel','Dreamland Pop']
DEFAULT_VARIANT=int(re.search(r'^#define CHMB_COLOR_VARIANT\s+(\d+)',
                            (GAME/'config.h').read_text(),re.M).group(1))
assert 0<=DEFAULT_VARIANT<len(COLOR_VARIANTS)
DEFAULT_COLORS=COLOR_VARIANTS[DEFAULT_VARIANT]
DMG=[0x131,0x9B1,0x363,0x363,0x8A1,0x9B1,0x8A1,0x9B1,
     0x9B1,0x363,0x8A1,0x363,0x8A1,0x9B1,0x9B1,0x9B1]
AMBER=[0x210,0xFD6,0x630,0x630,0xB70,0xFD6,0xB70,0xFD6,
       0xFD6,0x630,0xB70,0x630,0xB70,0xFD6,0xFD6,0xFD6]
PALETTES=[DEFAULT_COLORS,[0x000,0xFFF]+[0xFFF]*14,DMG,AMBER]


def arrays():
    text=re.sub(r'/\*.*?\*/|//[^\n]*','',(GAME/'bitmaps.h').read_text(),flags=re.S)
    result={}
    for name,body in re.findall(r'(\w+)\s*\[\s*\]\s*(?:PROGMEM\s*)?=\s*\{([^}]+)\}',text):
        if name=='levels':continue
        try:result[name]=[int(v.strip(),0) if v.strip().startswith(('0x','0b')) else int(v.strip()) for v in body.split(',') if v.strip()]
        except ValueError:pass
    return result


def rle(data):
    """PackBits: high control bit repeats a byte; low seven bits + 1 are length."""
    out=[]; i=0
    while i<len(data):
        run=1
        while i+run<len(data) and data[i+run]==data[i] and run<128:run+=1
        if run>=3:
            out.extend((128|run-1,data[i]));i+=run
        else:
            start=i;i+=run
            while i<len(data) and i-start<128:
                run=1
                while i+run<len(data) and data[i+run]==data[i] and run<3:run+=1
                if run>=3:break
                i+=min(run,128-(i-start))
            out.append(i-start-1);out.extend(data[start:i])
    return out


def declaration(name,data):
    lines=[f'static constexpr uint8_t {name}[] = {{']
    lines += ['    '+','.join(f'0x{v:02x}' for v in data[i:i+24])+',' for i in range(0,len(data),24)]
    return '\n'.join(lines+['};'])


def tint_mask(data,frame):
    """Interior white pixels, excluding the original opaque white matte."""
    w,h=data[:2]; offset=2+frame*w*((h+7)//8)
    white={(x,y) for y in range(h) for x in range(w)
           if data[offset+(y//8)*w+x] & (1<<(y&7))}
    edge=[p for p in white if p[0] in (0,w-1) or p[1] in (0,h-1)]
    outside=set(edge)
    while edge:
        x,y=edge.pop()
        for p in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
            if p in white and p not in outside:outside.add(p);edge.append(p)
    return white-outside


def player_color_sprite(data,masked=False):
    """Opaque black contours plus enclosed white body; discard clearing matte."""
    w,h=data[:2];stride=2 if masked else 1
    count=(len(data)-2)//(w*((h+7)//8)*stride) if masked else 12
    result=[w,h]
    canvas={(x,y) for y in range(h) for x in range(w)}
    def bits(frame,component=0):
        return {(x,y) for x,y in canvas
                if data[2+(frame*w*((h+7)//8)+(y//8)*w+x)*stride+component] & (1<<(y&7))}
    for frame in range(count):
        if masked:
            white=bits(frame);black=bits(frame,1)-white
        else:
            black=bits(frame);white=bits(12+frame//6)-black
        free=canvas-black
        todo=[p for p in free if p[0] in (0,w-1) or p[1] in (0,h-1)]
        outside=set(todo)
        while todo:
            x,y=todo.pop()
            for p in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
                if p in free and p not in outside:outside.add(p);todo.append(p)
        white-=outside
        opacity=black|white
        for page in range((h+7)//8):
            for x in range(w):
                for pixels in (white,opacity):
                    result.append(sum(((x,page*8+b) in pixels)<<b for b in range(8)))
    return result


def terrain_corners(data,frame):
    """White exterior at convex corners; joined tile edges stay opaque."""
    w,h=data[:2];offset=2+frame*w*((h+7)//8)
    white={(x,y) for y in range(h) for x in range(w)
           if data[offset+(y//8)*w+x] & (1<<(y&7))}
    # Neighbor bits are right/top/left/bottom. Only corners between two
    # exposed sides seed the exterior; open joins contain valid white fill.
    todo=[p for p,a,b in [((0,0),4,2),((w-1,0),1,2),
                          ((0,h-1),4,8),((w-1,h-1),1,8)]
          if not frame&(a|b) and p in white]
    outside=set(todo)
    while todo:
        x,y=todo.pop()
        for p in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
            if p in white and p not in outside:outside.add(p);todo.append(p)
    return outside


def scene(drawing=None):
    a=arrays()
    if drawing is None:drawing=Image.open(GAME/'tools/art/splash/continuous-128.png')
    assert drawing.mode=='P' and drawing.size==(128,128)
    color=list(drawing.tobytes())
    assert all(0<=c<16 for c in color)
    mono=[int(c!=0) for c in color]
    def pixel(x,y,c,b=1):
        if 0<=x<128 and 0<=y<128:color[y*128+x]=c;mono[y*128+x]=b
    def sprite(name,x,y,frame=0,scale=1,ink=1,paper=None,closed=False,trim=False):
        src=a[name];w,h=src[:2];stride=2 if 'plus_mask' in name else 1
        mask=tint_mask(src,frame) if closed else None
        for sy in range(h):
            first,last=0,w
            if trim:
                row=[sx for sx in range(w) if src[2+(frame*w*((h+7)//8)+(sy//8)*w+sx)*stride] & (1<<(sy&7))]
                if not row:continue
                first,last=min(row),max(row)+1
            for sx in range(first,last):
                at=2+(frame*w*((h+7)//8)+(sy//8)*w+sx)*stride
                bit=1<<(sy&7);on=bool(src[at]&bit)
                if closed and on and (sx,sy) not in mask:continue
                if not on and (paper is None or stride==2 and not src[at+1]&bit):continue
                c=(ink(sx,sy) if callable(ink) else ink) if on else paper
                for dy in range(scale):
                    for dx in range(scale):pixel(x+sx*scale+dx,y+sy*scale+dy,c,int(on))
    # Only the original title lettering overlays the single continuous drawing.
    sprite('badgeMysticBalloon',21,4,scale=2,ink=lambda x,y:1 if y<5 else 8,paper=0,trim=True)
    return color,mono


def main():
    color,mono=scene()
    pack=lambda values:[values[i] | values[i+1]<<4 for i in range(0,len(values),2)]
    # Monochrome needs one bit per pixel, not a four-bit palette index.
    mono_bits=[sum(mono[i+j]<<j for j in range(8)) for i in range(0,len(mono),8)]
    c,b=rle(pack(color)),rle(mono_bits)
    header='// Generated by tools/art/build_art.py; drawing source/provenance in tools/art/splash/.\n#pragma once\n#include <stdint.h>\nnamespace mystic { namespace art {\n'
    header+=f'static constexpr uint8_t defaultColorVariant = {DEFAULT_VARIANT};\n'
    header+='static constexpr uint16_t palettes[4][16] = {\n'
    header+='\n'.join('    {'+','.join(f'0x{v:03x}' for v in p)+'},' for p in PALETTES)+'\n};\n'
    header+='static constexpr uint16_t colorPalettes[4][16] = {\n'
    header+='\n'.join('    {'+','.join(f'0x{v:03x}' for v in p)+'},' for p in COLOR_VARIANTS)+'\n};\n'
    header+=declaration('splashColor',c)+'\n'+declaration('splashMono',b)+'\n'
    header+=declaration('kidColor',player_color_sprite(arrays()['kidSprite']))+'\n'
    header+=declaration('kidSuckColor',player_color_sprite(arrays()['kidSpriteSuck_plus_mask'],True))+'\n'
    tiles=arrays()['tileSetTwo'];offsets=[0];corners=[];opacity=[]
    for frame in range(16):
        outside=terrain_corners(tiles,frame)
        assert len(outside)<=8
        corners += [x|(y<<4) for x,y in sorted(outside)]
        offsets.append(len(corners))
        opacity += [255-sum(((x,page*8+b) in outside)<<b for b in range(8))
                    for page in range(2) for x in range(16)]
    header+=declaration('terrainCornerOffsets',offsets)+'\n'
    header+=declaration('terrainCorners',corners)+'\n'
    header+=declaration('terrainSky',tiles[2+16*32:2+17*32])+'\n'
    header+=declaration('tileSetTwoTint',opacity+[255]*32)+'\n'
    for name in ['elements','fan','sprSpikes','walkerSprite','door']:
        data=arrays()[name];w,h=data[:2];n=(len(data)-2)//(w*((h+7)//8));packed=[]
        for f in range(n):
            mask=tint_mask(data,f)
            packed += [sum(((x,page*8+bit) in mask)<<bit for bit in range(8))
                       for page in range((h+7)//8) for x in range(w)]
        header+=declaration(name+'Tint',packed)+'\n'
    header+='}}\n'
    (GAME/'SplashArt.h').write_text(header)
    labels=['COLOR','ORIGINAL','DMG GREEN','AMBER','SOUND ON','SOUND OFF',
            'RESUME','SAVE & QUIT','RESTARTS LEVEL','SAVE FAILED','AUDIO ATM','AUDIO BEEP']
    header='// Generated by tools/art/build_art.py; CHGame 3x5 menu lettering.\n#pragma once\n#include <stdint.h>\nnamespace mystic { namespace art {\n'
    for i,label in enumerate(labels):
        fb=pk.FB(0);fb.text35(0,0,label,1);w=pk.text35_width(label)
        data=[w,5]+[sum((fb.p[y*128+x]&1)<<y for y in range(5)) for x in range(w)]
        header+=declaration('label'+str(i),data)+'\n'
    header+='static constexpr const uint8_t *labels[] = {'+','.join('label'+str(i) for i in range(len(labels)))+'};\n'
    for name,w,h in [('configBlank',52,51),('pausePanel',88,56)]:
        data=[w,h]+[sum((x in (0,w-1) or page*8+b in (0,h-1))<<b
                          for b in range(min(8,h-page*8)))
                     for page in range((h+7)//8) for x in range(w)]
        header+=declaration(name,data)+'\n'
    header+='static constexpr uint8_t selected[] = {3,3,2,7,2};\n'
    header+='static constexpr uint8_t cursor[] = {3,5,31,14,4};\n}}\n'
    (GAME/'MenuArt.h').write_text(header)
    (GAME/'out/art').mkdir(parents=True,exist_ok=True)
    for name,indices,palette in [('splash',color,DEFAULT_COLORS),('splash-bw',mono,PALETTES[1])]:
        im=Image.new('RGB',(128,128));im.putdata([pk.rgb(palette[i]) for i in indices]);im.save(GAME/f'out/art/{name}.png')
    print(f'Splash: {len(c)} color bytes + {len(b)} monochrome bytes; 128x128 native pixels')

if __name__=='__main__':main()
