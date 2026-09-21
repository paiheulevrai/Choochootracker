# Feasibility study: August 9, 2026

This document evaluates ideas. It does not promise that they will become features.

> Update: Plaits, the Clouds Reverb and Delay sends, `PRO`, `MOD`, and `SPD` were implemented after this study. The text below records the initial analysis. Benchmarks and musical testing on RG353V are still required.

## Plaits

Plaits can be added as a new `Plaits` instrument. The Mutable snapshot in `inspirations/` is under the MIT license and contains 24 engines with Main and Aux outputs.

Sample rate is the main technical issue. Plaits runs natively at 48 kHz while ChooChooTracker mixes at 96 kHz for Braids. The safest option is to keep Plaits at 48 kHz and upsample its output by two. Changing the internal constants of every engine would be harder to validate.

Each voice needs a work buffer of about 16 KiB in addition to the `Voice` object and its tables. Memory is not a concern on RG353V. CPU cost should be measured with one voice, then eight voices using the most expensive engines, before committing to interface or project format work.

Verdict: feasible, medium risk. A DSP prototype and hardware benchmark are required.

## Alternative Plaits model banks

Lyle Mills' `eurorack` repo includes an alternative Plaits firmware path that expands the stock engine set with a second catalog of modules. The upstream catalog lists 87 total modules: 62 stock Mutable Instruments engines and 25 additional models from Rubato/Community packs. The most notable examples are `Glisson`, `GENDY`, `Scanned`, `Pulsar`, `Loopback`, `Lockstep`, `Tapfield`, `Phase Weave`, `Sideband Bank`, `Attractor`, `Undertow`, `Reed Pipe`, `Brass`, `Shakers`, `Helix`, and `Bytebeat`.

These additions are a larger palette: granular, chaotic, physical, additive,
semi-acoustic, and “wild digital” models beside the stock Plaits bank.
ChooChooTracker should expose all 87 through a separate `Plaits-Alt` instrument
type, never by replacing the 24 stock `Plaits` IDs. Existing projects must keep
their current engine numbers and sound unchanged.

The hardware firmware's 24/32-slot palette limit does not apply directly to the
desktop/console tracker. `Plaits-Alt` needs its own registry and voice wrapper,
with stable catalog IDs, while stock `plaits::Voice` remains untouched. The
catalog's families should drive the hierarchical popup, and validation must
ensure every available model appears in exactly one category.

All models require their own short control description, license/provenance
check, offline render test, and CPU/retrigger/filter validation. Expensive
models may remain selectable but should surface a clear performance warning
when the eight-voice target cannot be met with Reverb and Delay enabled.

License conclusion: the Plaits-Alt DSP sources reviewed in the Lyle Mills fork
are MIT-licensed. Importing them does not change this project's license, but
must retain the original notices and a `NOTICE` mapping each model to its source
files and copyright holders (Mutable Instruments, Lyle/Dylan, and STK credits
for Brass and Shakers).

Verdict: feasible, medium-to-high integration risk. The catalog and sources are
available, but the project must isolate the alternate DSP registry, preserve
project compatibility, and validate all 87 models before calling the bank
stable.

## Filter characters and open-source references

The current multimode filter remains the `Clean` option: predictable, low-cost,
and available in every mode. Three later characters are enough to cover the
useful contrast without turning the tracker into a filter museum: `Classic`
(a smooth 24 dB low-pass inspired by ladder/OTA designs), `Aggressive` (an
MS-20-inspired high-pass into low-pass path with drive), and `Acid` (a
low-pass diode-ladder character where filter envelope and accent matter as
much as the core).

These are sound-design targets, not claims of circuit-perfect emulation. Each
character should retain LP, HP, and BP modes: the mode is a musical choice,
while the character controls the resonance, drive, feedback, and response.

Published circuit analyses and open-source projects are useful references for
topology, parameter ranges, listening tests, and stress cases. The preferred
implementation is a small in-tree implementation derived from those ideas.
Only permissively licensed code (MIT/BSD/ISC/Apache) may be vendored after a
file-level license review, pinned source revision, and attribution. GPL code is
reference material only unless the project licensing decision changes.

Validation must cover fast cutoff/resonance automation, extreme settings,
retrigger and note-off behavior, finite output, CPU use, and musical A/B tests
on RG353V. Start with `Clean`; add one character at a time only after it passes
the same checks.

Verdict: feasible, low risk for the Clean/Classic path and medium risk for
Aggressive/Acid because their non-linear feedback needs more stability and
aliasing tests.

## Reverb and Delay sends

The mixer can expose two send levels per track. During mixing, the audio engine should accumulate two stereo buses, process each effect once per callback, then add both returns to the master. It must not create a reverb or delay instance for every track.

The MIT-licensed `clouds::Reverb` class can be separated from the rest of Clouds. It was designed around 32 kHz and a 16,384-word buffer. It can either run at 32 kHz with conversion to and from 96 kHz, or have its delay lengths and memory scaled for 96 kHz. The first option is smaller but requires careful listening tests.

A synchronized delay is straightforward DSP, but "BPM" must be defined in relation to grooves. A robust control would use musical divisions based on the nominal duration of four rows, leaving swing in the sequencer. Each return filter can reuse the modern engine's stereo filter.

The mixer can add `REV` and `DLY` columns. `Select+Up` and `Select+Down` are available to open the two effect screens. Send levels and global effect parameters must be saved in `.cct`.

Verdict: feasible, medium risk. Validate a global reverb without UI first, then measure Reverb, Delay, and eight voices together.

## Playback FX and conditions


### Cross-screen FX clipboard and control-path bounce

Tables can act as a control-path resampling target: turn live or generated automation into explicit, editable FX values that can loop and be reused. The first prerequisite is a shared FX-only clipboard between Phrase and Table screens. Copying a selection made only of FX name/value columns should allow paste into any FX lane of either screen. Complete row copies remain screen-specific because note, instrument, transpose and table pitch columns have different meanings. A paste that exceeds the target's 16 rows or available FX lanes is truncated; Phrase has three FX lanes and Table has four.

This makes a recorded Motion Record passage easy to "bounce" into a chosen table: record the gesture in the Phrase as normal, copy its FX cells, then paste them into a table for looping or further editing. It avoids making real-time table recording the default, which would be ambiguous when table lanes run at different speeds or loop back over the same row. The selected target table is always explicit because tables can be shared by instruments.

A later LFO-to-table bounce can use the same destination and clipboard conventions. It samples one full LFO cycle into the 16 rows of a user-chosen table and FX lane. Only modulation destinations with an existing direct FX representation should be eligible; generic modulation parameters and destinations without an FX mapping are excluded. This is a generated control curve, not a replacement for the live Motion Record workflow.

Verdict: the cross-screen FX clipboard is feasible with low risk; LFO-to-table bounce is feasible with medium UI and validation risk. Test lane offsets, Phrase/Table lane truncation, the 16-row boundary, cut behavior, and that source-screen copy/paste remains unchanged.

### Live Mode inspired by Little Piggy Tracker

Little Piggy Tracker's Live Mode lets the performer queue a chain on an individual song track. The queued chain begins only when that track reaches the end of its current chain, so tracks can switch independently. This is useful for live arrangement without abrupt note cuts.

ChooChooTracker already has the necessary foundations: every track owns its playback position, transport commands cross the UI/audio boundary through the fixed command queue, and phrase playback already supports a queued transition. A Live Mode on the Song screen can extend that model with a queue-chain command per track. While playback is active, PLAY on the current Song cell queues its chain for that column; a selected range queues each non-empty cell in its respective track. A queued empty cell stops its track at its next chain boundary. The existing global STOP remains immediate.

The UI needs a Song/Live mode indicator and a visible queued-chain marker per track. The playback status should expose the queued target without exposing mutable audio state. Queue replacement should be last-input-wins for each track, and range loops should disable or clear queued live transitions so their semantics remain unambiguous. Project format changes are not required.

The recommended start behavior is direct playback from the selected Song row, followed by Live Mode queues. This is more natural in the current interface than reproducing Piggy's queue-first stopped state, while preserving the musical chain-boundary transitions.

Verdict: feasible, medium implementation risk and low DSP risk. Test independent track boundaries, replacing a pending queue, empty-cell stops, range-loop interaction, command-queue overflow, visual status, and Windows/Web/PortMaster controls.

### Existing glide

`PSL` already glides toward a new note over a duration in ticks. A second Glide FX would duplicate the same behavior. The existing command should be tested and its help text renamed if necessary.

### Per-track speed

An `SPD` FX can change sequencer speed for one track without changing global BPM, pitch, or sample playback rate. One possible encoding was `01=/4`, `02=/2`, `03=x1`, `04=x2`, `05=x4`. The value would remain active until the next `SPD` and reset to `x1` when playback starts.

Each track already has its own groove counter and playhead, so tracks can drift apart without a new architecture. A fractional accumulator should apply the multiplier to groove time so swing is preserved without rounding drift. Speed changes must follow the normal row path to keep notes, conditions, `HOP`, and chain endings consistent.

The engine tick is the limiting factor. With a standard six-tick groove, `x2` gives three ticks per row and `x4` alternates correctly around 1.5 ticks. If a row already lasts one tick, `x2` or `x4` would need several triggers inside one tick. The current engine cannot assign distinct audio duration to those triggers. The first implementation should therefore cap advancement at one row per tick. Exact behavior in every groove would require splitting audio rendering within a tick.

Verdict: feasible with low CPU cost. Risk is low with the one-row-per-tick cap and medium if `x2` or `x4` must remain exact in very fast grooves.




## CPU measurements from August 9

- Empty project: 37%
- One Braids voice: 41%
- Eight Braids voices: 47%

Profiling showed that all eight AY emulators were rendering even for empty, Braids, and Sample tracks. They are now skipped when no AY instrument uses them. All three measurements must be repeated on RG353V before estimating the remaining budget for Plaits and sends.


-> Update from August 10: Skipping unused AY voices rendering dropped the empty project CPU load to 2%. Huge improvement.

## Complex Osc v2: Loquelic x EMX-1

The existing Braids and Plaits instruments already cover parts of this sound
space: Braids provides VOSIM, FM, feedback/chaotic FM, sync and ring-style
multi-oscillator models; Plaits provides 2-op/6-op FM, phase distortion and
waveshaping. Neither exposes the Loquelic-style architecture as independent
Pitch A/Pitch B controls with selectable oscillator sync, cross phase
modulation, AM/ring blend and a final wavefolder.

A small dedicated voice is therefore feasible and musically distinct. Use the
Loquelic-style two-oscillator architecture, but the EMX-1's mode-led control
surface: Dual, Ring, Sync, Cross Mod, VPM and Waveshape. Give every mode six
macros: Wave A, Wave B, Interval, Amount, Mix and Shape; their musical meaning
may vary by mode (for example, Amount is balance in Dual and modulation depth
in VPM). Keep the model selector separate, as Bogie does.

Start with the PM path: two band-limited oscillators, independent pitch
offsets, one-way or bidirectional phase modulation, AM/ring blend, selectable
sync and a final wavefolder. VOSIM and the summation-series path can follow if
the PM voice proves useful. Do not reproduce Loquelic's variable-time aliasing
strategy initially: it is a deliberate hardware character, costly to fit into
the fixed-rate tracker renderer, and not required for the core patchability.

Verdict: feasible, medium risk. Prototype the PM voice and measure CPU with
eight voices, reverb and delay before adding other algorithms. Reference:
[Noise Engineering Loquelic Iteritas manual](https://manuals.noiseengineering.us/li/).

## Serum-compatible wavetable oscillator

Serum wavetables are mono RIFF/WAV files containing consecutive single-cycle
frames. The conventional frame length is 2048 samples. Serum can also write a
`clm ` RIFF metadata chunk containing the frame length and its preferred
interpolation mode; the audio remains ordinary PCM/float WAV data. Supporting
the audio frames and treating missing `clm ` metadata as 2048-sample frames is
enough to import the common format. The optional chunk should be parsed, but
not required, so ordinary concatenated WAV wavetables work too.

This is a new modern instrument, not an extension of the 32-step/4-bit AY
wavetable editor. A practical first version stores up to 256 frames, converts
input to mono 16-bit (or keeps float internally), resamples every frame to a
fixed internal length such as 512 samples, linearly interpolates within a
frame and between neighbouring frames, and exposes Position, Frame LFO,
Detune, Unison and the shared post-filter/envelope. At 256 x 512 x 16-bit, one
loaded table is 256 KiB; allocation must therefore be per instrument rather
than embedded in the project struct. Project files should retain a source path
and reload it, like PCM Sample and 2xSCWF, rather than serializing the full
table.

Spectral interpolation and Serum's imported metadata modes are worthwhile
later, but not for v1: time-domain frame interpolation is small, predictable
and musical. Add mipmaps or harmonic-band tables before calling high notes
production-ready; otherwise bright Serum tables will alias. Reference:
[Xfer's format notes](https://xferrecords.com/forums/general/file-types).

Verdict: feasible, medium risk. Parsing/import is low risk; anti-aliasing,
memory limits and a usable frame-position UI are the real implementation work.

## Drum synthesizers: PO32 and Weird Drums

These are two new instrument families, not plug-in ports. The tracker remains
the sequencer: one ChooChoo instrument produces one drum voice, and a kit is
made by placing several instruments on tracker tracks. Neither integration
imports JUCE, a pad-grid sequencer, a mixer, or the master FX from the source
projects.

Both source families are MIT-compatible with ChooChooTracker. `libpo32` is a
freestanding C99 drum-synthesis and PO-32 transfer library; its synthesis core
has no platform audio or DSP dependencies. WeirdDrums is a JUCE plug-in, so
only its MIT DSP design/code may be ported into a native ChooChoo voice. The
eventual import/export of PO-32 acoustic transfer audio is explicitly separate
from the synth integration.

### Shared product decision

Drum voices use their own native envelopes. They do **not** expose the shared
ChooChoo ADSR, Trigger FX, or ADSR preview. The post-voice ChooChoo filter is
kept: Status, mode, slope, cutoff, resonance, modulation, reverb send and
delay send remain available exactly as for modern voices.

This requires separating the common filter controls from the current
`InstrumentVoicePostSettings` envelope controls in the implementation. The
instrument catalogue should advertise the filter capability without advertising
ADSR/Trigger capability; screens and FX then hide unsupported shared controls
instead of adding special cases to their layout.

The drum screen is a new shared layout, rather than a variation of the
Braids/Plaits screen. It follows the designbook rules: the usual instrument
header is unchanged; the engine has the left column; the common post-filter has
the right column; headings use `textTitles`; unfocused values use `textDefault`;
focused values use `textValue`; values use the established columns 11 and 26.
There is no contextual navigation text in the title, no ADSR graph, and no
conditional row movement. The screen is 40 by 20 characters.

### PO32 Drum

`libpo32` supplies 21 underlying patch fields. A first ChooChoo screen exposes
the nine sound-shaping fields below, plus the common `Vol` in the header. The
remaining fields (oscillator attack, modulation rate, noise filter frequency
and Q, noise-envelope attack/decay, EQ, and velocity responses) are set by
presets in v1. There is deliberately no advanced page.

```text
INSTRUMENT 00

Type    PO32 Drum       Load  Save
Name    PO32 KICK
Transp. Off     Tbl.Tic 01   Vol FF

SOUND                   FILTER
Wave      Sine          Status   On
Pitch     050           Mode     LP
Decay     030           Slope    12 dB
Mod       Drop          Cutoff   6000 Hz
Bend      040           Reso     20

NOISE
Mix       050
Filter    BP
Envelope  Exp
Dist      010
```

`NOISE Filter` is the PO32 internal noise filter; `FILTER` is always the
post-synth ChooChoo filter. This wording makes their distinct roles visible.
The desired later addition is an optional PO-32 transfer WAV export; acoustic
recording/import is deferred until input support is reliable on every intended
platform.

Verdict: feasible, low integration risk. Start with the sound engine and the
screen above; do not add transfers, advanced editing, or kit management in the
same release.

### Weird Drums

The ChooChoo version uses the original drum-synth idea, not the full
eight-voice Weird Dreams machine. The common header provides level, so a
separate per-voice volume row is unnecessary. The screen exposes nine native
controls: oscillator shape/pitch/decay/pitch modulation/drive, then noise
mix/filter/cutoff/decay. Presets provide useful starting roles such as kick,
snare, hat, clap and tom without adding a separate kit editor.

```text
INSTRUMENT 00

Type    Weird Drums     Load  Save
Name    WEIRD SNARE
Transp. Off     Tbl.Tic 01   Vol FF

TONE                    FILTER
Wave      Tri           Status   On
Pitch     060           Mode     HP
Decay     035           Slope    12 dB
Pitch Mod 045           Cutoff   7200 Hz
Drive     020           Reso     30

NOISE
Mix       065
Filter    BP
Cutoff    4800 Hz
Decay     050
```

The `NOISE` labels make the engine's `Cutoff` and `Decay` unambiguous next to
the ChooChoo post-filter. Panning is not added solely for this instrument: it
would first need a consistent cross-engine ChooChoo panning model.

Verdict: feasible, medium integration risk only because the JUCE plug-in DSP
must be isolated/ported and benchmarked. Keep v1 to one voice per instrument,
native drum envelopes, presets, and the shared filter.

## PSP-1000 port

The original PSP has 32 MiB of main RAM, a MIPS CPU that can run up to 333 MHz
in homebrew, and a 480x272 display. [PlayStation Portable hardware](https://en.wikipedia.org/wiki/PlayStation_Portable_hardware)
This makes a PSP-1000 port feasible as a measurement target, but medium-to-high
risk as a complete product: do not promise full-performance parity before
testing on the physical console.

The first POC should remain single-core and build the complete tracker,
including every current engine and Plaits-Alt. It should run the PSP CPU at
333 MHz, use 1024-frame stereo buffers and the PSP's native 44.1 kHz output,
and retain the Mixer CPU meter. The purpose is to measure actual engine cost,
not to hide it behind reduced polyphony or a reduced build.

44.1 kHz is a PSP-only host-rate exception; Windows and PortMaster remain
48 kHz targets. The master mix, AY, PCM, SCWF, BYOWTBL, aChChid, envelopes,
filters, Tilt, delay and reverb run at the PSP host rate. Braids retains its
96 kHz source domain and existing decimator; Plaits and Plaits-Alt retain their
48 kHz generation and use their existing wrapper conversion to the host rate.
No global 48-to-44.1 kHz SRC is needed.

The port needs a PSPSDK/C++17 compatibility pass, RAM and link-map inspection,
audio-deadline testing, and a PSP graphics backend that scales the tracker
480x320 logical canvas proportionally to the 480x272 display. Media Engine
work is explicitly deferred. Likewise, a true 48 kHz Braids mode is not part
of the POC: it requires changing its rate-dependent tables and increments, then
validating pitch, aliasing and all 47 models. Consider it only if physical PSP
measurements justify that DSP work.

The next decision follows hardware measurements: record average and peak Mixer
CPU, active voices, send/effect state and audible crackles for every engine on
the PSP-1000. Use those results before setting engine polyphony caps or
investing in Media Engine offload.

Verdict: feasible as a mono-core hardware POC, medium-to-high risk. The real
console benchmark decides whether a production PSP profile needs reduced
polyphony, engine restrictions or Media Engine work.
