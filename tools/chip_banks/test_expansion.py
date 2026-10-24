import unittest,struct
from pathlib import Path
from expansion import parse_wopn,tone_tables,ORIGINALS

class Expansion(unittest.TestCase):
 def test_wopn_register_order_and_features(self):
  header=b'WOPN2-B2NK\0'+struct.pack('<H',2)+struct.pack('>HHB',1,0,9)
  entry=bytearray(69);entry[:4]=b'Test';entry[35]=4|(3<<3);entry[36]=0x23
  for n in range(4):entry[37+n*7:44+n*7]=bytes([0x20+n,10+n,0x9f,0x8c,7,0x56,8])
  data=header+bytes(34)+entry+bytes(69*127)
  entries,excluded=parse_wopn(data);self.assertEqual(len(entries),1)
  e=entries[0];self.assertEqual([o[0] for o in e['ops']],[0,2,1,3]);self.assertEqual([o[1] for o in e['ops']],[2]*4)
  self.assertEqual(e['base'][1:8],[4,3,3,2,3,1,1]);self.assertEqual(e['ops'][0],[0,2,10,2,31,12,7,6,5,8,0,1])
  self.assertEqual(len(excluded),127)
  for bad in (data[:-1],data+b'\0',data[:11]+b'\3\0'+data[13:],data[:17]+b'\x10'+data[18:]):
   with self.assertRaises(ValueError):parse_wopn(bad)
 def test_pinned_sources_and_deliberate_programs(self):
  root=Path(__file__).parent/'sources/expansion'
  tones=tone_tables((root/'emu2413.c').read_bytes());self.assertEqual([len(t) for t in tones],[15,15,15])
  entries,excluded=parse_wopn((root/'16bit-station.wopn').read_bytes());self.assertEqual(len(entries),49)
  self.assertTrue(any(e['reason']=='note offset' for e in excluded))
  self.assertEqual(len({tuple(row[2]) for row in ORIGINALS}),len(ORIGINALS))
  for _,_,patch in ORIGINALS:self.assertEqual(len(patch),8)

if __name__=='__main__':unittest.main()
