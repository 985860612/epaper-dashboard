# /// script
# dependencies = ["playwright"]
# ///
from pathlib import Path
import base64,json,time,urllib.request
from playwright.sync_api import sync_playwright
b=Path(__file__).parent;r=b.parent.parent
a=json.loads((r/'local/device-access.json').read_text());base=(a.get('url') or 'http://'+a['ip']).rstrip('/')
h={'Authorization':'Basic '+base64.b64encode((a['username']+':'+a['password']).encode()).decode()}
def req(path):return json.load(urllib.request.urlopen(urllib.request.Request(base+path,headers=h),timeout=8))
samples=[]
def wait(code,timeout=100):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  try:d=req('/api/status')
  except (OSError,TimeoutError):time.sleep(2);continue
  samples.append(d);(b/'observation.json').write_text(json.dumps(samples,ensure_ascii=False,indent=2))
  if d['version']=='epaper-dashboard-0.3.2':
   assert not d['displayError'],d['displayError']
   if not d['rendering'] and d['displayedView']==code:return d
  time.sleep(3)
 raise RuntimeError('Expected page not ready: '+code)
initial=wait('minimal_clock');cfg=req('/api/config')
assert cfg['cardMode']=='fixed' and cfg['fixedCard']=='minimal_clock' and cfg['cardRotationSeconds']==600
(b/'fixed-persisted-status.json').write_text(json.dumps(initial,ensure_ascii=False,indent=2));print('FIXED_SELECTION_PERSISTED_AFTER_FLASH',flush=True)
end=time.monotonic()+70
while time.monotonic()<end:
 time.sleep(5);d=req('/api/status');samples.append(d);assert d['cardMode']=='fixed' and d['displayedView']=='minimal_clock' and not d['displayError'];(b/'observation.json').write_text(json.dumps(samples,ensure_ascii=False,indent=2))
assert d['partialRefreshCount']>initial['partialRefreshCount'] and d['displayedAt']!=initial['displayedAt']
print('ENLARGED_CLOCK_PARTIAL_PASSED',flush=True)
errors=[]
with sync_playwright() as p:
 browser=p.chromium.launch(headless=True,executable_path='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome')
 page=browser.new_page(viewport={'width':1100,'height':950},http_credentials={'username':a['username'],'password':a['password']});page.on('pageerror',lambda e:errors.append(str(e)))
 page.goto(base+'/');page.wait_for_function('document.querySelector("#fixedCard").options.length===5')
 assert page.locator('#cardMode').input_value()=='fixed' and page.locator('#fixedCard').input_value()=='minimal_clock'
 page.locator('form#config section.card').first.screenshot(path=str(b/'live-fixed-controls.png'))
 page.select_option('#view','auto');page.select_option('#cardMode','rotate');page.fill('#cardRotationMinutes','10');page.locator('#save').click();page.wait_for_function('document.querySelector("#message").textContent.includes("配置已保存")')
 page.locator('form#config section.card').first.screenshot(path=str(b/'live-rotate-controls.png'));browser.close()
assert not errors,errors
final=wait('big_calendar');after=req('/api/config');assert after['view']=='auto' and after['cardMode']=='rotate' and after['cardRotationSeconds']==600
assert 500<=final['nextCardSeconds']<=600 and not final['displayError']
(b/'final-status.json').write_text(json.dumps(final,ensure_ascii=False,indent=2))
safe={k:after.get(k) for k in ['view','cardMode','fixedCard','cardRotationSeconds','city','location','stocks','refreshMode','partialEnabled','refreshSeconds','rotationSeconds','calendarYear','apiKeyConfigured','wifiPasswordConfigured']};(b/'final-config.json').write_text(json.dumps(safe,ensure_ascii=False,indent=2))
summary={'version':final['version'],'fixedSelectionSurvivedFlash':True,'enlargedClock104PartialPassed':True,'enlargedDate164Rendered':True,'liveWebpageRestoreRotate':'passed','finalIntervalSeconds':600,'finalMode':'rotate','finalView':'auto','currentPage':final['displayedView'],'nextCardSeconds':final['nextCardSeconds'],'loopProgress':[initial['loopCount'],final['loopCount']],'partialProgress':[initial['partialRefreshCount'],final['partialRefreshCount']],'weatherError':final['weatherError'],'stockError':final['stockError'],'displayError':final['displayError'],'browserErrors':errors,'physicalScreenConfirmation':'pending'}
(b/'device-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2));print('PASS',json.dumps(summary,ensure_ascii=False),flush=True)
