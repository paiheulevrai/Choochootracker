#!/usr/bin/env python3
"""Offline user-library conversion. Imports never enter the factory manifest.
All source validation precedes native writing; output is a new directory.
"""
import argparse,hashlib,json,subprocess,tempfile,shutil
from pathlib import Path
from four_op import parse_tfi,parse_opm,write_patch
from dx7 import sysex,display_name
from convert import parse_woplx,normalize,category

def convert_user(source,output,writer):
 if output.exists():raise ValueError('output must be a new directory')
 if source.stat().st_size>4*1024*1024:raise ValueError('input exceeds 4 MiB limit')
 data=source.read_bytes();ext=source.suffix.lower();patches=[]
 if ext=='.tfi':
  base,ops=parse_tfi(data);patches=[(25,dict(name=source.stem[:63],category='Unsorted',base=base,ops=ops))]
 elif ext=='.opm':patches=[(26,dict(e,category='Unsorted')) for e in parse_opm(data)]
 elif ext=='.syx':patches=[(24,dict(name=display_name(v),category='Unsorted',voice=v)) for v in sysex(data)]
 elif ext=='.woplx':
  for e in parse_woplx(data):
   if 'BLANK' in e['flags']:continue
   base,ops=normalize(e,32768);chip=19 if not base[1] and all(o[6]<=3 for o in ops) else 20
   patches.append((chip,dict(name=e['name'] or f"Program {e['program']}",category=category(e),base=base,ops=ops)))
 else:raise ValueError('supported inputs: .tfi (42-byte), .opm (VOPM text), .syx (original DX7), .woplx (BANK 1)')
 if not patches:raise ValueError('no supported patches')
 # A failed conversion leaves no partially published user library.
 with tempfile.TemporaryDirectory(prefix='choochoo-import-') as temp:
  stage=Path(temp);records=[]
  for number,(chip,e) in enumerate(patches):
   name=e['name']
   if any(ord(c)<32 for c in name) or len(name.encode())>63:raise ValueError('invalid preset display name')
   filename=f'user-{number:04d}-{hashlib.sha256(json.dumps(e,sort_keys=True).encode()).hexdigest()[:20]}'
   if chip in (25,26):
    e['base'][13]=32768;write_patch(stage,writer,chip,e,filename)
   else:
    ir=stage/(filename+'.chip');lines=[f'CHOOCHOO-OPL-IR 1 {chip}']
    if chip==24:lines.extend([f'- DX7 base: 1,0,100,32768,{number}',f'- DX7 name: {name}','- DX7 voice: '+','.join(map(str,e['voice']))])
    else:
     lines.extend(['- OPL base: '+','.join(map(str,e['base'])),f'- OPL name: {name}']);lines.extend(f'- OPL op{i}: '+','.join(map(str,o)) for i,o in enumerate(e['ops']))
    ir.write_text('\n'.join(lines)+'\n# END\n');subprocess.run([str(writer),str(ir),str(stage/(filename+'.cni'))],check=True,stdout=subprocess.DEVNULL);ir.unlink()
   records.append(dict(type=chip,bank_id=32768,bank=source.name[:63],category=e['category'],name=name,path=filename+'.cni'))
  (stage/'catalog.tsv').write_text('CCT-CHIP-CATALOG\t1\n'+''.join('\t'.join(str(r[k]) for k in ('type','bank_id','bank','category','name','path'))+'\n' for r in records))
  (stage/'user-import.json').write_text(json.dumps(dict(source=source.name,source_sha256=hashlib.sha256(data).hexdigest(),count=len(records),redistribution_status='user-imported; no redistribution clearance asserted',entries=records),indent=2)+'\n')
  shutil.copytree(stage,output)
 return len(patches)
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('source',type=Path);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--writer',type=Path,required=True);a=parser.parse_args()
 try:print('Imported',convert_user(a.source.resolve(),a.output.resolve(),a.writer.resolve()),'user presets')
 except (ValueError,UnicodeError,OSError,subprocess.CalledProcessError) as error:parser.exit(1,str(error)+'\n')
