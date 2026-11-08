# Third-party material

## Sophie for Schwung

`sophie.c` is a fixed-point port of the original Sophie metallic-percussion
algorithm in `../schwung-sophie/src/dsp/sophie.c`, adapted for one Digitakt
track/voice at 48 kHz. The source project is MIT licensed.

Copyright (c) 2026 Matt Estela.

MIT License: Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including without
limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions: The
above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS
IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## digihealth

`diagnostics/digihealth/` is a locally modified copy of
[irpina/digihealth](https://github.com/irpina/digihealth), commit
`72f0183383e67c3313146bf77df6f71dd7996a8f`. It is
GPL-2.0-or-later, not MIT. Its full license is retained at
`diagnostics/digihealth/LICENSE`. The local change makes FAST AUDIO opt-in
instead of enabling it at boot. Its source is included so that the exact
diagnostic configuration used during hardware testing is reproducible.

## digisophie-dn (Digitone port)

`digisophie-dn/` carries SOPHIE to the Digitone mk1/Keys (machine 11 SOPH,
OS 1.43 + 1.44) through the stock filter/mixer/AMP. It reuses the shared
`sophie.c` core above; Digitone-only behavior lives in the adapter, not
the core. Developed alongside this repo at
[x2k77m1/digisophie-dn](https://github.com/x2k77m1/digisophie-dn).
