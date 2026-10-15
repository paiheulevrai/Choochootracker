"""Reconcile the pinned MIT-licensed upstream manual into the existing guide.

The page map is editorial, not a replacement for the guide's navigation.
Generated sections remain editable in reference/sections.json.
"""
from pathlib import Path
import re, json
from html import escape
import markdown2
from bs4 import BeautifulSoup

ROOT = Path(__file__).resolve().parent.parent
SHA = '902f96aa8750ea36acd69d94642a8df37b00ffdd'
source = (ROOT/'reference/upstream-manual.md').read_text()
records = []
def slug(s):
    return re.sub(r'[^a-z0-9]+', '-', s.lower()).strip('-')

def add(page, title, text, origin='upstream', note=''):
    if not text.strip(): return
    ident = page+'-ref-'+slug(title)
    if any(r['id']==ident for r in records):
        prior = next(r for r in records if r['id']==ident)
        # The second portable-library passage predates external preset import.
        # Its other paragraphs repeat the current passage; keep the current one.
        if title == 'Portable factory collections and USER presets' and 'Direct WOPL/WOPLX loading is not supported' in text:
            return
        prior['markdown'] += '\n\n'+text.strip()
        return
    records.append(dict(id=ident,page=page,title=title,markdown=text.strip(),source=origin,note=note))

def parts(text, level=3):
    result=[]; heading='Overview'; buf=[]
    for line in text.splitlines():
        m=re.match(r'^'+('#'*level)+r' (.+)$',line)
        if m and m[1]!='Controls':
            result.append((heading,'\n'.join(buf))); heading=m[1];buf=[]
        else:buf.append(line)
    result.append((heading,'\n'.join(buf)))
    return result

chapters=parts(source,2)
for chapter,body in chapters:
    num=re.match(r'(\d+)\.',chapter)
    number=int(num[1]) if num else 0
    if number==1:
        for title,text in parts(body):
            add('file-browser' if title=='Directory configuration' else 'start',
                'Installation and files' if title=='Overview' else title,text)
    elif number==2:
        for title,text in parts(body):
            if title=='Overview':
                # Keep this guide's established mapping-independent language.
                text=text[text.find('Mappings can be changed'):]
                text=re.sub(r'\*\*Settings > Key mapping\*\*','**Settings → Controls → Key mapping**',text)
                add('controls','Controllers and touch controls',text)
            elif title=='Android working folder':add('manage',title,text)
            elif title=='Navigation':add('start','Navigation and editing',text)
            elif title=='Selection and clipboard':
                a=text.find('On the Phrase screen')
                add('phrase','Selection tools: fills, mutation and arpeggiation',text[a:])
            else:add('start',title,text)
    elif number==3:
        for title,text in parts(body):
            if title=='Overview':add('start','Song structure and screen map',text)
            elif title=='Project page spacing':add('project',title,text)
            elif title=='Mixer meters':add('mixer',title,text)
            elif title=='Audio display telemetry':add('audio','Understanding the audio displays',text)
            else:add('settings',title,text)
    elif number==4:
        for title,text in parts(body):
            if title=='Song':
                before,bounce=text.split('#### Bounce',1)
                add('song','Arrangement and Live Mode',before)
                add('export','Bounce to Sample',bounce)
            elif title=='Chain':add('chain','Chain playback and navigation',text)
            elif title=='Phrase':add('phrase','Phrase editing and playback',text)
            elif title.startswith('Key jazz'):add('phrase',title,text)
    elif number==5:
        for title,text in parts(body):
            if title=='Overview':add('instrument','Shared instrument settings',text)
            elif title=='Subtractive engines':
                for sub,subtext in parts(text,4):
                    if sub=='Sampler':add('sample-editor','Sampler: regions, slices, processing and saving',subtext)
                    else:add('instrument','Shared filter and envelope' if sub=='Overview' else sub,subtext)
            else:add('instrument','SoundFont' if title=='SOUNDFONT (experimental)' else title,text)
    elif number==6:
        for title,text in parts(body):add('modulation','Sources, destinations and LFOs' if title=='Overview' else title,text)
    elif number==7:
        for title,text in parts(body):add('table','Table timing, loops and auxiliary tables' if title=='Overview' else title,text)
    elif number==8:
        for title,text in parts(body):
            if title=='Instrument Pool':add('instrument-pool','Reordering and preview',text)
            elif title=='AY Wavetable':add('ay-wavetable','Shared waves, copy/paste and LFO view',text)
    elif number==9:
        for title,text in parts(body):add('fx-browser','Reading FX values' if title=='Overview' else title,text)
    elif number==10:
        for title,text in parts(body):
            page={'Overview':'mixer','Master FX':'insert-fx','Reverb':'reverb','Ping-pong Delay':'delay'}[title]
            add(page,'Track levels, pan and sends' if title=='Overview' else title,text)
    elif number==11:
        for title,text in parts(body):
            if title=='Export location':
                first,imports=text.split('**Load** also accepts',1)
                add('export',title,first)
                add('project','Importing and exchanging songs','**Load** also accepts'+imports)
            else:add('scale' if title=='Scale / Quantize' else 'project','Project files' if title=='Overview' else title,text)
    elif number==12:add('settings','Preferences and support reports',body)
    elif number==13:
        for title,text in parts(body):add('controls' if title=='Input feels wrong' else 'audio',title,text)
    elif number==14:add('midi','MIDI devices, audition and controller mapping',body)
    elif number==15:add('settings','Credits and licensing',body)
    elif chapter=='Track insert effects':
        for title,text in parts(body):
            if title=='Overview':add('insert-fx','Modules, parameters and automation',text)
            elif title=='Handheld workload guidance for native chips':add('audio',title,text)
            elif title=='Portable factory collections and USER presets':add('file-browser',title,text)
            elif title=='DX7 FM (development)':
                dx7,editor=text.split('#### FM operator editor',1)
                editor,other=editor.split('Genesis FM (YM2612)',1)
                other,imports=other.split('For a local external bank,',1)
                add('instrument','DX7 FM',dx7)
                add('instrument','FM operator editor',editor)
                add('instrument','Genesis FM and Arcade FM','Genesis FM (YM2612)'+other)
                add('file-browser','Importing native FM presets','For a local external bank,'+imports)
            else:add('instrument',title.replace(' (development)',''),text,note='Included in the pinned upstream build. The upstream reference describes these native engines as development features; platform limits still apply.')

# Reconcile documented conflicts against the pinned upstream implementation.
for r in records:
    t=r['markdown'].replace('device personal build','target handheld build')
    t=t.replace('**SHIFT','**SELECT').replace('SHIFT +','SELECT +')
    if r['page']=='ay-wavetable':
        t=t.replace('**OPT + [LEFT/RIGHT]**: move through the wavetable list','**OPT + [UP/DOWN]**: move through the wavetable list by one')
        t=t.replace('**OPT + [UP/DOWN]**: move through the list by 16 waves','**OPT + [LEFT/RIGHT]**: move through the list by 16 waves')
        t=t.replace('**OPT + PLAY**: switch between Standard and LFO view','**Double-tap OPT**: switch between Standard and LFO view')
    t=t.replace('There is no full operator editor.','Use EDIT beside Fine ct for the native operator editor; see the FM operator editor reference below.')
    t=t.replace('Select a range, then use **EDIT + [UP/DOWN]**: rotate the phrase rows','Select rows across all columns, then use **EDIT + [UP/DOWN]**: rotate the phrase rows')
    if r['title']=='DX7 FM':
        t=t.replace('**`instruments/banks/dx7/`** beside the existing instrument library.','**`instruments/USER/dx7/`**. The older `instruments/banks/dx7/` location remains accessible for existing libraries.')
    r['markdown']=t

# Removed Auto Mix is deliberately excluded at the owner's request.

add('instrument','Sideloaded presets',
    'Put sideloaded presets in **`instruments/USER/<engine>/`** — for example, `USER/dx7/`, `USER/sid/` or `USER/soundfont/` inside the instruments folder. Keep collection subfolders; supported ZIP packs can stay zipped. Included collections live separately in `instruments/FACTORY/`.\n\n'
    '**Web — import a whole USER collection:** ZIP your USER folder, keeping archive paths such as `USER/dx7/...` or `instruments/USER/dx7/...`. Start the tracker, click **Import USER ZIP**, and select the ZIP. Supported engine folders and their subfolders are imported together; existing files are kept. **Import bank folder** is specifically for SoundFont banks, rather than a whole USER collection. Browser storage limits apply; keep the original ZIP as a backup.\n\n'
    '1. Choose the matching **Type** on the Instrument screen.\n'
    '2. For native chip engines, choose **Bank → USER**, then open **Preset** to browse your folders, banks or ZIP packs. Press **EDIT** to load a sound; **EDIT + PLAY** auditions it where supported.\n'
    '3. Save the project to keep the chosen instrument.\n\n'
    '**SoundFont** uses **Rescan**, then **Preset**, to find and choose sounds. Keep its external sample files and relative folder paths intact.\n\n'
    'For engine folders, supported formats and import details, see [Portable factory collections and USER presets](#file-browser-ref-portable-factory-collections-and-user-presets).')
first_instrument=next(i for i,r in enumerate(records) if r['page']=='instrument')
records.insert(first_instrument,records.pop())

add('sampler','Sample files and detailed editing',
    'Load a PCM WAV, then use EDIT beside its name for regions, slicing, processing and saving. See [the complete Sample editor reference](#sample-editor-ref-sampler-regions-slices-processing-and-saving). A project stores the sample path: keep the WAV with your library when moving the project. Processing and saving the project are separate operations.')
add('export','Exchange formats',
    'For MIDI, M8 and LSDj import/export, including template requirements and features that do not transfer, see [Importing and exchanging songs](#project-ref-importing-and-exchanging-songs).')
add('scale','Scale and chord automation',
    'See [Scale automation](#fx-browser-ref-scale-automation) for SCL and [Chords](#fx-browser-ref-chords) for CRD. Note Lock changes note entry; it does not rewrite earlier notes.')
add('manage','Moving a project safely',
    'Save the project and save any edited WAVs before moving them. Keep externally referenced samples and SoundFont/SFZ/EXS sample libraries with their relative paths. Download a .cct backup from the browser and keep a separate copy of your USER library; browser storage is not a portable backup. For supported archive layouts, see [Factory and USER presets](#file-browser-ref-portable-factory-collections-and-user-presets).')

# Normalise source links into this guide; documentation attachments remain pinned upstream links.
anchors={}
for r in records:
    anchors.setdefault(slug(r['title']),r['id'])
    for h in re.findall(r'^#{3,6} (.+)$',r['markdown'],re.M):anchors.setdefault(slug(h),r['id']+'-'+slug(h))
anchors.update({'5-instruments':'instrument','9-tracker-fx':'fx-browser','12-settings':'settings','14-midi':'midi','midi-out':'instrument-ref-midi-out','key-jazz-desktop-only':'phrase-ref-key-jazz-desktop-only','soundfont-experimental':'instrument-ref-soundfont'})
for r in records:
    html=markdown2.markdown(r['markdown'],extras=['tables','fenced-code-blocks','cuddled-lists'])
    soup=BeautifulSoup(html,'html.parser')
    for h in soup.find_all(re.compile('^h[1-6]$')):
        h['id']=r['id']+'-'+slug(h.get_text());h.name='h4'
    for a in soup.find_all('a',href=True):
        href=a['href']
        if href.startswith('#'):
            a['href']='#'+anchors.get(href[1:],href[1:])
        elif not re.match(r'^https?://',href):a['href']=f'https://github.com/paiheulevrai/Choochootracker/blob/{SHA}/docs/'+href
    for table in soup.find_all('table'):
        for th in table.find_all('th'):th['scope']='col'
        columns=len(table.find('tr').find_all(['th','td'],recursive=False))
        wrapper=soup.new_tag('div',attrs={'class':'reference-table','data-columns':str(columns),'tabindex':'0','role':'region','aria-label':r['title']+' table'})
        table.wrap(wrapper)
    r['html']=str(soup)

(ROOT/'reference/sections.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')
(ROOT/'reference/coverage.json').write_text(json.dumps({'upstream_revision':SHA,'source_chapters':[c for c,b in chapters if c!='Overview'],'sections':len(records),'destinations':{p:sum(r['page']==p for r in records) for p in sorted(set(r['page'] for r in records))},'reconciliations':['AY wave navigation: Up/Down by 1; Left/Right by 16; double OPT toggles view, verified in screen_ay_wavetable.cpp.','OPLL/VRC7 operator editor exists in screen_fm_edit.cpp; removed obsolete no-editor assertion.','Auto Mix absent from pinned current source.', 'Removed duplicate obsolete USER paragraph; WOPL/WOPLX support verified in chipnomad_lib/external_presets.cpp.','Personal-build wording changed to target-handheld wording; no personal source or assets used.']},indent=2)+'\n')
print('Prepared',len(records),'reference sections across',len(set(r['page'] for r in records)),'existing pages')
