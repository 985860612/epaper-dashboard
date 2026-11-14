# /// script
# dependencies = ["Pillow>=12,<13"]
# ///
"""Compile and exercise the real page/typography headers, then export fixture previews."""
from pathlib import Path
import argparse,json,platform,shutil,subprocess
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=root/'build/typography-check');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    compiler=shutil.which('clang++') or shutil.which('g++')
    if not compiler:raise RuntimeError('A C++17 host compiler and zlib development headers are required')
    cmd=[compiler,'-std=c++17','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function']
    if platform.system()=='Darwin':
        sdk=Path(subprocess.check_output(['xcrun','--show-sdk-path'],text=True).strip())/'usr/include/c++/v1'
        if sdk.exists():cmd+=['-isystem',str(sdk)]
    cmd+=['-I'+str(root/'tests/host'),str(root/'tests/test_typography.cpp'),'-lz','-o',str(out/'test-typography')]
    subprocess.run(cmd,check=True);result=subprocess.run([str(out/'test-typography'),str(out)],capture_output=True,text=True)
    (out/'tests.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
    names=['大字日历','月历天气','黄历','极简时钟','今日总览','股票分时','字体与字号','温湿度曲线']
    board=Image.new('RGB',(860,1448),'white');d=ImageDraw.Draw(board);d.fontmode='1'
    chinese=ImageFont.truetype(str(root/'local/fonts/FangZhengJingXiHei-1.ttf'),20)
    d.text((20,6),'实际页面代码与字模 · 示例数据 · 非设备实拍',font=chinese,fill='black')
    for i,name in enumerate(names):
        image=Image.open(out/f'page-{i}.ppm');image.save(out/f'page-{i}.png');image.resize((1200,900),Image.Resampling.NEAREST).save(out/f'page-{i}-3x.png')
    for n,i in enumerate([7,3,5,0,1,2,4]):
        x=20+n%2*420;y=48+n//2*348;d.text((x,y),names[i],font=chinese,fill='black');board.paste(Image.open(out/f'page-{i}.png'),(x,y+28));d.rectangle((x-1,y+27,x+400,y+328),outline='black')
    board.save(out/'all-pages.png');board.resize((1720,2896),Image.Resampling.NEAREST).save(out/'all-pages-2x.png')
    shutil.copy2(out/'page-6.png',out/'font-test-400x300.png');shutil.copy2(out/'page-6-3x.png',out/'font-test-1200x900.png')
    report={'pageSource':'firmware/epaper_dashboard/page_draw.h and cards_draw.h','exampleData':True,'normalPages':7,'fontProofPages':1,'allGlyphsDecoded':True,'glyphBoundsAndTextOverlap':'passed','timeWeatherPartialWindows':'passed','monthFixtures':'all 36 months 2026–2028; first and last days','inputFixtures':'long cities, long names, extreme numbers, missing data, UTF-8 fitting','family':'FangZhengJingXiHei + Arial Regular; StockName PingFang SC Regular, Price Arial Bold','waveformChanges':False}
    (out/'layout-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
