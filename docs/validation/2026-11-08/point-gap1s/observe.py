# /// script
# dependencies = ["pyserial>=3.5"]
# ///
from pathlib import Path
import base64,json,re,serial,time,urllib.request

b=Path(__file__).parent
r=b.parent.parent
a=json.loads((r/'local/device-access.json').read_text())
headers={'Authorization':'Basic '+base64.b64encode((a['username']+':'+a['password']).encode()).decode()}
def status():
    return json.load(urllib.request.urlopen(urllib.request.Request('http://'+a['ip']+'/api/status',headers=headers),timeout=4))

events=[];samples=[];sent=False;failed=False;finished=False;initial=None
next_poll=0;deadline=time.monotonic()+700;complete_at=None
with serial.Serial('/dev/cu.usbmodem2101',115200,timeout=.05) as p,(b/'serial.log').open('w',buffering=1) as log:
    print('OBSERVER_READY',flush=True)
    while time.monotonic()<deadline:
        if (b/'stop.request').exists():
            p.write(b'{"command":"partial_stop"}\n');p.flush();failed=True;break
        now=time.monotonic()
        if now>=next_poll:
            next_poll=now+5
            try:
                d=status()
                sample={k:d.get(k) for k in ['version','time','rendering','displayError','fullRefreshCount','partialRefreshCount','lastFrameElapsedMs','partialStatus','freeHeap']}
                samples.append(sample);(b/'observation.json').write_text(json.dumps(samples,ensure_ascii=False,indent=2))
                if d['version']!='epaper-dashboard-0.1.9-point-gap1s':
                    raise RuntimeError('Unexpected firmware version')
                if d['displayError'] or (sent and d['fullRefreshCount']!=initial['fullRefreshCount']):
                    print('DEVICE_ERROR',json.dumps(sample,ensure_ascii=False),flush=True);failed=True;break
                if not sent and not d['rendering'] and d['fullRefreshCount']==1 and d['partialStatus']=='history_replay_ready':
                    assert d['lastFrameElapsedMs']>=1000
                    initial=d;(b/'initial-status.json').write_text(json.dumps(d,ensure_ascii=False,indent=2))
                    p.write(b'{"command":"partial_probe"}\n');p.flush();sent=True
                    print('REPLAY_TRIGGERED full_ms='+str(d['lastFrameElapsedMs'])+' total='+str(len(d['stock']['points'])),flush=True)
            except (OSError,TimeoutError,json.JSONDecodeError) as e:
                print('STATUS_RETRY',type(e).__name__,flush=True)
        line=p.readline().decode(errors='replace').strip()
        if line:
            log.write(line+'\n')
            m=re.match(r'STOCK_REPLAY_FRAME step=(\d+) visible=(\d+)/(\d+) header=([0-9a-f]+)',line)
            if m and int(m[1])>0:
                event={'step':int(m[1]),'visible':int(m[2]),'total':int(m[3]),'header':m[4],'hostMonotonic':time.monotonic()}
                events.append(event);(b/'frame-events.json').write_text(json.dumps(events,indent=2))
                if event['step']==1 or event['step']%10==0 or 64<=event['step']<=67 or event['step']==event['total']:
                    print(line,flush=True)
            if line.startswith(('PARTIAL_FAILED','PARTIAL_PROBE_REJECTED')):
                print(line,flush=True);failed=True;break
            if line.startswith('WATCH_START'):print(line,flush=True)
            if line.startswith('WATCH_PAUSED'):
                print(line,flush=True);finished=True;complete_at=time.monotonic()
        if complete_at and time.monotonic()-complete_at>1:break
    if failed or not finished:
        p.write(b'{"command":"partial_stop"}\n');p.flush()
print('MONITOR_CLOSED',flush=True)
d=status();(b/'final-status.json').write_text(json.dumps(d,ensure_ascii=False,indent=2))
intervals=[events[i]['hostMonotonic']-events[i-1]['hostMonotonic'] for i in range(1,len(events))]
total=len(initial['stock']['points']) if initial else 0
summary={'completed':finished and not failed,'expectedPoints':total,'observedFrames':len(events),'singlePointEachFrame':bool(events) and all(e['step']==i+1 and e['visible']==i+1 and e['total']==total for i,e in enumerate(events)),'headerUnchanged':len({e['header'] for e in events})==1,'fullRefreshCount':d['fullRefreshCount'],'partialRefreshCount':d['partialRefreshCount'],'lastFrameElapsedMs':d['lastFrameElapsedMs'],'displayError':d['displayError'],'minCompletionIntervalSeconds':min(intervals) if intervals else None,'maxCompletionIntervalSeconds':max(intervals) if intervals else None,'meanCompletionIntervalSeconds':sum(intervals)/len(intervals) if intervals else None,'visualConfirmation':'pending'}
(b/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2));print(json.dumps(summary,ensure_ascii=False),flush=True)
if not summary['completed']:raise SystemExit(2)
assert len(events)==total and summary['singlePointEachFrame'] and summary['headerUnchanged'] and d['partialRefreshCount']==total and d['fullRefreshCount']==1 and not d['displayError']
