# /// script
# requires-python = ">=3.11"
# dependencies = ["Pillow>=12,<13"]
# ///
"""用固件现有的1位字模和记录的历史快照核对排版，不生成或刷新行情。"""
from pathlib import Path
import json
import math
import re
import zlib
from PIL import Image, ImageDraw

ROOT=Path('/Users/wangxiaojie/xj-studio/.worktrees/epaper-stock-replay/projects/epaper-dashboard')
MAIN=Path('/Users/wangxiaojie/xj-studio/projects/epaper-dashboard')
OUT=MAIN/'docs/validation/2026-11-08/stock-chart-fill'
OUT.mkdir(exist_ok=True)
SKETCH=ROOT/'firmware/epaper_dashboard'

def array(text,name):
    body=re.search(r'\b'+name+r'\[\]\s+PROGMEM\s*=\s*\{(.*?)\};',text,re.S).group(1)
    return [int(x) for x in re.findall(r'-?\d+',body)]

raw=(SKETCH/'font_data.h').read_text()
codes=array(raw,'GLYPH_CODES');offsets=array(raw,'GLYPH_BLOCKS')
compressed=bytes(array(raw,'GLYPH_ZLIB'));advances=array(raw,'ASCII_ADVANCE')
glyphs={}
for block in range(len(offsets)-1):
    expanded=zlib.decompress(compressed[offsets[block]:offsets[block+1]])
    for i in range(len(expanded)//32):
        glyphs[codes[block*1024+i]]=Image.frombytes('1',(16,16),expanded[i*32:(i+1)*32])

styles_raw=(SKETCH/'generated_styles.h').read_text()
styles={}
for name in ['Header18','Title26','Quote48']:
    bits=bytes(array(styles_raw,name+'Bits'))
    body=re.search(name+r'Glyphs\[\]\s+PROGMEM\s*=\s*\{(.*?)\};',styles_raw,re.S).group(1)
    items={}
    for row in re.findall(r'\{([^}]+)\}',body):
        code,off,w,h,advance,dx,dy=map(int,row.split(','))
        bitmap=Image.frombytes('1',(w,h),bits[off:off+((w+7)//8)*h])
        items[code]=(bitmap,advance,dx,dy)
    styles[name]=items

snapshot=json.loads((MAIN/'docs/validation/2026-11-08/stock-replay/final-status.json').read_text())
stock=snapshot['stock'];points=stock['points'];visible=len(points)
price=points[visible-1][1];previous=stock['previous'];change=price-previous;percent=100*change/previous

def draw(source):
    image=Image.new('RGB',(400,300),'white');d=ImageDraw.Draw(image)
    def width(text):return sum(advances[ord(c)-32] if 32<=ord(c)<127 else 16 for c in text)
    def text(x,y,value,right=False):
        if right:x-=width(value)
        for c in value:
            mask=glyphs.get(ord(c),glyphs[ord('?')]);image.paste('black',(x,y-16),mask)
            x+=advances[ord(c)-32] if 32<=ord(c)<127 else 16
    def styled(x,y,value,face,right=False):
        items=styles[face]
        if right:x-=sum(items[ord(c)][1] for c in value)
        for c in value:
            mask,a,dx,dy=items[ord(c)];image.paste('black',(x+dx,y+dy),mask);x+=a
    styled(20,27,'11.08  21:11','Header18')
    styled(380,27,'北京顺义 3 C','Header18',True)
    d.line((20,38,379,38),fill='black');d.rectangle((6,13,9,29),fill='#b51d22')
    styled(20,73,'长江电力','Title26');text(380,69,'历史回放',True);text(210,69,f'{visible}/{len(points)}')
    date_y=int(re.search(r'label\(380,(\d+),sourceDate',source).group(1))
    delta_y=int(re.search(r'latin\(380,\s*(\d+), delta',source).group(1))
    text(380,date_y,'2026-11-06',True)
    styled(18,120,f'{price:.2f}','Quote48')
    text(380,delta_y,f'{change:+.2f}  {percent:+.2f}%',True)
    lo=min([previous]+[p[1] for p in points]+[p[2] for p in points if p[2] is not None])
    hi=max([previous]+[p[1] for p in points]+[p[2] for p in points if p[2] is not None])
    margin=max((hi-lo)*.1,previous*.002);lo-=margin;hi+=margin
    left,right,top,bottom=map(int,re.search(r'left = (\d+), right = (\d+), top = (\d+), bottom = (\d+)',source).groups())
    def py(v):return bottom-int(math.floor((v-lo)/(hi-lo)*(bottom-top)+.5))
    for x in range(left,right+1,8):d.line((x,py(previous),min(x+3,right),py(previous)),fill='black')
    for i in range(1,visible):
        x0=left+int(points[i-1][0]*(right-left)/240);x1=left+int(points[i][0]*(right-left)/240)
        d.line((x0,py(points[i-1][1]),x1,py(points[i][1])),fill='black')
        if i%3 and points[i-1][2] is not None and points[i][2] is not None:
            d.line((x0,py(points[i-1][2]),x1,py(points[i][2])),fill='black')
    if 'label(20, 145,' in source:
        text(20,145,f'均价 {points[visible-1][2]:.2f}')
        d.line((260,137,271,137),fill='black');text(278,145,'价格')
        for x in range(326,338,4):d.line((x,137,x+1,137),fill='black')
        text(344,145,'均价')
    text(50,top+6,f'{hi:.2f}',True);text(50,bottom+4,f'{lo:.2f}',True)
    text(395,top+6,f'{(hi/previous-1)*100:.2f}%',True)
    text(395,bottom+4,f'{(lo/previous-1)*100:.2f}%',True)
    return image,dict(status_baseline=69,date_baseline=date_y,delta_baseline=delta_y,
                      date_to_delta_pixels=delta_y-date_y,summary_row='label(20, 145,' in source,chart_top=top,chart_bottom=bottom,chart_height=bottom-top)

before,old=draw((OUT/'active-screen-before.cpp').read_text())
after,new=draw((SKETCH/'screen.cpp').read_text())
before.save(OUT/'stock-before-400x300.png');after.save(OUT/'stock-after-400x300.png')
comparison=Image.new('RGB',(1600,600),'white')
comparison.paste(before.resize((800,600),Image.Resampling.NEAREST),(0,0))
comparison.paste(after.resize((800,600),Image.Resampling.NEAREST),(800,0))
comparison.save(OUT/'stock-spacing-comparison.png')
assert not old['summary_row'] and not new['summary_row']
assert old['chart_top']==160 and new['chart_top']==136
assert new['chart_height']==old['chart_height']+24
assert new['chart_bottom']==old['chart_bottom']==284
assert new['date_to_delta_pixels']==27
assert after.crop((12,130,390,150)).getbbox() is not None
(OUT/'layout-preview.json').write_text(json.dumps(dict(before=old,after=new,
    data_source='保存的2026-11-06历史快照，展示全部242点',price=price,preview_is_hardware_validation=False),ensure_ascii=False,indent=2))
print(json.dumps(dict(before=old,after=new,comparison=str(OUT/'stock-spacing-comparison.png')),ensure_ascii=False,indent=2))
