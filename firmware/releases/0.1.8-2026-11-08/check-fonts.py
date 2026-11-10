# /// script
# dependencies = ["Pillow>=12,<13"]
# ///
from pathlib import Path
import argparse, json, re, zlib, statistics
from PIL import Image, ImageDraw, ImageFont

parser=argparse.ArgumentParser(description='Check embedded Chinese counter strokes and unchanged typography')
parser.add_argument('before',type=Path)
parser.add_argument('after',type=Path)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();root=args.before;stage=args.after;out=args.output
out.mkdir(parents=True,exist_ok=True)

def array(text, name):
    body = re.search(r'\b'+name+r'\[\].*?\{(.*?)\};', text, re.S).group(1)
    return [int(x) for x in body.replace('\n', '').split(',') if x.strip()]

def read_fonts(project):
    text = (project/'firmware/epaper_dashboard/font_data.h').read_text()
    codes = array(text, 'GLYPH_CODES'); offsets = array(text, 'GLYPH_BLOCKS')
    blob = bytes(array(text, 'GLYPH_ZLIB'))
    raw = b''.join(zlib.decompress(blob[a:b]) for a,b in zip(offsets, offsets[1:]))
    assert len(raw) == len(codes)*32
    body = {c:raw[i*32:i*32+32] for i,c in enumerate(codes)}
    text = (project/'firmware/epaper_dashboard/generated_styles.h').read_text()
    styles = {}
    for name in ('Header18', 'Title26', 'Quote48', 'Clock80'):
        bits = bytes(array(text, name+'Bits'))
        data = re.search(name+r'Glyphs\[\].*?\{(.*?)\};',text,re.S).group(1)
        rows = {v[0]:v[1:] for row in re.findall(r'\{([^{}]+)\}', data)
                if (v:=[int(x) for x in row.split(',')])}
        styles[name] = (bits, rows)
    return body, styles

def glyph(fonts, kind, char):
    if kind == 'body': return Image.frombytes('1',(16,16),fonts[0][ord(char)])
    bits,rows = fonts[1][kind]
    off,w,h,advance,dx,dy = rows[ord(char)]
    return Image.frombytes('1',(w,h),bits[off:off+((w+7)//8)*h])

def counters(image):
    w,h=image.size; seen=set(); holes=[]
    for y in range(h):
        for x in range(w):
            if image.getpixel((x,y)) or (x,y) in seen: continue
            stack=[(x,y)]; seen.add((x,y)); cells=[]; edge=False
            while stack:
                xx,yy=stack.pop(); cells.append((xx,yy))
                edge |= xx in (0,w-1) or yy in (0,h-1)
                for nx,ny in ((xx-1,yy),(xx+1,yy),(xx,yy-1),(xx,yy+1)):
                    if 0<=nx<w and 0<=ny<h and (nx,ny) not in seen and not image.getpixel((nx,ny)):
                        seen.add((nx,ny)); stack.append((nx,ny))
            if not edge:
                # Isolated white pixels at diagonal joins are not mouth counters.
                xs=[x for x,y in cells]; ys=[y for x,y in cells]
                if max(xs)>min(xs) and max(ys)>min(ys): holes.append(cells)
    return holes

def strokes(image, char):
    result=[]
    for cells in counters(image):
        # Test the actual mouth radical, excluding gaps between other strokes.
        if char=='和' and min(x for x,y in cells)<image.width//2:continue
        if char=='周' and min(y for x,y in cells)<image.height//2:continue
        rows=sorted({y for x,y in cells}); left=[]; right=[]
        for y in rows:
            xs=[x for x,yy in cells if yy==y]; lo,hi=min(xs),max(xs)
            n=0; x=lo-1
            while x>=0 and image.getpixel((x,y)):n+=1;x-=1
            left.append(n);n=0;x=hi+1
            while x<image.width and image.getpixel((x,y)):n+=1;x+=1
            right.append(n)
        # Intersections with other radicals can extend a run on a few rows.
        # The median measures the straight wall, away from those junctions.
        top=[]; bottom=[]
        for x in sorted({x for x,y in cells}):
            ys=[y for xx,y in cells if xx==x];lo,hi=min(ys),max(ys)
            n=0;y=lo-1
            while y>=0 and image.getpixel((x,y)):n+=1;y-=1
            top.append(n);n=0;y=hi+1
            while y<image.height and image.getpixel((x,y)):n+=1;y+=1
            bottom.append(n)
        result.append({'left':statistics.median(left),'right':statistics.median(right),
                       'top':statistics.median(top),'bottom':statistics.median(bottom)})
    return result

old=read_fonts(root); new=read_fonts(stage)
assert old[0].keys()==new[0].keys()
for code,data in old[0].items():
    if 32<=code<127: assert data==new[0][code], 'ASCII changed'
    if any(data): assert any(new[0][code]), f'Newly empty glyph: {chr(code)}'
for name in ('Title26','Quote48','Clock80'):assert old[1][name]==new[1][name]
for code,row in old[1]['Header18'][1].items():
    if code>=128:continue
    nr=new[1]['Header18'][1][code];assert row[1:]==nr[1:]
    off,w,h,*_=row; noff=nr[0];n=((w+7)//8)*h
    assert old[1]['Header18'][0][off:off+n]==new[1]['Header18'][0][noff:noff+n]

cases=[('body','日',2),('body','口',1),('body','和',1),('body','周',1),('Header18','日',2),('Header18','周',1)]
checks=[]
for kind,char,expected in cases:
    before=strokes(glyph(old,kind,char),char); after=strokes(glyph(new,kind,char),char)
    assert len(after)==expected, (kind,char,'counter opened/merged',after)
    assert all(all(width==1 for width in v.values()) for v in after), (kind,char,after)
    checks.append({'kind':kind,'char':char,'before':before,'after':after,'closedCounters':expected})

report={'glyphs':len(new[0]),'asciiAndLargeFontsUnchanged':True,'noNewEmptyGlyphs':True,'onePixelCounterWalls':checks}
(out/'font-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
canvas=Image.new('RGB',(560,len(cases)*176),'white');d=ImageDraw.Draw(canvas)
label=ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',14)
for i,(kind,char,_) in enumerate(cases):
    d.text((8,i*176+4),kind+' U+'+format(ord(char),'04X')+'  before / after',font=label,fill='black')
    for col,fs in enumerate((old,new)):
        im=glyph(fs,kind,char);im=im.resize((im.width*8,im.height*8),Image.Resampling.NEAREST)
        canvas.paste('black',(24+col*264,i*176+28),im)
canvas.save(out/'glyph-gridfit-comparison.png')
print(json.dumps(report,ensure_ascii=False))
