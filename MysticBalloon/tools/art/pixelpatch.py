#!/usr/bin/env python3
"""Exact indexed-pixel edits: inspect a crop, apply tiny text stencils, review the diff."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image,ImageDraw


def digest(image):
    return hashlib.sha256(image.tobytes()).hexdigest()


def indexed(path):
    image=Image.open(path)
    if image.mode!='P' or max(image.tobytes())>15:
        raise ValueError('Expected an indexed PNG using palette indices 0..15')
    return image


def apply(image,spec):
    if image.mode!='P' or list(image.size)!=spec['size']:
        raise ValueError('Canvas mode or size changed; inspect the new source first')
    if digest(image)!=spec['base_sha256']:
        raise ValueError('Source pixels changed; inspect them and update the patch deliberately')
    result=image.copy()
    cells=0
    for patch in spec['patches']:
        x,y=patch['at'];rows=patch['rows'];width=len(rows[0])
        if not width or any(len(row)!=width for row in rows):
            raise ValueError('Stencil rows must have one consistent nonzero width')
        if x<0 or y<0 or x+width>image.width or y+len(rows)>image.height:
            raise ValueError('Stencil is outside the canvas')
        for dy,row in enumerate(rows):
            for dx,symbol in enumerate(row):
                if symbol=='.':continue
                if symbol not in '0123456789ABCDEF':
                    raise ValueError(f'Unknown palette symbol {symbol!r}; use 0..F or .')
                result.putpixel((x+dx,y+dy),int(symbol,16));cells+=1
    changed=[(i%image.width,i//image.width) for i,(a,b) in
             enumerate(zip(image.tobytes(),result.tobytes())) if a!=b]
    bounds=None if not changed else [min(x for x,y in changed),min(y for x,y in changed),
                                     max(x for x,y in changed)+1,max(y for x,y in changed)+1]
    return result,dict(changed=len(changed),bounds=bounds,stencil_cells=cells,
                       canvas_cells=image.width*image.height,sha256=digest(result))


def review(before,after,box,path):
    x,y,w,h=box;crop=(x,y,x+w,y+h);scale=12
    width=max(w*scale,before.width*3+8);height=h*scale+before.height*2+48
    sheet=Image.new('RGB',(width*3,height),'#171926');draw=ImageDraw.Draw(sheet)
    diff=Image.new('RGB',before.size,'#171926')
    for py in range(before.height):
        for px in range(before.width):
            if before.getpixel((px,py))!=after.getpixel((px,py)):diff.putpixel((px,py),(255,102,136))
    for column,(label,image) in enumerate([('BEFORE',before),('AFTER',after),('CHANGED',diff)]):
        ox=column*width;draw.text((ox+4,4),label,fill='white')
        sheet.paste(image.crop(crop).convert('RGB').resize((w*scale,h*scale),Image.Resampling.NEAREST),(ox,22))
        rgb=image.convert('RGB')
        sheet.paste(rgb.resize((image.width*2,image.height*2),Image.Resampling.NEAREST),(ox,30+h*scale))
        sheet.paste(rgb,(ox+image.width*2+4,30+h*scale))
    sheet.save(path)


def overview(before,after,path):
    if before.size!=after.size:raise ValueError('Overview canvases must have equal dimensions')
    before=before.convert('RGB');after=after.convert('RGB')
    w,h=before.size;zoom=3;column=w*zoom
    sheet=Image.new('RGB',(column*3,38+h*4),'#171926');draw=ImageDraw.Draw(sheet)
    diff=Image.new('RGB',before.size,'#171926');changed=0
    for y in range(h):
        for x in range(w):
            if before.getpixel((x,y))!=after.getpixel((x,y)):
                diff.putpixel((x,y),(255,102,136));changed+=1
    for i,(label,image) in enumerate([('BEFORE / FULL COMPOSITION',before),
                                     ('AFTER / FULL COMPOSITION',after),('EXACT PIXEL DIFFERENCE',diff)]):
        x=i*column;draw.text((x+5,5),label,fill='white')
        sheet.paste(image.resize((column,h*zoom),Image.Resampling.NEAREST),(x,22))
        sheet.paste(image,(x,30+h*zoom))
        draw.text((x+w+6,34+h*zoom),'1:1 native size',fill='white')
    path.parent.mkdir(parents=True,exist_ok=True);sheet.save(path)
    print(json.dumps(dict(changed=changed,view='whole canvas at 3x and 1x',size=before.size)))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation',choices=['inspect','draw','overview'])
    parser.add_argument('image',type=Path)
    parser.add_argument('--box',type=int,nargs=4,metavar=('X','Y','W','H'))
    parser.add_argument('--patch',type=Path)
    parser.add_argument('--out',type=Path)
    parser.add_argument('--review',type=Path)
    parser.add_argument('--after',type=Path)
    args=parser.parse_args()
    try:
        if args.operation=='overview':
            if not args.after or not args.out:raise ValueError('overview requires --after PNG --out PNG')
            overview(Image.open(args.image),Image.open(args.after),args.out);return
        image=indexed(args.image)
        if args.operation=='inspect':
            if not args.box:raise ValueError('inspect requires --box X Y W H')
            x,y,w,h=args.box
            if w*h>1024 or min(x,y)<0 or min(w,h)<1 or x+w>image.width or y+h>image.height:
                raise ValueError('Inspect a valid crop of at most 1024 pixels')
            print(json.dumps(dict(size=image.size,base_sha256=digest(image),box=args.box)))
            print('    '+''.join(str((x+i)//10%10) for i in range(w)))
            print('    '+''.join(str((x+i)%10) for i in range(w)))
            for j in range(h):print(f'{y+j:3} '+''.join(f'{image.getpixel((x+i,y+j)):X}' for i in range(w)))
        else:
            if not args.patch or not args.out:raise ValueError('draw requires --patch FILE --out PNG')
            spec=json.loads(args.patch.read_text())
            result,report=apply(image,spec)
            args.out.parent.mkdir(parents=True,exist_ok=True);result.save(args.out)
            if args.review:review(image,result,args.box or spec['review'],args.review)
            print(json.dumps(report))
    except (ValueError,KeyError,IndexError) as error:parser.error(str(error))


if __name__=='__main__':main()
