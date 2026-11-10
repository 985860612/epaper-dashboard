# /// script
# dependencies = ["pyserial>=3.5"]
# ///
from pathlib import Path
import base64,json,time,urllib.request,urllib.error,serial
b=Path(__file__).parent;r=b.parent.parent
a=json.loads((r/'local/device-access.json').read_text());base=(a.get('url') or 'http://'+a['ip']).rstrip('/')
h={'Authorization':'Basic '+base64.b64encode((a['username']+':'+a['password']).encode()).decode()}
observations=[]
def req(path,data=None):
 headers=dict(h)
 if data is not None:headers['Content-Type']='application/json'
 q=urllib.request.Request(base+path,headers=headers,data=None if data is None else json.dumps(data).encode())
 return json.load(urllib.request.urlopen(q,timeout=8))
def status():
 d=req('/api/status');short={k:d.get(k) for k in ['version','time','cardMode','cardCode','displayedView','rendering','nextCardSeconds','fullRefreshCount','partialRefreshCount','displayError','weatherError','stockError','loopCount','screenCheckCount']};observations.append(short);(b/'observation.json').write_text(json.dumps(observations,ensure_ascii=False,indent=2));return d
def wait_view(code,minimum_full=1,timeout=100):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  try:d=status()
  except (OSError,TimeoutError):time.sleep(2);continue
  if d.get('version')!='epaper-dashboard-0.3.0':time.sleep(2);continue
  if d['displayError']:raise RuntimeError(d['displayError'])
  if not d['rendering'] and d['displayedView']==code and d['fullRefreshCount']>=minimum_full:
   print('VIEW_READY',code,d['time'],'full',d['fullRefreshCount'],'partial',d['partialRefreshCount'],flush=True);return d
  time.sleep(3)
 raise RuntimeError('View did not become ready: '+code)

with serial.Serial('/dev/cu.usbmodem2101',115200,timeout=.2) as p,(b/'startup-serial.log').open('w') as log:
 end=time.monotonic()+3
 while time.monotonic()<end:
  line=p.readline().decode(errors='replace').strip()
  if line:log.write(line+'\n')
print('SERIAL_CLOSED',flush=True)
first=wait_view('big_calendar');(b/'initial-status.json').write_text(json.dumps(first,ensure_ascii=False,indent=2))
cfg=req('/api/config');safe=['view','cardMode','fixedCard','cardRotationSeconds','city','location','stocks','refreshMode','partialEnabled','refreshSeconds','rotationSeconds','calendarYear','apiKeyConfigured','wifiPasswordConfigured']
before={k:cfg.get(k) for k in safe};(b/'config-before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2));assert cfg['cardMode']=='rotate' and cfg['cardRotationSeconds']==600 and cfg['view']=='auto'
for invalid in [{'fixedCard':'unknown'},{'cardRotationSeconds':0},{'cardMode':'bad'}]:
 try:req('/api/config',invalid);raise AssertionError('Invalid config accepted')
 except urllib.error.HTTPError as e:assert e.code==400
views=[]
for code in ['month_weather','almanac','minimal_clock','overview','big_calendar']:
 target=status()['fullRefreshCount']+1
 while True:
  try:req('/api/card/next',{});break
  except urllib.error.HTTPError as e:
   assert e.code==409;time.sleep(2)
 d=wait_view(code,target);(b/(code+'-status.json')).write_text(json.dumps(d,ensure_ascii=False,indent=2));views.append(code)
req('/api/config',{'cardMode':'fixed','fixedCard':'almanac','cardRotationSeconds':60})
fixed_start=wait_view('almanac')
try:req('/api/card/next',{});raise AssertionError('Fixed mode advanced')
except urllib.error.HTTPError as e:assert e.code==409
print('FIXED_HOLD_BEGIN',flush=True)
end=time.monotonic()+68
while time.monotonic()<end:
 time.sleep(5);d=status();assert d['cardCode']=='almanac' and d['cardMode']=='fixed';assert not d['displayError']
(b/'fixed-after-interval.json').write_text(json.dumps(d,ensure_ascii=False,indent=2));print('FIXED_HOLD_PASSED',flush=True)
req('/api/config',{'cardMode':'rotate','cardRotationSeconds':60,'view':'auto'})
rotating_start=wait_view('big_calendar');print('AUTOMATIC_ROTATION_WAIT',flush=True)
rotated=wait_view('month_weather',rotating_start['fullRefreshCount']+1,timeout=110)
(b/'automatic-rotation-status.json').write_text(json.dumps(rotated,ensure_ascii=False,indent=2))
req('/api/config',{'cardMode':'rotate','cardRotationSeconds':600,'view':'auto'})
final=wait_view('month_weather');after=req('/api/config');(b/'config-after.json').write_text(json.dumps({k:after.get(k) for k in safe},ensure_ascii=False,indent=2))
for key in ['city','location','stocks','refreshSeconds','rotationSeconds','calendarYear','apiKeyConfigured','wifiPasswordConfigured','refreshMode','partialEnabled']:assert after[key]==before[key],key
assert after['cardRotationSeconds']==600 and after['cardMode']=='rotate' and after['view']=='auto'
assert final['loopCount']>first['loopCount'] and not final['displayError']
summary={'version':final['version'],'allFiveViewsPassed':views,'fixedModeHeldBeyondConfiguredInterval':True,'fixedNextRejected':True,'automaticRotationAt60SecondsObserved':True,'finalDefaultSeconds':600,'finalMode':'rotate','tradingPriority':'preserved, native boundary tests; no live trading at night','settingsPreserved':True,'serialMonitorClosedThroughoutRuntimeTest':True,'physicalScreenConfirmation':'pending','fullRefreshCount':final['fullRefreshCount'],'partialRefreshCount':final['partialRefreshCount'],'weatherError':final['weatherError'],'stockError':final['stockError']}
(b/'device-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2));print('PASS',json.dumps(summary,ensure_ascii=False),flush=True)
