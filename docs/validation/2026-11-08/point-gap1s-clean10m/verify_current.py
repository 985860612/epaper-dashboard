# /// script
# dependencies = ["pyserial>=3.5"]
# ///
from pathlib import Path
import json,base64,urllib.request,time,serial,re
b=Path(__file__).parent;r=b.parent.parent
a=json.loads((r/'local/device-access.json').read_text())
h={'Authorization':'Basic '+base64.b64encode((a['username']+':'+a['password']).encode()).decode()}
def status():
 return json.load(urllib.request.urlopen(urllib.request.Request('http://'+a['ip']+'/api/status',headers=h),timeout=6))
expected='epaper-dashboard-0.1.9-point-gap1s-layout-clean10m'
events=[];initial=None;next_poll=0;sent=False;deadline=time.monotonic()+120
with serial.Serial('/dev/cu.usbmodem2101',115200,timeout=.1) as p,(b/'serial.log').open('w',buffering=1) as log:
 print('MONITOR_READY',flush=True)
 while time.monotonic()<deadline:
  if time.monotonic()>next_poll:
   next_poll=time.monotonic()+4
   try:d=status()
   except (OSError,TimeoutError):continue
   assert d['version']==expected,d['version']
   assert not d['displayError'],d['displayError']
   if not sent and not d['rendering'] and d['fullRefreshCount']==1 and d['partialStatus']=='history_replay_ready':
    initial=d;(b/'initial-status.json').write_text(json.dumps(d,ensure_ascii=False,indent=2));assert d['lastFrameElapsedMs']>=1000
    p.write(b'{"command":"partial_probe"}\n');p.flush();sent=True;print('REPLAY_TRIGGERED full_ms='+str(d['lastFrameElapsedMs']),flush=True)
  line=p.readline().decode(errors='replace').strip()
  if not line:continue
  log.write(line+'\n')
  if line.startswith(('PARTIAL_FAILED','PARTIAL_PROBE_REJECTED')):
   p.write(b'{"command":"partial_stop"}\n');p.flush();raise RuntimeError(line)
  m=re.match(r'STOCK_REPLAY_FRAME step=(\d+) visible=(\d+)/(\d+) header=([0-9a-f]+)',line)
  if m and int(m[1])>0:
   events.append({'step':int(m[1]),'visible':int(m[2]),'total':int(m[3]),'header':m[4]});print(line,flush=True)
  if len(events)>=6 and line.startswith('DISPLAY_DONE kind=partial'):break
assert initial and len(events)>=6,'No confirmed partial frames'
print('MONITOR_CLOSED; OBSERVING_INDEPENDENT_PROGRESS',flush=True)
samples=[]
for i in range(4):
 time.sleep(3)
 d=status();samples.append({k:d.get(k) for k in ['version','time','rendering','displayError','fullRefreshCount','partialRefreshCount','lastFrameElapsedMs','partialStatus']})
 print(json.dumps(samples[-1],ensure_ascii=False),flush=True)
(b/'closed-monitor-observation.json').write_text(json.dumps(samples,ensure_ascii=False,indent=2));(b/'frame-events.json').write_text(json.dumps(events,indent=2))
assert all(x['version']==expected and x['fullRefreshCount']==1 and not x['displayError'] for x in samples)
assert samples[-1]['partialRefreshCount']>samples[0]['partialRefreshCount']>=6
assert all(e['step']==i+1 and e['visible']==i+1 for i,e in enumerate(events))
assert len({e['header'] for e in events})==1
(b/'final-status.json').write_text(json.dumps(d,ensure_ascii=False,indent=2))
summary={'version':expected,'initialFullMs':initial['lastFrameElapsedMs'],'verifiedFirstFrames':len(events),'partialAfterMonitorClosed':samples[-1]['partialRefreshCount'],'independentProgress':True,'fullCount':d['fullRefreshCount'],'displayError':d['displayError'],'periodicFullIntervalMs':600000,'tenMinuteHardwareTriggerObserved':False,'stockReplayStillRunning':True}
(b/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2));print('PASS',json.dumps(summary,ensure_ascii=False),flush=True)
