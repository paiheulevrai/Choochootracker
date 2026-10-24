# MSFA dependency boundary and modifications

Source: maintained `Source/msfa` from asb2m10/dexed,
revision `2e182b3db85c09083ab13c8b9b00565ce7d9ff85`.
Every selected upstream source/header has an Apache-2.0 notice. The repository's
GPL application license is not applied to this separately licensed component.
Original Google MSFA revision `f67d41d313b7dc85f6fb99e79e515cc9d208cfff` was read as
format/reference material; no second synthesis core is included. Apache text is
from its COPYING file. File-level original and vendored hashes are in provenance.

Changes to the selected scalar DSP closure:

- Enclose symbols in `choochoo_msfa`; add header guards where needed. Normalize
  line endings. Keep standard includes outside the namespace.
- Remove unused `../Dexed.h` in env.cc and the unused application-coupled
  controllers.h include in fm_core.h. Replace synth.h with only required integer
  types, block constants and min/max helpers. No app tracing or OS atomics needed.
- Use unsigned oscillator phase increments in fm_op_kernel.cc and fm_core.cc;
  wrap is intentional. Bound excessive frequency-table shifts in freqlut.cc.
- `note.h/cc` adapt the Apache dx7note assembly: owned six envelopes, pitch
  envelope, operator phases/gains and feedback. Keep maintained detune,
  keyboard/rate/velocity scaling, amplitude/pitch modulation and Modern core
  routing. Replace tuning/MTS/controller/portamento/MPE services with ordinary
  equal temperament and a continuous Q24 pitch offset. No GPL implementation
  of those services is copied. Copy native patch values at note start; do not
  retain pointers into a bank. Oscillator sync resets phases only when enabled.
- `Note::compute` accepts optional brightness and feedback overrides from the
  tracker. Brightness offsets only modulator envelope levels (0.75 dB steps,
  bounded in Q24); feedback changes its shift without clearing running history.
  Zero/default arguments preserve the reference DSP. Saved DX7 bytes, algorithm,
  carrier levels, oscillator phases and native envelope progression are retained.
- `Note::compute` also accepts six optional live operator level offsets. OP1–OP6
  map to Yamaha canonical OP6–OP1 storage in reverse order. Zero offsets leave
  the existing DSP unchanged; offsets do not change saved patch bytes.
- LFO sits in the original ChooChoo `DX7Part` adapter, one per track/patch part,
  shared by bounded chord slots; preview owns another part. Its mutable phase,
  random state and delay never cross unrelated parts. Retrigger calls keydown
  once per part quantum. Free-running LFO advances while a DX7 part is selected.
- Fixed internal 44100 tables are initialized once off callback. This deliberate
  policy supports simultaneous live/offline renderers with different host rates
  without process-global rate races. The shared ChooChoo FIR handles host rates.
  Reworking upstream global tables into rate-owned objects remains a possible
  optimization, not an unimplemented requirement for correct rate conversion.

Only scalar kernels are enabled. No assembly, architecture probing library,
Android glue, Java, JNI, effects, plugin host, alternate quality core, or preset
bytes are included. Engine code and bank licenses are separately recorded.

Validation so far: every algorithm at multiple host rates; independently gated
parts/chords; block independence and awkward events; ratio/fixed frequency,
velocity, direct-reference assembly, CNI/CCT ownership and queued preview tests.
An ASan/UBSan scalar harness exercised all algorithms with extreme legal values.
This does not establish hardware bit identity or substitute for listening.

FM Phrase FX extension: `Env::setRates` adjusts the current rate while retaining
stage, level and gate (including the remaining static-stage countdown).
`Note::updateTimbre` updates operator rates/frequencies and native LFO depths
without restarting the note or clearing phase/feedback. The tracker adapter
supplies a temporary effective patch; preset bytes remain unchanged. Neutral
macros retain the original path. LFO rate changes retain its phase and delay.

Absolute operator levels: the adapter supplies native VCED output levels in a
temporary patch. `Note::updateTimbre` reapplies the existing keyboard/velocity
scaling and `Env::setOutputLevel` shifts the running log envelope without
retriggering its stage or gate. Legacy operator offsets remain a separate path.
