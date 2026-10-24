"""Convert prepared MIT SID recipes into self-contained native CNI programs."""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'tracker/packaging/common/instruments/FACTORY'
WAVE={'TRIANGLE':1,'SAWTOOTH':2,'PULSE':4,'NOISE':8}
MODE={'off':0,'lowpass':1,'bandpass':2,'highpass':4}
def parameters(p):
 v=[0]*25
 v[1]=WAVE[p['waveform']]
 v[2:6]=[p.get(k,d) for k,d in zip(['attack','decay','sustain','release'],[0,4,0,0])]
 v[6]=(p.get('pw_hi',4)&15)<<8;v[7]=p.get('filter_cutoff',0x90)*8;v[8]=p.get('filter_resonance',15);v[9]=MODE[p.get('filter_mode','off')]
 v[12]=2;v[13]=50
 v[14]=0 if p.get('original') or p.get('loop') else p.get('duration_frames',10)
 v[15]=0 if p.get('note_relative') else (p.get('freq_hi',0x10)<<8)|p.get('freq_lo',0)
 v[16]=(p.get('sweep_target_hi',0)<<8)|p.get('sweep_target_lo',0)
 v[17]=p.get('sweep_frames') or p.get('duration_frames',10);v[18]=int(p.get('sweep_type','exponential')=='exponential')
 if 'pulse_sweep' in p:v[19]=p['pulse_sweep']['depth'];v[20]=round(p['pulse_sweep']['rate']*10)
 v[21]=p.get('vibrato_depth',0);v[22]=round(p.get('vibrato_rate',0)*10)
 v[23]=p['filter_cutoff_sweep']*8 if p.get('filter_cutoff_sweep') else 65535
 if 'arpeggio' in p:v[24]=[[0,4,7],[0,3,7],[0,7,12],[0,5,9]].index(p['arpeggio'])+1
 return v

def main():
 lines=(OUT/'catalog.tsv').read_text().splitlines();lines=[l for l in lines if not l.startswith('27\t')]
 for source,bank,title in [('originals',600,'ChooChoo SID Originals'),('sidkit-normalized',601,'SIDkit Effects')]:
  patches=json.loads((ROOT/'tools/sid_prep'/f'{source}.json').read_text())
  for idx,p in enumerate(patches):
   name=p['name'].replace('_',' ').title() if bank==601 else p['name'];filename=f'sid-{bank}-{idx:03d}.cni'
   category='Effects' if bank==601 else 'Bass' if 'Bass' in name else 'Pads' if 'Pad' in name else 'Percussion' if 'Drum' in name else 'Arpeggios' if 'Arp' in name or 'Chord' in name else 'Leads'
   text='# ChipNomad Instrument 7.0\n\n### Instrument 0\n\n'
   text+=f'- Name: {name[:24]}\n- Type: 27\n- Table speed: 1\n- Volume: 255\n- Transpose: 1\n- Modulation:\n'
   text+=''.join(f'- Mod{m+1}: 0,0,0,0,0,0,0,0\n' for m in range(4))
   text+=f'- Chip data:\n- SID identity: 1,{bank}\n- SID name: {name}\n- SID parameters: '+','.join(map(str,parameters(p)))+'\n'
   text += "\n### Table 0 (Retrig: Inst)\n\n```\n" + "~ 00 -- --- 00 --- 00 --- 00 --- 00\n" * 16 + "```\n"
   (OUT/filename).write_text(text)
   lines.append(f'27\t{bank}\t{title}\t{category}\t{name}\t{filename}')
 (OUT/'catalog.tsv').write_text('\n'.join(lines)+'\n')
 print('Packaged 56 SID programs in two named banks')
if __name__=='__main__':main()
