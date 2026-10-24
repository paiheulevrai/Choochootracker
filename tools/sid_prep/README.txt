SID preparation — October 5, 2026

This is a runnable, isolated preparation prototype. Nothing here is linked into
ChooChooTracker or copied into its factory bank/package directories. The installed
instrument revision remains source ff887af. Branch: feature/sid-preparation.

WHAT WORKS

The pinned, unmodified floooh/chips m6581 core renders SID register programs to
48 kHz mono WAV. Its existing digital oscillators, envelopes, pulse width,
noise and shared multimode filter are available. The probe measures 1/2/4/8
simultaneous core instances, each with three oscillators and filtering enabled.

prepare.py creates 32 original programs: eight families with four variations
(Round Bass, Moving Pulse, Saw Pluck, Soft Pad, Arp Bell, Noise Drum, Bright Lead,
Pulse Chord). These use envelopes, PWM, filter sweeps, vibrato and/or arpeggios.
They are project-authored recipes, not rips or verified recreations of games.

The same tool reads 24 MIT-licensed c64SIDkit SFX definitions without executing
source Python. It adapts their oscillator/envelope/sweep/filter/vibrato settings
into register traces. Original definitions remain intact under sources/sidkit/.
Additional JSON patches are pinned for review but are not included in the 24.

Twelve CC-BY GoatTracker GTI5 files are parsed into header plus wave/pulse/filter/
speed tables. They represent THREE instrument families (piano, acoustic guitar,
violin), two optimization approaches and two target chip models. These are
candidates for a future table interpreter, not twelve finished conversions.
Corresponding JSON and instrument READMEs are retained for source comparison.
No HVSC songs, reference-piano extractions, recordings or full music players
were downloaded or included. Mainstream and TPE programs are distinct payloads;
that does not imply twelve unrelated timbres.

REPRODUCE LOCALLY

From the repository root:

  mkdir -p .tmp/sid-prep
  c++ -std=c++17 -O2 -Wall -Wextra tools/sid_prep/probe.cpp -o .tmp/sid-prep/probe
  python3 tools/sid_prep/prepare.py .tmp/sid-prep/programs
  .tmp/sid-prep/probe
  .tmp/sid-prep/probe .tmp/sid-prep/programs/originals-04-60.regs .tmp/sid-prep/pulse.wav 4

prepare.py verifies all pinned source hashes. The trace format is three decimal
integers per line: macro-frame, register address, byte. Events use a 50 Hz macro
clock and a 985248 Hz SID clock; one register is written per SID cycle. A
register trace is a preview format, not the proposed native preset format.
The adapter uses an explicit test-bit reset, finite preview gate/release, and
native register masking. Exact playback equivalence with source trackers has
not been established. Long releases can extend beyond the four-second audition.

originals.json and sidkit-normalized.json are reproducible generated program
lists. preset-inventory.json describes candidates. source-manifest.json pins
all source blobs and their SHA-256 hashes. CC-BY candidates have not been
musically validated; no claim of authenticity or listening approval is made.

VALIDATION

112 host renders completed with finite, nonzero, unclipped output. This covers
all 32 originals, testing pitched originals at MIDI 48/60/72 and fixed noise at
one pitch, plus all 24 SFX programs. PCM hashes are distinct. Full-file AC RMS
was checked as well as raw energy, so a DC offset alone cannot pass. A very
short 'bounce' effect ends before 100 ms; analyzing only the sustain region
would incorrectly classify it as silent. Evidence: render-results.json.
This is an engineering check; subjective listening and real-chip comparison
remain necessary. No app engine sources changed, so app/UI tests were not
rerun for this separate prototype.

The R36H baseline (-O2, standalone, low process priority, frontend running)
used approximately 25/50/100/201 percent of ONE CPU core for 1/2/4/8 instances.
This is elapsed rendering cost divided by emulated duration, not process-wide
CPU telemetry. Each instance's state is 240 bytes; this excludes the shared
cutoff table, code, buffers and future program storage. See device-benchmark.txt.
The optional -O3 follow-up could not start because USB SSH became unreachable
after the successful installation and baseline probe. No -O3 performance
result is claimed; this remains a useful next measurement.

These are short 2.13-second synthetic measurements, not a full app audio test.
They omit tracker DSP, mixing, UI, nonlinear filter improvements and ALSA.
Four full SID cores already approach a callback thread's entire baseline
budget. Eight full cores are unsuitable in this baseline. An independent core
per track per chord note would multiply this cost further. Integration must
choose a bounded allocation policy and benchmark real mixed projects first.
Variant selection changes an instance's parameters, not its instance count.

LICENSES AND SOURCES

Our probe, parser, trace adapter and original recipes follow the repository MIT
license. The emulator remains zlib licensed, with its existing tedplay/Unlicense
attribution intact. Preserve vendor/LICENSE.chips and the source header notice;
mark future modifications. This dependency can coexist with MIT project code.

c64SIDkit code/programs: MIT, copyright 2026 c64SIDkit contributors. Preserve
sources/sidkit/LICENSE. Adapted programs are sidkit-normalized.json and the
sidkit traces generated by prepare.py. We did not incorporate its optional
reSID/VICE backends or assume their licenses were MIT.

JC-000/c64-sid-instruments programs: CC-BY-4.0, not MIT. Preserve that license,
source URLs and attribution with eventual distributed presets. Pinned copies
here remain unmodified. The upstream README delegates author credit to each
instrument README, but those copies do not consistently identify a named
instrument author; resolve attribution and model-specific optimization-source
notes before promoting these candidates to factory banks. Do not treat the
repository license as permission for its separately referenced HVSC music.

Primary sources:
https://github.com/floooh/chips/blob/9e88298ce56319953ac7a43213a1120359f7a3a6/chips/m6581.h
https://github.com/floooh/chips/blob/9e88298ce56319953ac7a43213a1120359f7a3a6/LICENSE
https://github.com/devinvenable/c64SIDkit
https://github.com/JC-000/c64-sid-instruments

ALTERNATIVE CORES REVIEWED

libresidfp/reSID provide more sophisticated 6581/8580 behavior, but are GPL.
MIT source can be combined with GPL code under GPL obligations; that does not
preserve an MIT-only combined distribution. No GPL core code or coefficient
sets have been incorporated here.
https://github.com/libsidplayfp/libresidfp
https://github.com/daglem/reSID

perfect6581 has an MIT license but explicitly simulates only the digital parts;
it does not solve our analogue filter requirement. It was not incorporated.
https://github.com/libsidplayfp/perfect6581

FUTURE INTEGRATION

See docs/sid-implementation-prep-20261005.txt for the agreed interface, shared
bank model, hardware-variant boundary, allocation choices, import stages,
persistence requirements and acceptance checks. This prototype deliberately
has no SID entry or Chip Variant control in the installed app yet.
