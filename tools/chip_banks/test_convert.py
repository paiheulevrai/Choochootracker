import importlib.util
import unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('convert',Path(__file__).with_name('convert.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Banks(unittest.TestCase):
    def setUp(self):self.data=(Path(__file__).parent/'sources/fatman-2op.woplx').read_bytes()
    def test_counts_and_determinism(self):
        for name,count in [('fatman-2op',181),('fatman-4op',181),('dmxopl3',335)]:
            data=(Path(__file__).parent/'sources'/f'{name}.woplx').read_bytes();a=m.parse_woplx(data);self.assertEqual(len(a),count);self.assertEqual(a,m.parse_woplx(data))
    def test_truncation_and_bad_ranges(self):
        for data in [self.data[:-30],self.data.replace(b'AT=15',b'AT=16',1),self.data.replace(b'2OP;',b'UNKNOWN;',1),self.data.replace(b'WOPLX-BANK',b'WOPLX-BANK-2',1)]:
            with self.assertRaises(ValueError):m.parse_woplx(data)
    def test_operator_order(self):
        e=m.parse_woplx(self.data)[0];base,ops=m.normalize(e,1)
        self.assertEqual(ops[0][1],e['ops'][1]['TL']);self.assertEqual(ops[1][1],e['ops'][0]['TL']);self.assertEqual(base[1],0)
    def test_four_and_dual_distinct(self):
        a=m.parse_woplx((Path(__file__).parent/'sources/fatman-4op.woplx').read_bytes())[0]
        b=m.parse_woplx((Path(__file__).parent/'sources/dmxopl3.woplx').read_bytes())[0]
        self.assertEqual(m.normalize(a,2)[0][1],1);self.assertEqual(m.normalize(b,3)[0][1],2)
if __name__=='__main__':unittest.main()
