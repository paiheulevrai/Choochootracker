# Feasibility study: August 9, 2026

This document evaluates ideas. It does not promise that they will become features.

> Update: Plaits, the Clouds Reverb and Delay sends, `PRO`, `MOD`, and `SPD` were implemented after this study. The text below records the initial analysis. Benchmarks and musical testing on RG353V are still required.

## Plaits

Plaits can be added as a new `Plaits` instrument. The Mutable snapshot in `inspirations/` is under the MIT license and contains 24 engines with Main and Aux outputs.

Sample rate is the main technical issue. Plaits runs natively at 48 kHz while ChooChooTracker mixes at 96 kHz for Braids. The safest option is to keep Plaits at 48 kHz and upsample its output by two. Changing the internal constants of every engine would be harder to validate.

Each voice needs a work buffer of about 16 KiB in addition to the `Voice` object and its tables. Memory is not a concern on RG353V. CPU cost should be measured with one voice, then eight voices using the most expensive engines, before committing to interface or project format work.

Verdict: feasible, medium risk. A DSP prototype and hardware benchmark are required.

## Reverb and Delay sends

The mixer can expose two send levels per track. During mixing, the audio engine should accumulate two stereo buses, process each effect once per callback, then add both returns to the master. It must not create a reverb or delay instance for every track.

The MIT-licensed `clouds::Reverb` class can be separated from the rest of Clouds. It was designed around 32 kHz and a 16,384-word buffer. It can either run at 32 kHz with conversion to and from 96 kHz, or have its delay lengths and memory scaled for 96 kHz. The first option is smaller but requires careful listening tests.

A synchronized delay is straightforward DSP, but "BPM" must be defined in relation to grooves. A robust control would use musical divisions based on the nominal duration of four rows, leaving swing in the sequencer. Each return filter can reuse the modern engine's stereo filter.

The mixer can add `REV` and `DLY` columns. `Select+Up` and `Select+Down` are available to open the two effect screens. Send levels and global effect parameters must be saved in `.cct`.

Verdict: feasible, medium risk. Validate a global reverb without UI first, then measure Reverb, Delay, and eight voices together.

## Playback FX and conditions

### Tables as automation

Tables already support this. Their four FX columns accept Braids and Sample FX and replay them at each column's own speed. A table can sequence cutoff, resonance, timbre, color, start, end, or volume. FX reset happens before table initialization so row 0 takes effect immediately on a trigger.

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

## Fixes prompted by testing

- New projects now initialize the linear pitch table in cents.
- Braids octave and fine tuning were corrected when Linear Pitch is disabled.
- Sample accepts 8-bit and 16-bit PCM WAV files in mono or stereo.
- Sample load errors stay on screen three times longer.
- Mixer cell indexes are guarded. The reported crash is not considered solved until it can be reproduced or logged.
