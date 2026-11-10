"""Generate a local, self-contained design review; no device or data API writes."""
from pathlib import Path
from html import escape
import calendar
import json

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/page-themes"
OUT.mkdir(parents=True, exist_ok=True)
RED = "#b01423"

def text(x, y, value, size=16, color="#000", weight=400, anchor="start", **extra):
    attrs = " ".join(f'{k.replace("_", "-")}="{escape(str(v))}"' for k, v in extra.items())
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" font-weight="{weight}" text-anchor="{anchor}" {attrs}>{escape(str(value))}</text>'

def line(x1, y1, x2, y2, color="#000"):
    return f'<path d="M{x1} {y1}H{x2}" stroke="{color}" fill="none"/>' if y1 == y2 else f'<path d="M{x1} {y1}L{x2} {y2}" stroke="{color}" fill="none"/>'

def rect(x,y,w,h,fill="#000", **extra):
    attrs=" ".join(f'{k}="{v}"' for k,v in extra.items())
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" {attrs}/>'

def cloud(x,y,s=1):
    return f'<g transform="translate({x} {y}) scale({s})" stroke="#000" stroke-width="2" fill="#fff"><path d="M4 26A9 9 0 0 1 9 9A13 13 0 0 1 33 9A9 9 0 0 1 35 27H8Z"/></g>'

def header(title, detail="2026.11.08"):
    return rect(18,18,4,19,RED)+text(32,34,title,18,weight=600)+text(382,33,detail,14,anchor="end")+line(18,47,382,47)

pages=[]
def page(key,name,description,body,needs):
    svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="400" height="300" viewBox="0 0 400 300" role="img" aria-label="{escape(name)}"><rect width="400" height="300" fill="#fff"/><g font-family="Arial, PingFang SC, Microsoft YaHei, sans-serif">{body}</g></svg>'
    (OUT/f'{key}.svg').write_text(svg)
    pages.append(dict(key=key,name=name,description=description,svg=svg,needs=needs))

body=rect(18,18,4,19,RED)+text(32,34,"2026 · 十一月",18,weight=600)+text(382,36,"21:45",26,weight=600,anchor="end")+line(18,47,382,47)+text(382,74,"星期日",16,anchor="end")
body+=text(200,214,"08",164,RED,700,"middle",letter_spacing="-8")
body+=line(18,239,382,239)+cloud(19,255,.8)+text(61,277,"北京顺义  8°C",19)
body+=text(380,275,"-2° / 8°",17,anchor="end")
page("01-big-calendar","大字日历","远距离看日期，红色大字成为页面中心。",body,"日期、星期、时间、单城天气")

body=header("2026年11月","星期日")+text(66,155,"08",76,RED,700,"middle")+text(66,184,"星期日",16,anchor="middle")+text(66,219,"21:45",24,weight=600,anchor="middle")+line(126,65,126,231)
weeks=calendar.Calendar(firstweekday=6).monthdayscalendar(2026,11)
for i,day in enumerate("日一二三四五六"):
    body+=text(151+i*34,83,day,14,RED if i in [0,6] else "#000",anchor="middle")
body+=line(137,91,381,91)
for row,week in enumerate(weeks):
    for col,n in enumerate(week):
        if not n:continue
        x,y=151+col*34,115+row*25
        if n==8:body+=rect(x-13,y-18,26,25,RED,rx=2)
        body+=text(x,y,str(n),16,"#fff" if n==8 else RED if col in [0,6] else "#000",600 if n==8 else 400,"middle")
body+=line(18,244,382,244)+cloud(18,258,.7)+text(56,278,"北京顺义  8°C  阴",18)+text(382,278,"-2° / 8°",16,anchor="end")
page("02-month-weather","月历天气","整月安排与今天并列，适合常驻桌面。",body,"日期、星期、时间、月历、单城天气")

# Lunar date for 2026-11-08 was computed with Intl Chinese calendar in Asia/Shanghai.
body=header("黄历","2026.11.08")+text(18,140,"08",90,RED,700)
body+=text(152,98,"农历九月三十",25,weight=600)+text(152,133,"丙午年",18)+text(382,133,"星期日",18,anchor="end")+line(18,160,382,160)
body+=rect(18,178,31,31,RED)+text(33.5,201,"宜",21,"#fff",600,"middle")+text(68,201,"祭祀  祈福  出行",20)
body+=rect(18,222,31,31)+text(33.5,245,"忌",21,"#fff",600,"middle")+text(68,245,"嫁娶  动土  安葬",20)
body+=line(18,267,382,267)+text(18,288,"宜忌仅为版式示例",13,RED)+text(382,288,"21:45",18,anchor="end")
page("03-almanac","黄历","农历与传统黄历分区，宜忌一眼可辨。",body,"农历按Asia/Shanghai的Intl Chinese日历换算；宜忌仅为排版示例，未接入当天黄历数据。")

body=header("2026.11.08","星期日")+text(200,179,"21:45",102,weight=600,anchor="middle",letter_spacing="-5")
body+=line(18,209,382,209)+cloud(18,237,1.1)+text(79,268,"8°",46,weight=600)+text(382,243,"阴",19,anchor="end")+text(382,272,"北京顺义  -2° / 8°",16,anchor="end")
page("04-minimal-clock","极简时钟","时间占主视觉，天气集中在底部。",body,"北京时间、星期、单城天气；可沿用现有数据")

body=header("今日总览","2026年11月")+text(19,141,"08",89,RED,700)+line(142,66,142,153)+text(382,111,"21:45",48,weight=600,anchor="end")+text(382,143,"星期日",18,anchor="end")
body+=line(18,165,382,165)+cloud(18,180,.8)+text(64,201,"北京顺义  阴",18)+text(382,202,"8°C",29,weight=600,anchor="end")+line(18,215,382,215)
body+=text(18,239,"长江电力",17)+text(382,238,"11.06 收盘",13,anchor="end")+text(18,281,"29.91",37,weight=600)+text(382,279,"+2.15%",24,RED,600,"end")
body+='<path d="M163 281L169 270L175 273L181 263L187 269L193 258L199 261L205 256L211 259L217 249L223 253L229 247L235 252L241 245L247 248L253 243L259 246L265 241" fill="none" stroke="#000"/>'
page("05-daily-overview","今日总览","日期、时间、天气与一只股票集中展示。",body,"价格示例沿用11月6日历史数值，曲线为版式示意；未接入新页面")

cards=[]
for i,p in enumerate(pages):
    cards.append(f'''<article class="card" id="{p['key']}"><div class="cardtop"><span class="num">0{i+1}</span><h2>{p['name']}</h2><button class="pick" data-index="{i}" type="button" aria-pressed="false">选中</button></div><button class="screen" data-open="{i}" type="button" aria-label="放大{p['name']}">{p['svg']}</button><p>{p['description']}</p><details><summary>数据与接入范围</summary>{p['needs']}</details><div class="exports"><button data-svg="{i}">下载 SVG</button><button data-png="{i}">下载 400×300 PNG</button></div></article>''')

html='''<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>墨水屏 · 五种主题卡片</title><style>
*{box-sizing:border-box}body{margin:0;color:#151515;background:#f4f3ef;font:15px -apple-system,BlinkMacSystemFont,"PingFang SC",sans-serif}main{max-width:1110px;margin:0 auto;padding:42px 24px 50px}.kicker{font-size:12px;letter-spacing:.14em;color:#a01423}h1{font-size:32px;line-height:1.3;margin:10px 0 14px;font-weight:650}header p{max-width:780px;line-height:1.8;color:#555;margin:0 0 8px}.bar{margin:23px 0 25px;padding:12px 0;border-top:1px solid #c9c6bf;border-bottom:1px solid #c9c6bf;display:flex;justify-content:space-between;gap:16px;flex-wrap:wrap}.bar strong{font-weight:550}.grid{display:grid;grid-template-columns:1fr 1fr;gap:22px}.card{background:white;border:1px solid #d0ccc6;padding:19px;scroll-margin-top:20px}.card.selected{outline:2px solid #b01423;outline-offset:-2px}.cardtop{display:flex;align-items:center;gap:12px;margin-bottom:17px}.num{color:#a01423;font-size:13px;font-variant-numeric:tabular-nums}h2{font-size:18px;margin:0;font-weight:600}.pick{margin-left:auto}.screen{display:block;border:1px solid #555;background:white;padding:0;width:100%;aspect-ratio:4/3;cursor:zoom-in}.screen svg{display:block;width:100%;height:100%}.card p{font-size:14px;line-height:1.65;margin:15px 0 10px;color:#444}button{font:inherit;background:white;border:1px solid #bcb8b1;padding:6px 11px;cursor:pointer;color:inherit}button:hover{border-color:#a01423}button:focus-visible,summary:focus-visible{outline:2px solid #b01423;outline-offset:3px}.pick[aria-pressed=true]{background:#b01423;color:white;border-color:#b01423}.exports{display:flex;gap:9px;margin-top:13px}.exports button{font-size:12px}details{font-size:12px;color:#666;line-height:1.7}summary{cursor:pointer}footer{margin-top:30px;color:#666;font-size:13px;line-height:1.8}dialog{border:1px solid #888;max-width:min(92vw,860px);padding:20px;background:#fff}dialog::backdrop{background:#0009}dialog .dialogbar{display:flex;justify-content:space-between;gap:25px;align-items:center;margin-bottom:14px}dialog .large svg{display:block;width:100%;height:auto}dialog .large{width:min(800px,82vw)}@media(max-width:720px){main{padding:25px 15px}.grid{grid-template-columns:1fr}h1{font-size:27px}.card{padding:14px}.bar{font-size:13px}}@media print{body{background:white}main{padding:0}.grid{gap:12px}.card{break-inside:avoid}.pick,.exports,dialog{display:none}details{display:none}h1{font-size:23px}}</style></head><body><main><header><div class="kicker">E-PAPER / THEME STUDIES</div><h1>五种主题，一块屏幕。</h1><p>400 × 300 横屏，黑、白、红。参考大字日历和月历天气面板，制作五种不同的信息布局。点卡片可放大，选中后告诉我编号即可。</p><p>以下均为版式预览与示例数据，尚未接入设备；实际字形以固件字模为准。黄历中的宜忌仅为版式示例，尚未接入真实黄历数据。</p></header><div class="bar"><span>01 大字日历 · 02 月历天气 · 03 黄历</span><strong id="choice" role="status" aria-live="polite">尚未选择</strong></div><section class="grid" aria-label="主题卡片">'''+''.join(cards)+'''</section><footer>04 极简时钟 · 05 今日总览<br>选择仅用于本地预览，不会连接设备、修改配置或烧录。SVG 和 PNG 可下载作进一步排版比较。</footer></main><dialog id="zoom"><div class="dialogbar"><strong id="zoomtitle"></strong><button id="close" type="button">关闭</button></div><div class="large"></div></dialog><script>
const pages=__PAGES__;let selected=-1;
function choose(i){selected=i;document.querySelectorAll('.card').forEach((c,j)=>{c.classList.toggle('selected',i===j);const b=c.querySelector('.pick');b.setAttribute('aria-pressed',String(i===j));b.textContent=i===j?'已选中':'选中'});document.querySelector('#choice').textContent='已选 '+String(i+1).padStart(2,'0')+' · '+pages[i].name;try{localStorage.setItem('epaper-theme-review-v2',String(i))}catch(e){}}
function download(blob,name){const a=document.createElement('a'),u=URL.createObjectURL(blob);a.href=u;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(u),1000)}
document.querySelectorAll('[data-index]').forEach(b=>b.addEventListener('click',()=>choose(+b.dataset.index)));
document.querySelectorAll('[data-open]').forEach(b=>b.addEventListener('click',()=>{const p=pages[+b.dataset.open];document.querySelector('#zoomtitle').textContent=p.name;document.querySelector('.large').innerHTML=p.svg;document.querySelector('#zoom').showModal()}));
document.querySelector('#close').addEventListener('click',()=>document.querySelector('#zoom').close());
document.querySelectorAll('[data-svg]').forEach(b=>b.addEventListener('click',()=>{const p=pages[+b.dataset.svg];download(new Blob([p.svg],{type:'image/svg+xml;charset=utf-8'}),p.key+'.svg')}));
document.querySelectorAll('[data-png]').forEach(b=>b.addEventListener('click',()=>{const p=pages[+b.dataset.png],u=URL.createObjectURL(new Blob([p.svg],{type:'image/svg+xml;charset=utf-8'})),img=new Image();img.onload=()=>{const c=document.createElement('canvas');c.width=400;c.height=300;c.getContext('2d').drawImage(img,0,0);c.toBlob(blob=>{download(blob,p.key+'.png');URL.revokeObjectURL(u)},'image/png')};img.src=u}));
try{const s=localStorage.getItem('epaper-theme-review-v2');if(s!==null && +s>=0 && +s<pages.length)choose(+s)}catch(e){}
</script></body></html>'''
html=html.replace('__PAGES__',json.dumps(pages,ensure_ascii=False).replace('</','<\\/'))
(OUT/'index.html').write_text(html)
(OUT/'themes.json').write_text(json.dumps([{k:v for k,v in p.items() if k!='svg'} for p in pages],ensure_ascii=False,indent=2))
print(OUT/'index.html')
