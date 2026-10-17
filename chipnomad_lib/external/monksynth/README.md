# MonkSynth vocal DSP

Source: https://github.com/JonET/monksynth
Pinned commit: `53fdf3927e9d75930e92e5c919196d19a3ae7704`.
License: MIT; see LICENSE and the packaged LICENSE.monksynth.txt.

Voice, ten-source unison and stereo delay use the original native DSP. All source
and delay storage is allocated before playback; the audio callback does not allocate.
Tracker chords each own an independent synth. No plugin SDK is required.

`voice.c`, `voice.h`, `delay.c`, `delay.h` and `synth_internal.h` are unchanged.
`synth.c` casts calloc for C++; `synth.h` omits a duplicate utility declaration
already provided by voice.h. C++ bridges expose the original envelope enumerators.

All sixteen original parameters are saved in instruments and songs. Normalized
values use 00..FF; unison uses 01..0A voices. Attack, decay and release span 0..5
seconds, detune 0..50 cents, voice spread 0..0.5. Pitch and vowel glide targets
are updated only when changed, so repeated tracker ticks do not restart ramps.
The tracker post-filter and duplicate envelope are removed.
