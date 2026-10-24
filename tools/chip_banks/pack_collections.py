#!/usr/bin/env python3
"""Package generated CNI collections as deterministic, independently portable ZIPs.

Run after convert.py. Existing ZIP-backed catalogs can also be repacked. CNI bytes
are preserved, and deletion of loose generated files happens only after verification.
"""
import argparse
import hashlib
import re
import zipfile
from pathlib import Path


def pack(folder: Path, notices: Path):
    catalogs, banks = {}, {}
    for name in ('catalog.tsv', 'builtins.tsv'):
        path = folder / name
        lines = path.read_text().splitlines()
        if lines[0] != 'CCT-CHIP-CATALOG\t1':
            raise ValueError('Unknown catalog format')
        rows = [line.split('\t') for line in lines[1:]]
        catalogs[name] = rows
        for row in rows:
            if len(row) not in (6, 7):
                raise ValueError('Invalid catalog row')
            if len(row) == 7:
                with zipfile.ZipFile(folder / row[6]) as source:
                    data = source.read(row[5])
            else:
                data = (folder / row[5]).read_bytes()
            banks.setdefault((row[1], row[2]), []).append((row[:6], data))
    loose = []
    for (bank_id, name), entries in banks.items():
        slug = re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')
        filename = f'{bank_id}-{slug}.zip'
        output = folder / filename
        temporary = output.with_suffix('.zip.tmp')
        def add(archive, name, data):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
        with zipfile.ZipFile(temporary, 'w') as archive:
            add(archive, 'catalog.tsv', ('CCT-CHIP-CATALOG\t1\n' + ''.join('\t'.join(r) + '\n' for r, _ in entries)).encode())
            for row, data in entries:
                add(archive, row[5], data)
            # Keep source notices with each portable collection.
            for notice in sorted(notices.rglob('*')):
                if notice.is_file():
                    add(archive, 'licenses/' + notice.relative_to(notices).as_posix(), notice.read_bytes())
        with zipfile.ZipFile(temporary) as archive:
            if archive.testzip() is not None:
                raise ValueError('ZIP integrity failure')
            for row, data in entries:
                assert hashlib.sha256(archive.read(row[5])).digest() == hashlib.sha256(data).digest()
        temporary.replace(output)
        for rows in catalogs.values():
            for row in rows:
                if (row[1], row[2]) == (bank_id, name):
                    if len(row) == 6: row.append(filename)
                    else: row[6] = filename
                    loose.append(folder / row[5])
    for name, rows in catalogs.items():
        (folder / name).write_text('CCT-CHIP-CATALOG\t1\n' + ''.join('\t'.join(r) + '\n' for r in rows))
    for path in loose:
        if path.exists(): path.unlink()
    print(f'Packed {len(loose)} presets into {len(banks)} collections; all preset bytes verified.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--notices', type=Path, default=Path('tracker/packaging/common/licenses/chip-banks'))
    args = parser.parse_args()
    pack(args.folder, args.notices)
