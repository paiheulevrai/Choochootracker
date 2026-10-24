#!/usr/bin/env python3
"""Headless USER ZIP import, preservation, SoundFont audio and reload regression."""
import functools
import http.server
import json
import os
from pathlib import Path
import threading
import zipfile
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parents[1]
work = root / '.tmp/web-soundbanks'
work.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(work / 'user.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.write(root / 'tracker/tests/fixtures/soundbanks/tone.sf2',
                  'instruments/USER/soundfont/tone.sf2')
    archive.writestr('instruments/USER/sid/keep.txt', 'must not overwrite')
    archive.writestr('instruments/USER/dx7/check.txt', 'imported')
    archive.writestr('documents/readme.txt', 'not a preset')
with zipfile.ZipFile(work / 'unsafe.zip', 'w') as archive:
    archive.writestr('../escaped.txt', 'must not escape')
source = (root / 'tracker/packaging/common/projects/gm-midi-demo.cct').read_text()
a, b = source.index('### Instrument 0'), source.index('### Instrument 1')
instrument = source[a:b].split('- Chip data:')[0].replace('- Type: 16', '- Type: 28')
instrument += '- Chip data:\n- Sound bank: tone.sf2\n- Sound preset: 0:0\n\n'
source = source[:a] + instrument + source[b:]

class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

server = http.server.ThreadingHTTPServer(('127.0.0.1', 0),
    functools.partial(Handler, directory=os.environ.get('WEB_TEST_DIRECTORY', str(root / 'web/dist'))))
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=os.environ.get('CHROME_EXECUTABLE'), headless=True)
        try:
            page = browser.new_page()
            errors = []
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.goto(os.environ.get('WEB_TEST_URL', f'http://127.0.0.1:{server.server_port}/'))
            def ready():
                page.wait_for_function("window.Module && typeof calledRun !== 'undefined' && calledRun && !document.querySelector('#import-project').disabled", timeout=90000)
            ready()
            page.locator('#canvas').click()
            page.keyboard.press('x')
            page.evaluate("FS.mkdirTree('/user/instruments/USER/sid');FS.writeFile('/user/instruments/USER/sid/keep.txt','existing')")
            page.locator('#user-zip-input').set_input_files(str(work / 'user.zip'))
            page.wait_for_function("!document.querySelector('#import-user-zip').disabled && FS.analyzePath('/user/instruments/USER/soundfont/tone.sf2').exists", timeout=60000)
            assert page.evaluate("FS.readFile('/user/instruments/USER/sid/keep.txt',{encoding:'utf8'})") == 'existing'
            assert page.evaluate("FS.readFile('/user/instruments/USER/dx7/check.txt',{encoding:'utf8'})") == 'imported'
            assert not page.evaluate("FS.analyzePath('/user/instruments/documents/readme.txt').exists")
            page.locator('#user-zip-input').set_input_files(str(work / 'unsafe.zip'))
            page.wait_for_function("!document.querySelector('#import-user-zip').disabled", timeout=60000)
            assert not page.evaluate("FS.analyzePath('/user/escaped.txt').exists || FS.analyzePath('/escaped.txt').exists")
            page.reload()
            ready()
            assert page.evaluate("FS.stat('/user/instruments/USER/soundfont/tone.sf2').size") > 0
            assert page.evaluate("FS.readFile('/user/instruments/USER/sid/keep.txt',{encoding:'utf8'})") == 'existing'
            page.locator('#canvas').click()
            page.keyboard.press('x')
            page.evaluate("text=>FS.writeFile('/user/projects/soundfont.cct',text)", source)
            assert page.evaluate("Module.ccall('webLoadProject','number',['string'],['/user/projects/soundfont.cct'])") == 0
            page.wait_for_timeout(1000)
            page.wait_for_function('Asyncify.state===0 && Asyncify.currData===null', timeout=15000)
            page.evaluate("window.probe={peak:0,finite:true,callbacks:0};const node=Module.SDL2.audio.scriptProcessorNode;node.addEventListener('audioprocess',e=>{probe.callbacks++;for(const x of e.outputBuffer.getChannelData(0)){probe.finite=probe.finite&&Number.isFinite(x);probe.peak=Math.max(probe.peak,Math.abs(x));}});void 0")
            def button(number, down):
                page.evaluate("([n,d])=>Module.ccall('webQueueButton',null,['number','number'],[n,d])", [number,int(down)])
                page.wait_for_timeout(75)
            button(6, True)
            button(6, False)
            page.wait_for_timeout(1500)
            audio = page.evaluate('probe')
            if not audio['peak']:
                page.screenshot(path=str(work / 'failure.png'))
                print(page.evaluate("({audio:getAudio().state,recovering:audioRecovering,status:document.querySelector('#audio-status').textContent,async:Asyncify.state,data:Asyncify.currData})"),errors,flush=True)
            assert audio['peak'] > 0 and audio['finite'], audio
            button(6, True)
            button(6, False)
            for _ in range(3):
                button(7, True); button(3, True); button(3, False); button(7, False)
            page.screenshot(path=str(work / 'instrument.png'))
            def tap(number):
                button(number, True); button(number, False)
            def edit_step(number):
                button(4, True); tap(number); button(4, False)
            for _ in range(4):
                tap(1)
            tap(4)  # One preset browser, scanning automatically on its first use.
            page.wait_for_timeout(1200)
            page.screenshot(path=str(work / 'preset-folders.png'))
            tap(4)  # Select Test.
            page.wait_for_timeout(750)
            tap(5)  # Return to Instrument.
            tap(3); tap(4)  # Edit beside Preset.
            edit_step(0)  # Tune +100 cents.
            tap(1); edit_step(3)  # Cutoff Bank -> 20 Hz.
            for _ in range(8):
                tap(1)
            edit_step(2)  # Volume FF -> FE.
            tap(1); edit_step(3)  # Pan 80 -> 81.
            tap(1); edit_step(3)  # Legacy Stock bend step -> 1 cent.
            page.screenshot(path=str(work / 'soundfont-edit.png'))
            assert page.evaluate("Module.ccall('webSaveProject','number',['string'],['/user/projects/roundtrip.cct'])") == 0
            saved = page.evaluate("FS.readFile('/user/projects/roundtrip.cct',{encoding:'utf8'})")
            assert '- Type: 28' in saved and '- Sound bank: tone.sf2' in saved and '- Sound preset: 0:0' in saved
            assert '- Sound controls: 100 20 -1 -1 -1 -1 -1 -1 -1' in saved, saved[saved.index('### Instrument 0'):saved.index('### Instrument 1')]
            saved_instrument = saved[saved.index('### Instrument 0'):saved.index('### Instrument 1')]
            assert '- Volume: 254' in saved_instrument and '- Pan: 129' in saved_instrument, saved_instrument
            assert '- Sound bend step: 1' in saved_instrument, saved_instrument
            assert page.evaluate("Module.ccall('webLoadProject','number',['string'],['/user/projects/roundtrip.cct'])") == 0
            page.wait_for_timeout(1000)
            page.wait_for_function('Asyncify.state===0 && Asyncify.currData===null', timeout=15000)
            assert not errors, errors
            result = dict(browser=browser.version, audio=audio, user_zip=True, existing_files_kept=True,
                          traversal_rejected=True, import_persisted=True, song_roundtrip=True, folder_browser=True, direct_editor=True, errors=errors)
            (work / 'result.json').write_text(json.dumps(result,indent=2)+'\n')
            print(json.dumps(result))
        finally:
            browser.close()
finally:
    server.shutdown()
