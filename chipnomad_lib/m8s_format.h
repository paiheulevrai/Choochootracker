#ifndef __CHIPNOMAD_LIB__M8S_FORMAT_H__
#define __CHIPNOMAD_LIB__M8S_FORMAT_H__

// Layout of a Dirtywave M8 song file (.m8s), firmware 2.x - 4.x. The song,
// chain, phrase and instrument tables sit at the same fixed offsets in all
// these versions (see also AlexCharlton/m8-files). Only what the importer and
// the exporter touch is described here.

#define M8S_MAGIC "M8VERSION"
#define M8S_HEADER_SIZE 14
#define M8S_VERSION_LSB_OFFSET 10
#define M8S_VERSION_MSB_OFFSET 11
#define M8S_DIRECTORY_SIZE 128
#define M8S_TRANSPOSE_OFFSET (M8S_HEADER_SIZE + M8S_DIRECTORY_SIZE)
#define M8S_TEMPO_OFFSET (M8S_TRANSPOSE_OFFSET + 1) // little-endian float, BPM
#define M8S_NAME_OFFSET (M8S_TEMPO_OFFSET + 5)
#define M8S_NAME_SIZE 12
#define M8S_SONG_OFFSET 0x2EE
#define M8S_PHRASES_OFFSET 0xAEE
#define M8S_CHAINS_OFFSET 0x9A5E
#define M8S_INSTRUMENTS_OFFSET 0x13A3E
#define M8S_SONG_ROWS 256
#define M8S_TRACKS 8
#define M8S_PHRASES 255
#define M8S_PHRASE_STEPS 16
#define M8S_PHRASE_STEP_SIZE 9 // note, velocity, instrument, 3 x (fx command, fx value)
#define M8S_CHAINS 255
#define M8S_CHAIN_STEPS 16
#define M8S_INSTRUMENTS 128
#define M8S_INSTRUMENT_SIZE 215
#define M8S_INSTRUMENT_NAME_SIZE 12
// Instrument type ids (byte 0 of an instrument record) and MacroSynth layout.
#define M8S_INST_MACROSYNTH 1
#define M8S_MACRO_SHAPE 18 // offsets from the start of the record
#define M8S_MACRO_TIMBRE 19
#define M8S_MACRO_COLOR 20
#define M8S_MACRO_FILTER 23 // 0 off, 1 LP, 2 HP, 3 BP, 4 band stop, 5 LP>HP, 6 ZDF LP, 7 ZDF HP
#define M8S_MACRO_CUTOFF 24
#define M8S_MACRO_RES 25
#define M8S_MACRO_PAN 28
#define M8S_INST_WAVSYNTH 0
#define M8S_INST_SAMPLER 2
#define M8S_INST_MIDIOUT 3
#define M8S_MIDI_CHANNEL 16 // MIDIOut: port 15, channel, bank, program, 3 pad bytes, 10 x (CC number, value) at 22
#define M8S_MIDI_BANK 17
#define M8S_MIDI_PROGRAM 18
#define M8S_MIDI_CC 22
#define M8S_INST_FMSYNTH 4
#define M8S_INST_HYPERSYNTH 5 // firmware 3.0 and later
#define M8S_WAV_SHAPE 18 // WavSynth: shape, size, mult, warp, scan, then the filter block at 23
#define M8S_WAV_SCAN 22
#define M8S_SAMPLER_PLAY_MODE 18 // Sampler: play mode, slice, start, loop start, length, degrade
#define M8S_SAMPLER_START 20
#define M8S_SAMPLER_LENGTH 22
#define M8S_SAMPLER_FILTER 24 // then cutoff, resonance, amp, limit, pan (29)
#define M8S_SAMPLER_PAN 29
#define M8S_SAMPLER_PITCH_OFFSET 24 // semitones added to imported samples, see import_m8s.cpp
#define M8S_SAMPLER_PATH 0x57 // 128 chars, the path on the M8 SD card
#define M8S_SAMPLER_PATH_SIZE 128
#define M8S_FM_ALGO 18 // FMSynth: algo, 4 shapes, 4 x (ratio, fine), 4 x (level, feedback)
#define M8S_FM_RATIO 23
#define M8S_FM_LEVEL 31
#define M8S_HYPER_SWARM 27 // HyperSynth: 7 chord notes, scale, shift, swarm, width, subosc, filter at 30
#define M8S_HYPER_WIDTH 28
#define M8S_HYPER_FILTER 30
#define M8S_MACRO_SHAPE_COUNT 47 // shapes 0-46 are the same list as Braids models 0-46
#define M8S_EMPTY 0xFF
#define M8S_NOTE_OFF 0x80
// Pitch table index = M8 note value - this (M8 value = MIDI note, index N is
// MIDI note 12 + N). The M8 labels octaves two higher than the usual naming
// (MIDI 60 is C-6 there, C-4 here), so the same sound has a different label.
#define M8S_INDEX_OFFSET 12
#define M8S_MIN_FILE_SIZE (M8S_INSTRUMENTS_OFFSET + M8S_INSTRUMENTS * M8S_INSTRUMENT_SIZE)
#define M8S_MAX_FILE_SIZE (4 * 1024 * 1024)
#define M8S_STEPS_PER_BEAT 4
#define M8S_GROOVE_TICKS 6 // the app's default groove (see project.cpp projectInit)

#endif // __CHIPNOMAD_LIB__M8S_FORMAT_H__
