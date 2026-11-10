# /// script
# dependencies = ["pyserial>=3.5"]
# ///
from pathlib import Path
import base64,json,serial,time,urllib.request
b=Path(__file__).parent;r=b.parent.parent
a=json.loads((r/'local/device-access.json').read_text())
base=(a.get('url') or 'http://'+a['ip']).rstrip('/')
headers={'Authorization':'Basic '+base64.b64encode((a['username']+':'+a['password']).encode()).decode()}
def request(path,data=None):
 h=dict(headers)
 if data is not None:h['Content-Type']='application/json'
 req=urllib.request.Request(base+path,headers=h,data=None if data is None else json.dumps(data).encode())
 return json.load(urllib.request.urlopen(req,timeout=8))
keys=['view','refreshMode','partialEnabled','refreshSeconds','rotationSeconds','city','location','stocks','calendarYear','apiKeyConfigured','wifiPasswordConfigured']
p=serial.Serial('/dev/cu.usbmodem2101',115200,timeout=.1)
log=(b/'serial.log').open('w',buffering=1)
configured=False;closed_at=None;deadline=time.monotonic()+240;next_status=0;samples=[];cfg_before=None
try:
 while time.monotonic()<deadline:
  if p.is_open:
   line=p.readline().decode(errors='replace').strip()
   if line:
    log.write(line+'\n')
    if line.startswith(('START','DISPLAY_DONE','WEATHER_FETCH','NTP_')):print(line,flush=True)
  else:time.sleep(.1)
  if time.monotonic()<next_status:continue
  next_status=time.monotonic()+5
  try:d=request('/api/status')
  except (OSError,TimeoutError):continue
  if d.get('version')!='epaper-dashboard-0.2.0':continue
  samples.append(d);(b/'observation.json').write_text(json.dumps(samples,ensure_ascii=False,indent=2))
  if d['displayError']:raise RuntimeError('Display failed: '+d['displayError'])
  if not configured and not d['rendering'] and d['fullRefreshCount']>=1:
   cfg=request('/api/config');cfg_before={k:cfg.get(k) for k in keys};(b/'stored-config-before.json').write_text(json.dumps(cfg_before,ensure_ascii=False,indent=2))
   change={k:v for k,v in {'view':'auto','refreshMode':'bw_partial','partialEnabled':True}.items() if cfg.get(k)!=v}
   if change:
    request('/api/config',change);print('RESTORED_AUTO_SETTINGS',json.dumps(change),flush=True)
   configured=True;continue
  if configured and not closed_at and not d['rendering'] and d.get('configuredView')=='auto' and d.get('displayedView')=='clock' and d['fullRefreshCount']>=1:
   p.close();closed_at=time.monotonic();(b/'serial-closed-initial-status.json').write_text(json.dumps(d,ensure_ascii=False,indent=2));print('SERIAL_CLOSED; OBSERVING_REAL_SCHEDULER',flush=True)
  if closed_at:
   print(json.dumps({k:d.get(k) for k in ['time','displayedAt','displayedView','loopCount','screenCheckCount','weatherFetchCount','partialRefreshCount','weatherError','displayError']},ensure_ascii=False),flush=True)
   if time.monotonic()-closed_at>=75:break
finally:
 if p.is_open:p.close()
 log.close()
d=request('/api/status');cfg=request('/api/config');after={k:cfg.get(k) for k in keys}
(b/'final-status.json').write_text(json.dumps(d,ensure_ascii=False,indent=2));(b/'stored-config-after.json').write_text(json.dumps(after,ensure_ascii=False,indent=2))
first=json.loads((b/'serial-closed-initial-status.json').read_text())
assert d['version']=='epaper-dashboard-0.2.0' and d['configuredView']=='auto' and d['displayedView']=='clock' and d['phase']=='closed'
assert not d['displayError'] and not d['weatherError'] and d['weather']['valid'] and d['lastNtpSync']
assert d['loopCount']>first['loopCount'] and d['screenCheckCount']>=first['screenCheckCount']+2
assert d['partialRefreshCount']>first['partialRefreshCount'] and d['displayedAt']!=first['displayedAt']
for key in ['city','location','stocks','calendarYear','apiKeyConfigured','wifiPasswordConfigured','refreshSeconds','rotationSeconds']:assert after[key]==cfg_before[key],key
summary={'version':d['version'],'configuredView':cfg['view'],'displayedView':d['displayedView'],'weekdayExpected':'Sunday clock','weatherValid':d['weather']['valid'],'weatherError':d['weatherError'],'displayError':d['displayError'],'ntpIntervalSeconds':d['ntpIntervalSeconds'],'loopProgress':[first['loopCount'],d['loopCount']],'screenChecks':[first['screenCheckCount'],d['screenCheckCount']],'partialProgress':[first['partialRefreshCount'],d['partialRefreshCount']],'displayedTime':[first['displayedAt'],d['displayedAt']],'serialClosedSeconds':time.monotonic()-closed_at,'settingsPreserved':True,'liveTradingVerified':False,'physicalScreenConfirmation':'pending'}
(b/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2));print('PASS',json.dumps(summary,ensure_ascii=False),flush=True)
