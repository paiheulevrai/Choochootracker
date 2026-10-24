#!/usr/bin/env python3
"""Offline WOPLX conversion. Source bytes must match the reviewed allowlist.
Produces typed chip records for the canonical C++ instrument writer, never CNI.
No player code or player volume curves are used.
"""
import argparse, hashlib, json, re, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GM = ('Acoustic Grand|Bright Piano|Electric Grand|Honky Tonk|Electric Piano 1|Electric Piano 2|Harpsichord|Clavinet|Celesta|Glockenspiel|Music Box|Vibraphone|Marimba|Xylophone|Tubular Bells|Dulcimer|Drawbar Organ|Percussive Organ|Rock Organ|Church Organ|Reed Organ|Accordion|Harmonica|Tango Accordion|Nylon Guitar|Steel Guitar|Jazz Guitar|Clean Guitar|Muted Guitar|Overdrive Guitar|Distorted Guitar|Guitar Harmonics|Acoustic Bass|Finger Bass|Pick Bass|Fretless Bass|Slap Bass 1|Slap Bass 2|Synth Bass 1|Synth Bass 2|Violin|Viola|Cello|Contrabass|Tremolo Strings|Pizzicato Strings|Orchestral Harp|Timpani|Strings 1|Strings 2|Synth Strings 1|Synth Strings 2|Choir Aahs|Voice Oohs|Synth Voice|Orchestra Hit|Trumpet|Trombone|Tuba|Muted Trumpet|French Horn|Brass Section|Synth Brass 1|Synth Brass 2|Soprano Sax|Alto Sax|Tenor Sax|Baritone Sax|Oboe|English Horn|Bassoon|Clarinet|Piccolo|Flute|Recorder|Pan Flute|Blown Bottle|Shakuhachi|Whistle|Ocarina|Square Lead|Saw Lead|Calliope Lead|Chiff Lead|Charang Lead|Voice Lead|Fifths Lead|Bass Lead|New Age Pad|Warm Pad|Polysynth Pad|Choir Pad|Bowed Pad|Metallic Pad|Halo Pad|Sweep Pad|Rain FX|Soundtrack FX|Crystal FX|Atmosphere FX|Brightness FX|Goblins FX|Echoes FX|Sci Fi FX|Sitar|Banjo|Shamisen|Koto|Kalimba|Bagpipe|Fiddle|Shanai|Tinkle Bell|Agogo|Steel Drums|Woodblock|Taiko Drum|Melodic Tom|Synth Drum|Reverse Cymbal|Guitar Fret Noise|Breath Noise|Seashore|Bird Tweet|Telephone|Helicopter|Applause|Gunshot').split('|')
OP_FIELDS = {'AT':15,'DC':15,'ST':15,'RL':15,'WF':7,'ML':15,'TL':63,'KL':3,'VB':1,'AM':1,'EG':1,'KR':1}
ATTR_LIMITS = {'NOTE_OFF_1':(-127,127),'NOTE_OFF_2':(-127,127),'FINE_TUNE':(-128,127),'VEL_OFF':(-128,127),'DRUM_KEY':(0,127),'DUR_K_ON':(0,65535),'DUR_K_OFF':(0,65535)}
def number(s,lo,hi):
    if not re.fullmatch(r'-?\d{1,6}',s): raise ValueError('invalid integer')
    n=int(s)
    if not lo<=n<=hi: raise ValueError(f'out of range {n}')
    return n

def assignments(s,limits):
    out={}
    for part in s.split(';'):
        if not part.strip():continue
        key,value=part.strip().split('=',1)
        if key not in limits or key in out:raise ValueError('unknown/duplicate attribute '+key)
        lo,hi=limits[key];out[key]=number(value,lo,hi)
    return out

def parse_woplx(data):
    if len(data)>4*1024*1024:raise ValueError('bank too large')
    text=data.decode('utf-8-sig')
    if not text.startswith('WOPLX-BANK\n'):raise ValueError('unsupported WOPLX header/version')
    text=re.sub(r'BANK_INFO:.*?BANK_INFO_END','',text,flags=re.S)
    globals_={};bank=None;entry=None;entries=[]
    for line in text.splitlines()[1:]:
        line=line.strip()
        if not line:continue
        if len(line)>1024:raise ValueError('line too long')
        if line in ('MELODIC_BANK:','PERCUSSION_BANK:'):
            if bank is not None:raise ValueError('nested bank')
            bank={'percussion':int(line.startswith('PERCUSSION'))};entry=None
        elif line in ('MELODIC_BANK_END','PERCUSSION_BANK_END'):
            if bank is None:raise ValueError('unbalanced bank')
            bank=None;entry=None
        elif line.startswith('INSTRUMENT='):
            if bank is None or 'MIDI_BANK_MSB' not in bank or 'MIDI_BANK_LSB' not in bank:raise ValueError('missing bank identity')
            entry={**globals_,**bank,'program':number(line[11:-1],0,127),'ops':{},'attrs':{},'name':''};entries.append(entry)
            if len(entries)>8192:raise ValueError('too many entries')
        elif line.startswith('NAME='):
            if entry is None:
                if bank is None:raise ValueError('name without bank')
                bank['bank_name']=line[5:];continue
            entry['name']=line[5:]
            if len(entry['name'].encode())>63 or '\t' in entry['name']:raise ValueError('invalid name')
        elif line.startswith('FLAGS:'):
            if entry is None or 'flags' in entry:raise ValueError('invalid flags')
            flags=set(line[6:].strip('; ').split(';'))
            if flags-{'2OP','4OP','DV','FN','BLANK'}:raise ValueError('unsupported flags')
            if len(flags&{'2OP','4OP','DV'})!=1:raise ValueError('ambiguous topology')
            entry['flags']=sorted(flags)
        elif line.startswith('ATTRS:'):
            if entry is None:raise ValueError('attributes without instrument')
            entry['attrs']=assignments(line[6:],ATTR_LIMITS)
        elif line.startswith('FBCONN:'):
            if entry is None:raise ValueError('routing without instrument')
            entry['feedback']=assignments(line[7:],{'FB1':(0,7),'CONN1':(0,1),'FB2':(0,7),'CONN2':(0,1)})

        elif re.match(r'OP[0-3]:',line):
            if entry is None:raise ValueError('operator without instrument')
            i=int(line[2])
            if i in entry['ops']:raise ValueError('duplicate operator')
            op=assignments(line[4:],{k:(0,v) for k,v in OP_FIELDS.items()})
            if len(op)!=12:raise ValueError('incomplete operator')
            entry['ops'][i]=op
        elif '=' in line:
            key,value=line.split('=',1)
            if key in ('DEEP_VIBRATO','DEEP_TREMOLO','VOLUME_MODEL') and bank is None:
                if key in globals_:raise ValueError('duplicate global')
                globals_[key]=number(value,0,11 if key=='VOLUME_MODEL' else 1)
            elif key in ('MIDI_BANK_MSB','MIDI_BANK_LSB') and bank is not None and entry is None:
                if key in bank:raise ValueError('duplicate bank field')
                bank[key]=number(value,0,127)
            else:raise ValueError('unsupported field '+key)
        else:raise ValueError('unsupported line '+line)
    if bank is not None:raise ValueError('truncated bank')
    identities=set()
    for e in entries:
        if 'flags' not in e or 'feedback' not in e:raise ValueError('incomplete instrument')
        two='2OP' in e['flags']
        if set(e['ops'])!=({0,1} if two else {0,1,2,3}):raise ValueError('incomplete operators')
        if set(e['feedback'])!=({'FB1','CONN1'} if two else {'FB1','CONN1','FB2','CONN2'}):raise ValueError('incomplete routing')
        if two:
            e['ops'][2]={k:0 for k in OP_FIELDS};e['ops'][3]={k:0 for k in OP_FIELDS}
            e['feedback']['FB2']=0;e['feedback']['CONN2']=0
        ident=(e['percussion'],e['MIDI_BANK_MSB'],e['MIDI_BANK_LSB'],e['program'])
        if ident in identities:raise ValueError('duplicate program')
        identities.add(ident)
    return entries

def category(e):
    if e['percussion']:return 'Percussion'
    p=e['program']
    return ('Keys' if p<8 else 'Bell-Mallet' if p<16 else 'Organ' if p<24 else 'Pluck' if p<32 else 'Bass' if p<40 else 'Other' if p<56 else 'Brass-Reed' if p<80 else 'Lead' if p<88 else 'Pad' if p<96 else 'FX' if p<104 else 'Other' if p<112 else 'Percussion' if p<120 else 'FX')

def normalize(e,bankid):
    f=e['flags'];a=e['attrs'];r=e['feedback'];topology=1 if '4OP' in f else 2 if 'DV' in f else 0
    base=[1,topology,r['FB1'],r['FB2'],r['CONN1'],r['CONN2'],3,3,e['DEEP_VIBRATO'],e['DEEP_TREMOLO'],e['percussion'],int('FN'in f),a.get('DRUM_KEY',60),a.get('NOTE_OFF_1',0),a.get('NOTE_OFF_2',0),a.get('FINE_TUNE',0),a.get('VEL_OFF',0),0,a.get('DUR_K_ON',0),a.get('DUR_K_OFF',0),bankid,e['MIDI_BANK_MSB']*128+e['MIDI_BANK_LSB'],e['program'],e['VOLUME_MODEL']]
    # WOPLX source order is carrier1, modulator1, carrier2, modulator2.
    keys=['ML','TL','AT','DC','ST','RL','WF','KL','VB','AM','EG','KR']
    ops=[[e['ops'][i][k] for k in keys] for i in [1,0,3,2]]
    return base,ops

def convert(output,writer):
    manifest=json.loads((ROOT/'tools/chip_banks/manifest.json').read_text());output.mkdir(parents=True,exist_ok=True)
    records=[];reports=[];unique={}
    for bank in manifest['banks']:
        data=(ROOT/bank['path']).read_bytes()
        if hashlib.sha256(data).hexdigest()!=bank['sha256']:raise ValueError('source hash mismatch: '+bank['key'])
        if bank.get('license')!='MIT' or not bank.get('author') or bank.get('license_evidence')!='BANK_INFO' or b'MIT License' not in data:raise ValueError('missing provenance/license')
        entries=parse_woplx(data);counts={'bank':bank['key'],'source':len(entries),'included':0,'percussion':0,'four_operator':0,'dual_voice':0,'excluded':[]}
        for e in entries:
            if 'BLANK'in e['flags']:counts['excluded'].append({'program':e['program'],'reason':'blank'});continue
            base,ops=normalize(e,bank['id']);chip=19 if not base[1] and all(o[6]<=3 for o in ops) else 20
            name=e['name'] or (f"Percussion {e['program']:03}" if e['percussion'] else GM[e['program']])
            audible=base[:18]+[base[23]] # source identity/duration estimates do not change audio
            # Tracker uses software amplitude, with bank velocity model retained as source metadata.
            audible[16]=0;audible[-1]=0
            if base[1]==0: audible[3]=audible[5]=0;audible[7]=3;audible[14]=audible[15]=0
            fingerprint=hashlib.sha256(json.dumps([chip,audible,ops[:2] if base[1]==0 else ops],separators=(',',':')).encode()).hexdigest()
            uid=fingerprint[:20];unique.setdefault(uid,[]).append([bank['key'],base[21],base[22],base[10]])
            filename=f"{bank['key']}-{base[10]}-{base[21]:05}-{base[22]:03}"
            ir=output/(filename+'.chip')
            lines=[f'CHOOCHOO-OPL-IR 1 {chip}',f'- OPL base: '+','.join(map(str,base)),f'- OPL name: {name}']
            lines += [f'- OPL op{i}: '+','.join(map(str,op)) for i,op in enumerate(ops)]
            ir.write_text('\n'.join(lines)+'\n# END\n')
            subprocess.run([str(writer),str(ir),str(output/(filename+'.cni'))],check=True,stdout=subprocess.DEVNULL)
            ir.unlink()
            records.append({'path':filename+'.cni','type':chip,'bank':bank['name'],'bank_id':bank['id'],'category':category(e),'name':name,'supplied_name':not bool(e['name']),'uid':uid,'source_bank':base[21],'source_program':base[22],'percussion':base[10],'topology':base[1]})
            counts['included']+=1;counts['percussion']+=base[10];counts['four_operator']+=base[1]==1;counts['dual_voice']+=base[1]==2
        reports.append(counts)
    from dx7 import convert_dx7
    dx7_records,dx7_report=convert_dx7(output,writer,manifest.get('dx7_sources',[]))
    records.extend(dx7_records)
    from four_op import convert_originals
    four_records=convert_originals(output,writer)
    records.extend(four_records)
    subprocess.run([str(writer),"--builtins",str(output)],check=True)
    builtin_records=[]
    for row in (output/'builtins.tsv').read_text().splitlines()[1:]:
        t,b,bank,cat,name,path=row.split('\t')
        builtin_records.append(dict(type=int(t),bank_id=int(b),bank=bank,category=cat,name=name,path=path,source='pinned ymfm tone table' if int(t)<19 else 'ChooChoo original recipe',license='BSD-3-Clause' if int(t)<19 else 'MIT'))
    records.extend(r for r in builtin_records if r['type'] in (17,18))
    from expansion import convert_expansion
    expansion_records=convert_expansion(output,writer,builtin_records)
    records.extend(expansion_records)
    # OPLL now participates in the shared bank browser; keep one inventory row.
    builtin_records=[r for r in builtin_records if r['type'] not in (17,18)]
    (output/'builtins.tsv').write_text('CCT-CHIP-CATALOG\t1\n'+''.join('\t'.join(str(r[k]) for k in ['type','bank_id','bank','category','name','path'])+'\n' for r in builtin_records))
    (output/'builtins-manifest.json').write_text(json.dumps(dict(schema=1,count=len(builtin_records),entries=builtin_records),indent=2)+'\n')
    (output/'dx7-manifest.json').write_text(json.dumps(dx7_report,indent=2)+'\n')
    (output/'catalog.tsv').write_text('CCT-CHIP-CATALOG\t1\n'+''.join('\t'.join(str(r[k]) for k in ['type','bank_id','bank','category','name','path'])+'\n' for r in records))
    (output/'manifest.json').write_text(json.dumps({'schema':1,'sources':manifest['banks'],'conversion_version':1,'entries':records,'unique_patches':len(unique)+dx7_report['distinct_parameter_patches']+len(four_records)+30+len(expansion_records),'dx7':dx7_report,'aliases':unique,'reports':reports,'playback_policy':'Native full-velocity patch levels; tracker software gain. Source volume-model and velocity offsets retained, not applied as MIDI player curves. Duration estimates retained and never used to truncate notes. Human listening pending.'},indent=2)+'\n')
    print(json.dumps({'entries':len(records),'unique':len(unique)+dx7_report['distinct_parameter_patches']+len(four_records)+30+len(expansion_records),'banks':reports},indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);parser.add_argument('--writer',type=Path,required=True);args=parser.parse_args()
    convert(args.output.resolve(),args.writer.resolve())
