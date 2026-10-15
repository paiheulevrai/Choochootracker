from pathlib import Path
from html import escape as e
import base64, json
import reference_content
ROOT=Path(__file__).resolve().parent
pages=[]
def page(id,title,group,route,description,fields,controls,note=''):
 pages.append(dict(id=id,title=title,group=group,route=route,description=description,fields=fields,controls=controls,note=note))
page('song','Song','Arrange','Main screen · S',
'The arrangement starts here. Each column is a track, and each numbered cell plays a chain. Stack chains down a column to build a part; place them across the eight tracks to build a song.',
[('Chain cells','Choose which chain plays at each position. Reusing a chain also reuses its contents.'),('Track columns','Eight independent tracks. Keep chain lengths aligned when you want parts to stay together.'),('Song rows','Positions run from 00 to FF. Empty rows can separate sections or alternative arrangements.')],
[('EDIT','Enter a chain; double-tap to create a new chain where supported.'),('EDIT + direction','Change the chain number. With a selection, UP / DOWN moves the selected rows.'),('OPT + UP / DOWN','Jump through the arrangement in blocks of 16 rows.'),('SELECT + RIGHT','Open the selected Chain.'),('SELECT + UP / DOWN','Open Project / Settings.'),('OPT + SELECT / OPT + PLAY','Mute / solo the current track or selected columns. Release OPT first to keep it active.'),('Double-tap OPT','Enter or leave Live Mode.'),('PLAY in Live Mode','Start or queue the selected chain; an empty cell queues a stop.')],
'An asterisk beside a chain or phrase means it is used elsewhere. Edits to reused material affect every place that uses it.')
page('chain','Chain','Arrange','Song → SELECT + RIGHT',
'A chain joins phrases into a longer musical part. Each row chooses a phrase and can transpose it, so one pattern can become a bassline, a melody, or a chord progression without rewriting its notes.',
[('P — Phrase','The phrase number to play. A chain holds up to 16 phrase rows.'),('T — Transpose','Shift that phrase by a number of semitones.')],
[('EDIT','Enter a phrase; double-tap to create a new phrase where supported.'),('EDIT + direction','Adjust the selected phrase number or transposition.'),('OPT + LEFT / RIGHT','Move between tracks.'),('OPT + UP / DOWN','Move between chains in the current track.'),('SELECT + RIGHT / LEFT','Open Phrase / return to Song.'),('SELECT + EDIT, with a selection','Clone the selected phrases.'),('Double-tap EDIT, with a selection','Open Bounce to Sample for the selected chain rows.')])
page('phrase','Phrase','Arrange','Chain → SELECT + RIGHT',
'This is where you write the notes. A phrase contains 16 steps, numbered 0–F. Read each row from left to right: note, instrument, volume, then up to three effect commands.',
[('N — Note','Pitch and octave, such as C-4. A note-off ends a held note.'),('I — Instrument','The instrument that plays the note.'),('V — Volume','Step volume, from 00 to 7F. An empty value inherits the previous setting.'),('FX1 / FX2 / FX3','A command and its value. Available commands follow the instrument used on that row.')],
[('EDIT + direction','Adjust the selected note, instrument, volume, or effect value.'),('OPT + EDIT on an empty note','Insert a note-off.'),('EDIT + UP / DOWN on an FX name','Open the effect command browser.'),('OPT + LEFT / RIGHT','Move between tracks.'),('OPT + UP / DOWN','Move between phrases in the current chain.'),('SELECT + RIGHT / LEFT','Open Instrument / return to Chain.'),('SELECT + UP','Open Groove.'),('Double-tap EDIT, with a selection','Open Bounce to Sample for the selected steps.')],
'A phrase is shared data. Clone it in Chain when you want to make a variation without changing every use.')
page('fx-browser','FX browser','Arrange','Phrase → select an FX name → EDIT + UP / DOWN',
'Browse commands by purpose, then choose one for the current step. The list combines shared tracker commands with commands supported by the row’s instrument. The help area explains the highlighted command.',
[('Category','Groups related commands, including sequencer, track, envelope, modulation, and instrument controls.'),('Command','The short code that appears in the FX column.'),('Help text','Explains the selected command and its value format.')],
[('Hold EDIT + LEFT / RIGHT','Move to the previous or next command.'),('Hold EDIT + UP / DOWN','Move by a row within the command grid and between groups.'),('Release EDIT and directions','Accept the highlighted command and return to the pattern.'),('EDIT + direction, back in Phrase','Set the command’s value in the adjacent value cell.')])
page('instrument','Instrument','Sound','Phrase → SELECT + RIGHT',
'Choose a sound engine and shape its voice. The top of the page contains shared instrument settings; the rest changes with the selected engine. This example uses Braids.',
[('Type / Name','Choose the engine and give the instrument a useful name.'),('Load / Save','Load an instrument preset or save the current sound.'),('Transp. / TIC','Allow note transposition and set the default table speed in ticks.'),('PAN / VOL','Instrument pan and volume. Track mixing is applied separately.'),('Model / Timbre / Color','Braids’ synthesis model and its two sound-shaping parameters.'),('Filter / ADSR','Filter type, mode, slope, cutoff and resonance; envelope attack, decay, sustain, release and shape.')],
[('EDIT','Open a chooser or activate the selected command.'),('EDIT + direction','Adjust the selected parameter.'),('OPT + LEFT / RIGHT','Browse instruments.'),('EDIT + PLAY','Preview the instrument.'),('SELECT + OPT / SELECT + EDIT','Copy / paste the instrument.'),('SELECT + UP / DOWN','Open Modulation / Instrument Pool.'),('SELECT + RIGHT / LEFT','Open Table / return to Phrase.')],
'Engine controls vary. Start with one sound, then use Modulation and Table to add movement.')
page('sampler','Sampler','Sound','Instrument → Type → Sampler',
'Turn a WAV file into a playable instrument. Set its pitch and playback behaviour, then use the filter and envelope to make it fit the track. Open Sample Edit for regions, slices, and audio processing.',
[('Sample','Load the source WAV file.'),('EDIT beside Sample','Open the waveform editor for detailed sample work.'),('Pitch / Loop / Speed','Transpose playback, choose looping behaviour, and adjust playback speed.'),('Filter / ADSR','Shape the sample’s tone and amplitude, as with a synth voice.')],
[('EDIT on Sample','Open the sample browser.'),('EDIT + LEFT / RIGHT on Sample','Load the previous or next WAV in the same folder.'),('EDIT on the sample’s EDIT field','Open the sample editor.'),('EDIT + direction','Adjust the selected playback or tone parameter.'),('EDIT + PLAY','Preview the instrument.')])
page('sample-editor','Sample editor','Sound','Sampler → EDIT beside the sample name',
'Work directly with the waveform. Define the part that plays, divide it into slices, and process a selected region. The selection used for processing is separate from the instrument’s playback region.',
[('Waveform','Shows the sample, playback region, and processing selection.'),('Region / Select','Set the playback limits and the region used by editing operations.'),('Slice','Choose Off, Equal, Auto, or Lazy slicing. Adjust the count and individual slice starts.'),('Spd algo','Choose Dirty or Clean time-stretch processing.'),('Process / GO','Choose Crop, Normalize, Delete, Silence, Fade In, Fade Out, or Reverse, then apply it.'),('UNDO','Toggle the most recent processing operation; this history is cleared when you leave the editor.'),('File / GO','Choose Save or Save As and write the edited WAV.')],
[('Direction','Move between editor fields.'),('EDIT + direction','Adjust the selected boundary or value.'),('EDIT on GO','Run the selected process or file action.'),('EDIT on UNDO','Restore the previous processing state.'),('OPT / SELECT + LEFT','Return to the instrument.')],
'Processing changes the sample in memory. Save writes the WAV; save the project too to retain the instrument and arrangement settings.')
page('table','Table','Sound','Instrument → SELECT + RIGHT',
'A table is a short sequence that changes a sound while notes play. Use it for pitch movement, rhythmic volume changes, and effect automation. Its timing is measured in ticks rather than phrase steps.',
[('Pitch','Relative pitch (~) or an absolute note (=).'),('Volume','A 00–0F multiplier applied on top of the instrument envelope.'),('FX lanes','Four command lanes, each able to run at its own tick speed.'),('Retrigger','Choose whether the table restarts with the instrument, phrase, chain, or runs freely.')],
[('EDIT + direction','Adjust the selected pitch, volume, command, or value.'),('EDIT + UP / DOWN on an FX name','Open the effect chooser.'),('SELECT + OPT','Select table cells for shared editing or copying.'),('SELECT + LEFT','Return to Instrument.'),('SELECT + DOWN','Open AY Wavetable.')],
'Each instrument has a default table with the same number. A TIC command on the last row can set an individual FX lane’s speed.')
page('groove','Groove','Arrange','Phrase → SELECT + UP',
'A groove sets how many ticks each phrase step lasts. Equal values produce straight timing; alternating longer and shorter values create swing. The sequence repeats as the track plays.',
[('Groove number','The timing pattern being edited.'),('T — Timing','Ticks per step. The example uses 06, 06 for an even pulse.')],
[('UP / DOWN','Choose a groove step.'),('EDIT + direction','Adjust the tick count.'),('OPT + LEFT / RIGHT','Choose the previous or next groove.'),('SELECT + DOWN','Return to Phrase.')],
'Use GRV in a phrase to select a track groove, or GGR to change the global groove.')
page('modulation','Modulation','Sound','Instrument → SELECT + UP',
'Give an instrument movement with four modulation slots. Each slot connects a source to a destination and sets how strongly it acts. Envelope, LFO, and stick sources expose different controls.',
[('Mod1–Mod4','Choose the source for each slot.'),('Dest','The parameter to move. Available destinations depend on the instrument.'),('Amt','Depth and direction of modulation.'),('Source parameters','Envelope stages, LFO shape/trigger/period, or stick behaviour, depending on the source.')],
[('EDIT on a source or destination','Open its grouped chooser.'),('EDIT + direction','Adjust the selected value; on Destination, LEFT / RIGHT cycles destinations.'),('OPT + LEFT / RIGHT','Browse instruments.'),('SELECT + DOWN / UP','Return to Instrument / open Insert FX.'),('STICK LIVE','Apply configured stick modulation without writing it to a phrase.'),('MOTION RECORD / MOTION ERASE','During playback, write changed stick destinations into FX cells / erase matching motion FX.')])
page('instrument-pool','Instrument Pool','Sound','Instrument → SELECT + DOWN',
'An overview of the instruments stored in the project. Use the pool to find a sound quickly, preview it, and return to its editor.',
[('Instrument number','The number used by the I column in a phrase.'),('Instrument name','The saved name, or engine name for an unnamed instrument.')],
[('Direction','Move through the pool.'),('EDIT + PLAY','Preview the highlighted instrument.'),('EDIT / SELECT + UP','Open the highlighted instrument’s editor.'),('SELECT + OPT / SELECT + EDIT','Copy / paste the highlighted instrument.')])
page('ay-wavetable','AY Wavetable','Sound','Table → SELECT + DOWN',
'Draw a 32-step waveform for the AY wavetable oscillator or a wavetable LFO. The large preview shows the current wave; neighbouring previews help compare nearby waves.',
[('Waveform steps','32 values, each from 0 to F, defining the wave’s shape.'),('Load / Save','Import or export .aywave data.'),('Save count','Choose how many consecutive wavetables to save.')],
[('LEFT / RIGHT','Move along the waveform steps.'),('EDIT + direction','Change the current step’s height.'),('OPT + UP / DOWN','Choose a neighbouring wavetable.'),('OPT + LEFT / RIGHT','Move backward or forward by 16 wavetables.'),('EDIT on Load / Save','Open the corresponding file operation.'),('SELECT + UP','Return to Table.')],
'This editor is for AY-style wavetables. The BYOWTBL instrument loads its own WAV-based wavetable files.')
page('mixer','Mixer','Mix','Song → SELECT + LEFT',
'Balance the eight tracks and their shared effects. Mixer settings belong to a track, so they stay in place when that track changes instruments. The meters show left and right signal levels.',
[('LVL / PAN','Track level and stereo placement.'),('REV / DLY','Send levels into the shared reverb and delay.'),('TLT','Tilt EQ: 80 is neutral; lower values favour bass, higher values favour treble.'),('M / S','Mute or solo the track.'),('MASTER FX','Open two insert slots for the complete mix.'),('Meters / CPU / CLIP','Readouts for signal level, processing load, and clipping.')],
[('Direction','Choose a track and mixer field.'),('EDIT + direction','Adjust a level, pan, or tone value.'),('EDIT on M / S','Toggle mute or solo.'),('EDIT on MASTER FX','Open the master insert effects.'),('SELECT + UP / DOWN','Open Reverb / Delay.'),('SELECT + RIGHT','Return to Song.')])
page('insert-fx','Insert FX','Mix','Instrument → SELECT + UP → SELECT + UP',
'Process the selected track with two effects in series: TF1 feeds TF2. Each module exposes its own parameters. The Master FX version uses the same layout for processing the complete mix.',
[('TF1 / TF2','Choose an effect for each slot, or Off to leave it empty.'),('Bypass','Temporarily skip a slot while retaining its settings.'),('Numbered parameters','Controls for the selected module. Names change with the effect.'),('CPU','Current audio processing load.')],
[('Direction','Move between the two slots and their parameters.'),('EDIT on the module name','Open the module chooser.'),('EDIT + direction','Adjust a module or parameter value.'),('EDIT + LEFT / RIGHT on Bypass','Set that slot’s bypass off or on.'),('SELECT + DOWN','Return from track Insert FX to Modulation.'),('OPT, in Master FX','Return to Mixer.'),('OPT + LEFT / RIGHT, in track Insert FX','Select the previous or next track.')],
'Track inserts can be automated with matching FX and modulation destinations. Master inserts are project settings and do not use track automation.')
page('reverb','Reverb','Mix','Mixer → SELECT + UP',
'The shared reverb adds space around the mix. Set each track’s send in Mixer, then shape the common reverb here. This is the Clouds reverb section.',
[('Return','How much wet reverb reaches the master mix.'),('Time','Length of the reverb tail.'),('Damping','High-frequency absorption in the tail.'),('LP filter / HP filter','Low-pass and high-pass filtering for the reverb.')],
[('UP / DOWN','Select a reverb parameter.'),('EDIT + direction','Adjust its value.'),('SELECT + DOWN / LEFT','Return to Mixer.')])
page('delay','Delay','Mix','Mixer → SELECT + DOWN',
'The shared ping-pong delay adds repeats that cross between left and right. Track sends control what enters the delay; this page controls its timing, feedback, and return.',
[('Return','Wet delay level in the mix.'),('To Reverb','Send some of the delayed signal into the shared reverb.'),('Ticks','Delay time measured in tracker ticks.'),('Feedback','How strongly each repeat feeds the next.'),('Filter','Low-pass filtering of the delayed signal.')],
[('UP / DOWN','Select a delay parameter.'),('EDIT + direction','Adjust its value.'),('SELECT + UP / LEFT','Return to Mixer.')])
page('project','Project','Project','Song → SELECT + UP',
'Manage the song as a file and set its musical foundations. This page holds project identity, timing, tuning, and the entry points for exporting and cleanup.',
[('Load / Save / New','Open a project, save your work, or start a fresh project.'),('Export / Manage','Render audio or open project cleanup.'),('File / Title / Author','File name and descriptive project metadata.'),('Tick rate','The speed of the tracker clock; the page also shows the resulting BPM.'),('Pitch table / Scale','Open tuning and scale settings.')],
[('Direction','Choose a command or field.'),('EDIT','Activate a command or open a text/value editor.'),('EDIT + direction','Change characters or numerical values.'),('SELECT + DOWN','Return to Song.')],
'Save the project as a .cct file to keep the arrangement, instruments, and project settings together.')
page('scale','Scale','Project','Project → Scale',
'Choose a musical root and scale, then decide which tracks follow it. Quantizer changes the notes heard during playback; Note Lock restricts the notes entered into phrases.',
[('Mode','Quantizer or Note Lock.'),('Root / Scale','The root pitch class and scale preset.'),('Tracks','Enable the scale for individual tracks.'),('Notes','Inspect or edit the scale’s allowed pitch classes.')],
[('Direction','Move between mode, scale, track, and note fields.'),('EDIT + direction','Change the selected setting.'),('EDIT on a checkbox','Include or exclude a track or pitch class.'),('OPT','Return to Project.')],
'Quantizer preserves the notes stored in your phrases. Note Lock affects new note entry; changing its scale does not rewrite existing notes.')
page('pitch-table','Pitch table','Project','Project → Pitch table',
'The pitch table defines the tuning used by the project. Keep the default for ordinary note entry, or load an alternative tuning from a CSV file.',
[('Name','A label for the current tuning.'),('Load from CSV / Save to CSV','Import or export a pitch table.'),('Generate for current clock','Rebuild a pitch table for the current chip clock.')],
[('UP / DOWN','Choose the name or a file/generation command.'),('EDIT','Edit the name or activate the selected command.'),('OPT','Return to Project.')])
page('file-browser','File browser','Project','Open a Load command',
'The same browser is used to find projects, presets, samples, and other assets. Its title tells you what is being loaded, and the list is filtered for the relevant file type.',
[('Folder / file list','Browse available folders and matching files.'),('Current path','Shows where you are in the file hierarchy.')],
[('UP / DOWN','Highlight a folder or file.'),('EDIT','Open the highlighted folder or select the file.'),('EDIT on [..]','Go up to the parent folder.'),('OPT','Cancel and return to the page that opened the browser.')])
page('export','Export','Project','Project → Export',
'Render the arrangement to a WAV file, or export separate track stems for further mixing. Choose the start position, audio format, and destination before starting.',
[('Start row','The Song row where the export begins.'),('WAV — Song / Stems','Render a combined mix or separate tracks.'),('Sample rate / Bit depth','The format of the rendered audio.'),('Folder','Destination for exported files.'),('MIDI','Export the arrangement as a Standard MIDI File; audio-engine sound design is not carried across.')],
[('Direction','Select the export setting or format.'),('EDIT + direction','Adjust start row, sample rate, or bit depth.'),('EDIT on Folder','Choose an export folder.'),('OPT + EDIT on Folder','Restore the default export location.'),('EDIT on Song / Stems','Start the selected WAV export.'),('OPT while rendering','Cancel the export.')],
'For a smaller section, select rows in Song, Chain, or Phrase and double-tap EDIT to open Bounce to Sample. Desktop builds also display platform-specific export options.')
page('manage','Manage project','Project','Project → Manage',
'Clean up the project after editing. Remove unused material and consolidate duplicate pattern data to make the project easier to maintain.',
[('Phrases and Chains','Clean unused and duplicate phrases/chains.'),('Instruments and Tables','Clean unused instruments and tables.'),('Import project / Import WAV','Android file-import entries. These require the Android build.')],
[('UP / DOWN','Choose a cleanup operation.'),('EDIT','Run the selected cleanup flow.'),('OPT','Return to Project.')],
'Save a copy before cleanup if you want to keep unused sketches for later.')
page('settings','Settings','Settings','Song → SELECT + DOWN',
'Application preferences are grouped by purpose. Start here to change control mapping, audio behaviour, display appearance, or MIDI connections.',
[('Controls','Input repeat timing, stick live behaviour, and mapping.'),('Audio','Audio buffer size and a CPU readout.'),('MIDI','Input/output devices and mapping pages.'),('Synths','AY quality and Braids character settings.'),('Mixer','Overall mix volume and tilt pivot frequency.'),('Graphics','Theme, font, and display preferences.'),('Support report / Quit ChooChooTracker','Save diagnostic information or close the app.')],
[('UP / DOWN','Choose a settings category.'),('EDIT','Open the category or activate the selected command.'),('OPT / SELECT + UP','Return to Song.')])
page('controls','Controls','Settings','Settings → Controls',
'Make navigation feel comfortable and assign logical controls to your own keys or gamepad. All control names in this guide refer to these actions, so the guide still applies after remapping.',
[('Repeat delay','How long a held direction waits before repeating.'),('Repeat speed','The rate of repeated movement.'),('Stick live mode','Hold, Toggle, or Free behaviour for live stick modulation.'),('Key mapping','Assign the physical inputs that trigger each named action.')],
[('UP / DOWN','Choose a setting.'),('EDIT + direction','Adjust timing or stick behaviour.'),('EDIT on Key mapping','Open the mapping editor.'),('Done, in Key mapping','Save the new assignments.')],
'In the mapping editor, release the input used to select an action before pressing its new assignment.')
page('audio','Audio','Settings','Settings → Audio',
'Adjust the audio buffer and watch the processing load. A smaller buffer responds sooner; a larger buffer gives audio processing more time to complete.',
[('Buffer size','The number of audio frames processed in a block. Choices depend on the platform.'),('CPU load','A read-only indicator of the audio workload.')],
[('EDIT + direction on Buffer size','Select a supported buffer size.'),('OPT','Return to Settings.')],
'If a demanding project crackles, try a larger buffer or reduce its processing load.')
page('midi','MIDI','Settings','Settings → MIDI',
'Connect MIDI input and output, then decide how incoming notes and controller movements interact with the project. Device availability depends on the platform and connected hardware.',
[('MIDI In / MIDI Out','Choose an available port, or Off.'),('Channel mapping','Assign incoming MIDI channels to instruments for preview.'),('MIDI CC mapping','Connect controller messages to instrument parameters or global controls.')],
[('UP / DOWN','Choose a device field or mapping page.'),('EDIT + direction','Change the selected device.'),('EDIT on a mapping entry','Open that mapping page.'),('OPT','Return to Settings.')],
'To send notes to an external device, use a MIDI Out instrument on a track and choose its MIDI channel.')

# Selection is supported by these grid pages, not by settings or instrument forms.
selection_pages = {'song', 'chain', 'phrase', 'table', 'groove'}
for p in pages:
 if p['id'] in selection_pages:
  p['controls'] = [(a,b) for a,b in p['controls'] if a != 'SELECT + OPT']
  p['controls'] += [
   ('SELECT + OPT','Hold SELECT, then press OPT to enter selection mode. Release both; use directions to extend the range. Repeat SELECT + OPT to cycle selection ranges.'),
   ('OPT, with a selection','Tap and release OPT to copy selected cells and leave selection mode.'),
   ('OPT + EDIT, with a selection','Cut selected cells and leave selection mode.'),
   ('SELECT + EDIT, outside selection','Hold SELECT, then press EDIT to paste copied cells at the cursor.')]
 if p['id'] in {'instrument','instrument-pool'}:
  p['controls'] = [(a,b) for a,b in p['controls'] if a != 'SELECT + OPT / SELECT + EDIT']
  p['controls'] += [('SELECT + OPT','Copy the instrument and its default table.'),('SELECT + EDIT','Paste into the chosen instrument slot, replacing its contents.')]
 if p['id'] == 'song':
  p['controls'] += [('SELECT + EDIT, with a selection','Clone the selected chains. Their phrases remain shared.'),('Hold SELECT; press EDIT twice, with a selection','First press clones the chains; the second clones their phrases. Keep SELECT held between presses.')]
  p['note'] += ' The copy/paste examples use the prepared Song selection-copy fix. Builds with the September 15 regression may ignore a plain OPT tap.'
 if p['id'] == 'phrase':
  p['controls'] += [('SELECT + EDIT, with only instrument cells selected','Clone each distinct selected instrument and its default table; update the selected instrument numbers.')]
 if p['id'] == 'table':
  p['note'] += ' Start selection on a data row, below the timing header.'
reference_content.update_pages(pages)
examples = json.loads((ROOT/'examples.json').read_text())
example_lookup = {(x['page'],x['control']):x for x in examples if x['changed']}
used_examples = {}

group_order = ['Arrange','Sound','Mix','Project','Settings']
pages.sort(key=lambda p: group_order.index(p['group']))

# Kept as editable content alongside the generated HTML.
(ROOT/'pages.json').write_text(json.dumps(pages,ensure_ascii=False,indent=2)+'\n')

def table(rows,label,page_id=None):
 body=[]
 for a,b in rows:
  demo=example_lookup.get((page_id,a)) if page_id else None
  control=e(a)
  if demo:
   used_examples[demo['id']]=demo
   control='<button type="button" class="control-example" data-example="'+demo['id']+'" aria-haspopup="dialog" aria-describedby="tip-'+demo['id']+'">'+e(a)+'<span class="example-tooltip" role="tooltip" id="tip-'+demo['id']+'">Click to view example</span></button>'
  body.append('<tr><th scope="row">'+control+'</th><td>'+e(b)+'</td></tr>')
 return '<table><caption>'+e(label)+'</caption><thead><tr><th scope="col">Control</th><th scope="col">What it does</th></tr></thead><tbody>'+''.join(body)+'</tbody></table>'
nav='<div class="nav-start">'+reference_content.navigation('start','Start')+'</div>'
for group in group_order:
 nav+='<div class="nav-group"><span>'+group+'</span>'+''.join(reference_content.navigation(p['id'],p['title']) for p in pages if p['group']==group)+(reference_content.navigation('cloning','Cloning') if group == 'Arrange' else '')+'</div>'
sections=[]
for i,p in enumerate(pages,1):
 id=p['id']
 sections.append(f'''<section class="page" id="{id}" aria-labelledby="{id}-title">
 <div class="section-label"><span>{i:02d} / {p['group'].upper()}</span><a href="#contents">CONTENTS ↑</a></div>
 <h2 id="{id}-title">{e(p['title'])}</h2><p class="route">{e(p['route'])}</p>
 <div class="screen-summary"><figure><a class="screen-link" href="assets/{id}.png" aria-label="Open full-size {e(p['title'])} screenshot"><img src="assets/{id}.png" width="640" height="480" alt="Choo Choo {e(p['title'])} page" loading="lazy"></a><figcaption>{e(p['title'])}</figcaption></figure>
 <div class="screen-copy"><p class="description">{e(p['description'])}</p>
 <h3>On this screen</h3><dl class="fields">{''.join('<div><dt>'+e(a)+'</dt><dd>'+e(b)+'</dd></div>' for a,b in p['fields'])}</dl></div></div>
 {table(p['controls'],'Controls',id)}
 {('<p class="note">'+e(p['note'])+'</p>') if p['note'] else ''}
 {reference_content.render(id)}
 </section>''')
fonts = [
 ('departure', 'Departure Mono', 'DepartureMono-Regular.otf'),
 ('share-tech', 'Share Tech Mono', 'ShareTechMono-Regular.ttf'),
 ('vt323', 'VT323', 'vt323.ttf'),
 ('silkscreen', 'Silkscreen', 'silkscreen.ttf'),
 ('orbitron', 'Orbitron', 'orbitron.ttf'),
 ('oxanium', 'Oxanium', 'oxanium.ttf'),
 ('jetbrains', 'JetBrains Mono', 'JetBrainsMono-Regular.ttf'),
 ('fira', 'Fira Code', 'FiraCode-Regular.ttf'),
]
font_css = ''.join('@font-face{font-family:"'+name+'";src:url("assets/fonts/'+file+'");font-weight:100 900;font-display:swap;}' for _,name,file in fonts)
font_picker = '<label class="font-picker" for="guide-font">Font <select id="guide-font">'+''.join('<option value="'+id+'">'+name+'</option>' for id,name,_ in fonts)+'</select></label>'
font_families = json.dumps({id:name for id,name,_ in fonts})
css=font_css+'''

:root{--paper:#e9e8e0;--ink:#252926;--muted:#596159;--line:#bcc2b7;--accent:#286653;--guide-font:"Departure Mono";font-family:var(--guide-font),monospace;color:var(--ink);background:var(--paper);font-synthesis:none}*{box-sizing:border-box}html{scroll-behavior:smooth;scroll-padding-top:72px}body{margin:0;font-size:14px;line-height:1.45}a{color:inherit;text-underline-offset:3px}a:hover{color:var(--accent)}:focus-visible{outline:2px solid var(--accent);outline-offset:3px}.skip{position:fixed;left:16px;top:-70px;background:var(--paper);padding:8px}.skip:focus{top:8px;z-index:20}header{position:sticky;top:0;z-index:5;background:var(--paper);border-bottom:1px solid var(--ink);display:flex;align-items:center;gap:16px;padding:10px max(20px,calc((100vw - 1320px)/2));min-height:52px}.wordmark{font-size:20px;text-decoration:none;white-space:nowrap}.edition{font-size:10px;color:var(--muted);margin-right:auto}.font-picker{display:flex;align-items:center;gap:8px;font-size:11px;white-space:nowrap}select{font:inherit;color:var(--ink);background:#f5f4ed;border:1px solid var(--line);border-radius:0;padding:5px 24px 5px 8px;max-width:220px}.intro{max-width:1360px;margin:auto;padding:18px 20px 20px}.eyebrow{font-size:10px;margin:0 0 5px;color:var(--muted)}.intro h1{font-size:36px;font-weight:400;line-height:1.15;margin:0 0 5px}.intro .lead{font-size:14px;margin:0 0 4px}.intro .small{font-size:12px;max-width:850px;color:var(--muted);margin:0}.intro-links{display:flex;gap:18px;margin-top:9px;font-size:11px}.layout{max-width:1360px;margin:auto;padding:0 20px;display:grid;grid-template-columns:174px minmax(0,1fr);gap:24px;align-items:start}aside{position:sticky;top:68px;max-height:calc(100vh - 80px);overflow:auto;padding:0 7px 8px 0;scrollbar-width:thin}.nav-start{margin-bottom:10px}.nav-group{margin:0 0 10px}.nav-group>span{display:block;text-transform:uppercase;font-size:10px;color:var(--muted);margin-bottom:2px}.nav-group a,.nav-start a{display:block;padding:2px 5px;font-size:12px;text-decoration:none;line-height:1.4;border-left:2px solid transparent}.nav-group a[aria-current="location"],.nav-start a[aria-current="location"]{color:var(--accent);border-left-color:var(--accent);background:#dce1d5}.page,.basics{border-top:1px solid var(--ink);padding:26px 0 112px;scroll-margin-top:0}.section-label{display:flex;justify-content:space-between;gap:10px;font-size:10px;color:var(--muted)}.section-label a{text-decoration:none}h2{font-size:27px;font-weight:400;line-height:1.15;margin:9px 0 4px}h3{font-size:14px;font-weight:400;margin:12px 0 5px}.route{font-size:10px;color:var(--muted);margin:0 0 10px}.screen-summary{display:grid;grid-template-columns:minmax(0,480px) minmax(0,1fr);gap:16px;align-items:start}figure{margin:0}.screen-link{display:block;background:#bcc2b7;max-width:640px;border:1px solid var(--line);padding:5px}img{display:block;max-width:100%;width:640px;height:auto;image-rendering:pixelated}figcaption{font-size:9px;color:var(--muted);margin-top:4px}.description{font-size:14px;line-height:1.45;margin:0 0 10px}.fields{margin:0;font-size:12px;line-height:1.4}.fields>div{padding:5px 0;border-top:1px solid var(--line)}dt{font-weight:400;color:var(--ink)}dd{margin:2px 0 0;color:var(--muted)}table{width:100%;border-collapse:collapse;margin:14px 0 0;font-size:12px;line-height:1.4;text-align:left}caption{text-align:left;font-size:14px;margin-bottom:5px}thead th{font-size:9px;font-weight:400;text-transform:uppercase;color:var(--muted);padding:4px 12px 4px 0;border-top:1px solid var(--ink);border-bottom:1px solid var(--line)}tbody th,td{vertical-align:top;padding:6px 12px 6px 0;border-bottom:1px solid var(--line)}tbody th{font-weight:400;width:34%}td{color:var(--muted)}.note{font-size:11px;line-height:1.45;padding-left:10px;border-left:2px solid var(--accent);color:var(--muted);margin:10px 0 0}.basics h2{font-size:25px}.basics>p{font-size:13px;line-height:1.45;margin:8px 0;max-width:1000px}.spine{display:flex;flex-wrap:wrap;gap:4px 12px;padding:9px 0;border-top:1px solid var(--line);border-bottom:1px solid var(--line);margin:10px 0;font-size:13px}.spine a{text-decoration:none}.spine span{color:var(--muted)}footer{max-width:1320px;border-top:1px solid var(--ink);margin:10px auto 0;padding:12px 20px;display:flex;gap:20px;justify-content:space-between;align-items:start;font-size:9px;color:var(--muted)}.footer-note{margin:0;max-width:950px}button{font:inherit;color:inherit}.print{background:none;border:0;padding:0;text-decoration:underline;text-underline-offset:3px;cursor:pointer}dialog{border:0;padding:12px;background:var(--paper);max-width:calc(100vw - 24px);max-height:calc(100vh - 24px)}dialog::backdrop{background:#080500cc}dialog img{width:min(960px,100%);max-height:calc(100vh - 90px);object-fit:contain}dialog .close-view{display:block;margin:0 0 8px auto;border:0;background:none;cursor:pointer;padding:5px}@media(max-width:1080px){.screen-summary{grid-template-columns:minmax(0,1fr) minmax(0,1fr)}.layout{grid-template-columns:156px minmax(0,1fr);gap:18px}.description{font-size:13px}}@media(max-width:820px){.screen-summary{grid-template-columns:1fr}.screen-summary figure{max-width:480px}.screen-copy h3{margin-top:10px}.fields>div{display:grid;grid-template-columns:160px 1fr;gap:10px}.fields dd{margin:0}.edition{display:none}.font-picker{margin-left:auto}header{gap:10px}.layout{grid-template-columns:145px minmax(0,1fr);gap:14px}.nav-group a,.nav-start a{font-size:11px}}@media(max-width:600px){header{padding:8px 12px;min-height:48px;gap:8px}.wordmark{font-size:16px}.font-picker{font-size:10px;gap:5px}select{max-width:165px;padding:5px}.intro{padding:14px 12px}.intro h1{font-size:28px}.intro .lead{font-size:12px}.intro .small{font-size:11px}.layout{display:block;padding:0 12px}aside{position:static;max-height:none;overflow:visible;padding:0 0 10px;display:flex;flex-wrap:wrap;gap:6px 14px;font-size:11px}.nav-start{width:100%;margin:0}.nav-group{flex:1 1 135px;margin:0}.nav-group a,.nav-start a{font-size:11px;line-height:1.5;padding:3px 5px}.page,.basics{padding-bottom:88px}.section-label{font-size:9px}h2{font-size:25px}.description{font-size:13px}.fields>div{display:block}.fields dd{margin-top:2px}tbody th{width:38%}tbody th,td{padding-right:8px;overflow-wrap:anywhere}.route{font-size:9px}footer{padding:10px 12px;flex-direction:column;gap:8px}html{scroll-padding-top:60px}.page,.basics{scroll-margin-top:0}}@media(prefers-reduced-motion:reduce){html{scroll-behavior:auto}}@media print{html{scroll-behavior:auto}body{background:white;color:black}header{position:static}.font-picker,aside,.intro-links,.section-label a,.skip,.print{display:none}header,.intro,.layout,footer{display:block;padding:0;margin:0}.intro{padding:12px 0}.intro h1{font-size:30px}.page{break-before:page;padding-bottom:0}.screen-summary{display:grid;grid-template-columns:1fr 1fr;gap:14px}.page .description{font-size:11px}.fields,table{font-size:9px}.fields>div{display:block;padding:4px 0}tbody th,td{padding:4px}.page h2{font-size:25px}.page h3,caption{font-size:12px}figure,tr,.note{break-inside:avoid}}
'''
css+='''
.control-example{position:relative;display:inline;text-align:left;border:0;background:none;padding:0;font:inherit;line-height:inherit;color:inherit;text-decoration:underline dotted;text-underline-offset:4px;cursor:pointer}
.control-example:hover,.control-example:focus-visible{color:#b22222}
.example-tooltip{position:absolute;z-index:8;left:0;bottom:calc(100% + 7px);width:max-content;max-width:240px;padding:6px 9px;background:#252926;color:#fff;font-size:11px;line-height:1.4;text-decoration:none;box-shadow:0 2px 6px #0002;visibility:hidden;opacity:0;pointer-events:none;transition:opacity .12s}
.control-example:hover .example-tooltip,.control-example:focus-visible .example-tooltip{visibility:visible;opacity:1}
#example-view{width:min(680px,calc(100vw - 24px));padding:14px;max-height:calc(100dvh - 24px);overflow:auto}
.example-heading{display:flex;justify-content:space-between;align-items:center;gap:12px}.example-heading p{font-size:11px;color:var(--muted);margin:0}.example-heading .close-view{margin:0}
#example-title{font-size:19px;margin:6px 0}#example-summary{font-size:12px;color:var(--muted);margin:0 0 10px}#example-image{width:100%;max-height:calc(100dvh - 175px);object-fit:contain;background:#100c09}
body:has(dialog[open]){overflow:hidden}
@media(max-width:600px){#example-view{padding:10px}#example-title{font-size:16px}.example-tooltip{font-size:10px;max-width:200px}}
@media print{.example-tooltip{display:none}.control-example{text-decoration:none;color:inherit}dialog{display:none}}
'''
basics='''<section class="basics" id="start"><div class="section-label"><span>START HERE</span><span>YOUR MAPPINGS. THE SAME ACTIONS.</span></div><h2>Find your way around.</h2><p>Choo Choo builds a song from <strong>chains</strong>, chains from <strong>phrases</strong>, and phrases from notes played by <strong>instruments</strong>. Move along the main pages with <strong>SELECT + LEFT / RIGHT</strong>. The small screen map in the lower-right corner shows where you are.</p><nav class="spine" aria-label="Main screen order"><a href="#mixer">Mixer</a><span>→</span><a href="#song">Song</a><span>→</span><a href="#chain">Chain</a><span>→</span><a href="#phrase">Phrase</a><span>→</span><a href="#instrument">Instrument</a><span>→</span><a href="#table">Table</a></nav><p>Navigation and editing use <strong>logical control names</strong>, so your own keyboard or gamepad mappings still apply. Desktop key jazz has a separate keyboard layout, described under Phrase. <strong>SELECT</strong> is also called <strong>SHIFT</strong> in some Choo Choo help text. Check your assignments in <a href="#controls">Settings → Controls → Key mapping</a>.</p>'''+table([
('UP / DOWN / LEFT / RIGHT','Move the cursor between cells or fields.'),('EDIT','Enter a value, open a chooser, or activate the selected command.'),('EDIT + LEFT / RIGHT','Make a fine value adjustment.'),('EDIT + UP / DOWN','Make a coarse value adjustment, or open a contextual chooser.'),('SELECT + direction','Navigate between pages.'),('PLAY','Start from the cursor; press again to stop.'),('SELECT + PLAY','Start all tracks when used outside Song.'),('SELECT + EDIT','Paste copied data outside selection mode. See each page for selection and cloning controls.')],'Shared controls')+'''<p class="note">Many values use hexadecimal: after 09 comes 0A, then 0B through 0F, then 10. Phrase rows 0–F are sixteen steps.</p></section>'''
basics=basics.replace('</section>',reference_content.render('start')+'</section>')
css += reference_content.CSS
css += (ROOT/'cloning.css').read_text()
cloning = (ROOT/'cloning.html').read_text()
sections.insert(sum(p['group'] == 'Arrange' for p in pages), cloning)
html='''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="description" content="A visual Choo Choo user guide: major pages, screenshots, and controls by action name."><title>Choo Choo — User Guide</title><style>'''+css+'''</style></head><body id="top"><a class="skip" href="#start">Skip to guide</a><header><a class="wordmark" href="#top">Choo Choo</a><span class="edition">USER GUIDE / OCTOBER 2026</span>'''+font_picker+'''</header><div class="intro"><p class="eyebrow">EIGHT TRACKS. ONE JOURNEY.</p><h1>User guide.</h1><p class="lead">From your first phrase to the final mix.</p><p class="small">Find a page, see what it does, and learn its controls.<br>In each page’s control list, hover an underlined name: it turns red. Click to open a looping GIF example.</p></div><div class="layout"><aside id="contents" aria-label="Contents">'''+nav+'''</aside><main>'''+basics+'\n'.join(sections)+'''</main></div><dialog id="screen-view"><button type="button" class="close-view" aria-label="Close screenshot">Close ×</button><img alt=""></dialog><dialog id="example-view" aria-labelledby="example-title" aria-describedby="example-summary"><div class="example-heading"><p id="example-page"></p><button type="button" class="close-view" aria-label="Close example">Close ×</button></div><h2 id="example-title"></h2><p id="example-summary"></p><img id="example-image" alt=""></dialog><footer><p class="footer-note">Choo Choo / User Guide · Comprehensive upstream edition, 15 October 2026.<br>Text and captures: upstream 902f96a. NostromoAmberDa2 · Departure Mono · 640 × 480.<br>Reference material adapted from the <a href="https://github.com/paiheulevrai/Choochootracker/blob/902f96aa8750ea36acd69d94642a8df37b00ffdd/docs/USER_MANUAL.md">upstream manual</a> and compared with the <a href="https://choochootracker.vercel.app/manual">web manual</a>. Copyright 2025 ChipNomad and 2026 paiheulevrai; MIT license included under Settings → Credits and licensing.</p><a href="#top">BACK TO TOP ↑</a></footer><script>
const viewer=document.querySelector('#screen-view');
for(const link of document.querySelectorAll('.screen-link'))link.addEventListener('click',event=>{if(!viewer.showModal)return;event.preventDefault();const original=link.querySelector('img'),full=viewer.querySelector('img');full.src=original.src;full.alt=original.alt;viewer.showModal()});
viewer.querySelector('button').addEventListener('click',()=>viewer.close());viewer.addEventListener('click',e=>{if(e.target===viewer)viewer.close()});

const examples='''+json.dumps(used_examples,ensure_ascii=False).replace('</',r'<\/')+''';
const exampleViewer=document.querySelector('#example-view');
const exampleImage=document.querySelector('#example-image');
let exampleTrigger=null;
for(const control of document.querySelectorAll('[data-example]'))control.addEventListener('click',()=>{
 const demo=examples[control.dataset.example];exampleTrigger=control;
 document.querySelector('#example-page').textContent=demo.page.replaceAll('-',' ').toUpperCase()+' / '+demo.control;
 document.querySelector('#example-title').textContent=demo.title;
 document.querySelector('#example-summary').textContent=demo.result;
 exampleImage.alt=demo.title+' — looping control demonstration';exampleImage.src=demo.src;
 exampleViewer.showModal();
});
exampleViewer.querySelector('.close-view').addEventListener('click',()=>exampleViewer.close());
exampleViewer.addEventListener('click',event=>{const r=exampleViewer.getBoundingClientRect();if(event.target===exampleViewer&&(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom))exampleViewer.close()});
exampleViewer.addEventListener('close',()=>{exampleImage.removeAttribute('src');if(exampleTrigger)exampleTrigger.focus({preventScroll:true})});

const fontFamilies='''+font_families+''';
const fontSelect=document.querySelector('#guide-font');
function applyFont(id){if(!fontFamilies[id])id='departure';fontSelect.value=id;document.documentElement.style.setProperty('--guide-font','"'+fontFamilies[id]+'"');try{localStorage.setItem('choo-guide-font',id)}catch{}}
let savedFont='departure';try{savedFont=localStorage.getItem('choo-guide-font')||savedFont}catch{}applyFont(savedFont);
fontSelect.addEventListener('change',()=>applyFont(fontSelect.value));
const cloneTabs=[...document.querySelectorAll('.clone-tabs [role="tab"]')];
function chooseClone(tab,focus=false){
 for(const item of cloneTabs){const selected=item===tab;item.setAttribute('aria-selected',String(selected));item.tabIndex=selected?0:-1;document.getElementById(item.getAttribute('aria-controls')).hidden=!selected}
 if(focus)tab.focus({preventScroll:true});queueContents();
}
for(const tab of cloneTabs){
 tab.addEventListener('click',()=>chooseClone(tab));
 tab.addEventListener('keydown',event=>{let index=cloneTabs.indexOf(tab);if(event.key==='ArrowRight')index=(index+1)%cloneTabs.length;else if(event.key==='ArrowLeft')index=(index+cloneTabs.length-1)%cloneTabs.length;else if(event.key==='Home')index=0;else if(event.key==='End')index=cloneTabs.length-1;else return;event.preventDefault();chooseClone(cloneTabs[index],true)});
}
const links=[...document.querySelectorAll('#contents .nav-section-link')];
const trackedSections=[...document.querySelectorAll('.basics,.page')];
let trackingFrame=0;
let activeContentsSection='',activeContentsTopic='';
function updateContents(){
 trackingFrame=0;
 const line=document.querySelector('header').getBoundingClientRect().bottom+40;
 let current=trackedSections[0];
 for(const section of trackedSections){if(section.getBoundingClientRect().top<=line)current=section;else break}
 if(scrollY>0&&Math.ceil(scrollY+innerHeight)>=document.documentElement.scrollHeight-2)current=trackedSections.at(-1);
 const sidebarMode=matchMedia('(min-width:601px)').matches;
 for(const link of links){
  const active=link.hash==='#'+current.id;
  if(active)link.setAttribute('aria-current','location');else link.removeAttribute('aria-current');
  if(link.hasAttribute('aria-controls')){
   const expanded=active&&sidebarMode;
   link.setAttribute('aria-expanded',String(expanded));
   document.getElementById(link.getAttribute('aria-controls')).hidden=!expanded;
  }
 }
 let topic='';
 for(const candidate of current.querySelectorAll('.reference-topic[open]')){
  const rect=candidate.getBoundingClientRect();
  if(rect.bottom>line&&rect.top<=line+120){topic=candidate.id;break}
 }
 for(const link of document.querySelectorAll('.nav-topics a')){
  if(link.dataset.topic===topic)link.setAttribute('aria-current','true');else link.removeAttribute('aria-current');
 }
 // Keep the active item in the independently scrolling sidebar; never move the article.
 if(sidebarMode&&(activeContentsSection!==current.id||activeContentsTopic!==topic)){
  const aside=document.getElementById('contents');
  const active=topic?aside.querySelector('[data-topic="'+topic+'"]'):aside.querySelector('.nav-section-link[aria-current]');
  if(active){
   const box=aside.getBoundingClientRect(),item=active.getBoundingClientRect();
   const inset=topic?32:8;
   if(item.top<box.top+inset||item.bottom>box.bottom-8)aside.scrollTop+=item.top-box.top-inset;
  }
 }
 activeContentsSection=current.id;activeContentsTopic=topic;
}
function queueContents(){if(!trackingFrame)trackingFrame=requestAnimationFrame(updateContents)}
window.addEventListener('scroll',queueContents,{passive:true});window.addEventListener('resize',queueContents);document.fonts.addEventListener('loadingdone',queueContents);updateContents();
</script></body></html>'''
html=html.replace('</script></body>',reference_content.JS+'</script></body>')
licenses = '\n'.join(p.name+'\n'+p.read_text() for p in sorted((ROOT/'assets/fonts').glob('*.txt')))
html=html.replace('</html>','<!-- Bundled font copyright and license notices\n'+licenses+'\n--></html>')
html='\n'.join(line.rstrip() for line in html.splitlines())+'\n'
(ROOT/'index.html').write_text(html)
for _,_,file in fonts:
 path=ROOT/'assets/fonts'/file
 mime='font/otf' if file.endswith('.otf') else 'font/ttf'
 html=html.replace('assets/fonts/'+file,'data:'+mime+';base64,'+base64.b64encode(path.read_bytes()).decode())
for demo in used_examples.values():
 path=ROOT/demo['src']
 html=html.replace(demo['src'],'data:image/gif;base64,'+base64.b64encode(path.read_bytes()).decode())
for path in (ROOT/'assets').glob('*.png'):
 html=html.replace('assets/'+path.name,'data:image/png;base64,'+base64.b64encode(path.read_bytes()).decode())
(ROOT/'Choo-Choo-User-Guide.html').write_text(html)
print('Built',len(pages),'page sections, cloning reference,',len(used_examples),'looping examples; standalone HTML and editable index.html')
