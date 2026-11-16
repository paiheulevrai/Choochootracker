Optional retro fonts

Select these files with Settings -> Load font. Each supplies ASCII 32-126
at 12x16, 16x24, 24x36, 32x48 and 48x54. Installing them does not select
them or change the default font.

RetroC64.cnfont: derived from Sixtyfour by the Sixtyfour Project Authors,
  Copyright 2021. SIL Open Font License 1.1.
  https://github.com/jenskutilek/homecomputer-fonts
  Compiled source: Google Fonts, ofl/sixtyfour/Sixtyfour[BLED,SCAN].ttf
  Revision: 8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5
  SHA-256: 3d5a3bb3257a491124347bac83ca7f0c8c77a9961270f353b498f3153e751c68
  License: ../licenses/Sixtyfour-OFL.txt

RetroWorkbench.cnfont: derived from Workbench by the Workbench Project
  Authors, Copyright 2021. SIL Open Font License 1.1.
  https://github.com/jenskutilek/homecomputer-fonts
  Compiled source: Google Fonts, ofl/workbench/Workbench[BLED,SCAN].ttf
  Revision: 8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5
  SHA-256: 027940fb7aea63f260fb367957e419c08f904f357b384c819f99eb06e7af17d6
  License: ../licenses/Workbench-OFL.txt
  The source's vertically doubled raster is reduced to square pixels.

Segment14.cnfont: derived from DSEG14 Classic Regular by keshikan,
  Copyright 2017. SIL Open Font License 1.1; "DSEG" is a Reserved Font
  Name, so this conversion has a different name.
  https://github.com/keshikan/DSEG/releases/tag/v0.46
  Source: DSEG14Classic-Regular.ttf from fonts-DSEG_v046.zip.
  SHA-256: 0106818dd2e2c9ac9c6b5b7edbbe193df367397d48a18377a58090b00580d4e6
  License: ../licenses/DSEG-OFL.txt
  Missing ASCII # ; [ ] { } and the display-specific ! and ~ are replaced
  by IBM Plex Mono glyphs; see MachineMono below for source and license.
  Lowercase letters have the same appearance as uppercase in this source.

CodeMono.cnfont: derived from mononoki Regular by Matthias Tellen,
  Copyright 2022. SIL Open Font License 1.1; "mononoki" is a Reserved Font
  Name, so this conversion has a different name.
  https://github.com/madmalik/mononoki
  Revision: 4f329e225dde5e22287c86851ea519047548d17c
  Source: export/mononoki-Regular.ttf
  SHA-256: ecefad2b6deec9ba448ba4635c4a596091b515b5b6e67e67bc0bec0b487868bd
  License: ../licenses/mononoki-OFL.txt

MachineMono.cnfont: derived from IBM Plex Mono Regular, Copyright 2017 IBM
  Corp. SIL Open Font License 1.1; "Plex" is a Reserved Font Name, so this
  conversion has a different name.
  https://github.com/IBM/plex
  Compiled source: Google Fonts, ofl/ibmplexmono/IBMPlexMono-Regular.ttf
  Revision: 431022a4eeb0e1009ba0126f41353d8ca2375a2a
  SHA-256: 6a3412f058c7d8dfd9170c41e85ade48e5156ecb89356110ca57a0a27734af46
  License: ../licenses/IBMPlexMono-OFL.txt

The local font converter was adapted for these conversions: glyphs are
monochrome, centered in fixed-width cells with a shared baseline, and padded
to fit; the pixel-built bitmaps are enlarged by integer scaling. Segment 14,
Code Mono and Machine Mono are rasterized separately for each cell size.
These are converted adaptations, not the upstream font releases.
