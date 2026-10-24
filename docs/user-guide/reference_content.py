"""Detailed references in the guide's existing screen sections."""
from pathlib import Path
from html import escape
import json

ROOT=Path(__file__).resolve().parent
SECTIONS=json.loads((ROOT/'reference/sections.json').read_text())

MEDIA={
 'Arrangement and Live Mode':['song-live'], 'Bounce to Sample':['bounce'],
 'Sampler: regions, slices, processing and saving':['sample-slices'],
 'Modules, parameters and automation':['beads-insert'],
 'AY Classic, AY Plus and AY Sample':['engine-ay-classic','engine-ay-plus','engine-ay-sample'],
 'Braids':['engine-braids'], 'aChChid':['engine-achchid'], 'Bogie':['engine-bogie'],
 'Sintered':['engine-sintered'], 'MME':['engine-mme'], 'MIDI Out':['engine-midi-out'],
 'Plaits and Plaits-Alt':['engine-plaits','engine-plaits-alt'], '2xSCWF':['engine-scwf'],
 'BYOWTBL':['engine-byowtbl'], 'SoundFont':['engine-soundfont','soundfont-editor'],
 'Native OPLL and VRC7 instruments':['engine-opll','engine-vrc7'],
 'AdLib / OPL2 and OPL3':['engine-opl2','engine-opl3'],
 'Sega PSG and Game Boy native instruments':['engine-sega','engine-gb-pulse','engine-gb-noise'],
 'DX7 FM':['engine-dx7'], 'Genesis FM and Arcade FM':['engine-genesis','engine-arcade'],
 'FM operator editor':['fm-editor'],
 'SID instruments and Phrase FX':['engine-sid','sid-editor'],
 'Preferences and support reports':['graphics','synth-settings','mixer-settings'],
 'MIDI devices, audition and controller mapping':['midi-channels','midi-cc'],
}
CAPTIONS={
 'song-live':'Song in Live Mode','bounce':'Bounce a selected chain range',
 'sample-slices':'Sample editor with equal slices','beads-insert':'Beads in the first track insert slot',
 'engine-ay-classic':'AY Classic','engine-ay-plus':'AY Plus','engine-ay-sample':'AY Sample',
 'engine-achchid':'aChChid','engine-scwf':'2xSCWF','engine-byowtbl':'BYOWTBL',
 'engine-mme':'MME','engine-opll':'OPLL','engine-vrc7':'VRC7','engine-opl2':'AdLib / OPL2',
 'engine-opl3':'OPL3','engine-dx7':'DX7 FM','engine-sid':'SID / C64',
 'engine-gb-pulse':'Game Boy Pulse','engine-gb-noise':'Game Boy Noise',
 'engine-soundfont':'SoundFont — before loading a library',
 'soundfont-editor':'SoundFont Edit — Bank defaults',
 'fm-editor':'Genesis FM operator editor','sid-editor':'SID voice editor',
 'graphics':'Graphics preferences','synth-settings':'Synth preferences',
 'mixer-settings':'Mixer preferences','midi-channels':'MIDI channel mapping','midi-cc':'MIDI CC mapping',
}

def update_pages(pages):
    byid={p['id']:p for p in pages}
    p=byid['song'];p['note']=p['note'].split(' The copy/paste examples')[0]
    p=byid['reverb']
    p['description']='The shared reverb adds space around the mix. Set each track’s send in Mixer, then choose Clouds or Beads and shape the common reverb here.'
    p['fields']=[('Algorithm','Clouds or Beads; each retains its own controls.'),('Return','How much wet reverb reaches the master mix.'),('Time','Length of the reverb tail.'),('Damping / Tone','Clouds high-frequency damping or Beads feedback-filter tone.'),('LP / HP filters','Clouds provides low-pass and high-pass filters; Beads provides a pre-reverb high-pass filter.')]
    for row in json.loads((ROOT/'reference/new-controls.json').read_text()):
        byid[row['page']]['controls'].append((row['control'],row['description']))

def render(page):
    items=[r for r in SECTIONS if r['page']==page]
    if not items:return ''
    out=['<div class="reference"><h3>In depth</h3><p class="reference-intro">Open a topic for detailed controls, values and workflows.</p>']
    for r in items:
        out.append('<details class="reference-topic" id="'+r['id']+'"><summary>'+escape(r['title'])+'</summary><div class="reference-body">')
        if r['note']:out.append('<p class="note">'+escape(r['note'])+'</p>')
        media=MEDIA.get(r['title'],[])
        if media:
            out.append('<div class="reference-gallery">')
            for image in media:
                label=CAPTIONS.get(image,image.removeprefix('engine-').replace('-',' ').title())
                src='assets/'+image+'.png'
                out.append('<figure><a class="screen-link" href="'+src+'" aria-label="Open '+escape(label)+' screenshot"><img src="'+src+'" width="640" height="480" alt="'+escape(label)+' in upstream ChooChooTracker" loading="lazy"></a><figcaption>'+escape(label)+'</figcaption></figure>')
            out.append('</div>')
        out.append(r['html'])
        if r['title']=='Credits and licensing':
            out.append('<details><summary>Upstream MIT license</summary><pre class="license">'+escape((ROOT/'reference/LICENSE-upstream.txt').read_text())+'</pre></details>')
        out.append('</div></details>')
    out.append('</div>')
    return ''.join(out)

NAV_LABELS={
 'Shared instrument settings':'Shared settings',
 'AY Classic, AY Plus and AY Sample':'AY instruments',
 'Native OPLL and VRC7 instruments':'OPLL / VRC7',
 'AdLib / OPL2 and OPL3':'AdLib / OPL2 / OPL3',
 'Sega PSG and Game Boy native instruments':'Sega PSG / Game Boy',
 'SID instruments and Phrase FX':'SID instruments',
 'Selection tools: fills, mutation and arpeggiation':'Selection tools',
 'Sampler: regions, slices, processing and saving':'Regions, slices and saving',
 'Portable factory collections and USER presets':'Factory and USER presets',
 'MIDI devices, audition and controller mapping':'Devices and mapping',
 'Shared waves, copy/paste and LFO view':'Waves and LFO view',
}

def navigation(page,title):
    items=[r for r in SECTIONS if r['page']==page]
    label='<a class="nav-section-link" href="#'+page+'"'
    if items:label+=' aria-expanded="false" aria-controls="contents-'+page+'"'
    label+='>'+escape(title)+'</a>'
    if not items:return label
    children=''.join('<li><a href="#'+r['id']+'" data-topic="'+r['id']+'">'+escape(NAV_LABELS.get(r['title'],r['title']))+'</a></li>' for r in items)
    return '<div class="nav-section">'+label+'<ul class="nav-topics" id="contents-'+page+'" data-section="'+page+'" hidden>'+children+'</ul></div>'

CSS='''
#contents{overflow-anchor:none}
.nav-topics{list-style:none;padding:0;margin:3px 0 8px 7px;border-left:1px solid var(--line);animation:nav-unroll .16s ease-out}
.nav-topics[hidden]{display:none}
.nav-group .nav-topics a,.nav-start .nav-topics a{font-size:10px;line-height:1.45;padding:3px 4px 3px 8px;color:var(--muted);border-left:2px solid transparent}
.nav-topics a[aria-current="true"]{color:var(--accent);border-left-color:var(--accent);background:#dce1d5}
@media(min-width:601px){.nav-section>.nav-section-link[aria-current="location"]{position:sticky;top:0;z-index:1;background:#dce1d5}}
@keyframes nav-unroll{from{opacity:0;transform:translateY(-3px)}to{opacity:1;transform:translateY(0)}}
@media(prefers-reduced-motion:reduce){.nav-topics{animation:none}}
@media(max-width:600px){.nav-topics{display:none}}
.reference{margin-top:22px}.reference-intro{font-size:11px;color:var(--muted);margin:4px 0 10px}
.reference-topic{border-top:1px solid var(--line);scroll-margin-top:80px}
.reference-topic>summary{cursor:pointer;padding:10px 0;font-size:13px;color:var(--accent)}
.reference-topic[open]>summary{border-bottom:1px solid var(--line)}
.reference-body{padding:4px 0 16px;font-size:12px;line-height:1.6;overflow-wrap:anywhere}
.reference-body h4{font-size:13px;font-weight:400;margin:18px 0 6px;scroll-margin-top:80px}
.reference-body p{margin:10px 0}.reference-body li{margin:5px 0}.reference-body ul,.reference-body ol{padding-left:24px}
.reference-body code{background:#dfe1d7;padding:1px 3px;font:inherit}
.reference-body pre{white-space:pre-wrap;overflow-wrap:anywhere;font:inherit;padding:12px;border:1px solid var(--line)}
.reference-table{overflow-x:auto;margin:12px 0}.reference-table table{margin:0;min-width:440px}
.reference-table table th{width:auto}.reference-table table td{min-width:70px}.reference-table table td:last-child{min-width:180px}
.reference-table[data-columns="2"] table,.reference-table[data-columns="3"] table{min-width:0;table-layout:fixed}
.reference-table[data-columns="2"] table td,.reference-table[data-columns="3"] table td{min-width:0}
.reference-table[data-columns="3"] th:first-child{width:16%}.reference-table[data-columns="3"] th:nth-child(2){width:20%}
.reference-table[data-columns="2"] th:first-child{width:35%}
.reference-gallery{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px;margin:12px 0 18px;max-width:976px}
.reference-gallery figure:only-child{grid-column:1/-1;max-width:480px}.reference-gallery figcaption{padding-top:4px}
@media(max-width:820px){.reference-gallery{grid-template-columns:1fr}.reference-gallery figure{max-width:480px}}
@media print{.reference-topic{break-inside:auto}.reference-gallery{grid-template-columns:1fr 1fr}.reference-body{font-size:9px}.reference-table{overflow:visible}.reference-table table{min-width:0}.reference-table table td{min-width:0}}
'''

JS='''
function revealReference(){
 const id=decodeURIComponent(location.hash.slice(1));const target=document.getElementById(id);
 if(!target)return;
 for(let el=target;el;el=el.parentElement)if(el.tagName==='DETAILS')el.open=true;
 if(target.closest('.reference'))requestAnimationFrame(()=>target.scrollIntoView({block:'start'}));
}
window.addEventListener('hashchange',revealReference);revealReference();
document.querySelectorAll('.nav-topics a').forEach(link=>link.addEventListener('click',()=>{if(location.hash===link.hash)revealReference()}));
document.querySelectorAll('.reference-topic').forEach(el=>el.addEventListener('toggle',queueContents));
let printDetails=[];
window.addEventListener('beforeprint',()=>{printDetails=[...document.querySelectorAll('.reference details')].filter(el=>!el.open);printDetails.forEach(el=>el.open=true)});
window.addEventListener('afterprint',()=>{printDetails.forEach(el=>el.open=false);printDetails=[]});
'''
