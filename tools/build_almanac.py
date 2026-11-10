# /// script
# dependencies = ["lunar-python==1.4.9"]
# ///
"""Generate bounded offline lunar/folk-calendar data; no runtime service/key needed."""
from pathlib import Path
from datetime import date,timedelta
import hashlib,importlib.metadata,json,urllib.request
from lunar_python import Solar

ROOT=Path(__file__).resolve().parents[1]
start,end=date(2026,1,1),date(2029,1,1)
gan=list("甲乙丙丁戊己庚辛壬癸");zhi=list("子丑寅卯辰巳午未申酉戌亥")
cycle=[gan[i%10]+zhi[i%12] for i in range(60)]
terms=[""];codes={"":0};rows=[];checks={}
def word(s):
    if s not in codes:codes[s]=len(terms);terms.append(s)
    return codes[s]
for n in range((end-start).days):
    day=start+timedelta(days=n);l=Solar.fromYmd(day.year,day.month,day.day).getLunar()
    yi=l.getDayYi()[:2];ji=l.getDayJi()[:2]
    packed=(cycle.index(l.getYearInGanZhi())<<10)|(512 if l.getMonth()<0 else 0)|((abs(l.getMonth())-1)<<5)|(l.getDay()-1)
    rows.append([packed&255,packed>>8]+[word(s) for s in (yi+[""]*2)[:2]+(ji+[""]*2)[:2]])
    if day.isoformat() in ["2026-02-17","2026-11-08","2027-02-06","2028-02-29","2028-12-31"]:
        checks[day.isoformat()]={'month':l.getMonth(),'day':l.getDay(),'year':l.getYearInGanZhi(),'yi':yi,'ji':ji}
assert len(terms)<256
head=['#pragma once','#include <cstdint>','#ifndef PROGMEM','#define PROGMEM','#endif',
      '// Generated from lunar-python 1.4.9 (MIT), https://github.com/6tail/lunar-python',
      '// Traditional Yi/Ji are excerpts (first two entries); folk reference, not predictions.',
      'constexpr int ALMANAC_FIRST_YEAR=2026, ALMANAC_LAST_YEAR=2028;',
      'static const char* const ALMANAC_WORDS[] = {']
head += [json.dumps(t,ensure_ascii=False)+',' for t in terms]
head+=['};','static const uint8_t ALMANAC_DAYS[][6] PROGMEM = {']
head += ['{'+','.join(map(str,row))+'},' for row in rows]
head+=['};']
target=ROOT/'firmware/epaper_dashboard/almanac_data.h';target.write_text('\n'.join(head)+'\n')
out=ROOT/'assets/almanac';out.mkdir(exist_ok=True,parents=True)
meta={'library':'lunar-python','version':importlib.metadata.version('lunar-python'),'source':'https://github.com/6tail/lunar-python','range':['2026-01-01','2028-12-31'],'days':len(rows),'terms':len(terms),'bytes':len(rows)*6+sum(len(t.encode())+1 for t in terms),'yiJi':'first two source entries; traditional folk reference','checks':checks,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
(out/'manifest.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2))
license_dir=ROOT/'licenses';license_dir.mkdir(exist_ok=True)
license_path=license_dir/'lunar-python-MIT.txt'
if not license_path.exists():
    license_path.write_bytes(urllib.request.urlopen('https://raw.githubusercontent.com/6tail/lunar-python/master/LICENSE',timeout=15).read())
print(json.dumps(meta,ensure_ascii=False),flush=True)
