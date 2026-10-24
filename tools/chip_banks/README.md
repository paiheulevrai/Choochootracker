# Native chip content tools

Factory input is pinned in `manifest.json`. `convert.py` validates hashes and
provenance, parses data without executing source code, deduplicates DX7 parameter
records, records OPL aliases, and calls the production C++ native writer/reloader.
`four_op.py` contains 24 independent original MIT recipes for each of Genesis and
Arcade. DX7 original recipes are separately CC0. Read the notices per source.

From the repository root:

```sh
make -C tracker -f Makefile.test -j4 chip-factory
python3 tools/chip_banks/convert.py --output tracker/packaging/common/instruments/FACTORY --writer tracker/build/tests/chip_factory
python3 -m unittest discover -s tools/chip_banks -p 'test_*.py'
```

Output is 1,140 CNI files: 1,064 shared FM index entries and a 76-entry simple-chip
inventory. `expansion.py` adds source-pinned emu2413 tone tables, supported WOPN v2
melodic programs from 16-Bit FM Music Station, supported YMulator OPM programs,
and deliberately authored OPLL programs. Full source forms and licenses ship
with the converted data. `expansion-manifest.json` records exclusions, including
unsupported source features, duplicates and silent carrier operators. DX7 still
contributes 67 distinct factory sounds. Ten-thousand-entry tests use
synthetic metadata only. Consult `docs/chip-instruments-report.md` for counts,
measured validation, provenance exclusions and pending listening/device work.

User content never enters the factory manifest implicitly:

```sh
python3 tools/chip_banks/import_bank.py SOURCE.tfi --output NEW_DIRECTORY --writer tracker/build/tests/chip_factory
```

The offline converter supports: exact TFI42, original VOPM text fields (0/128 AM enable; no noise or
partial-pan adaptation), WOPLX BANK1, and checksum-validated original DX7 voice/
32-voice/multi-message SysEx. Binary WOPL, arbitrary DMP, four-op Yamaha SysEx,
DX7II/performance extensions, raw unframed dumps and bad-checksum overrides are
not accepted. Conversion stages in a temporary directory and publishes a new
user directory only after native validation. Its manifest makes no licensing
claim about user files. Load its CNI files using the ordinary instrument loader.

Native DX7 users can drop `.syx` files into `instruments/USER/dx7/`, including
subfolders, then choose Bank → USER and open Preset. This requires no offline converter.

After the base conversion, run `python3 tools/sid_prep/package_presets.py` to
add the 56 SID programs, giving 1,196 presets across 25 portable collections.

Factory ZIP packaging: after generating loose factory CNI files, run
`python3 tools/chip_banks/pack_collections.py tracker/packaging/common/instruments/FACTORY`.
The packer preserves CNI bytes, includes source notices, verifies every entry,
then updates both catalogs and removes only the verified loose generated files.
It can also repack existing ZIP-backed catalogs. Do not run it on personal USER
libraries or add user-provided banks to factory packages.

The runtime USER browser now imports compatible source presets directly through
`chipnomad_lib/external_presets.cpp`. The offline converter's format list above
is independent of runtime support. See the USER_MANUAL source-format table and
limits; community downloads stay outside factory assets and source control.

The runtime importer follows the Furnace single-instrument specifications
(`papers/newIns.md` and `papers/oldIns.md`, revision
`68f2c61273808170ca4900c6abed815520105050`), the WOPL/OPLI specification from
Wohlstand's OPL3 Bank Editor, and GoatTracker 2's documented GTI5 tables.
Its sequence player is bounded, allocation-free during playback, and stores
at most 512 source bytes per instrument. It does not run downloaded player code.
`tracker/tests/test_external_presets.cpp` exercises malformed input, register
mapping, sequences, audio chunking, owned file roundtrips and USER integration.
An optional corpus audit uses `CHOOCHOO_COMMUNITY_PACKS`; normal tests do not
require a download or include community bank contents in source control.

To exercise your own ready USER folders and ZIP banks, set
`CHOOCHOO_USER_PRESETS` to the directory containing the eleven engine folders
and run the `External ready ZIP libraries browse and load through USER` test.
These optional local audits use external data; no preset packs are bundled by
the test suite.
