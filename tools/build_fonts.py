# /// script
# dependencies = ["Pillow>=12,<13", "fonttools>=4,<5", "zopfli>=0.2,<1"]
# ///
"""Generate native Regular typography; compressed glyph records share a 32KB cache."""
from pathlib import Path
import gzip,json,math,re,zlib,hashlib
import zopfli.zlib
from PIL import Image,ImageDraw,ImageFont
from fontTools.ttLib import TTFont
root=Path(__file__).resolve().parents[1];sketch=root/'firmware/epaper_dashboard'
spec=json.loads((root/'tools/typography.json').read_text())
cp=root/spec['chinese'];lp=Path(spec['latin']);bp=Path(spec['latinBold'])
assert ImageFont.truetype(str(bp),24).getname()==('Arial','Bold')
cc=TTFont(cp).getBestCmap();lc=TTFont(lp).getBestCmap()
assert ImageFont.truetype(str(cp),20).getname()==('FZXHGB_YS','Regular')
assert ImageFont.truetype(str(lp),20).getname()==('Arial','Regular')
ui=set()
for path in sketch.iterdir():
    if path.suffix not in ('.h','.cpp','.ino') or path.name in ('font_data.h','generated_styles.h','web_page.h','web_gzip.h'):continue
    for literal in re.findall(r'"(?:\\.|[^"\\])*"',path.read_text()):
        ui.update(c for c in literal if ord(c)>127)
ui.update('北京顺义长江电力天气晴阴多云雨雪沙尘雾霾雷阵冰雹冻大中小暴特强转东南西北风晴间少雨夹雪°·…')
body_ui=set('北京顺义长江电力天气晴阴多云雨雪沙尘雾霾雷阵冰雹冻大中小暴特强转东南西北风晴间少雨夹雪扬浮轻浓干湿热冷霰龙卷飑°·…')
for name in ['page_draw.h','cards_draw.h','font_test_draw.h','screen.cpp','almanac_data.h','card_schedule.h']:
    for literal in re.findall(r'"(?:\\.|[^"\\])*"',(sketch/name).read_text()):
        body_ui.update(c for c in literal if ord(c)>127)
full=set(chr(c) for c in range(32,127))|ui
for hi in range(0xb0,0xd8):
    for lo in range(0xa1,0xff):
        try:full.add(bytes([hi,lo]).decode('gb2312'))
        except UnicodeDecodeError:pass
# Preserve all existing characters, including symbols. Explicitly alias a missing
# Japanese middle dot to the same typographic middle dot, never change families.
legacy=(root/'tools/typography_chars.txt').read_text().rstrip('\n')
full.update(c for c in legacy if 0x3400<=ord(c)<=0x9fff or ord(c)<128 or c in ui)
full.update('°·…');ui.update('°·…')
titles=set('字体与字号黄历今日总览农闰年月正二三四五六七八九十冬腊初廿一日')
ascii_chars=set(chr(c) for c in range(32,127))|set('°·…')
def array(name,values,ctype='uint8_t'):
    v=[str(x) for x in values];return [f'static const {ctype} {name}[] PROGMEM = {{']+[
        ','.join(v[i:i+40])+',' for i in range(0,len(v),40)]+['};']
def family(c):
    return 'cn' if (0x3400<=ord(c)<=0x9fff or (ord(c) in cc and (0x3000<=ord(c)<=0x303f or 0xff00<=ord(c)<=0xffef or ord(c) not in lc))) else 'en'
faces={};audit=[];missing=set();maxraw=0
lines=['#pragma once','#include <cstdint>','#include <cstddef>',
       'struct Typeface { const uint8_t* codes; const uint16_t* firstCodes; const uint16_t* codeOffsets; const uint32_t* blocks; const uint8_t* data; uint16_t count,size,blockGlyphs; };']
for role in spec['roles']:
    name=role['name'];size=role['size'];scope=role['scope'];chunk=spec['blockGlyphs']
    charset=full if scope=='full' else ascii_chars|body_ui if scope=='ui' else ascii_chars|titles if scope=='titles' else set('0123456789:- ') if scope=='clock' else set('0123456789') if scope=='digits' else set('0123456789.-+%: C°kM')
    charset=sorted(set(role.get("characters",charset)),key=ord);records=[];spans=[];weight=role.get("weight","Regular")
    role_cp=Path(role.get('chineseFont',str(cp)));role_index=role.get('chineseIndex',0)
    role_cc=cc if role_cp==cp else TTFont(role_cp,fontNumber=role_index).getBestCmap()
    role_family=ImageFont.truetype(str(role_cp),size,index=role_index).getname()
    if 'chineseFace' in role:assert role_family==tuple(role['chineseFace']),(name,role_family)
    for c in charset:
        fam=family(c);font_path=role_cp if fam=='cn' else bp if weight=='Bold' else lp
        font_index=role_index if fam=='cn' else 0;key=(fam,size,weight,str(font_path),font_index)
        if key not in faces:faces[key]=ImageFont.truetype(str(font_path),size,index=font_index)
        face=faces[key];cmap=role_cc if fam=='cn' else lc
        render='·' if c=='・' else c
        if ord(render) not in cmap:
            missing.add(c);render='?';face=faces.setdefault(('en',size,weight),ImageFont.truetype(str(bp if weight=='Bold' else lp),size))
        padded=Image.new('1',(size*3,size*3));d=ImageDraw.Draw(padded);d.fontmode='1'
        stroke=role.get('strokeWidth',0) if fam=='cn' else 0
        d.text((size,size*2),render,font=face,fill=1,anchor='ls',stroke_width=stroke,stroke_fill=1);box=padded.getbbox()
        if box:
            l,t,r,b=box;mask=padded.crop(box);w,h=mask.size;dx=l-size;dy=t-size*2;bits=mask.tobytes()
        else:w=h=1;dx=dy=0;bits=b'\0'
        advance=math.ceil(face.getlength(render));advance=max(advance,dx+w+1) if stroke else advance;payload=bytes([w,h,advance,dx&255,dy&255])+bits
        assert len(payload)<65536 and -128<=dx<=127 and -128<=dy<=127,(name,c)
        records.append(len(payload).to_bytes(2,"little")+payload);spans.append(advance)
    digit_adv=[spans[charset.index(c)] for c in '0123456789'];assert len(set(digit_adv))==1,(name,digit_adv)
    data=bytearray();offsets=[0]
    for start in range(0,len(records),chunk):
        raw=b''.join(records[start:start+chunk]);maxraw=max(maxraw,len(raw));assert len(raw)<=spec['cacheBytes']
        data.extend(zopfli.zlib.compress(raw,numiterations=spec.get("compressionIterations",8)));offsets.append(len(data))
    codebits=bytearray();first=[];codeOffsets=[]
    for start in range(0,len(charset),chunk):
        first.append(ord(charset[start]));codeOffsets.append(len(codebits));prev=ord(charset[start])
        for c in charset[start+1:start+chunk]:
            delta=ord(c)-prev;prev=ord(c)
            codebits.extend(bytes([delta]) if delta<255 else b'\xff'+delta.to_bytes(2,'little'))
    lines+=array(name+'Codes',codebits)+array(name+'First',first,'uint16_t')+array(name+'CodeOffsets',codeOffsets,'uint16_t')+array(name+'Blocks',offsets,'uint32_t')+array(name+'Data',data)
    lines.append(f'static const Typeface Font{name}={{{name}Codes,{name}First,{name}CodeOffsets,{name}Blocks,{name}Data,{len(charset)},{size},{chunk}}};')
    audit.append({'role':name,'px':size,'glyphs':len(charset),'compressedBytes':len(data),'digitAdvance':digit_adv[0],'weight':weight,'chineseStrokePx':role.get('strokeWidth',0),'chineseFamily':role_family,'chineseFontIndex':role_index})
lines+=['enum class TextRole : uint8_t {'+','.join(r['name'] for r in spec['roles'])+'};',
        'inline const Typeface& roleFont(TextRole role) { switch(role) {']
lines += [f'case TextRole::{r["name"]}:return Font{r["name"]};' for r in spec['roles']]
lines += ['}return FontBody;}','constexpr size_t FONT_CACHE_BYTES='+str(spec['cacheBytes'])+';',f'constexpr size_t FONT_MAX_BLOCK_BYTES={maxraw};', 'static_assert(FONT_MAX_BLOCK_BYTES<=FONT_CACHE_BYTES);']
(sketch/'font_data.h').write_text('\n'.join(lines)+'\n')
(sketch/'generated_styles.h').write_text('#pragma once\n// Typography metadata and glyphs are generated together from tools/typography.json.\n#include "font_data.h"\n')
page=(sketch/'web_page.h').read_text();html=re.search(r'R"PAGE\((.*)\)PAGE"',page,re.S).group(1)
(sketch/'web_gzip.h').write_text('#pragma once\n'+'\n'.join(array('WEB_GZIP',gzip.compress(html.encode(),mtime=0)))+'\n')
report={'families':{'chinese':ImageFont.truetype(str(cp),20).getname(),'latin':ImageFont.truetype(str(lp),20).getname()},'roles':audit,'maxBlockBytes':maxraw,'cacheBytes':spec['cacheBytes'],'missingGlyphs':sorted(missing),'explicitAliases':{'・':'·'},'retainedCommonHanCount':sum(0x3400<=ord(c)<=0x9fff for c in full),'nonUiLegacySymbolsOmitted':len(set(legacy)-full),'sourceSha256':{'chinese':hashlib.sha256(cp.read_bytes()).hexdigest(),'latin':hashlib.sha256(lp.read_bytes()).hexdigest(),'latinBold':hashlib.sha256(bp.read_bytes()).hexdigest()}}
(root/'build').mkdir(exist_ok=True);(root/'build/typography-font-audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
assert not missing,'Missing source glyphs must be resolved explicitly'
