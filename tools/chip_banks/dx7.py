"""Bounded data-only DX7 ingestion; never evaluates downloaded JavaScript."""
import ast
import hashlib
import json
import re
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
MAX_OP=[99]*11+[3,3,7,3,7,99,1,31,99,14]
MAX_GLOBAL=[99]*8+[31,7,1,99,99,99,99,1,5,7,48]+[127]*10

def validate(v):
    if len(v)!=155 or any(type(x)is not int or x<0 or x>m for x,m in zip(v,MAX_OP*6+MAX_GLOBAL)):
        raise ValueError('invalid DX7 VCED parameter')
    return v

def unpack(b):
    if len(b)!=128 or any(x>127 for x in b):raise ValueError('invalid packed DX7 voice')
    v=[]
    for op in range(6):
        s=b[op*17:op*17+17]
        if s[11]>15 or s[12]>119 or s[13]>31 or s[15]>63:raise ValueError('reserved packed DX7 bits')
        v+=list(s[:11])+[s[11]&3,s[11]>>2,s[12]&7,s[13]&3,s[13]>>2,s[14],s[15]&1,s[15]>>1,s[16],s[12]>>3]
    if b[111]>15:raise ValueError('reserved DX7 global bits')
    v+=list(b[102:111])+[b[111]&7,b[111]>>3]+list(b[112:116])+[b[116]&1,(b[116]>>1)&7,b[116]>>4]+list(b[117:128])
    return validate(v)

def sysex(data):
    if not 0<len(data)<=1048576:raise ValueError('DX7 file size limit')
    voices=[];pos=0
    while pos<len(data):
        m=data[pos:]
        if len(m)<8 or m[:2]!=b'\xf0\x43' or m[2]>15:raise ValueError('invalid Yamaha header')
        n={0:155,9:4096}.get(m[3],0)
        if not n:raise ValueError('unsupported Yamaha message')
        if m[4]>127 or m[5]>127 or m[4]*128+m[5]!=n or len(m)<n+8 or m[n+7]!=247:raise ValueError('invalid DX7 framing')
        if any(x>127 for x in m[6:n+7]) or sum(m[6:n+7])%128:raise ValueError('invalid DX7 checksum/data')
        voices.extend([validate(list(m[6:161]))] if n==155 else [unpack(m[6+i:6+i+128]) for i in range(0,4096,128)])
        if len(voices)>4096:raise ValueError('DX7 voice limit')
        pos+=n+8
    return voices

def default_voice(name):
    op=[99]*4+[99,99,99,0,39,0,0,0,0,0,0,0,0,0,1,0,7]
    return op*6+[99]*4+[50]*4+[0,0,0,35,0,0,0,0,0,0,24]+list(name[:10].ljust(10).encode('ascii'))

class LiteralParser:
    """Allowlisted literal grammar only: numbers/strings/bools/arrays/objects."""
    def __init__(self,text):
        self.tokens=re.findall(r"'(?:[^'\\]|\\.)*'|\d+|[A-Za-z_][A-Za-z_0-9]*|[{}\[\]():,]|\S",text);self.pos=0
    def pop(self,expected=None):
        if self.pos>=len(self.tokens):raise ValueError('truncated patch literal')
        t=self.tokens[self.pos];self.pos+=1
        if expected is not None and t!=expected:raise ValueError('unexpected token '+t)
        return t
    def value(self,depth=0):
        if depth>6:raise ValueError('nested literal limit')
        t=self.pop()
        if t.isdecimal():return int(t)
        if t in ('true','false'):return int(t=='true')
        if t.startswith("'"):return ast.literal_eval(t)
        if t in ('{','['):
            end='}' if t=='{' else ']';out={} if t=='{' else []
            while self.tokens[self.pos]!=end:
                if t=='{':
                    k=self.pop();self.pop(':')
                    if k in out:raise ValueError('duplicate literal key')
                    out[k]=self.value(depth+1)
                else:out.append(self.value(depth+1))
                if self.tokens[self.pos]!=end:self.pop(',')
            self.pop(end);return out
        raise ValueError('not a data literal: '+t)

def opendx7(data):
    if len(data)>1048576:raise ValueError('source limit')
    text=data.decode();section=text.split('export function generateFactoryPatches() {',1)[1]
    section=re.sub(r'//[^\n]*','',section)
    # Exact source pin is verified by caller. Only mkPatch literal calls are read.
    mappings={'out':16,'coarse':18,'fine':19,'detune':20,'vel':15,'ams':14,'mode':17,'krs':13,'bp':8,'ld':9,'rd':10,'lc':11,'rc':12}
    globals_={'lfoSpeed':137,'lfoDelay':138,'lfoPitchModDepth':139,'lfoAmpModDepth':140,'lfoSync':141,'lfoWave':142,'pitchModSens':143,'transpose':144,'oscSync':136}
    result=[]
    for chunk in section.split('mkPatch(')[1:]:
        p=LiteralParser(chunk);name=p.value();p.pop(',');algo=p.value();p.pop(',');feedback=p.value();p.pop(',');ops=p.value();extra={}
        if p.tokens[p.pos]==',':p.pop(',');extra=p.value()
        p.pop(')');v=default_voice(name);v[134]=algo;v[135]=feedback
        for idx,settings in ops.items():
            if not idx.isdecimal() or not 0<=int(idx)<6:raise ValueError('operator index')
            off=int(idx)*21
            for k,val in settings.items():
                if k in ('r','l'):
                    if len(val)!=4:raise ValueError('envelope length')
                    start=off+(4 if k=='l' else 0);v[start:start+4]=val
                elif k in mappings:v[off+mappings[k]]=val
                else:raise ValueError('unknown operator attribute '+k)
        for k,val in extra.items():v[globals_[k]]=val
        result.append((name,validate(v)))
    if len(result)!=31:raise ValueError('expected pinned 31 musical patches (INIT excluded)')
    return result

def display_name(v):return ''.join(chr(c) if 32<=c<127 else ' ' for c in v[145:]).strip() or 'Unnamed DX7'

def category(name):
    # Reviewed descriptive OpenDX7 names only; ambiguous user/YSE names stay Unsorted.
    explicit={'Elec Piano 1':'Electric Piano','Elec Piano 2':'Electric Piano','FM Bass':'Bass','Synth Bass':'Bass','Bright Bell':'Bell-Mallet','Tubular Bell':'Bell-Mallet','FM Brass':'Brass-Wind','Soft Brass':'Brass-Wind','String Pad':'Pad','Warm Strings':'Pad','Drawbar Organ':'Organ','Perc Organ':'Organ','Pluck Key':'Keys','Mallet Hit':'Bell-Mallet','Soft Mallet':'Bell-Mallet','Tremolo Bell':'Bell-Mallet','Flute Tone':'Brass-Wind','Reed Pipe':'Brass-Wind','Synth Lead':'Lead','Bright Lead':'Lead','Glass Pad':'Pad','Shimmer Pad':'Pad','Harpsichord':'Keys','Clavinet':'Keys','Metallic Hit':'Percussion','Choir Pad':'Pad','Deep Sub Bass':'Bass','Pluck Bass':'Bass','Crystal Keys':'Keys','Warm Pad':'Pad','Sync Lead':'Lead'}
    return explicit.get(name,'Unsorted')

def convert_dx7(output,writer,sources):
    records=[];reports=[];unique={};full=set();parsed=0
    for source in sources:
        raw=(ROOT/source['path']).read_bytes()
        if hashlib.sha256(raw).hexdigest()!=source['sha256']:raise ValueError('DX7 source hash mismatch')
        if source['status']!='approved' or source['license'] not in ('MIT','CC0-1.0'):raise ValueError('DX7 source not approved')
        if source['format']=='opendx7-literals':entries=[(n,v,category(n)) for n,v in opendx7(raw)]
        elif source['format']=='syx':entries=[(display_name(v),v,'Unsorted') for v in sysex(raw)]
        elif source['format']=='native-json':entries=[(e['name'],validate(e['voice']),e['category']) for e in json.loads(raw)['voices']]
        else:raise ValueError('unknown DX7 source format')
        included=0
        for program,(name,v,cat) in enumerate(entries):
            parsed+=1;full.add(hashlib.sha256(bytes(v)).hexdigest())
            fingerprint=hashlib.sha256(bytes(v[:145])+bytes([100,0])).hexdigest()
            alias={'source':source['key'],'program':program,'name':name,'full_voice_sha256':hashlib.sha256(bytes(v)).hexdigest()}
            if fingerprint in unique:unique[fingerprint].append(alias);continue
            unique[fingerprint]=[alias];filename='dx7-'+fingerprint[:24];ir=output/(filename+'.chip')
            ir.write_text('CHOOCHOO-OPL-IR 1 24\n- DX7 base: 1,0,100,'+str(source['id'])+','+str(program)+'\n- DX7 name: '+name+'\n- DX7 voice: '+','.join(map(str,v))+'\n# END\n')
            subprocess.run([str(writer),str(ir),str(output/(filename+'.cni'))],check=True,stdout=subprocess.DEVNULL);ir.unlink()
            records.append(dict(path=filename+'.cni',type=24,bank=source['name'],bank_id=source['id'],category=cat,name=name,uid=fingerprint,source_program=program,percussion=int(cat=='Percussion'),topology=6));included+=1
        reports.append(dict(bank=source['key'],parsed=len(entries),bundled=included))
    return records,dict(sources=sources,parsed_entries=parsed,distinct_full_records=len(full),distinct_parameter_patches=len(unique),bundled=len(records),aliases=unique,reports=reports,playable_validation='Converter verifies native reload; audio validation is recorded in docs/chip-instruments-report.md',large_library_target=1000,large_library_target_met=len(unique)>=1000)

def archive_data(data):
    """Inspect bounded ZIP data without extracting paths or running any member."""
    import io,zipfile
    from pathlib import PurePosixPath
    if len(data)>32*1024*1024:raise ValueError('archive size limit')
    result={};total=0
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        if len(archive.infolist())>4096:raise ValueError('archive entry limit')
        for entry in archive.infolist():
            path=PurePosixPath(entry.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in entry.filename or ':' in entry.filename:raise ValueError('unsafe archive path')
            if (entry.external_attr>>16)&0o170000==0o120000:raise ValueError('archive symlink')
            total+=entry.file_size
            if entry.file_size>1048576 or total>32*1024*1024:raise ValueError('archive expansion limit')
            if entry.flag_bits&1:raise ValueError('encrypted archive unsupported')
            if not entry.is_dir() and path.suffix.lower()=='.syx':
                if entry.filename in result:raise ValueError('duplicate archive name')
                result[entry.filename]=archive.read(entry)
    return result
