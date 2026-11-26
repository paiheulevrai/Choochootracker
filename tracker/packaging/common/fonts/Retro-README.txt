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

Elektron.cnfont: derived from Elektron Pixel Font by savingaurora,
  Copyright 2008. Creative Commons Attribution-ShareAlike 3.0 Unported.
  https://fontstruct.com/fontstructions/show/70152/elektron-pixel-font
  This is an unofficial fan recreation, not an official Elektron font.
  Source: elektron-pixel-font.ttf from the FontStruct download archive.
  SHA-256: 9a603457247c5950e735f8dec3084cbe7f1efb54e81df867902b2d14601dc8b4
  License: ../licenses/CC-BY-SA-3.0.txt
  This adaptation rasterizes the original ASCII glyphs in fixed-width cells
  at five sizes and is distributed under the same license. Most lowercase
  letters look like capitals.

OldCamcorder.cnfont: derived from OldCamcordersOSD by Sunbertrs,
  an open-source font inspired by camcorder and VCR displays.
  https://github.com/Sunbertrs/OldCamcorder-OSD-font
  Revision: f9230bd322c383eb4e0655313ee792f62d60cea3
  Source: OldCamcordersOSD.fon
  SHA-256: 2527e8a475423a29074962a3f7f031a02dca38daeb524b363c75e280bd88445e
  License: ../licenses/OldCamcorder-OFL.txt (SIL Open Font License 1.1).
  The original 14x21 bitmap is padded at 16x24 and resized with
  nearest-neighbor sampling at other sizes, including reduction at 12x16.

PixelOperator.cnfont: derived from Pixel Operator Mono by Jayvee Enaguas
  (HarvettFox96), Copyright 2009-2018. Creative Commons CC0 1.0.
  https://www.dafont.com/pixel-operator.font
  Source: PixelOperatorMono.ttf from pixel-operator.zip, version 2018.10.04-1.
  SHA-256: e171000f12d196e7a8a1a91c1a429ef3264a0ff007d5f62fa3dc44ef8df51226
  License: ../licenses/PixelOperator-CC0.txt

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
to fit; the pixel-built bitmaps are enlarged by integer scaling. Elektron,
Segment 14, Pixel Operator, Code Mono and Machine Mono are rasterized
separately for each cell size. OldCamcorder uses the bitmap conversion
described above.
These are converted adaptations, not the upstream font releases.
