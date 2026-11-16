# /// script
# dependencies = ["Pillow>=12,<13"]
# ///
"""Convert a photo for the 400x300 black/white/red panel and upload it to the
device's image-test page.

The panel format is two 15000-byte MSB-first planes (black, then red) where a
set bit means white and a cleared bit means colored -- the same layout the
firmware frame buffer uses, so the device only memcpys it.

Usage:
  uv run tools/image_test_upload.py photo.jpg --preview build/image-test.png
  uv run tools/image_test_upload.py photo.bin --upload --device-json local/device-access.json
  uv run tools/image_test_upload.py --off --device-json local/device-access.json
"""
from pathlib import Path
import argparse,base64,http.client,json,sys,time,urllib.request
from PIL import Image

W,H=400,300
PLANE=W*H//8
TOTAL=PLANE*2
# white, black, red -- the panel's three inks
PALETTE=[(255,255,255),(0,0,0),(255,0,0)]

def convert(source:Path,fit:bool=False,rotate:int=0) -> bytes:
    img=Image.open(source).convert('RGB')
    if rotate==90:img=img.transpose(Image.ROTATE_90)
    elif rotate==180:img=img.transpose(Image.ROTATE_180)
    elif rotate==270:img=img.transpose(Image.ROTATE_270)
    target=W/H
    w,h=img.size
    if fit:
        # letterbox on white; the whole photo stays visible
        img.thumbnail((W,H),Image.Resampling.LANCZOS)
        canvas=Image.new('RGB',(W,H),(255,255,255))
        canvas.paste(img,((W-img.size[0])//2,(H-img.size[1])//2))
        img=canvas
    else:
        # crop to 4:3 around the center, then resize
        if w/h>target:
            nw=int(h*target);img=img.crop(((w-nw)//2,0,(w+nw)//2,h))
        elif w/h<target:
            nh=int(w/target);img=img.crop((0,(h-nh)//2,w,(h+nh)//2))
        img=img.resize((W,H),Image.Resampling.LANCZOS)
    pal=Image.new('P',(1,1))
    pal.putpalette([c for rgb in PALETTE for c in rgb]+[0,0,0]*(256-len(PALETTE)))
    q=img.quantize(palette=pal,dither=Image.Dither.FLOYDSTEINBERG)
    black=bytearray(b'\xff'*PLANE)
    red=bytearray(b'\xff'*PLANE)
    px=q.load()
    for y in range(H):
        for x in range(W):
            i=y*(W//8)+x//8;bit=0x80>>(x%8)
            c=px[x,y]
            if c==1:black[i]&=~bit
            elif c==2:red[i]&=~bit
    return bytes(black+red)

def preview(blob:bytes,out:Path):
    img=Image.new('RGB',(W,H))
    px=img.load()
    for y in range(H):
        for x in range(W):
            i=y*(W//8)+x//8;bit=0x80>>(x%8)
            b=blob[i]&bit;r=blob[PLANE+i]&bit
            px[x,y]=(0,0,0) if not b else ((255,0,0) if not r else (255,255,255))
    out.parent.mkdir(parents=True,exist_ok=True)
    img.save(out)
    img.resize((W*3,H*3),Image.Resampling.NEAREST).save(out.with_name(out.stem+'-3x.png'))

def fnv1a(data:bytes)->int:
    h=2166136261
    for b in data:h=((h^b)*16777619)&0xFFFFFFFF
    return h

class Device:
    def __init__(self,path:Path):
        d=json.loads(path.read_text())
        self.url=d['url'].rstrip('/')
        self.auth='Basic '+base64.b64encode(f"{d['username']}:{d['password']}".encode()).decode()
    def post(self,path:str,body:bytes,ctype:str)->dict:
        req=urllib.request.Request(self.url+path,data=body,method='POST')
        req.add_header('Authorization',self.auth);req.add_header('Content-Type',ctype)
        return json.load(urllib.request.urlopen(req,timeout=30))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',nargs='?',type=Path)
    p.add_argument('--fit',action='store_true',help='白边适配（竖图完整显示）')
    p.add_argument('--rotate',type=int,choices=[90,180,270],default=0,help='先逆时针旋转 90/180/270 度')
    p.add_argument('--preview',type=Path,help='write dithered preview PNG')
    p.add_argument('--output',type=Path,help='write raw 30000-byte blob')
    p.add_argument('--upload',action='store_true')
    p.add_argument('--off',action='store_true',help='disable the image-test page')
    p.add_argument('--device-json',type=Path,default=Path('local/device-access.json'))
    a=p.parse_args()
    if a.off:
        d=Device(a.device_json)
        print(d.post('/api/image-test',json.dumps({'enabled':False}).encode(),'application/json'))
        return
    if not a.source:p.error('需要图片路径（或使用 --off）')
    blob=a.source.read_bytes() if a.source.suffix=='.bin' else convert(a.source,a.fit,a.rotate)
    if len(blob)!=TOTAL:p.error(f'数据长度 {len(blob)} != {TOTAL}')
    if a.preview:preview(blob,a.preview);print('预览:',a.preview)
    if a.output:a.output.write_bytes(blob);print('原始数据:',a.output)
    if a.upload:
        d=Device(a.device_json)
        boundary='----epaper7f3a1b'
        body=(f'--{boundary}\r\nContent-Disposition: form-data; name="data"; filename="image.bin"\r\n'
              f'Content-Type: application/octet-stream\r\n\r\n').encode()+blob+f'\r\n--{boundary}--\r\n'.encode()
        r=None
        for attempt in range(10):
            try:
                r=d.post('/api/image-test/data',body,f'multipart/form-data; boundary={boundary}')
                break
            except (http.client.RemoteDisconnected,TimeoutError,ConnectionError) as e:
                print(f'连接中断（设备可能在全刷），第 {attempt+1} 次重试…')
                time.sleep(8)
        if r is None:sys.exit('上传重试 10 次仍失败')
        print('上传 ->',r)
        if not r.get('ok') or r.get('received')!=TOTAL:sys.exit('上传不完整')
        local=format(fnv1a(blob),'x')
        if r.get('fnv1a')!=local:sys.exit(f'哈希不一致: 设备 {r.get("fnv1a")} != 本机 {local}')
        print('设备 FNV-1a 校验一致:',local)
        print(d.post('/api/image-test',json.dumps({'enabled':True}).encode(),'application/json'))

if __name__=='__main__':main()
