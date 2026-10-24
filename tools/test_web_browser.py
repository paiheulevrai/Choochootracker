#!/usr/bin/env python3
"""Optional browser integration check after a web build.
Needs an existing Playwright install and Chromium. Set CHROME_EXECUTABLE to
use an installed Chrome, or WEB_TEST_URL to check a deployed build in isolation.
Phone lock/unlock is simulated here; it does not establish physical iOS behavior.
"""
import functools,http.server,threading,pathlib,os,json
from playwright.sync_api import sync_playwright
root=pathlib.Path(__file__).resolve().parents[1]
(root/'.tmp').mkdir(exist_ok=True)
class Handler(http.server.SimpleHTTPRequestHandler):
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(root/'web/dist')))
threading.Thread(target=server.serve_forever,daemon=True).start()
url=os.environ.get('WEB_TEST_URL',f'http://127.0.0.1:{server.server_port}/')
with sync_playwright() as p:
 browser=p.chromium.launch(executable_path=os.environ.get('CHROME_EXECUTABLE'),headless=True)
 try:
  errors=[]
  context=browser.new_context(viewport={'width':1512,'height':900},device_scale_factor=2)
  page=context.new_page();page.on('pageerror',lambda e:errors.append(str(e)))
  def ready(page):page.wait_for_function("window.Module && typeof calledRun!=='undefined' && calledRun && !document.querySelector('#import-project').disabled && document.documentElement.style.getPropertyValue('--background')",timeout=60000)
  # Model iOS autoplay suspension explicitly: automation itself can grant
  # Chromium user activation, even before a simulated click or tap.
  startup=browser.new_context(viewport={'width':393,'height':852},device_scale_factor=3,is_mobile=True,has_touch=True)
  startup.add_init_script("""(()=>{
    const NativeAudioContext=window.AudioContext;
    window.startupUnlocked=false;
    document.addEventListener('touchend',e=>{if(e.isTrusted)startupUnlocked=true;},true);
    window.AudioContext=class extends NativeAudioContext {
      constructor(...args){super(...args);this.suspend();}
      resume(){return startupUnlocked?super.resume():new Promise(()=>{});}
    };
  })();""")
  first=startup.new_page();first.on('pageerror',lambda e:errors.append(str(e)))
  first.goto(url);ready(first)
  first.evaluate("window.initialAudio=getAudio();window.dispatchEvent(new Event('focus'));window.dispatchEvent(new Event('pageshow'));")
  first.wait_for_timeout(3000)
  assert first.locator('#audio-status').inner_text()=='',first.locator('#audio-status').inner_text()
  assert first.evaluate('!audioActivated && !audioRecovering')
  # A locked foreground/background cycle must also remain quiet.
  first.evaluate('setPageHidden(true);setPageHidden(false)')
  first.wait_for_timeout(2000)
  assert first.locator('#audio-status').inner_text()==''
  first.locator('[data-button="4"]').tap()
  first.wait_for_function("audioActivated && !audioRecovering && getAudio().state==='running'",timeout=5000)
  assert first.evaluate('getAudio()===initialAudio')
  assert first.locator('#audio-status').inner_text()==''
  startup.close()
  print('Autoplay-blocked startup/focus/wake stayed quiet; first touch activated the existing output',flush=True)
  page.goto(url);ready(page);page.locator('#canvas').click()
  def key(k):page.keyboard.press(k,delay=90);page.wait_for_timeout(300)
  key('x');key('Shift+ArrowDown')
  for _ in range(5):key('ArrowDown')
  key('x');key('x')
  page.screenshot(path=str(root/'.tmp/theme-before-edit.png'))
  before=page.evaluate("document.documentElement.style.getPropertyValue('--background')")
  key('x+ArrowRight')
  page.wait_for_timeout(500)
  after=page.evaluate("document.documentElement.style.getPropertyValue('--background')")
  assert before!=after,(before,after)
  assert page.locator('meta[name="theme-color"]').get_attribute('content')==after
  settings=page.evaluate("FS.readFile('/user/settings.txt',{encoding:'utf8'})")
  assert 'colorBackground: 0x'+after[1:] in settings
  page.reload();ready(page)
  assert page.evaluate("document.documentElement.style.getPropertyValue('--background')")==after
  print('Live edited color and reload persistence passed:',before,after,flush=True)
  # Load an actual theme using the same native settings screen.
  page.locator('#canvas').click();key('x');key('Shift+ArrowDown')
  for _ in range(5):key('ArrowDown')
  key('x');key('x')
  for _ in range(11):key('ArrowDown')
  key('ArrowRight');key('x');page.screenshot(path=str(root/'.tmp/theme-file-picker.png'))
  # File browser has parent entry, IDE folder, then alphabetically sorted files.
  for _ in range(5):key('ArrowDown')
  key('x');page.wait_for_timeout(600)
  page.screenshot(path=str(root/'.tmp/theme-loaded.png'))
  print('Selected theme settings:',page.evaluate("FS.readFile('/user/settings.txt',{encoding:'utf8'}).split('\\n').filter(x=>x.startsWith('themeName:')||x.startsWith('colorBackground:'))"),flush=True)
  assert 'themeName: NostromoAmberDa2' in page.evaluate("FS.readFile('/user/settings.txt',{encoding:'utf8'})")
  colors=page.evaluate("Object.fromEntries(['background','text','value','title','cursor','selection','warning'].map(n=>[n,getComputedStyle(document.documentElement).getPropertyValue('--'+n)]))")
  print('Active palette',colors,flush=True)
  page.reload();ready(page)
  assert page.evaluate("getComputedStyle(document.documentElement).getPropertyValue('--background')")==colors['background']
  page.locator('#canvas').click();key('x');page.wait_for_timeout(500)
  # Existing toolbar order and spacing are retained; only the two recovery
  # buttons join the existing file controls.
  assert page.locator('#file-controls button').evaluate_all("nodes=>nodes.map(n=>n.id)")==[
   'import-project','import-samples','import-folder','export-project','restart-audio','reload-app']
  assert page.evaluate("document.querySelector('#file-controls').compareDocumentPosition(document.querySelector('#canvas')) & Node.DOCUMENT_POSITION_FOLLOWING")
  assert page.locator('main').evaluate("e=>getComputedStyle(e).padding")=='16px'
  assert page.locator('#canvas').evaluate("e=>getComputedStyle(e).width")=='640px'
  assert page.locator('#import-project').evaluate("e=>getComputedStyle(e).padding")=='6.72px 11.2px'
  page.screenshot(path=str(root/'.tmp/web-theme-desktop.png'),full_page=True)
  mobile=browser.new_context(viewport={'width':393,'height':852},device_scale_factor=3,is_mobile=True,has_touch=True)
  phone=mobile.new_page();phone.goto(url);ready(phone)
  assert phone.locator('main').evaluate("e=>getComputedStyle(e).padding")=='8px'
  assert phone.locator('#canvas').evaluate("e=>getComputedStyle(e).width")=='377px'
  phone.screenshot(path=str(root/'.tmp/web-theme-phone.png'),full_page=True)
  mobile.close()
  print('Upstream toolbar order, canvas sizing, button padding and page spacing preserved',flush=True)
  # Exercise background save/pause/wake and gesture recovery without reloading.
  page.locator('#canvas').click()
  assert page.evaluate("Module.ccall('webLoadProject','number',['string'],['/projects/native-chip-audition.cct'])")==0
  page.evaluate("window.probe={callbacks:0,peak:0};const node=Module.SDL2.audio.scriptProcessorNode;const original=node.onaudioprocess;node.onaudioprocess=e=>{original(e);probe.callbacks++;for(const value of e.outputBuffer.getChannelData(0))probe.peak=Math.max(probe.peak,Math.abs(value));};void 0;")
  key('Space');page.wait_for_timeout(600);assert page.evaluate('probe.peak')>0
  for cycle in range(3):
   page.keyboard.down('ArrowRight');page.wait_for_timeout(100)
   page.evaluate("window.testHidden=true;Object.defineProperty(document,'hidden',{configurable:true,get:()=>window.testHidden});document.dispatchEvent(new Event('visibilitychange'));")
   page.wait_for_function("Module.SDL2.audioContext.state==='suspended'")
   assert page.evaluate("FS.stat('/user/autosave.cct').size")>0
   page.evaluate("testHidden=false;document.dispatchEvent(new Event('visibilitychange'));")
   page.keyboard.up('ArrowRight');page.locator('#canvas').click()
   page.wait_for_function("Module.SDL2.audioContext.state==='running'")
   page.wait_for_timeout(300);page.evaluate('probe.peak=0');key('Space');page.wait_for_timeout(600)
   assert page.evaluate('probe.peak')>0,('no playback after wake',cycle)
  page.evaluate("delete document.hidden")
  page.evaluate("Module.SDL2.audioContext.suspend()")
  page.locator('#canvas').click();page.wait_for_function("Module.SDL2.audioContext.state==='running'")
  print('Three background/save/wake cycles and gesture audio recovery passed',flush=True)
  # Preserve actual song bytes across replacing the browser audio output.
  def project_hash():
   assert page.evaluate("Module.ccall('webSaveBrowserState','number',[],[])")==0
   return page.evaluate("async()=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',FS.readFile('/user/autosave.cct')))).map(x=>x.toString(16).padStart(2,'0')).join('')")
  page.wait_for_function('!audioRecovering')
  before_restart=project_hash()
  # Inject a running clock that has frozen, as reported in WebKit's bug tracker.
  page.evaluate("window.frozenAudio=Module.SDL2.audioContext;const resume=frozenAudio.resume.bind(frozenAudio);Object.defineProperty(frozenAudio,'currentTime',{value:1.25,configurable:true});window.cycleResumes=0;frozenAudio.resume=()=>{cycleResumes++;delete frozenAudio.currentTime;return resume();};void 0;")
  try:
   page.wait_for_function("document.querySelector('#audio-status').textContent.includes('stopped responding')",timeout=7000)
  except Exception:
   print('Audio failure diagnostic',page.evaluate("({hidden:document.hidden,state:getAudio().state,time:getAudio().currentTime,observedTime,lastAudioProgress,lastAudioCallback,now:performance.now(),audioRecovering,audioGeneration,status:audioStatus.textContent})"),flush=True)
   raise
  page.locator('#canvas').click()
  page.wait_for_function("cycleResumes>0 && Module.SDL2.audioContext.state==='running'")
  page.wait_for_function("!document.querySelector('#audio-status').textContent",timeout=5000)
  assert project_hash()==before_restart
  print('Frozen-running clock recovery passed',flush=True)
  # A resume promise that never settles must not prevent an explicit restart.
  page.evaluate("window.stuckAudio=Module.SDL2.audioContext;stuckAudio.resume=()=>new Promise(()=>{});stuckAudio.suspend()")
  page.locator('#canvas').click()
  page.wait_for_function("document.querySelector('#audio-status').textContent.includes('Tap Restart audio')",timeout=5000)
  page.locator('#restart-audio').click()
  page.wait_for_function("Module.SDL2.audioContext!==stuckAudio && document.querySelector('#audio-status').textContent.includes('output restarted')",timeout=5000)
  assert project_hash()==before_restart
  print('Pending resume timeout and output restart preserved song bytes',flush=True)
  # Restart an output whose callback node has stopped, while its clock runs.
  page.evaluate("window.disconnectedAudio=Module.SDL2.audioContext;Module.SDL2.audio.scriptProcessorNode.disconnect()")
  try:
   page.wait_for_function("document.querySelector('#audio-status').textContent.includes('stopped responding')",timeout=7000)
  except Exception:
   print('Audio failure diagnostic',page.evaluate("({hidden:document.hidden,state:getAudio().state,time:getAudio().currentTime,observedTime,lastAudioProgress,lastAudioCallback,now:performance.now(),audioRecovering,audioGeneration,status:audioStatus.textContent})"),flush=True)
   raise
  page.locator('#restart-audio').click()
  page.wait_for_function("Module.SDL2.audioContext!==disconnectedAudio && document.querySelector('#audio-status').textContent.includes('output restarted')",timeout=5000)
  assert project_hash()==before_restart
  print('Missing audio callback detected and output restarted',flush=True)
  page.evaluate("window.closedAudio=Module.SDL2.audioContext;closedAudio.close()")
  page.locator('#restart-audio').click()
  page.wait_for_function("Module.SDL2.audioContext!==closedAudio && document.querySelector('#audio-status').textContent.includes('output restarted')",timeout=5000)
  assert project_hash()==before_restart
  print('Closed context restarted without losing the song',flush=True)

  # Establish stopped transport before testing a fresh Play command.
  page.evaluate('setPageHidden(true)');page.wait_for_timeout(150)
  page.evaluate('setPageHidden(false)');page.locator('#canvas').click()
  page.wait_for_function("!audioRecovering && getAudio().state==='running'")
  page.wait_for_timeout(300)
  page.evaluate("window.afterRestartPeak=0;const node=Module.SDL2.audio.scriptProcessorNode,callback=node.onaudioprocess;node.onaudioprocess=e=>{callback(e);for(const v of e.outputBuffer.getChannelData(0))afterRestartPeak=Math.max(afterRestartPeak,Math.abs(v));};void 0;")
  assert page.evaluate("Module.ccall('webLoadProject','number',['string'],['/projects/native-chip-audition.cct'])")==0
  page.locator('#canvas').click();key('Space')
  try:page.wait_for_function('afterRestartPeak>0',timeout=5000)
  except Exception:
   page.screenshot(path=str(root/'.tmp/web-playback-failure.png'),full_page=True)
   print('Playback diagnostic',page.evaluate("({state:getAudio().state,time:getAudio().currentTime,callback:lastAudioCallback,now:performance.now(),status:audioStatus.textContent})"),flush=True)
   raise
  # Reload must wait for durable storage and cancel on a storage error.
  page.evaluate("window.reloadMarker=1;window.realSyncfs=FS.syncfs;FS.syncfs=(populate,done)=>done(new Error('Injected storage failure'));void 0;")
  page.locator('#reload-app').click()
  page.wait_for_function("document.querySelector('#status').textContent.includes('Reload cancelled')")
  assert page.evaluate('reloadMarker')==1
  page.evaluate('FS.syncfs=realSyncfs;void 0;')
  page.wait_for_function('!storageSyncing')
  # A storage callback that never arrives must leave the app usable. A late
  # callback after cancellation must not unexpectedly reload it either.
  page.evaluate('FS.syncfs=(populate,done)=>{window.delayedStorageWrite=done;};void 0;')
  page.locator('#reload-app').click()
  page.wait_for_function("document.querySelector('#status').textContent.includes('Reload cancelled')",timeout=12000)
  assert page.evaluate('reloadMarker')==1
  assert page.locator('#reload-app').is_enabled()
  page.evaluate('FS.syncfs=realSyncfs;delayedStorageWrite(null);void 0;')
  page.wait_for_function('!storageSyncing')
  assert page.evaluate('reloadMarker')==1
  page.evaluate("FS.writeFile('/user/reload-check.txt','kept')")
  before_reload=project_hash()
  with page.expect_navigation(wait_until='load'):
   page.locator('#reload-app').click()
  ready(page)
  assert page.evaluate("FS.readFile('/user/reload-check.txt',{encoding:'utf8'})")=='kept'
  assert project_hash()==before_reload
  print('Reload cancels on storage failure/timeout, ignores late completion, and preserves saved song/data on success',flush=True)
  # Exercise the real Wasm gamepad polling/mapping path with a synthetic browser
  # Gamepad. This does not test USB hardware detection or Safari permissions.
  page.locator('#canvas').click();key('x')
  assert page.evaluate("Module.ccall('webLoadProject','number',['string'],['/projects/native-chip-audition.cct'])")==0
  page.evaluate("""window.pad={id:'Test standard controller',index:0,connected:true,mapping:'standard',timestamp:0,
    axes:[0,0,0,0],buttons:Array.from({length:17},()=>({pressed:false,touched:false,value:0}))};
    window.padConnected=true;window.realGetGamepads=navigator.getGamepads;
    navigator.getGamepads=()=>padConnected?[pad]:[];
    window.padPeak=0;const node=Module.SDL2.audio.scriptProcessorNode,callback=node.onaudioprocess;
    node.onaudioprocess=e=>{callback(e);for(const v of e.outputBuffer.getChannelData(0))padPeak=Math.max(padPeak,Math.abs(v));};void 0;""")
  for cycle in range(3):
   page.evaluate('setPageHidden(true)');page.wait_for_timeout(150)
   page.evaluate('setPageHidden(false)');page.locator('#canvas').click()
   page.wait_for_function("!audioRecovering && getAudio().state==='running'")
   if cycle==2:
    page.evaluate('padConnected=false');page.wait_for_timeout(150)
    page.evaluate('padConnected=true');page.wait_for_timeout(150)
   page.wait_for_timeout(500);page.evaluate('padPeak=0');page.wait_for_timeout(150)
   assert page.evaluate('padPeak')==0
   page.evaluate('pad.buttons[9]={pressed:true,touched:true,value:1}')
   page.wait_for_timeout(150)
   page.evaluate('pad.buttons[9]={pressed:false,touched:false,value:0}')
   page.wait_for_function('padPeak>0',timeout=5000)
  page.evaluate('navigator.getGamepads=realGetGamepads;void 0;')
  print('Synthetic gamepad starts playback after wake and reconnect; physical USB remains untested',flush=True)
  # Settings must be durable after Done, without relying on the periodic timer
  # or a pagehide event that a phone process termination might never deliver.
  mapping_context=browser.new_context()
  mapping_context.add_init_script("const interval=window.setInterval;window.setInterval=(fn,ms,...args)=>ms===10000?0:interval(fn,ms,...args);")
  page=mapping_context.new_page();page.on('pageerror',lambda e:errors.append(str(e)))
  page.goto(url);ready(page)
  def saved_mapping():return page.evaluate("FS.readFile('/user/settings.txt',{encoding:'utf8'}).split('\\n').filter(s=>/^key(Play|Shift)(Type)?:/.test(s))")
  assert page.evaluate("Module.ccall('webSaveBrowserState','number',[],[])")==0
  assert saved_mapping()==['keyPlay: 32,6,0','keyShift: 1073742049,4,0','keyPlayType: 2,3,0','keyShiftType: 2,3,0']
  page.locator('#canvas').click();key('x');key('Shift+ArrowDown')
  key('x') # Controls
  for _ in range(3):key('ArrowDown')
  key('x') # Key mapping
  for _ in range(6):key('ArrowDown')
  key('ArrowRight')
  page.evaluate("window.pad={id:'Test pad',index:0,connected:true,mapping:'standard',timestamp:0,axes:[0,0,0,0],buttons:Array.from({length:17},()=>({pressed:false,touched:false,value:0}))};navigator.getGamepads=()=>[pad];void 0;")
  for button in [2,3]:
   key('x')
   page.evaluate('(i)=>pad.buttons[i]={pressed:true,touched:true,value:1}',button);page.wait_for_timeout(150)
   page.evaluate('(i)=>pad.buttons[i]={pressed:false,touched:false,value:0}',button);page.wait_for_timeout(150)
   if button==2:key('ArrowDown')
  for _ in range(4):key('ArrowDown')
  key('x')
  expected=['keyPlay: 32,2,0','keyShift: 1073742049,3,0','keyPlayType: 2,3,0','keyShiftType: 2,3,0']
  assert saved_mapping()==expected
  page.wait_for_function("""expected=>new Promise((resolve,reject)=>{
    const request=indexedDB.open('/user');request.onerror=()=>reject(request.error);
    request.onsuccess=()=>{const db=request.result,read=db.transaction('FILE_DATA').objectStore('FILE_DATA').get('/user/settings.txt');
      read.onsuccess=()=>{const lines=new TextDecoder().decode(read.result.contents).split('\\n').filter(s=>/^key(Play|Shift)(Type)?:/.test(s));db.close();resolve(JSON.stringify(lines)===JSON.stringify(expected));};
      read.onerror=()=>{db.close();reject(read.error);};};
  })""",arg=expected,timeout=3000)
  page.reload();ready(page)
  assert page.evaluate("Module.ccall('webSaveBrowserState','number',[],[])")==0
  assert saved_mapping()==expected
  page.evaluate('setPageHidden(true)');page.wait_for_timeout(150);page.evaluate('setPageHidden(false)')
  assert page.evaluate("Module.ccall('webSaveBrowserState','number',[],[])")==0
  assert saved_mapping()==expected
  print('Upstream Start/Back defaults passed; custom X/Y flushed on Done and retained after reload/wake without the periodic timer',flush=True)
  mapping_context.close()
  print('JavaScript errors:',errors,flush=True);assert not errors
 finally:browser.close();server.shutdown()
