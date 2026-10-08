#include <chipnomad_lib.h>
#include <project_utils.h>
#include "import_m8s.h"
#include "import_common.h"
#include <m8s_format.h>
#include <synth/multimode_filter.h>
#include <synth/sample_voice.h>

#include <math.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copyName(char* dest, size_t destSize, const uint8_t* src, size_t srcSize) {
  size_t len = 0;
  while (len < srcSize && src[len] != 0 && src[len] != M8S_EMPTY && len < destSize - 1) {
    dest[len] = (char)src[len];
    len++;
  }
  dest[len] = 0;
}

// M8 filter types: 0 off, 1 LP, 2 HP, 3 BP, 4 band stop, 5 LP>HP, 6 ZDF LP, 7 ZDF HP.
// The cutoff curve of the M8 is not documented: exponential 20 Hz - 20 kHz.
static void convertFilter(InstrumentVoicePostSettings* post, const uint8_t* f) {
  static const int8_t modes[8] = {-1, 0, 2, 1, -1, -1, 0, 2}; // CCT: 0 LP, 1 BP, 2 HP
  int mode = f[0] < 8 ? modes[f[0]] : -1;
  post->filterEnabled = mode >= 0;
  if (mode < 0) return;
  post->filterMode = (uint8_t)mode;
  double ratio = (double)FILTER_CUTOFF_MAX_HZ / FILTER_CUTOFF_MIN_HZ;
  post->filterCutoffHz = (uint16_t)(FILTER_CUTOFF_MIN_HZ * pow(ratio, f[1] / 255.0) + 0.5);
  post->filterResonance = f[2];
}

// MacroSynth is Braids: shapes 0-46 are Braids models 0-46 in the same order.
// Timbre and color are 8 bit on the M8 and 15 bit here. Shape 47 (Morse
// noise) has no equivalent and stays AY.
static bool convertMacroSynth(Instrument* inst, const uint8_t* src) {
  if (src[M8S_MACRO_SHAPE] >= M8S_MACRO_SHAPE_COUNT) return false;
  getInstrumentFunctions(InstrumentType::Braids).init(inst);
  InstrumentBraids* b = &inst->chip.braids;
  b->model = src[M8S_MACRO_SHAPE];
  b->timbre = (uint16_t)(src[M8S_MACRO_TIMBRE] * 129);
  b->color = (uint16_t)(src[M8S_MACRO_COLOR] * 129);
  inst->pan = src[M8S_MACRO_PAN];
  convertFilter(b, src + M8S_MACRO_FILTER);
  return true;
}

static bool fileExists(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  fclose(f);
  return true;
}

static bool isSeparator(char c) { return c == '/' || c == '\\'; }

// Tries root + suffix for each suffix of the M8 path (Samples/A/B.wav,
// A/B.wav, B.wav) so that root can be the SD card copy, its Samples folder or
// any subfolder. The first component is also tried as Samples and samples,
// as Linux paths are case sensitive. The bare file name is only tried when
// allowBase is set, to avoid matching unrelated files in distant parents.
static bool findUnder(const char* root, const char* samplePath, bool allowBase, char* out, size_t outSize) {
  const char* p = samplePath;
  while (isSeparator(*p)) p++;
  bool first = true;
  while (*p) {
    const char* slash = p;
    while (*slash && !isSeparator(*slash)) slash++;
    bool isBase = *slash == 0;
    if (!isBase || allowBase) {
      for (int variant = 0; variant < (first ? 3 : 1); variant++) {
        const char* names[3] = {p, "Samples", "samples"};
        char tail[512];
        if (variant == 0) snprintf(tail, sizeof(tail), "%s", p);
        else if (first && strncasecmp(p, "samples", 7) == 0 && (p[7] == '/' || p[7] == '\\')) snprintf(tail, sizeof(tail), "%s%s", names[variant], p + 7);
        else continue;
        snprintf(out, outSize, "%s/%s", root, tail);
        if (fileExists(out)) return true;
      }
    }
    if (isBase) break;
    p = slash + 1;
    first = false;
  }
  return false;
}

// The .m8s holds only the path of the sample on the M8 SD card
// (/Samples/Drums/Kick.wav). Look for the WAV under the folder of the .m8s
// and each of its parents (a copy of the SD card layout), then under the
// app's sample folder and its parents. When nothing is found the M8 path is
// kept as is and the instrument stays silent until the file is picked.
static bool findSampleFile(const char* m8sPath, const char* extraDir, const char* samplePath, char* out, size_t outSize) {
  for (int s = 0; s < 2; s++) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", s == 0 ? m8sPath : extraDir ? extraDir : "");
    if (!dir[0]) continue;
    if (s == 0) {
      char* cut = strrchr(dir, '/');
#ifdef _WIN32
      char* back = strrchr(dir, '\\');
      if (back && (!cut || back > cut)) cut = back;
#endif
      if (!cut) continue;
      *cut = 0; // the folder of the .m8s
    }
    for (int level = 0; level < 8 && dir[0]; level++) {
      if (findUnder(dir, samplePath, level == 0, out, outSize)) return true;
      char* cut = dir + strlen(dir);
      while (cut > dir && !isSeparator(*cut)) cut--;
      *cut = 0;
    }
  }
  return false;
}

static bool convertSampler(Instrument* inst, const uint8_t* src, const char* m8sPath, const char* sampleDir) {
  getInstrumentFunctions(InstrumentType::Sample).init(inst);
  InstrumentSample* s = &inst->chip.sample;
  char m8Path[M8S_SAMPLER_PATH_SIZE + 1];
  copyName(m8Path, sizeof(m8Path), src + M8S_SAMPLER_PATH, M8S_SAMPLER_PATH_SIZE);
  char found[PROJECT_SAMPLE_PATH_LENGTH + 1];
  if (m8Path[0] && findSampleFile(m8sPath, sampleDir, m8Path, found, sizeof(found))) {
    snprintf(s->path, sizeof(s->path), "%s", found);
    char error[64];
    sampleLoadWav16(s->path, s, error, sizeof(error));
  } else {
    snprintf(s->path, sizeof(s->path), "%s", m8Path);
  }
  // Play modes: 0 fwd, 1 rev, 2 fwd loop, 3 rev loop, 4 fwd ping-pong, 5 rev
  // ping-pong, 6-8 oscillator. Reverse playback does not exist here.
  uint8_t mode = src[M8S_SAMPLER_PLAY_MODE];
  s->loopMode = (mode == 2 || mode == 3 || mode == 6 || mode == 7) ? 1 : (mode == 4 || mode == 5 || mode == 8) ? 2 : 0;
  int start = src[M8S_SAMPLER_START];
  int length = src[M8S_SAMPLER_LENGTH];
  s->start = (uint8_t)start;
  s->end = (length == 0xFF || start + length > 255) ? 255 : (uint8_t)(start + length);
  // A CCT sample plays at its original pitch on index 48 (MIDI 60). The M8
  // plays it at its original pitch on its C-4, MIDI note 36, hence index 24
  // here: raise the sample by two octaves (checked by ear).
  s->pitch = M8S_SAMPLER_PITCH_OFFSET;
  inst->pan = src[M8S_SAMPLER_PAN];
  convertFilter(s, src + M8S_SAMPLER_FILTER);
  return true;
}

// There is no 1:1 FM engine: the YM2612 (Genesis FM) is the closest. The M8
// has 12 algorithms and the YM2612 8; 0-7 map directly and the rest are a
// best guess. Operators keep ratio (multiplier, 15 max) and level; envelopes
// are a plain sustained organ shape and the operator waveforms are lost.
static bool convertFmSynth(Instrument* inst, const uint8_t* src) {
  getInstrumentFunctions(InstrumentType::GenesisFM).init(inst);
  InstrumentFourOp* p = &inst->chip.fourOp;
  static const uint8_t algorithms[12] = {0, 1, 2, 3, 4, 5, 6, 7, 5, 5, 6, 7};
  p->algorithm = algorithms[src[M8S_FM_ALGO] % 12];
  int feedback = 0;
  for (int i = 0; i < 4; i++) {
    FourOpOperator* o = &p->operators[i];
    int ratio = src[M8S_FM_RATIO + i * 2];
    int level = src[M8S_FM_LEVEL + i * 2];
    o->multiplier = (uint8_t)(ratio > 15 ? 15 : ratio);
    o->detune = 0;
    o->level = (uint8_t)(127 - level * 127 / 255);
    o->attack = 31;
    o->decay = 0;
    o->sustainRate = 0;
    o->sustainLevel = 0;
    o->release = 7;
    int fb = src[M8S_FM_LEVEL + i * 2 + 1];
    if (fb > feedback) feedback = fb;
  }
  p->feedback = (uint8_t)(feedback * 7 / 255);
  return true;
}

// WavSynth and HyperSynth go to aChChid, the closest one-voice synth. Pulse
// shapes and saw keep their waveform; the other shapes use a Braids model.
// HyperSynth is a supersaw, so it becomes SAW-SWARM with swarm and width.
static bool convertToAChChid(Instrument* inst, const uint8_t* src, bool hyper) {
  getInstrumentFunctions(InstrumentType::AChChid).init(inst);
  InstrumentAChChid* a = &inst->chip.achchid;
  const uint8_t* filter;
  if (hyper) {
    a->wave = AChChidWave::braids;
    a->model = 14;
    a->timbre = (uint16_t)(src[M8S_HYPER_SWARM] * 129);
    a->color = (uint16_t)(src[M8S_HYPER_WIDTH] * 129);
    filter = src + M8S_HYPER_FILTER;
  } else {
    uint8_t shape = src[M8S_WAV_SHAPE];
    if (shape <= 3) a->wave = AChChidWave::square;
    else if (shape == 4) a->wave = AChChidWave::saw;
    else {
      a->wave = AChChidWave::braids;
      a->model = shape == 5 || shape == 6 ? 3 : shape <= 8 ? 41 : 37; // SINE-TRI, FILTER-NOISE, WAVETABLES
      a->timbre = shape == 6 ? 0 : (uint16_t)(src[M8S_WAV_SCAN] * 129);
    }
    filter = src + M8S_WAV_SCAN + 1;
  }
  if (filter[0] >= 1 && filter[0] < 8) {
    double ratio = (double)FILTER_CUTOFF_MAX_HZ / 200;
    a->cutoff = (uint16_t)(200 * pow(ratio, filter[1] / 255.0) + 0.5);
    a->resonance = (uint8_t)(filter[2] * 100 / 255);
  } else {
    a->cutoff = FILTER_CUTOFF_MAX_HZ;
  }
  return true;
}

// MIDIOut: channel, bank select, program change and the first four custom CC
// numbers (their values are not used here, the phrase FX carry the value).
// The M8 port (MIDI/USB) has no equivalent.
static bool convertMidiOut(Instrument* inst, const uint8_t* src) {
  getInstrumentFunctions(InstrumentType::Midi).init(inst);
  InstrumentMidi* m = &inst->chip.midi;
  m->channel = src[M8S_MIDI_CHANNEL] & 0x0F;
  m->bankHigh = src[M8S_MIDI_BANK] < 128 ? src[M8S_MIDI_BANK] : EMPTY_VALUE_8;
  m->program = src[M8S_MIDI_PROGRAM] < 128 ? src[M8S_MIDI_PROGRAM] : EMPTY_VALUE_8;
  for (int i = 0; i < 4; i++) {
    uint8_t cc = src[M8S_MIDI_CC + i * 2];
    m->ccNumber[i] = cc < 128 ? cc : EMPTY_VALUE_8;
  }
  return true;
}

static bool convertInstrument(Instrument* inst, const uint8_t* src, const char* m8sPath, const char* sampleDir) {
  switch (src[0]) {
    case M8S_INST_WAVSYNTH: return convertToAChChid(inst, src, false);
    case M8S_INST_MACROSYNTH: return convertMacroSynth(inst, src);
    case M8S_INST_SAMPLER: return convertSampler(inst, src, m8sPath, sampleDir);
    case M8S_INST_MIDIOUT: return convertMidiOut(inst, src);
    case M8S_INST_FMSYNTH: return convertFmSynth(inst, src);
    case M8S_INST_HYPERSYNTH: return convertToAChChid(inst, src, true);
  }
  return false;
}

int projectLoadM8S(Project* project, const char* path) {
  return projectLoadM8SWithSamples(project, path, NULL);
}

int projectLoadM8SWithSamples(Project* project, const char* path, const char* sampleDir) {
  if (!project || !path) return 1;

  FILE* f = fopen(path, "rb");
  if (!f) {
    snprintf(projectFileError, 40, "Cannot open M8 file");
    return 1;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size < M8S_MIN_FILE_SIZE || size > M8S_MAX_FILE_SIZE) {
    fclose(f);
    snprintf(projectFileError, 40, "Not a valid M8 song");
    return 1;
  }
  uint8_t* data = (uint8_t*)malloc(size);
  size_t got = data ? fread(data, 1, size, f) : 0;
  fclose(f);
  if (!data || got != (size_t)size) {
    free(data);
    snprintf(projectFileError, 40, "Cannot read M8 file");
    return 1;
  }

  int major = data[M8S_VERSION_MSB_OFFSET] & 0x0F;
  if (memcmp(data, M8S_MAGIC, strlen(M8S_MAGIC)) != 0 || major < 2 || major > 4) {
    free(data);
    snprintf(projectFileError, 40, major > 4 ? "M8 version not supported" : "Not a valid M8 song");
    return 1;
  }

  Project p;
  projectInitAY(&p);

  float bpm;
  memcpy(&bpm, data + M8S_TEMPO_OFFSET, sizeof(bpm));
  if (!(bpm >= 30.0f && bpm <= 300.0f)) bpm = 120.0f;
  double stepSeconds = 60.0 / bpm / M8S_STEPS_PER_BEAT;
  p.tickRate = (float)(M8S_GROOVE_TICKS / stepSeconds);

  copyName(p.title, sizeof(p.title), data + M8S_NAME_OFFSET, M8S_NAME_SIZE);

  // Song: one chain index per track, 0xFF empty.
  for (int row = 0; row < M8S_SONG_ROWS && row < PROJECT_MAX_LENGTH; row++) {
    for (int t = 0; t < M8S_TRACKS && t < PROJECT_MAX_TRACKS; t++) {
      uint8_t chain = data[M8S_SONG_OFFSET + row * M8S_TRACKS + t];
      p.song[row][t] = (chain < M8S_CHAINS && chain < PROJECT_MAX_CHAINS) ? chain : EMPTY_VALUE_16;
    }
  }

  // Chains: (phrase, transpose) pairs. Transpose is a signed semitone offset.
  for (int c = 0; c < M8S_CHAINS && c < PROJECT_MAX_CHAINS; c++) {
    chainClear(&p.chains[c]);
    for (int s = 0; s < M8S_CHAIN_STEPS; s++) {
      const uint8_t* step = data + M8S_CHAINS_OFFSET + (c * M8S_CHAIN_STEPS + s) * 2;
      if (step[0] < M8S_PHRASES) {
        p.chains[c].rows[s].phrase = step[0];
        p.chains[c].rows[s].transpose = step[1];
      }
    }
  }

  // Phrases: note, velocity, instrument, then three FX pairs (ignored).
  // An M8 note value is a MIDI note number and pitch table index N is MIDI
  // note 12+N (same rule as import_midi.cpp), so index = value - 12. The M8
  // labels it two octaves higher than we do, but the sound is the same. The
  // lowest octave (sub-audio) clamps to 0.
  uint8_t instrumentUsed[M8S_INSTRUMENTS] = {0};
  for (int ph = 0; ph < M8S_PHRASES && ph < PROJECT_MAX_PHRASES; ph++) {
    phraseClear(&p.phrases[ph]);
    for (int s = 0; s < M8S_PHRASE_STEPS; s++) {
      const uint8_t* step = data + M8S_PHRASES_OFFSET + (ph * M8S_PHRASE_STEPS + s) * M8S_PHRASE_STEP_SIZE;
      PhraseRow* dest = &p.phrases[ph].rows[s];
      initEmptyPhraseRow(dest);

      uint8_t note = step[0];
      if (note == M8S_EMPTY) continue;
      if (note >= M8S_NOTE_OFF) {
        dest->note = NOTE_OFF;
        continue;
      }
      int index = note >= M8S_INDEX_OFFSET ? note - M8S_INDEX_OFFSET : 0;
      if (p.pitchTable.length > 0 && index >= p.pitchTable.length) index = p.pitchTable.length - 1;
      dest->note = (uint8_t)index;
      if (step[1] != M8S_EMPTY) dest->volume = step[1] > PHRASE_VOLUME_MAX ? PHRASE_VOLUME_MAX : step[1];
      if (step[2] < M8S_INSTRUMENTS && step[2] < PROJECT_MAX_INSTRUMENTS) {
        dest->instrument = step[2];
        instrumentUsed[step[2]] = 1;
      }
    }
  }

  // Instruments: every referenced slot gets a default AY instrument named
  // after the M8 instrument (see convertInstrument), so notes are
  // audible right away.
  for (int i = 0; i < M8S_INSTRUMENTS && i < PROJECT_MAX_INSTRUMENTS; i++) {
    if (!instrumentUsed[i]) continue;
    Instrument* inst = &p.instruments[i];
    const uint8_t* src = data + M8S_INSTRUMENTS_OFFSET + i * M8S_INSTRUMENT_SIZE;
    if (!convertInstrument(inst, src, path, sampleDir)) {
      getInstrumentFunctions(InstrumentType::AY1).init(inst);
      inst->type = InstrumentType::AY1;
    }
    char name[M8S_INSTRUMENT_NAME_SIZE + 1];
    copyName(name, sizeof(name), src + 1, M8S_INSTRUMENT_NAME_SIZE);
    if (name[0]) snprintf(inst->name, sizeof(inst->name), "%s", name);
    else snprintf(inst->name, sizeof(inst->name), "M8 %02X", i);
  }

  free(data);
  projectFree(project);
  *project = p;
  return 0;
}
