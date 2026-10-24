import hashlib
import io
import json
import unittest
import zipfile
from pathlib import Path
import dx7
import dx7_originals
class DX7Banks(unittest.TestCase):
    def test_accepted_inventory_and_aliases(self):
        root=Path(__file__).parent/'sources/dx7'
        open_=dx7.opendx7((root/'opendx7-patch.js').read_bytes())
        yse=dx7.sysex((root/'yse-originals.syx').read_bytes())
        own=dx7_originals.generate()['voices']
        self.assertEqual(len(open_),31);self.assertEqual(len(yse),32);self.assertEqual(len(own),32)
        self.assertEqual(len({bytes(v[:145]) for v in yse}),4)
        all_=[v for n,v in open_]+yse+[p['voice'] for p in own]
        self.assertEqual(len({bytes(v[:145]) for v in all_}),67)
        self.assertEqual(json.loads((root/'choochoo-originals.json').read_text()),dx7_originals.generate())
    def test_name_excluded_but_sound_changes_retained(self):
        a=dx7.default_voice('One');b=dx7.default_voice('Two');self.assertEqual(a[:145],b[:145]);self.assertNotEqual(a,b)
        b[134]=31;self.assertNotEqual(hashlib.sha256(bytes(a[:145])).digest(),hashlib.sha256(bytes(b[:145])).digest())
    def test_literal_parser_cannot_execute(self):
        for text in ['__import__("os")','require("child_process")','process.exit(1)','{x:call()}','{x:1,x:2}']:
            with self.assertRaises(ValueError):dx7.LiteralParser(text).value()
        self.assertEqual(dx7.LiteralParser('{a:0,b:false,c:[0,99],}').value(),{'a':0,'b':0,'c':[0,99]})
    def test_bad_sysex(self):
        good=(Path(__file__).parent/'sources/dx7/yse-originals.syx').read_bytes()
        for data in [b'',good[:-1],good+b'\0',good[:6]+bytes([128])+good[7:],good[:161],good[:3]+bytes([6])+good[4:]]:
            with self.assertRaises(ValueError):dx7.sysex(data)
        self.assertEqual(len(dx7.sysex(good+good)),64)
    def test_bounded_archive_data_only(self):
        def archive(name,payload):
            b=io.BytesIO()
            with zipfile.ZipFile(b,'w',zipfile.ZIP_DEFLATED)as z:z.writestr(name,payload)
            return b.getvalue()
        self.assertEqual(dx7.archive_data(archive('bank.syx',b'abc')),{'bank.syx':b'abc'})
        self.assertEqual(dx7.archive_data(archive('do-not-run.exe',b'abc')),{})
        for name in ['../escape.syx','/absolute.syx','folder\\escape.syx']:
            with self.assertRaises(ValueError):dx7.archive_data(archive(name,b'x'))
        with self.assertRaises(ValueError):dx7.archive_data(archive('huge.syx',b'\0'*1048577))
    def test_manifest_hashes(self):
        m=json.loads((dx7.ROOT/'tools/chip_banks/manifest.json').read_text())
        for s in m['dx7_sources']:
            self.assertEqual(hashlib.sha256((dx7.ROOT/s['path']).read_bytes()).hexdigest(),s['sha256']);self.assertEqual(s['status'],'approved');self.assertTrue(s['license_evidence'])
if __name__=='__main__':unittest.main()
