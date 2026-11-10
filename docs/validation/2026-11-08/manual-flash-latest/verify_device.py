# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5,<4"]
# ///
"""验证已更新固件，等待首刷后恢复既有回放；凭据只读私有配置。"""
import base64
import json
import re
from pathlib import Path
import time
import urllib.request
import serial

ROOT=Path('/Users/wangxiaojie/xj-studio/projects/epaper-dashboard')
OUT=ROOT/'docs/validation/2026-11-08/manual-flash-latest'
access=json.loads((ROOT/'local/device-access.json').read_text())
url=(access.get('url') or 'http://'+access['ip']).rstrip('/')+'/api/status'
auth='Basic '+base64.b64encode((access['username']+':'+access['password']).encode()).decode()

def status():
    request=urllib.request.Request(url,headers={'Authorization':auth})
    with urllib.request.urlopen(request,timeout=5) as response:
        return json.load(response)

manifest=json.loads((OUT/'source-manifest.json').read_text())
header=(Path(manifest['frozen_source'])/'app.h').read_text()
expected=re.search(r'VERSION\s*=\s*"([^"]+)"',header).group(1)
deadline=time.monotonic()+75
last_error=''
while time.monotonic()<deadline:
    try:
        initial=status()
        if initial.get('version')==expected and initial.get('fullRefreshCount',0)>=1:
            break
    except Exception as error:
        last_error=type(error).__name__
    time.sleep(1)
else:
    raise RuntimeError('New firmware initial frame not ready: '+last_error)
assert not initial.get('displayError'),initial.get('displayError')
(OUT/'initial-status.json').write_text(json.dumps(initial,ensure_ascii=False,indent=2))
print('New firmware running; initial full refresh completed.',flush=True)

if initial.get('partialRefreshCount',0)==0 and initial.get('partialStatus')=='history_replay_ready':
    port=serial.Serial(port=None,baudrate=115200,timeout=.2)
    port.dtr=False;port.rts=False;port.port='/dev/cu.usbmodem2101'
    port.open()
    try:
        port.reset_input_buffer();port.write(b'partial_probe\n');port.flush()
        end=time.monotonic()+5;lines=[]
        while time.monotonic()<end:
            line=port.readline().decode(errors='replace').strip()
            if line.startswith(('WATCH_','STOCK_REPLAY_','DISPLAY_','CUSTOM_')):
                lines.append(line)
        (OUT/'replay-start.log').write_text('\n'.join(lines)+'\n')
    finally:
        port.close()
else:
    (OUT/'replay-start.log').write_text('回放已经运行；仅通过HTTP观察，未重新发送启动命令。\n')

first=status();time.sleep(6);second=status()
assert first['version']==second['version']==expected
assert second.get('partialRefreshCount',0)>first.get('partialRefreshCount',0)
assert not second.get('displayError'),second.get('displayError')
assert first.get('fullRefreshCount')==second.get('fullRefreshCount')==1
(OUT/'serial-closed-first-status.json').write_text(json.dumps(first,ensure_ascii=False,indent=2))
(OUT/'serial-closed-final-status.json').write_text(json.dumps(second,ensure_ascii=False,indent=2))
receipt={k:second.get(k) for k in ['version','time','fullRefreshCount','partialRefreshCount','partialStatus','displayError','lastFrameElapsedMs']}
receipt.update(serial_closed_partial_count_before=first['partialRefreshCount'],serial_closed_partial_count_after=second['partialRefreshCount'],upload_verified=True,physical_screen_confirmation='待用户确认')
(OUT/'device-validation.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2))
print(json.dumps(receipt,ensure_ascii=False,indent=2),flush=True)
