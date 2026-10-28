#include "doctest.h"
#include "project_instruments.h"
#include "project.h"
#include "project_io_common.h"
#include "synth/achchid_voice.h"
#include "synth/sid_voice.h"
#include "rosic_MipMappedWaveTable.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

TEST_CASE("Open303 prototype initialization preserves the mip tables") {
  struct Probe : rosic::MipMappedWaveTable {
    bool preservesTables() {
      for (int i = 0; i < 4; ++i) tableSet[0][i] = 42.0;
      initPrototypeTable();
      for (int i = 0; i < tableLength; ++i) if (prototypeTable[i] != 0) return false;
      for (int i = 0; i < 4; ++i) if (tableSet[0][i] != 42.0) return false;
      return true;
    }
  };
  auto* probe = new Probe;
  CHECK(probe->preservesTables());
  delete probe;
}

TEST_CASE("aChChid has native controls and dedicated FX") {
  Instrument instrument = {};
  getInstrumentFunctions(InstrumentType::AChChid).init(&instrument);
  CHECK(instrument.type == InstrumentType::AChChid);
  CHECK(instrument.chip.achchid.wave == AChChidWave::saw);
  CHECK(instrument.chip.achchid.decay == 1000);
  CHECK(instrument.chip.achchid.saturation == 0);
  CHECK(instrument.chip.achchid.sidShape == 4);
  CHECK(instrument.chip.achchid.sidPulseWidth == 2048);
  CHECK(instrumentVoicePostSettings(&instrument) == nullptr);
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxASL));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxATY));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxADC));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxAAC));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxATM));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxACL));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxACF));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxARS));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxAEM));
  CHECK_FALSE(instrumentFXAvailable(InstrumentType::AChChid, fxBMD));
}

TEST_CASE("aChChid output is softly bounded before the mixer") {
  AChChidVoice voice;
  voice.init(48000.0f);
  voice.configure(1, 0, 0, 16384, 16384, 0, 4, 2048, 400, 100, 100, 2000, 100, 4.0f);
  voice.noteOn(48, true, false, 0);
  std::vector<float> output(4096);
  voice.render(output.data(), (int)output.size());
  for (float sample : output) {
    CHECK(std::isfinite(sample));
    CHECK(std::fabs(sample) <= 0.85001f);
  }
}

TEST_CASE("aChChid Braids Shaper visibly reshapes the oscillator") {
  AChChidVoice clean;
  AChChidVoice asym;
  clean.init(48000.0f);
  asym.init(48000.0f);
  clean.configure(2, 0, 0, 16384, 16384, 0, 4, 2048, 20000, 0, 0, 2000, 0, 1.0f);
  asym.configure(2, 0, 0, 16384, 16384, 255, 4, 2048, 20000, 0, 0, 2000, 0, 1.0f);
  clean.noteOn(48, false, false, 0);
  asym.noteOn(48, false, false, 0);
  std::vector<float> cleanOutput(4096), asymOutput(4096);
  clean.render(cleanOutput.data(), (int)cleanOutput.size());
  asym.render(asymOutput.data(), (int)asymOutput.size());

  float difference = 0.0f;
  for (size_t i = 1024; i < cleanOutput.size(); ++i)
    difference += std::fabs(cleanOutput[i] - asymOutput[i]);
  CHECK(difference / (cleanOutput.size() - 1024) > 0.05f);
}

TEST_CASE("aChChid Braids oscillator keeps Open303's octave at 48 kHz") {
  AChChidVoice voice;
  voice.init(48000.0f);
  voice.configure(2, 0, 0, 0, 0, 0, 4, 2048, 20000, 0, 0, 2000, 0, 1.0f);
  voice.noteOn(45, false, false, 0); // A3 = MIDI 57 = 220 Hz.
  std::vector<float> output(48000);
  voice.render(output.data(), (int)output.size());

  int crossings = 0;
  for (size_t i = 4801; i < output.size(); ++i)
    if (output[i - 1] <= 0.0f && output[i] > 0.0f) ++crossings;
  CHECK(crossings == doctest::Approx(198).epsilon(0.1));
}

TEST_CASE("aChChid SID uses all eight chip waveforms and pulse width") {
  SIDOscillator sid;
  sid.init(48000);
  std::vector<float> output(4096);
  for (int shape = 1; shape <= 8; ++shape) {
    sid.configure((uint8_t)shape, 2048);
    sid.noteOn(false);
    for (float& sample : output) sample = sid.sample(220);
    float minimum = 1, maximum = -1;
    for (float sample : output) {
      CHECK(std::isfinite(sample));
      minimum = std::min(minimum, sample);
      maximum = std::max(maximum, sample);
    }
    CHECK(maximum - minimum > 0.01f);
  }
  sid.configure(4, 1024); sid.noteOn(false);
  float narrow = 0;
  for (float& sample : output) { sample = sid.sample(220); narrow += sample; }
  sid.configure(4, 3072); sid.noteOn(false);
  float wide = 0;
  for (float& sample : output) { sample = sid.sample(220); wide += sample; }
  CHECK(std::fabs(narrow - wide) > 500);
}

TEST_CASE("aChChid SID tracks A3 and its Shaper changes the 303 output") {
  SIDOscillator sid; sid.init(48000); sid.configure(2, 2048); sid.noteOn(false);
  int crossings = 0; float previous = 0;
  for (int i = 0; i < 48000; ++i) {
    float sample = sid.sample(220);
    if (i > 4800 && previous <= 0 && sample > 0) ++crossings;
    previous = sample;
  }
  CHECK(crossings == doctest::Approx(198).epsilon(0.03));

  AChChidVoice clean, shaped;
  clean.init(48000); shaped.init(48000);
  clean.configure(3, 0, 0, 16384, 16384, 0, 2, 2048, 20000, 0, 0, 2000, 0, 1);
  shaped.configure(3, 0, 0, 16384, 16384, 255, 2, 2048, 20000, 0, 0, 2000, 0, 1);
  clean.noteOn(45, false, false, 0); shaped.noteOn(45, false, false, 0);
  float difference = 0, level = 0;
  std::vector<float> a(4096), b(4096);
  clean.render(a.data(), (int)a.size()); shaped.render(b.data(), (int)b.size());
  for (size_t i = 1024; i < a.size(); ++i) {
    difference += std::fabs(a[i] - b[i]); level += std::fabs(a[i]);
    CHECK(std::isfinite(a[i])); CHECK(std::fabs(a[i]) <= 0.85001f);
    CHECK(std::isfinite(b[i])); CHECK(std::fabs(b[i]) <= 0.85001f);
  }
  CHECK(level > 10);
  CHECK(difference > 10);
}

TEST_CASE("aChChid SID glides into the next pitch") {
  AChChidVoice voice;
  voice.init(48000);
  voice.configure(3, 0, 0, 16384, 16384, 0, 2, 2048, 20000, 0, 0, 2000, 0, 1);
  voice.noteOn(45, false, false, 0);
  std::vector<float> warm(4800), early(960), gap(9600), late(960);
  voice.render(warm.data(), (int)warm.size());
  voice.noteOn(57, false, true, 0);
  voice.render(early.data(), (int)early.size());
  voice.render(gap.data(), (int)gap.size());
  voice.render(late.data(), (int)late.size());
  auto crossings = [](const std::vector<float>& samples) {
    int count = 0;
    for (size_t i = 1; i < samples.size(); ++i)
      if (samples[i - 1] <= 0 && samples[i] > 0) ++count;
    return count;
  };
  CHECK(crossings(early) >= 3);
  CHECK(crossings(late) > crossings(early) + 1);
  CHECK(crossings(late) >= 7);
}

TEST_CASE("aChChid SID settings survive save and old instruments get defaults") {
  Instrument saved{};
  getInstrumentFunctions(InstrumentType::AChChid).init(&saved);
  saved.chip.achchid.wave = AChChidWave::sid;
  saved.chip.achchid.sidShape = 7;
  saved.chip.achchid.sidPulseWidth = 1234;
  FILE* file = tmpfile(); REQUIRE(file != nullptr);
  REQUIRE(instrumentSaveData(file, 0, &saved) == 0);
  rewind(file); resetPeekConsume();
  char line[128]; for (int i = 0; i < 3; ++i) REQUIRE(fgets(line, sizeof(line), file) != nullptr);
  Instrument loaded{};
  REQUIRE(instrumentLoadData(file, &loaded, nullptr) == 0);
  CHECK(loaded.chip.achchid.wave == AChChidWave::sid);
  CHECK(loaded.chip.achchid.sidShape == 7);
  CHECK(loaded.chip.achchid.sidPulseWidth == 1234);
  fclose(file); resetPeekConsume();

  file = tmpfile(); REQUIRE(file != nullptr);
  fprintf(file, "- Name: Old Acid\n- Type: %u\n- Table speed: 0\n- Volume: 255\n- Pan: 128\n- Transpose: 0\n- Chip data:\n- Wave: 1\n# End\n", (unsigned)InstrumentType::AChChid);
  rewind(file); Instrument old{};
  REQUIRE(instrumentLoadData(file, &old, nullptr) == 0);
  CHECK(old.chip.achchid.wave == AChChidWave::saw);
  CHECK(old.chip.achchid.sidShape == 4);
  CHECK(old.chip.achchid.sidPulseWidth == 2048);
  fclose(file); resetPeekConsume();
}
