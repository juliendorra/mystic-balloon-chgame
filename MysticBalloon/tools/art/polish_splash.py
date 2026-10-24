#!/usr/bin/env python3
"""Hand-directed indexed-pixel drawing; whole-scene consistency and clean compositing."""
from pathlib import Path
import hashlib
import json
from PIL import Image,ImageDraw

DIRECTORY=Path(__file__).resolve().parent/'splash'
BASE_SHA256='8782b016ca15c22e996497e124ec2a5e6ebe103bc9b4786d78d18d1e204292aa'


def refine(base):
    if base.mode!='P' or base.size!=(128,128) or hashlib.sha256(base.tobytes()).hexdigest()!=BASE_SHA256:
        raise ValueError('Inspect the new base before reusing the composition drawing')
    image=base.copy()
    # Eight deliberate colors. Clean quantizer shades by material,
    # preserving the existing semantic indices and the directly drawn eyes.
    skin=Image.new('1',base.size);s=ImageDraw.Draw(skin)
    s.polygon([(10,83),(20,75),(28,72),(39,70),(48,79),(48,90),
               (39,96),(25,97),(16,92),(10,88)],fill=1)
    s.rectangle((4,94,23,105),fill=1);s.rectangle((53,84,64,93),fill=1)
    remap={2:12,3:5,4:5,7:14,9:8,10:12,11:0,13:1,15:1}
    for y in range(128):
        for x in range(128):
            value=base.getpixel((x,y))
            if skin.getpixel((x,y)) and value in (3,4,7,11):value=14
            else:value=remap.get(value,value)
            if value==14 and not skin.getpixel((x,y)):value=4
            image.putpixel((x,y),value)
    # Preserve a deliberate foreground cut of the child. Background wind
    # is behind this mask, with a plum silhouette instead of colored halos.
    outline=[(29,69),(39,69),(44,73),(47,77),(48,83),(53,85),(55,84),
             (59,84),(63,87),(64,90),(62,93),(57,94),(54,97),(49,101),
             (43,106),(35,111),(28,116),(25,117),(21,114),(15,114),
             (10,111),(8,108),(4,107),(0,107),(0,83),(4,86),(6,90),
             (9,92),(11,92),(9,88),(10,84),(13,81),(18,78),(23,74),(26,73)]
    child_mask=Image.new('1',base.size);ImageDraw.Draw(child_mask).polygon(outline,fill=1)
    child=image.copy()
    for y in range(69,118):
        for x in range(65):
            if child_mask.getpixel((x,y)) and child.getpixel((x,y)) in (4,5,8):child.putpixel((x,y),0)
    # Remove the old cropped balloon, fragmented reflections and loose blue arc.
    draw=ImageDraw.Draw(image);draw.rectangle((86,0,127,78),fill=0)
    for y in range(84):
        for x in range(65,128):
            if image.getpixel((x,y))==12:image.putpixel((x,y),0)
    # Clear decorative specks, retaining only the enemies' intentional eyes.
    for y in range(128):
        for x in range(128):
            eyes=(x<25 and 28<=y<71) or (x>=105 and 76<=y<89)
            if image.getpixel((x,y))==8 and not eyes:image.putpixel((x,y),0)
    # Central negative space is part of the drawing, rather than a late mask
    # cutting through objects. Keep the wind at the left and outside CONF.
    for y in range(40,85):
        for x in range(43,104):
            if image.getpixel((x,y)) in (1,5):image.putpixel((x,y),0)
    # Remove the old string and its incidental specks before drawing the
    # replacement. No illustrative subject occupies this reserved pocket.
    draw=ImageDraw.Draw(image);draw.rectangle((49,40,103,83),fill=0)
    draw.rectangle((49,84,103,95),fill=0)
    for y in range(65,81):
        for x in range(28,47):
            if image.getpixel((x,y))==5:image.putpixel((x,y),0)
    draw=ImageDraw.Draw(image)
    draw.polygon([(16,62),(26,64),(34,67),(40,71),(44,76),
                  (40,74),(34,71),(27,69),(19,68),(12,66)],fill=5)
    draw.line([(127,65),(119,69),(114,73),(110,79)],fill=5,width=2)
    # The wind below the string is a continuous tapered ribbon, not a
    # closed curl that could be read as a second knot.
    draw.polygon([(65,88),(85,88),(85,103),(78,105),(68,104),(64,99)],fill=0)
    draw.polygon([(65,98),(69,99),(72,101),(77,103),(86,106),(94,108),
                  (103,109),(107,109),(101,112),(93,111),(85,109),
                  (76,106),(70,104),(66,102)],fill=5)
    # Two short trouser legs resolve the lower body against the light vapor.
    draw.polygon([(15,109),(27,113),(28,118),(24,122),(23,125),(18,127),(11,127),(9,124),(12,119)],fill=0)
    draw.polygon([(16,111),(26,114),(26,118),(22,122),(20,125),(13,125),(12,123),(15,117)],fill=12)
    draw.line([(21,117),(17,123),(16,126)],fill=0,width=1)
    draw.line([(11,126),(16,126)],fill=1,width=1)
    draw.line([(20,124),(22,124)],fill=1,width=1)
    image.paste(child,(0,0),child_mask)
    draw=ImageDraw.Draw(image)
    # Restore the original upward pull: a large diagonal balloon extending
    # deliberately beyond the right edge, with the title in front of it.
    draw.polygon([(121,10),(127,10),(127,45),(123,50),(116,54),(109,57),
                  (102,58),(97,56),(93,52),(90,46),(91,38),(94,30),
                  (99,23),(106,17),(114,12)],fill=0)
    draw.polygon([(121,11),(127,11),(127,43),(122,48),(115,52),(108,55),
                  (102,56),(98,54),(95,50),(92,45),(93,38),(96,31),
                  (101,24),(107,18),(114,14)],fill=12)
    draw.line([(127,40),(122,46),(115,50),(108,53),(102,53)],fill=0,width=2)
    draw.line([(115,17),(110,21),(107,27)],fill=1,width=2)
    draw.line([(95,40),(95,45),(98,49)],fill=1,width=1)
    draw.polygon([(98,55),(103,57),(102,62),(96,60)],fill=0)
    draw.polygon([(99,56),(102,58),(100,60),(98,59)],fill=12)
    # A taut diagonal joins the knot to the hand. The bordered foreground
    # menu cards occlude it naturally, instead of bending the string round UI.
    string=[(99,60),(62,88)]
    draw.line(string,fill=0,width=3)
    draw.line(string,fill=1,width=1)
    # Cleanly cut fingers around the string. Every skin pixel belongs to
    # the palm or wrist; the bright line passes between the closing fingers.
    hand=['000000000000','000EEE000000','00EEEEEE0001','0EEEEEEE0010',
          '0EEEEE0EE100','0EEEEE0E0000','EEEEEEEE0000','EEEEEEE00000',
          'EEEEE0000000','000000000000']
    assert all(len(row)==12 for row in hand)
    for y,row in enumerate(hand):
        for x,color in enumerate(row):image.putpixel((53+x,84+y),int(color,16))
    # A few intentional stars; no random highlights inside the menu or face.
    for x,y in [(14,9),(89,104),(64,120)]:
        draw.line((x-1,y,x+1,y),fill=8);draw.line((x,y-1,x,y+1),fill=8)
        draw.point((x,y),fill=1)
    return image


def main():
    from build_art import scene,DEFAULT_COLORS
    from pixelpatch import apply
    base,_=apply(Image.open(DIRECTORY/'continuous-base.png'),json.loads((DIRECTORY/'eyes.json').read_text()))
    image=refine(base)
    out=DIRECTORY.parents[2]/'out/splash-polish';out.mkdir(parents=True,exist_ok=True)
    image.save(out/'candidate.png')
    for name,drawing in [('before',base),('after',image)]:
        values,_=scene(drawing)
        rgb=[((c>>8&15)*17,(c>>4&15)*17,(c&15)*17) for c in DEFAULT_COLORS]
        rendered=Image.new('RGB',(128,128));rendered.putdata([rgb[c] for c in values])
        rendered.save(out/(name+'-native.png'))
        rendered.resize((512,512),Image.Resampling.NEAREST).save(out/(name+'-global.png'))
    changed=sum(a!=b for a,b in zip(base.tobytes(),image.tobytes()))
    print(json.dumps(dict(changed=changed,colors=len(set(image.tobytes())),sha256=hashlib.sha256(image.tobytes()).hexdigest())))


if __name__=='__main__':main()
