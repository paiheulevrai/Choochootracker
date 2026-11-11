#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/scwf_voice.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

TEST_CASE("2xSCWF mixes two forward-looping oscillators and keeps the detune scale musical") {
  int16_t a[] = {0, 16000, 0, -16000};
  int16_t b[] = {16000, 0, -16000, 0};
  InstrumentSCWF instrument;
  std::memset(&instrument, 0, sizeof(instrument));
  instrument.oscillator[0].data = a;
  instrument.oscillator[0].frameCount = 4;
  instrument.oscillator[0].channels = 1;
  instrument.oscillator[1].data = b;
  instrument.oscillator[1].frameCount = 4;
  instrument.oscillator[1].channels = 1;
  instrument.mix = 128;
  instrument.sustain = 255;

  SCWFVoice voice;
  float output[128 * 2];
  voice.init(48000.0f);
  voice.configure(&instrument, 6000.0f, 1.0f, 0, instrument.mix, 20000, 0);
  voice.noteOn();
  voice.render(output, 128);
  double energy = 0.0;
  for (float sample : output) { CHECK(std::isfinite(sample)); energy += std::fabs(sample); }
  CHECK(energy > 0.0);
  CHECK(scwfDetuneCents(1) == 5);
  CHECK(scwfDetuneCents(20) == 100);
  CHECK(scwfDetuneCents(21) == 110);
  CHECK(scwfDetuneCents(SCWF_DETUNE_MAX) == 2400);
  CHECK(scwfFrequencyHz(6900) == doctest::Approx(440.0));
}

TEST_CASE("2xSCWF reads a complete waveform once per requested period") {
  int16_t waveform[] = {-16000, 0, 16000, 0};
  InstrumentSCWF instrument;
  std::memset(&instrument, 0, sizeof(instrument));
  instrument.oscillator[0].data = waveform;
  instrument.oscillator[0].frameCount = 4;
  instrument.oscillator[0].channels = 1;
  instrument.sustain = 255;

  SCWFVoice voice;
  std::vector<float> output(48000 * 2);
  voice.init(48000.0f);
  voice.configure(&instrument, 5700.0f, 1.0f, 0, 0, 20000, 0); // A3 = 220 Hz
  voice.noteOn();
  voice.render(output.data(), 48000);

  int risingCrossings = 0;
  for (size_t i = 2; i < output.size(); i += 2)
    if (output[i - 2] < 0.0f && output[i] >= 0.0f) ++risingCrossings;
  CHECK(risingCrossings == doctest::Approx(220).epsilon(0.01));
}

TEST_CASE("BYOWTBL interpolates between independently selected frames") {
  int16_t frames[] = {0, 0, 0, 0, 16000, 16000, 16000, 16000};
  InstrumentSCWF instrument{};
  instrument.oscillator[0].data = frames;
  instrument.oscillator[0].frameCount = 8;
  instrument.oscillator[0].channels = 1;
  instrument.sustain = 255;
  uint16_t frameSize[] = {4, 0};
  uint8_t frameIndex[] = {0, 0};
  SCWFVoice voice;
  float output[2]{};
  voice.init(48000.0f);
  voice.configure(&instrument, 6000.0f, 1.0f, 0, 0, 20000, 0, frameSize, frameIndex);
  voice.setWavetablePosition(0, 127.5f);
  voice.noteOn();
  voice.render(output, 1);
  CHECK(output[0] == doctest::Approx(8000.0 / 32768.0).epsilon(0.01));
}

TEST_CASE("BYOWTBL cubic morph stays within adjacent frame values") {
  int16_t frames[] = {0, 0, 0, 0, 16000, 16000, 16000, 16000,
                      -16000, -16000, -16000, -16000};
  InstrumentSCWF instrument{};
  instrument.oscillator[0].data = frames;
  instrument.oscillator[0].frameCount = 12;
  instrument.oscillator[0].channels = 1;
  instrument.sustain = 255;
  uint16_t frameSize[] = {4, 0};
  uint8_t frameIndex[] = {0, 0};
  SCWFVoice voice;
  voice.init(48000.0f);
  voice.configure(&instrument, 6000.0f, 1.0f, 0, 0, 20000, 0, frameSize, frameIndex);
  voice.setWavetablePosition(0, 63.75f);
  voice.noteOn();
  float output[2]{};
  voice.render(output, 1);
  CHECK(output[0] >= 0.0f);
  CHECK(output[0] <= 16000.0f / 32768.0f);
}

TEST_CASE("BYOWTBL preview positions span distinct frames at C3") {
  int16_t frames[] = {0, 0, 0, 0, 16000, 16000, 16000, 16000};
  InstrumentSCWF instrument{};
  instrument.oscillator[0].data = frames;
  instrument.oscillator[0].frameCount = 8;
  instrument.oscillator[0].channels = 1;
  instrument.sustain = 255;
  uint16_t frameSize[] = {4, 0};
  uint8_t first[] = {0, 0}, last[] = {255, 0};
  float firstOutput[2]{}, lastOutput[2]{};

  SCWFVoice voice;
  voice.init(48000.0f);
  voice.configure(&instrument, 4800.0f, 1.0f, 0, 0, 20000, 0, frameSize, first);
  voice.noteOn();
  voice.render(firstOutput, 1);
  voice.configure(&instrument, 4800.0f, 1.0f, 0, 0, 20000, 0, frameSize, last);
  voice.noteOn();
  voice.render(lastOutput, 1);

  CHECK(scwfFrequencyHz(4800) == doctest::Approx(130.8128).epsilon(0.001));
  CHECK(firstOutput[0] == doctest::Approx(0.0));
  CHECK(lastOutput[0] == doctest::Approx(16000.0 / 32768.0).epsilon(0.01));
}

TEST_CASE("2xSCWF and BYOWTBL retriggers preserve audible oscillators and filter history") {
  int16_t a[] = {-16000, 0, 16000, 0, 8000, -8000, 4000, -4000};
  int16_t b[] = {12000, 4000, -12000, -4000, -8000, 0, 8000, 0};
  for (bool wavetable : {false, true}) {
    for (int character = 0; character <= 6; ++character) {
      CAPTURE(wavetable);
      CAPTURE(character);
      InstrumentSCWF instrument{};
      instrument.oscillator[0].data = a;
      instrument.oscillator[1].data = b;
      for (auto& oscillator : instrument.oscillator) {
        oscillator.frameCount = 8;
        oscillator.channels = 1;
      }
      instrument.sustain = 255;
      instrument.release = 48;
      instrument.filterEnabled = character != 0;
      instrument.filterCharacter = character;
      uint16_t frameSize[] = {4, 4};
      uint8_t frameIndex[] = {80, 175};
      SCWFVoice held, retriggered;
      for (auto* voice : {&held, &retriggered}) {
        voice->init(48000);
        voice->configure(&instrument, 6900, 1, 37, 127, 400, 80,
                         wavetable ? frameSize : nullptr, wavetable ? frameIndex : nullptr);
        voice->noteOn();
      }
      float expected[514], actual[514];
      held.render(expected, 257);
      retriggered.render(actual, 257);
      retriggered.noteOn();
      held.render(expected, 257);
      retriggered.render(actual, 257);
      float maxDifference = 0;
      for (int i = 0; i < 514; ++i)
        maxDifference = std::fmax(maxDifference, std::fabs(expected[i] - actual[i]));
      CHECK(maxDifference < 1e-6f);

      held.kill();
      retriggered.kill();
      held.noteOn();
      retriggered.noteOn();
      held.render(expected, 257);
      retriggered.render(actual, 257);
      for (int i = 0; i < 514; ++i) CHECK(expected[i] == actual[i]);
    }
  }
}

TEST_CASE("2xSCWF and BYOWTBL retrigger their envelope during release") {
  int16_t waveform[] = {16000, 16000, 16000, 16000};
  for (bool wavetable : {false, true}) {
    CAPTURE(wavetable);
    InstrumentSCWF instrument{};
    instrument.oscillator[0].data = waveform;
    instrument.oscillator[0].frameCount = 4;
    instrument.oscillator[0].channels = 1;
    instrument.sustain = 255;
    instrument.release = 48;
    instrument.envelopeShape = 128;
    uint16_t frameSize[] = {4, 0};
    SCWFVoice voice;
    float output[512];
    voice.init(48000);
    voice.configure(&instrument, 6900, 1, 0, 0, 20000, 0,
                    wavetable ? frameSize : nullptr);
    voice.noteOn();
    voice.render(output, 256);
    voice.noteOff();
    voice.render(output, 256);
    const float previous = voice.envelopeLevel();
    CHECK(previous > 0);
    CHECK(previous < 1);
    instrument.attack = 32;
    voice.configure(&instrument, 6900, 1, 0, 0, 20000, 0,
                    wavetable ? frameSize : nullptr);
    voice.noteOn();
    voice.render(output, 1);
    CHECK(output[0] == doctest::Approx(previous * 16000.0f / 32768.0f));
    voice.render(output, 256);
    CHECK(voice.envelopeLevel() > previous);
  }
}

TEST_CASE("2xSCWF and BYOWTBL default ADSR durations reduce clicks") {
  int16_t waveform[] = {16000, 16000, 16000, 16000};
  for (auto type : {InstrumentType::SCWF, InstrumentType::BYOWTBL}) {
    CAPTURE((int)type);
    Instrument instrument{};
    getInstrumentFunctions(type).init(&instrument);
    auto& p = instrument.chip.scwf;
    p.oscillator[0].data = waveform;
    p.oscillator[0].frameCount = 4;
    p.oscillator[0].channels = 1;
    uint16_t frameSize[] = {4, 0};
    SCWFVoice voice;
    voice.init(48000);
    voice.configure(&p, 6900, 1, 0, 0, 20000, 0,
                    type == InstrumentType::BYOWTBL ? frameSize : nullptr);
    float frame[2];
    voice.noteOn();
    int attackFrames = 0;
    while (voice.envelopeLevel() < 1 && attackFrames < 48000) {
      voice.render(frame, 1);
      ++attackFrames;
    }
    CHECK(attackFrames >= 96);
    CHECK(attackFrames <= 480);
    voice.noteOff();
    int releaseFrames = 0;
    while (voice.envelopeLevel() > 0 && releaseFrames < 48000) {
      voice.render(frame, 1);
      ++releaseFrames;
    }
    CHECK(releaseFrames >= 480);
    CHECK(releaseFrames <= 2400);
  }
}

TEST_CASE("2xSCWF and BYOWTBL retain the filter state on repeated constant waves") {
  int16_t waveform[] = {16000, 16000, 16000, 16000};
  for (bool wavetable : {false, true}) {
    for (int character = 1; character <= 6; ++character) {
      CAPTURE(wavetable);
      CAPTURE(character);
      InstrumentSCWF instrument{};
      instrument.oscillator[0].data = waveform;
      instrument.oscillator[0].frameCount = 4;
      instrument.oscillator[0].channels = 1;
      instrument.sustain = 255;
      instrument.filterEnabled = 1;
      instrument.filterCharacter = character;
      uint16_t frameSize[] = {4, 0};
      SCWFVoice held, retriggered;
      for (auto* voice : {&held, &retriggered}) {
        voice->init(48000);
        voice->configure(&instrument, 6900, 1, 0, 0, 400, 80,
                         wavetable ? frameSize : nullptr);
        voice->noteOn();
      }
      float expected[512], actual[512];
      for (int block = 0; block < 8; ++block) {
        held.render(expected, 256);
        retriggered.render(actual, 256);
      }
      retriggered.noteOn();
      held.render(expected, 256);
      retriggered.render(actual, 256);
      float maxDifference = 0;
      for (int i = 0; i < 512; ++i)
        maxDifference = std::fmax(maxDifference, std::fabs(expected[i] - actual[i]));
      CHECK(maxDifference < 1e-6f);
    }
  }
}

TEST_CASE("2xSCWF and BYOWTBL restart cleanly after an instant release") {
  int16_t waveform[] = {16000, 0, -16000, 0};
  for (bool wavetable : {false, true}) {
    CAPTURE(wavetable);
    InstrumentSCWF instrument{};
    instrument.oscillator[0].data = waveform;
    instrument.oscillator[0].frameCount = 4;
    instrument.oscillator[0].channels = 1;
    instrument.sustain = 255;
    instrument.filterEnabled = 1;
    instrument.filterCharacter = 1;
    uint16_t frameSize[] = {4, 0};
    SCWFVoice restarted, fresh;
    for (auto* voice : {&restarted, &fresh}) {
      voice->init(48000);
      voice->configure(&instrument, 6900, 1, 0, 0, 400, 80,
                       wavetable ? frameSize : nullptr);
    }
    float expected[514], actual[514];
    restarted.noteOn();
    restarted.render(actual, 257);
    restarted.noteOff();
    restarted.noteOn();
    fresh.noteOn();
    restarted.render(actual, 257);
    fresh.render(expected, 257);
    for (int i = 0; i < 514; ++i) CHECK(actual[i] == expected[i]);
  }
}

TEST_CASE("2xSCWF and BYOWTBL phrase retriggers stay continuous directly and through Layer") {
  int16_t waveform[] = {-16000, 0, 16000, 0, 8000, -8000, 4000, -4000};
  for (auto type : {InstrumentType::SCWF, InstrumentType::BYOWTBL}) {
    for (bool layer : {false, true}) {
      CAPTURE((int)type);
      CAPTURE(layer);
      using State = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>;
      State held(chipnomadCreate(), chipnomadDestroy);
      State repeated(chipnomadCreate(), chipnomadDestroy);
      for (auto* state : {held.get(), repeated.get()}) {
        REQUIRE(state);
        auto& p = state->project;
        p.chipsCount = p.tracksCount = 1;
        p.tickRate = 50;
        p.linearPitch = 1;
        calculateLinearPitchTable12TET(&p);
        getInstrumentFunctions(type).init(&p.instruments[1]);
        auto& scwf = p.instruments[1].chip.scwf;
        scwf.oscillator[0].data = waveform;
        scwf.oscillator[0].frameCount = 8;
        scwf.oscillator[0].channels = 1;
        scwf.mix = 0;
        scwf.filterEnabled = 1;
        scwf.filterCharacter = 1;
        scwf.filterCutoffHz = 400;
        scwf.filterResonance = 80;
        scwf.attack = scwf.decay = 0;
        scwf.sustain = 255;
        if (type == InstrumentType::BYOWTBL) {
          p.instruments[1].chip.byowtbl.frameSize[0] = 4;
          p.instruments[1].chip.byowtbl.frameIndex[0] = 100;
        }
        p.instruments[1].volume = 255;
        if (layer) {
          getInstrumentFunctions(InstrumentType::Layer).init(&p.instruments[0]);
          p.instruments[0].chip.layer.entries[0] = {1, 128, 255};
          p.instruments[0].volume = 255;
        }
        p.trackVolume[0] = 100;
        p.trackReverbSend[0] = p.trackDelaySend[0] = 0;
        p.reverbReturn = p.delayReturn = 0;
        for (auto& speed : p.grooves[0].speed) speed = 3;
        p.song[0][0] = 0;
        p.chains[0].rows[0].phrase = 0;
        for (int row = 0; row < 16; ++row) {
          if (state == held.get() && row != 0) continue;
          auto& note = p.phrases[0].rows[row];
          note.note = 48;
          note.instrument = layer ? 0 : 1;
          note.volume = PHRASE_VOLUME_MAX;
        }
        state->ownsProjectResources = 0;
        chipnomadInitChips(state, 48000, nullptr);
        REQUIRE(chipnomadReserveRenderBuffers(state, 960) == 0);
        REQUIRE(chipnomadQueuePlaybackStartSong(state, 0, 0, 1));
      }
      float a[1920], b[1920], maxDifference = 0, energy = 0;
      for (int tick = 0; tick < 12; ++tick) {
        REQUIRE(chipnomadRender(held.get(), a, 960) == 960);
        REQUIRE(chipnomadRender(repeated.get(), b, 960) == 960);
        for (int i = 0; i < 1920; ++i) {
          energy += std::fabs(a[i]);
          maxDifference = std::fmax(maxDifference, std::fabs(a[i] - b[i]));
        }
      }
      CHECK(energy > .01f);
      CHECK(maxDifference < 1e-6f);
    }
  }
}
