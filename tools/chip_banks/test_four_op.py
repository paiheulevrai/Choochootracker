import unittest,json
from four_op import parse_tfi,parse_opm,originals,validate
class FourOp(unittest.TestCase):
 def test_tfi_order_detune_and_bounds(self):
  data=bytes([4,5]+sum(([i+1,i,12+i,2,31,8,3,7,4,0] for i in range(4)),[]));base,ops=parse_tfi(data)
  self.assertEqual([o[0] for o in ops],[1,3,2,4]);self.assertEqual([o[1] for o in ops],[7,5,6,0])
  for bad in (data[:-1],data+b'\0',bytes([8])+data[1:],data[:3]+bytes([7])+data[4:]):
   with self.assertRaises(ValueError):parse_tfi(bad)
 def test_opm_fields_and_rejection(self):
  data=b'//MiOPMdrv sound bank Paramer Ver2002.04.22\n@:0 Original\nLFO: 10 0 0 0 0\nCH: 64 5 4 0 0 120 0\n'+b''.join(f'{name}: 31 8 2 7 4 {i*10} 1 {i+1} 0 0 0\n'.encode() for i,name in enumerate(('M1','C1','M2','C2')))
  e=parse_opm(data)[0];self.assertEqual(e['base'][3],3);self.assertEqual([o[0] for o in e['ops']],[1,2,3,4]);self.assertEqual(e['base'][11],15)
  enabled=data.replace(b'M1: 31 8 2 7 4 0 1 1 0 0 0',b'M1: 31 8 2 7 4 0 1 1 0 0 128')
  self.assertEqual(parse_opm(enabled)[0]['ops'][0][11],1)
  for bad in (data.replace(b'Ver2002.04.22',b'Ver2099.01.01'),data[:-8],data.replace(b'120 0',b'120 128'),data.replace(b'64 5',b'33 5'),data+data,data+b'UNKNOWN: 0\n',data.replace(b'31 8',b'32 8',1)):
   with self.assertRaises(ValueError):parse_opm(bad)
 def test_originals_unique_and_legal(self):
  for chip in (25,26):
   voices=originals(chip);self.assertEqual(len(voices),24);self.assertEqual(len({json.dumps([v['base'][:13],v['ops']]) for v in voices}),24)
   for v in voices:validate(chip,v['base'],v['ops'])
if __name__=='__main__':unittest.main()
