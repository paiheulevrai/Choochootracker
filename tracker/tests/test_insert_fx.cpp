#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
#include "chipnomad_lib.h"
#include "app.h"
#include "selection_popup.h"
#include "doctest.h"
#include "mocks/app_ui_mock.h"
#include "pitch_table_utils.h"
#include "playback_internal.h"
#include "screen_instrument.h"
#include "screens.h"
#include "help.h"
#include "corelib_gfx.h"

namespace {
std::vector<float> signal(int frames) {
  std::vector<float> v(frames * 2);
  for (int i = 0; i < frames; ++i) {
    v[2 * i] = .3f * sinf(i * .047f);
    v[2 * i + 1] = .17f * cosf(i * .079f);
  }
  return v;
}
void values(const InsertConfig c[2], uint8_t v[2][8]) {
  for (int i = 0; i < 2; ++i) memcpy(v[i], c[i].values, 8);
}
struct Engine {
  ChipNomadState* s = chipnomadCreate();
  Engine() {
    auto& p = s->project;
    p.chipsCount = 1;
    p.tracksCount = 1;
    p.tickRate = 50;
    p.linearPitch = 1;
    calculateLinearPitchTable12TET(&p);
    getInstrumentFunctions(InstrumentType::Braids).init(&p.instruments[0]);
    p.instruments[0].volume = 180;
    p.trackVolume[0] = 65;
    p.trackTilt[0] = 67;
    p.reverbReturn = 0;
    p.delayReturn = 0;
    p.perceptualEffects = 0;
    p.trackReverbSend[0] = 25;
    p.trackDelaySend[0] = 50;
  }
  void start() {
    chipnomadInitChips(s, 48000, nullptr);
    chipnomadQueuePlaybackPreviewNote(s, 0, 45, 0);
  }
  ~Engine() { chipnomadDestroy(s); }
};
std::string read(const char* path) {
  std::ifstream f(path);
  return std::string(std::istreambuf_iterator<char>(f), {});
}
void write(const char* path, const std::string& text) {
  std::ofstream f(path);
  f << text;
}
}  // namespace
TEST_SUITE("track inserts") {
  TEST_CASE("Rotary speed spans slow LFO rates through the existing maximum") {
    float previous = 0;
    for (int value = 0; value <= 255; ++value) {
      float hz = insertMap(insertRotary, 0, value);
      CHECK(hz > previous);
      previous = hz;
    }
    CHECK(insertMap(insertRotary, 0, 0) == doctest::Approx(0.01f));
    CHECK(insertMap(insertRotary, 0, 255) == doctest::Approx(25.f));
    CHECK(insertMap(insertRotary, 0, insertDescriptor(insertRotary).parameters[0].initial)
          == doctest::Approx(0.8f).epsilon(0.02));
    // Count real left/right amplitude cycles on steady input, after the
    // initial delay fill and bypass fade. This exercises the DSP's rate too.
    for (int speed : {0, 143, 255}) {
      InsertConfig config[2]{};
      insertSelect(&config[0], insertRotary);
      insertEdit(&config[0], 0, speed);
      insertEdit(&config[0], 1, 255);
      insertEdit(&config[0], 3, 255);
      InsertAutomation automation{};
      InsertChain chain(48000);
      chain.sync(config, &automation);
      uint8_t effective[2][8];
      values(config, effective);
      const int seconds = speed == 0 ? 105 : 4;
      std::vector<float> audio(seconds * 48000 * 2, 0.25f);
      chain.process(audio.data(), seconds * 48000, effective);
      int crossings = 0;
      int firstCrossing = 0;
      for (int frame = 48001; frame < seconds * 48000; ++frame) {
        float before = audio[2 * frame - 1] - audio[2 * frame - 2];
        float now = audio[2 * frame + 1] - audio[2 * frame];
        if (before <= 0 && now > 0) {
          if (!crossings) firstCrossing = frame;
          ++crossings;
        }
      }
      CHECK(std::abs(crossings - (seconds - 1) * insertMap(insertRotary, 0, speed)) <= 1);
      if (speed == 0) CHECK(firstCrossing / 48000.f == doctest::Approx(100.f).epsilon(0.001));
    }
  }

  TEST_CASE("tracker insert popup describes every module on the selected track and slot") {
    auto* previous = chipnomadState;
    auto* previousScreen = currentScreen;
    chipnomadState = chipnomadCreate();
    screensInitAll();
    currentScreen = &screenPhrase;
    // A deliberately different first track catches accidental track-zero lookup.
    insertSelect(&chipnomadState->project.trackInserts[0][0], insertDistortion);
    *pSongTrack = 3;
    for (int module = 0; module < insertModuleCount; ++module) {
      for (int slot = 0; slot < 2; ++slot) {
        insertSelect(&chipnomadState->project.trackInserts[3][slot], module);
        const auto& descriptor = insertDescriptor(module);
        for (int parameter = 0; parameter < 8; ++parameter) {
          CAPTURE(module);
          CAPTURE(slot);
          CAPTURE(parameter);
          auto fx = FX(fxF11 + slot * 8 + parameter);
          std::string description = helpFXDescription(fx, EMPTY_VALUE_8);
          size_t lineStart = 0;
          do {
            size_t lineEnd = description.find('\n', lineStart);
            if (lineEnd == std::string::npos) lineEnd = description.size();
            CHECK(lineEnd - lineStart <= 33); // keep clear of the track sidebar
            lineStart = lineEnd + 1;
          } while (lineStart < description.size());
          CHECK(description.find(descriptor.name) != std::string::npos);
          if (parameter < descriptor.count) {
            CHECK(description.find(descriptor.parameters[parameter].name) != std::string::npos);
            CHECK(description.find("absolute track control") != std::string::npos);
          } else {
            CHECK(description.find("no effect") != std::string::npos);
          }
          for (int table : {0, 1}) {
            gfxClear();
            fxEditFullDraw(fx, EMPTY_VALUE_8, table);
            if (parameter < descriptor.count) {
              CHECK(std::string(mockGfxCells[1], 40).find(descriptor.name) != std::string::npos);
            } else {
              std::string drawn;
              for (const auto& row : mockGfxCells) drawn.append(row, 35);
              CHECK(drawn.find("Unused") == std::string::npos);
              CHECK(drawn.find(fxNames[fx].name) == std::string::npos);
            }
          }
        }
      }
    }
    insertSelect(&chipnomadState->project.trackInserts[3][1], insertRotary);
    FXCell fx = {fxF21, 254}, last = {fxF21, 0};
    editFXValue(CellEditAction::increase, &fx, &last, 0, EMPTY_VALUE_8);
    CHECK(fx.value == 255);
    CHECK(std::string(screenGetActiveMessage()) == "Rotary Speed: 25 Hz");
    chipnomadDestroy(chipnomadState);
    chipnomadState = previous;
    currentScreen = previousScreen;
  }

  TEST_CASE("roll and popup offer only configured insert controls") {
    auto* previous = chipnomadState;
    auto* previousScreen = currentScreen;
    chipnomadState = chipnomadCreate();
    screensInitAll();
    currentScreen = &screenPhrase;
    *pSongTrack = 3;
    insertSelect(&chipnomadState->project.trackInserts[0][0], insertCompressor);
    for (int first = 0; first < insertModuleCount; ++first) {
      for (int second = 0; second < insertModuleCount; ++second) {
        CAPTURE(first); CAPTURE(second);
        auto* config = chipnomadState->project.trackInserts[3];
        insertSelect(&config[0], first);
        insertSelect(&config[1], second);
        config[0].bypass = config[1].bypass = 1; // Still editable while bypassed.
        std::vector<int> expected;
        for (int slot = 0; slot < 2; ++slot)
          for (int p = 0; p < insertDescriptor(config[slot].module).count; ++p)
            expected.push_back(fxF11 + slot * 8 + p);
        for (int table : {0, 1}) {
          for (int direction : {-1, 1}) {
            FXCell fx = {(FXId)(direction > 0 ? fxF11 - 1 : fxF28 + 1), 73};
            FXCell last = fx;
            std::vector<int> seen;
            for (int attempts = 0; attempts < fxTotalCount; ++attempts) {
              const int before = fx.command;
              editFX(direction > 0 ? CellEditAction::increase : CellEditAction::decrease,
                     &fx, &last, table, EMPTY_VALUE_8);
              if (fx.command == before || fx.command < fxF11 || fx.command > fxF28) break;
              seen.push_back(fx.command);
              CHECK(fx.value == 73);
            }
            if (direction < 0) std::reverse(seen.begin(), seen.end());
            CHECK(seen == expected);
          }
          // Traverse through both slot groups using real popup input.
          fxEditFullDraw(fxARP, EMPTY_VALUE_8, table);
          std::string popup;
          for (const auto& row : mockGfxCells) popup.append(row, 35);
          for (int slot = 0; slot < 2; ++slot) {
            std::string title = "TF" + std::to_string(slot + 1) + ": " + insertDescriptor(config[slot].module).name;
            CHECK((popup.find(title) != std::string::npos) == (config[slot].module != insertOff));
          }
          std::vector<int> seen;
          int before = -1;
          for (int attempts = 0; attempts < fxTotalCount; ++attempts) {
            FXCell selected = {FX_ID_EMPTY, 73}, last = {};
            fxEditInput(0, 1, &selected, &last);
            if (selected.command == before) break;
            before = selected.command;
            if (selected.command >= fxF11 && selected.command <= fxF28) seen.push_back(selected.command);
            fxEditInput(keyEdit | keyRight, 1, &selected, &last);
          }
          CHECK(seen == expected);
          // Cached FX from another track cannot insert an unused address.
          FXCell stale = {fxF28, 73}, cell = {FX_ID_EMPTY, 0};
          editFX(CellEditAction::tap, &cell, &stale, table, EMPTY_VALUE_8);
          CHECK(cell.value == 73);
          if (cell.command >= fxF11 && cell.command <= fxF28)
            CHECK(std::find(expected.begin(), expected.end(), cell.command) != expected.end());
          else CHECK(expected.empty());
        }
      }
    }
    chipnomadDestroy(chipnomadState);
    chipnomadState = previous;
    currentScreen = previousScreen;
  }

  TEST_CASE("metadata addresses all native controls with exact neutral and discrete values") {
    CHECK(fxF11 > fxATY);
    CHECK(fxF28 < 255);
    CHECK(genericModFirstInsert == 29);
    const int counts[] = {0, 8, 4, 2, 6, 6, 5, 4, 5, 4, 4, 4, 4, 6, 8, 8, 6};
    static_assert(sizeof(counts) / sizeof(counts[0]) == insertModuleCount, "Test covers every module");
    for (int m = 0; m < insertModuleCount; ++m) {
      CHECK(insertDescriptor(m).count == counts[m]);
      InsertConfig c{};
      insertSelect(&c, m);
      for (int p = 0; p < counts[m]; ++p) {
        const auto& d = insertDescriptor(m).parameters[p];
        CHECK(insertClamp(m, p, 0) == 0);
        CHECK(insertMap(m, p, 0) == doctest::Approx(d.minimum));
        CHECK(insertMap(m, p, 255) == doctest::Approx(d.maximum));
        if (d.mapping == InsertMapping::bipolar) CHECK(insertMap(m, p, 128) == 0);
        for (int v = -1; v <= 256; ++v) CHECK(std::isfinite(insertMap(m, p, v)));
      }
      CHECK(insertClamp(m, 8, 255) == 0);
    }
    CHECK(insertDescriptor(999).count == 0);
  }
  TEST_CASE("trance gate has named patterns, deterministic cycles and smooth dry/wet control") {
    InsertConfig c[2]{};
    insertSelect(&c[0], insertTranceGate);
    CHECK(std::string(insertDescriptor(insertTranceGate).name) == "Trance Gate");
    char described[64];
    insertDescribe(described, sizeof(described), insertTranceGate, 0, 14);
    CHECK(std::string(described) == "0E Gate Sweep");
    insertDescribe(described, sizeof(described), insertTranceGate, 5, 3);
    CHECK(std::string(described) == "Chaos");
    c[0].values[0] = 1;   // Four Floor
    c[0].values[1] = 12;  // 1/32 = 3 ticks
    c[0].values[2] = 15;
    c[0].values[3] = 255;
    c[0].values[6] = 0;
    c[0].values[7] = 255;
    InsertAutomation a{};
    InsertChain whole(48000), split(48000);
    whole.setTempo(2400); split.setTempo(2400); // 60 frames per 1/32 step
    whole.sync(c, &a); split.sync(c, &a);
    uint8_t v[2][8]; values(c, v);
    std::vector<float> x(960 * 2, .25f), y = x;
    whole.process(x.data(), 960, v);
    for (int frame = 0; frame < 960; ++frame) split.process(y.data() + frame * 2, 1, v);
    for (size_t i = 0; i < x.size(); ++i) CHECK(x[i] == doctest::Approx(y[i]).epsilon(1e-6));
    CHECK(x[200] < .2f); // The 1st 1/32 is open; the following seven fade closed.
    whole.trigger();
    std::vector<float> retrigger(120 * 2, .25f);
    whole.process(retrigger.data(), 120, v);
    CHECK(retrigger[0] < retrigger[100]);
    c[0].values[7] = 0;
    values(c, v);
    std::vector<float> dry(240 * 2, .25f);
    split.process(dry.data(), 240, v);
    for (float sample : dry) CHECK(sample == doctest::Approx(.25f).epsilon(1e-6));
  }
  TEST_CASE("all DSP modules handle both slots, silence, stereo and variable blocks") {
    for (float rate : {32000.f, 44100.f, 48000.f, 96000.f})
      for (int m = 1; m < insertModuleCount; ++m)
        for (int slot = 0; slot < 2; ++slot) {
          CAPTURE(rate);
          CAPTURE(m);
          CAPTURE(slot);
          InsertConfig c[2]{};
          insertSelect(&c[slot], m);
          InsertAutomation a{};
          BeadsPool pool;
          InsertChain chain(rate);
          if (m == insertBeads) chain.attachBeads(slot, pool.prepare(slot, rate));
          REQUIRE(chain.ready());
          chain.sync(c, &a);
          uint8_t v[2][8];
          values(c, v);
          std::vector<float> input(8192 * 2, 0);
          input[0] = .8f;
          input[1] = -.6f;
          auto sine = signal(4096);
          std::copy(sine.begin(), sine.end(), input.begin() + 4096 * 2);
          int pos = 0;
          for (int block : {1, 3, 127, 511, 2048, 5502}) {
            chain.process(input.data() + 2 * pos, block, v);
            pos += block;
          }
          CHECK(pos == 8192);
          for (float x : input) {
            CHECK(std::isfinite(x));
            CHECK(fabsf(x) <= 32);
          }
          CHECK(std::any_of(input.begin(), input.end(), [](float x) { return fabsf(x) > .001f; }));
        }
  }
  TEST_CASE("new insert effects honor wet mix and expose decoded choice labels") {
        const int modules[] = {insertChorus,     insertFlanger, insertPhaser, insertRotary,
                               insertSaturation, insertBitcrusher, insertDestruction};
        for (int module : modules) {
          CAPTURE(module);
          InsertConfig c[2]{};
          insertSelect(&c[0], module);
          InsertAutomation automation{};
          InsertChain chain(48000);
          REQUIRE(chain.ready());
          insertEdit(&c[0], 3, 0);
          chain.sync(c, &automation);
          uint8_t v[2][8];
          values(c, v);
          auto dry = signal(4096);
          auto out = dry;
          chain.process(out.data(), 4096, v);
          CHECK(out == dry);

          insertEdit(&c[0], 3, 255);
          chain.sync(c, &automation);
          values(c, v);
          out = dry;
          chain.process(out.data(), 4096, v);
          CHECK(out != dry);
          for (float sample : out) CHECK(std::isfinite(sample));
        }
        char text[32];
        insertDescribe(text, sizeof(text), insertRotary, 0, 0);
        CHECK(std::string(text) == "0.01 Hz");
        insertDescribe(text, sizeof(text), insertRotary, 0, 1);
        CHECK(insertMap(insertRotary, 0, 1) > 0.01f);
        insertDescribe(text, sizeof(text), insertRotary, 0, 255);
        CHECK(std::string(text) == "25 Hz");
        insertDescribe(text, sizeof(text), insertBitcrusher, 0, 12);
        CHECK(std::string(text) == "16 bit");
        insertDescribe(text, sizeof(text), insertBitcrusher, 1, 31);
        CHECK(std::string(text) == "32x");
        insertDescribe(text, sizeof(text), insertDestruction, 0, 2);
        CHECK(std::string(text) == "Crush");
  }
  TEST_CASE("chorus and phaser extend from subtle to extreme modulation") {
    for (int module : {insertChorus, insertPhaser}) {
      InsertConfig c[2]{};
      insertSelect(&c[0], module);
      InsertAutomation automation{};
      InsertChain chain(48000);
      REQUIRE(chain.ready());
      uint8_t v[2][8];
      values(c, v);
      auto dry = signal(12000), subtle = dry;
      chain.sync(c, &automation);
      chain.process(subtle.data(), 12000, v);
      float subtleDifference = 0;
      for (size_t i = 4096; i < dry.size(); ++i) subtleDifference += fabsf(subtle[i] - dry[i]);

      for (int parameter = 0; parameter < insertDescriptor(module).count; ++parameter)
        insertEdit(&c[0], parameter, 255);
      InsertChain extremeChain(48000);
      REQUIRE(extremeChain.ready());
      extremeChain.sync(c, &automation);
      values(c, v);
      auto extreme = dry;
      extremeChain.process(extreme.data(), 12000, v);
      float extremeDifference = 0;
      for (size_t i = 4096; i < dry.size(); ++i) extremeDifference += fabsf(extreme[i] - dry[i]);
      CHECK(subtleDifference > 1);
      CHECK(extremeDifference > subtleDifference * 2);
    }
  }
  TEST_CASE("OFF and fully bypassed slots are exactly transparent") {
    InsertConfig c[2]{};
    InsertAutomation a{};
    InsertChain chain(48000);
    chain.sync(c, &a);
    uint8_t v[2][8]{};
    auto dry = signal(1024), out = dry;
    chain.process(out.data(), 1024, v);
    CHECK(out == dry);
    CHECK_FALSE(chain.active());
    for (int m = 1; m < insertModuleCount; ++m) {
      insertSelect(&c[0], m);
      c[0].bypass = 1;
      chain.sync(c, &a);
      values(c, v);
      out = dry;
      chain.process(out.data(), 1024, v);
      CHECK(out == dry);
    }
  }
  TEST_CASE("parametric EQ is neutral at zero gain, shapes bands and soft clips boosts") {
    InsertConfig c[2]{};
    insertSelect(&c[0], insertParametricEQ);
    CHECK(insertMap(insertParametricEQ, 1, 0) == doctest::Approx(60.f));
    CHECK(insertMap(insertParametricEQ, 1, 255) == doctest::Approx(500.f));
    CHECK(insertMap(insertParametricEQ, 3, 0) == doctest::Approx(250.f));
    CHECK(insertMap(insertParametricEQ, 5, 255) == doctest::Approx(14000.f));
    InsertAutomation a{};
    InsertChain chain(48000);
    chain.sync(c, &a);
    uint8_t v[2][8];
    values(c, v);
    auto dry = signal(4096), out = dry;
    chain.process(out.data(), 4096, v);
    CHECK(out == dry);

    auto shapedPeak = [](int gainParameter, int gain, float frequency) {
      InsertConfig config[2]{};
      insertSelect(&config[0], insertParametricEQ);
      insertEdit(&config[0], gainParameter, gain);
      InsertAutomation automation{};
      InsertChain eq(48000);
      eq.sync(config, &automation);
      uint8_t effective[2][8];
      values(config, effective);
      std::vector<float> signal(48000 * 2);
      for (int i = 0; i < 48000; ++i)
        signal[2 * i] = .001f * sinf(6.28318530718f * frequency * i / 48000.f);
      eq.process(signal.data(), 48000, effective);
      float peak = 0;
      for (int i = 24000; i < 48000; ++i) peak = std::max(peak, fabsf(signal[2 * i]));
      for (int i = 0; i < 48000; ++i) CHECK(signal[2 * i + 1] == 0);
      return peak;
    };
    for (const auto& band : {std::array<float, 2>{0, 60}, {2, 1000}, {4, 12000}}) {
      int gainParameter = (int)band[0];
      CHECK(shapedPeak(gainParameter, 255, band[1]) > .003f);
      CHECK(shapedPeak(gainParameter, 0, band[1]) < .0004f);
    }

    insertEdit(&c[0], 0, 255);
    insertEdit(&c[0], 2, 128);
    chain.sync(c, &a);
    values(c, v);
    std::vector<float> loud(4096 * 2, 4.f);
    chain.process(loud.data(), 4096, v);
    for (int i = 1024; i < 4096 * 2; ++i) {
      CHECK(std::isfinite(loud[i]));
      CHECK(fabsf(loud[i]) <= 1.5f);
    }
  }
  TEST_CASE("repeated effects are serial and each slot has independent state") {
    InsertConfig c[2]{};
    insertSelect(&c[0], insertDistortion);
    insertSelect(&c[1], insertDistortion);
    c[0].values[0] = 205;
    c[1].values[1] = 3;
    InsertAutomation a{};
    InsertChain pair(48000);
    pair.sync(c, &a);
    uint8_t v[2][8];
    values(c, v);
    auto actual = signal(4096), expected = actual;
    pair.process(actual.data(), 4096, v);
    for (int slot = 0; slot < 2; ++slot) {
      InsertConfig one[2]{};
      one[0] = c[slot];
      InsertAutomation x{};
      InsertChain chain(48000);
      chain.sync(one, &x);
      uint8_t q[2][8];
      values(one, q);
      chain.process(expected.data(), 4096, q);
    }
    CHECK(actual == expected);
    InsertChain independent(48000);
    InsertAutomation b{};
    independent.sync(c, &b);
    auto clean = signal(4096);
    independent.process(clean.data(), 4096, v);
    CHECK(clean == actual);
  }
  TEST_CASE(
      "edits clear only their own override; bypass keeps automation; module change clears slot") {
    InsertConfig c[2]{};
    insertSelect(&c[0], insertCompressor);
    insertSelect(&c[1], insertTape);
    InsertAutomation a{};
    InsertChain chain(48000);
    chain.sync(c, &a);
    a.valid[0] = 255;
    a.valid[1] = 63;
    insertEdit(&c[0], 1, c[0].values[1]);
    chain.sync(c, &a);
    CHECK(a.valid[0] == 253);
    CHECK(a.valid[1] == 63);
    c[0].bypass = 1;
    chain.sync(c, &a);
    CHECK(a.valid[0] == 253);
    insertSelect(&c[0], insertDoubler);
    chain.sync(c, &a);
    CHECK(a.valid[0] == 0);
    CHECK(a.valid[1] == 63);
  }
  TEST_CASE("Fxx Phrase and Table events accept zero and FF with last assignment winning") {
    auto p = std::make_unique<Project>();
    projectInit(p.get());
    insertSelect(&p->trackInserts[0][0], insertCompressor);
    auto s = std::make_unique<PlaybackState>();
    playbackInit(s.get(), p.get());
    FXCell fx = {fxF11, 255};
    initFX(s.get(), 0, &fx, nullptr, 0);
    CHECK(s->tracks[0].inserts.values[0][0] == 255);
    fx.value = 0;
    PlaybackTableState table{};
    initFX(s.get(), 0, &fx, &table, 3);
    CHECK(s->tracks[0].inserts.values[0][0] == 0);
    CHECK(s->tracks[0].inserts.valid[0] == 1);
    fx.command = fxF15;
    fx.value = 255;
    initFX(s.get(), 0, &fx, nullptr, 2);
    CHECK(s->tracks[0].inserts.values[0][4] == 7);
    fx.command = fxF21;
    initFX(s.get(), 0, &fx, nullptr, 0);
    CHECK(s->tracks[0].inserts.valid[1] == 0);
    p->trackInserts[0][0].bypass = 1;
    fx.command = fxF18;
    initFX(s.get(), 0, &fx, nullptr, 0);
    CHECK(s->tracks[0].inserts.values[0][7] == 255);
    playbackStop(s.get());
    CHECK(s->tracks[0].inserts.valid[0] == 0);
  }
  TEST_CASE("configuration roundtrips; old projects default OFF; malformed insert fields fail") {
    auto p = std::make_unique<Project>();
    projectInit(p.get());
    p->chipsCount = 1;
    p->tickRate = 50;
    calculateLinearPitchTable12TET(p.get());
    fillFXNames();
    for (int t = 0; t < 8; ++t)
      for (int slot = 0; slot < 2; ++slot) {
        auto& c = p->trackInserts[t][slot];
        insertSelect(&c, 1 + (t * 2 + slot) % (insertModuleCount - 1));
        c.bypass = t % 2;
        insertEdit(&c, 0, 255);
      }
    for (int slot = 0; slot < 2; ++slot) {
      auto& c = p->masterInserts[slot];
      insertSelect(&c, slot ? insertTape : insertCompressor);
      c.bypass = slot;
      insertEdit(&c, 0, 255);
    }
    const char* path = "build/tests/inserts.cct";
    REQUIRE(projectSave(p.get(), path) == 0);
    auto original = read(path);
    CHECK(original.find("edits") == std::string::npos);
    auto loaded = std::make_unique<Project>();
    projectInit(loaded.get());
    REQUIRE(projectLoad(loaded.get(), path) == 0);
    for (int t = 0; t < 8; ++t)
      for (int s = 0; s < 2; ++s) {
        auto& a = p->trackInserts[t][s];
        auto& b = loaded->trackInserts[t][s];
        CHECK(a.module == b.module);
        CHECK(a.bypass == b.bypass);
        CHECK(memcmp(a.values, b.values, 8) == 0);
        CHECK(b.selection == 0);
      }
    for (int slot = 0; slot < 2; ++slot) {
      auto& a = p->masterInserts[slot];
      auto& b = loaded->masterInserts[slot];
      CHECK(a.module == b.module);
      CHECK(a.bypass == b.bypass);
      CHECK(memcmp(a.values, b.values, 8) == 0);
    }
    auto start = original.find("- Track inserts:");
    auto end = original.find("- Track volumes:");
    REQUIRE(start != std::string::npos);
    REQUIRE(end > start);
    auto old = original;
    old.erase(start, end - start);
    write(path, old);
    REQUIRE(projectLoad(loaded.get(), path) == 0);
    for (auto& t : loaded->trackInserts)
      for (auto& c : t) CHECK(c.module == 0);
    for (auto& c : loaded->masterInserts) CHECK(c.module == 0);
    for (auto bad :
         {std::string("- Track inserts: 1,9,2\n"),
          std::string("- Track inserts: 1,8,2\n- Insert: 0,0,255,0,0,0,0,0,0,0,0,0\n"),
          std::string("- Track inserts: 1,8,2\n- Insert: 0,0,1,0,256,0,0,0,0,0,0,0\n")}) {
      auto corrupt = original;
      corrupt.replace(start, end - start, bad);
      write(path, corrupt);
      CHECK(projectLoad(loaded.get(), path) != 0);
    }
    auto masterStart = original.find("- Master inserts:");
    REQUIRE(masterStart != std::string::npos);
    auto masterEnd = original.find("- Track volumes:");
    auto corrupt = original;
    corrupt.replace(masterStart, masterEnd - masterStart,
                    "- Master inserts: 1,2\n- Master insert: 0,255,0,0,0,0,0,0,0,0,0\n");
    write(path, corrupt);
    CHECK(projectLoad(loaded.get(), path) != 0);
    write(path, original.substr(0, start) + "- Track inserts: 1,8,2\n");
    CHECK(projectLoad(loaded.get(), path) != 0);
    projectFree(p.get());
    projectFree(loaded.get());
  }
  TEST_CASE("one combined track feeds serial inserts then the original send gains") {
    Engine dry, wet;
    insertSelect(&wet.s->project.trackInserts[0][0], insertDistortion);
    dry.start();
    wet.start();
    float a[512], b[512];
    REQUIRE(chipnomadRender(dry.s, a, 256) == 256);
    REQUIRE(chipnomadRender(wet.s, b, 256) == 256);
    InsertChain expected(48000);
    InsertAutomation automation{};
    auto* config = wet.s->project.trackInserts[0];
    expected.sync(config, &automation);
    uint8_t v[2][8];
    values(config, v);
    expected.process(a, 256, v);
    for (int i = 0; i < 512; ++i) {
      CHECK(b[i] == doctest::Approx(a[i]).epsilon(1e-6));
      CHECK(wet.s->reverbBuffer[i] == doctest::Approx(b[i] * .25f).epsilon(1e-6));
      CHECK(wet.s->delayBuffer[i] == doctest::Approx(b[i] * .5f).epsilon(1e-6));
    }
  }
  TEST_CASE("master inserts process the completed mix") {
    Engine dry, wet;
    insertSelect(&wet.s->project.masterInserts[0], insertDistortion);
    dry.start();
    wet.start();
    float a[512], b[512];
    REQUIRE(chipnomadRender(dry.s, a, 256) == 256);
    REQUIRE(chipnomadRender(wet.s, b, 256) == 256);
    InsertChain expected(48000);
    InsertAutomation automation{};
    expected.sync(wet.s->project.masterInserts, &automation);
    uint8_t v[2][8];
    values(wet.s->project.masterInserts, v);
    expected.process(a, 256, v);
    for (int i = 0; i < 512; ++i) CHECK(b[i] == doctest::Approx(a[i]).epsilon(1e-6));
  }
  TEST_CASE("insert MOD recomputes from bases and does not drift or change saved configuration") {
    Engine e;
    auto& p = e.s->project;
    insertSelect(&p.trackInserts[0][0], insertDistortion);
    auto& mod = p.instruments[0].modulation[0];
    mod = {};
    mod.type = ModulationType::StickLinear;
    mod.destination = getInstrumentFunctions(InstrumentType::Braids).modDestinationsCount + 1 +
                      genericModFirstInsert;
    mod.amount = 32;
    mod.p1 = 0;
    chipnomadSetLiveStickEnabled(1);
    chipnomadSetLiveStickAxes(1, 0, 0, 0);
    e.start();
    float out[1920];
    REQUIRE(chipnomadRender(e.s, out, 960) == 960);
    int v = e.s->insertValues[0][0][0];
    CHECK(v > 128);
    for (int i = 0; i < 10; ++i) {
      chipnomadRender(e.s, out, 960);
      CHECK(e.s->insertValues[0][0][0] == v);
    }
    CHECK(p.trackInserts[0][0].values[0] == 128);
    chipnomadSetLiveStickEnabled(0);
    chipnomadSetLiveStickAxes(0, 0, 0, 0);
  }
  TEST_CASE("muted tracks cannot leak insert output or sends") {
    Engine e;
    insertSelect(&e.s->project.trackInserts[0][0], insertDoubler);
    e.start();
    float out[1920];
    chipnomadRender(e.s, out, 960);
    uint8_t enabled[8]{};
    chipnomadQueueTrackEnabled(e.s, enabled);
    chipnomadRender(e.s, out, 960);
    for (int i = 0; i < 1920; ++i) {
      CHECK(out[i] == 0);
      CHECK(e.s->reverbBuffer[i] == 0);
      CHECK(e.s->delayBuffer[i] == 0);
    }
  }
  TEST_CASE(
      "insert page fits, skips inactive cells, clamps cursor and selects tracks without changing "
      "instruments") {
    auto* previous = chipnomadState;
    chipnomadState = chipnomadCreate();
    screensInitAll();
    chipnomadState->project.tracksCount = 8;
    *pSongTrack = 3;
    cInstrument = 17;
    screenInsertFX.setup(-1);
    screenInsertFX.fullRedraw();
    REQUIRE(mockScreenData);
    auto* d = mockScreenData;
    for (int module = 0; module < insertModuleCount; ++module) {
      insertSelect(&chipnomadState->project.trackInserts[3][0], module);
      screenInsertFX.fullRedraw();
      d->drawStatic();
      for (int row = 0; row < 10; ++row)
        for (int col = 0; col < 2; ++col) {
          d->drawField(col, row, CellState::normal);
          if (d->isCellValid(col, row)) {
            d->drawCursor(col, row);
            CHECK(mockCursorX >= 0);
            CHECK(mockCursorX + mockCursorWidth <= 34);
            CHECK(mockCursorY < 19);
          }
        }
    }
    d->cursorCol = 1;
    d->cursorRow = 4;
    insertSelect(&chipnomadState->project.trackInserts[3][0], insertDoubler);
    screenInsertFX.fullRedraw();
    CHECK(d->cursorRow == 0);
    CHECK(d->cursorCol == 0);
    screenInsertFX.onInput(1, keyOpt | keyRight, 0);
    CHECK(*pSongTrack == 4);
    CHECK(cInstrument == 17);
    *pSongTrack = 7;
    screenInsertFX.onInput(1, keyOpt | keyRight, 0);
    CHECK(*pSongTrack == 7);
    screenInsertFX.onInput(1, keyShift | keyDown, 0);
    CHECK(currentScreen == &screenModulation);
    chipnomadDestroy(chipnomadState);
    chipnomadState = previous;
  }
}

TEST_CASE("insert automation survives instruments and full phrase loops until stop") {
  Engine e;
  auto& p = e.s->project;
  insertSelect(&p.trackInserts[0][0], insertDistortion);
  getInstrumentFunctions(InstrumentType::Plaits).init(&p.instruments[1]);
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  for (auto& x : p.grooves[0].speed) x = 1;
  auto& a = p.phrases[0].rows[0];
  a.note = 45;
  a.instrument = 0;
  a.fx[2][0] = fxF11;
  a.fx[2][1] = 255;
  auto& b = p.phrases[0].rows[1];
  b.note = 48;
  b.instrument = 1;
  chipnomadInitChips(e.s, 48000, nullptr);
  chipnomadQueuePlaybackStartSong(e.s, 0, 0, 1);
  float out[1920];
  for (int i = 0; i < 34; ++i) {
    REQUIRE(chipnomadRender(e.s, out, 960) == 960);
    CHECK(e.s->playbackState.tracks[0].inserts.values[0][0] == 255);
    CHECK((e.s->playbackState.tracks[0].inserts.valid[0] & 1) != 0);
  }
  CHECK(p.trackInserts[0][0].values[0] == 128);
  chipnomadQueuePlaybackStop(e.s);
  chipnomadRender(e.s, out, 960);
  CHECK(e.s->playbackState.tracks[0].inserts.valid[0] == 0);
}
TEST_CASE("insert effects recover from nonfinite input and rapid bypass/module changes") {
  InsertChain chain(48000);
  InsertConfig c[2]{};
  InsertAutomation a{};
  uint8_t v[2][8]{};
  for (int k = 0; k < 60; ++k) {
    insertSelect(&c[0], 1 + k % 5);
    c[0].bypass = k % 3 == 0;
    chain.sync(c, &a);
    values(c, v);
    auto audio = signal(1024);
    audio[17] = NAN;
    audio[18] = INFINITY;
    chain.process(audio.data(), 1024, v);
    if (!c[0].bypass)
      for (float x : audio) CHECK(std::isfinite(x));
  }
}
TEST_CASE(
    "insert motion recording resolves current track metadata and keeps discrete indices valid") {
  Engine e;
  auto& p = e.s->project;
  insertSelect(&p.trackInserts[0][0], insertDistortion);
  auto& mod = p.instruments[0].modulation[0];
  mod = {};
  mod.type = ModulationType::StickLinear;
  mod.destination = getInstrumentFunctions(InstrumentType::Braids).modDestinationsCount + 1 +
                    genericModFirstInsert + 1;
  mod.amount = 127;
  mod.p1 = 0;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  p.phrases[0].rows[0].note = 45;
  p.phrases[0].rows[0].instrument = 0;
  chipnomadSetLiveStickEnabled(1);
  chipnomadSetLiveStickAxes(1, 0, 0, 0);
  chipnomadSetMotionRecordMode(1, 0);
  chipnomadInitChips(e.s, 48000, nullptr);
  chipnomadQueuePlaybackStartSong(e.s, 0, 0, 1);
  float out[1920];
  chipnomadRender(e.s, out, 960);
  MotionRecordEvent event{};
  bool found = false;
  while (chipnomadConsumeMotionRecordEvent(&event)) {
    if (event.fx == fxF12) {
      found = true;
      CHECK(event.value == 4);
    }
  }
  CHECK(found);
  chipnomadSetMotionRecordMode(0, 0);
  chipnomadSetLiveStickEnabled(0);
  chipnomadSetLiveStickAxes(0, 0, 0, 0);
}

TEST_CASE("insert delay and control smoothing are independent of callback block size") {
  for (int module = 1; module < insertModuleCount; ++module) {
    CAPTURE(module);
    InsertConfig c[2]{};
    insertSelect(&c[0], module);
    if (module == insertTape) {
      c[0].values[2] = 180;
      c[0].values[3] = 180;
      c[0].values[4] = 200;
    }
    InsertAutomation a{}, b{};
    BeadsPool pool;
    InsertChain whole(48000), singles(48000);
    if (module == insertBeads) {
      whole.attachBeads(0, pool.prepare(0, 48000));
      singles.attachBeads(0, pool.prepare(1, 48000));
    }
    whole.sync(c, &a);
    singles.sync(c, &b);
    uint8_t v[2][8];
    values(c, v);
    auto x = signal(4096), y = x;
    whole.process(x.data(), 4096, v);
    for (int i = 0; i < 4096; ++i) singles.process(y.data() + i * 2, 1, v);
    for (int i = 0; i < 8192; ++i) CHECK(x[i] == doctest::Approx(y[i]).epsilon(1e-6));
  }
}

TEST_CASE("fresh live and preview starts clear inserts without disturbing other live tracks") {
  Engine e;
  e.s->project.tracksCount = 2;
  chipnomadInitChips(e.s, 48000, nullptr);
  auto& state = e.s->playbackState;
  state.p->song[0][0] = 0;
  state.p->chains[0].rows[0].phrase = 0;
  auto& track = state.tracks[0];
  track.inserts.valid[0] = 255;
  auto serial = track.insertReset;
  state.tracks[1].mode = PlaybackMode::live;
  state.tracks[1].inserts.valid[0] = 255;
  playbackStartLiveChain(&state, 0, 0);
  CHECK(track.inserts.valid[0] == 0);
  CHECK(track.insertReset == serial + 1);
  CHECK(state.tracks[1].inserts.valid[0] == 255);
  playbackStop(&state);
  track.inserts.valid[1] = 255;
  serial = track.insertReset;
  playbackPreviewNote(&state, 0, 45, 0);
  CHECK(track.inserts.valid[1] == 0);
  CHECK(track.insertReset == serial + 1);
}

TEST_CASE("AY, chord and stereo voices each run one summed post-EQ serial insert chain") {
  for (auto type : {InstrumentType::AY1, InstrumentType::Plaits, InstrumentType::Sample}) {
    CAPTURE((int)type);
    Engine dry, wet;
    for (auto* state : {dry.s, wet.s}) {
      auto& p = state->project;
      getInstrumentFunctions(type).init(&p.instruments[0]);
      p.instruments[0].volume = 120;
      p.chipSetup.ay.clock = 1773400;
      if (type == InstrumentType::Sample) {
        auto& sample = p.instruments[0].chip.sample;
        sample.channels = 2;
        sample.frameCount = 1024;
        sample.sampleRate = 48000;
        sample.loopMode = 1;
        sample.data = (int16_t*)malloc(4096);
        for (int i = 0; i < 1024; ++i) {
          sample.data[i * 2] = (int16_t)(12000 * sin(i * .031));
          sample.data[i * 2 + 1] = (int16_t)(8000 * cos(i * .052));
        }
      }
      p.song[0][0] = 0;
      p.chains[0].rows[0].phrase = 0;
      auto& row = p.phrases[0].rows[0];
      row.note = 48;
      row.instrument = 0;
      if (type == InstrumentType::Plaits) {
        row.fx[0][0] = fxCRD;
        row.fx[0][1] = 0;
      }
    }
    insertSelect(&wet.s->project.trackInserts[0][0], insertDistortion);
    insertSelect(&wet.s->project.trackInserts[0][1], insertDoubler);
    for (auto* state : {dry.s, wet.s}) {
      chipnomadInitChips(state, 48000, nullptr);
      chipnomadQueuePlaybackStartSong(state, 0, 0, 1);
    }
    InsertChain expected(48000);
    InsertAutomation a{};
    auto* c = wet.s->project.trackInserts[0];
    expected.sync(c, &a);
    uint8_t v[2][8];
    values(c, v);
    double energy = 0;
    for (int n = 0; n < 32; ++n) {
      float x[512], y[512];
      REQUIRE(chipnomadRender(dry.s, x, 256) == 256);
      REQUIRE(chipnomadRender(wet.s, y, 256) == 256);
      expected.process(x, 256, v);
      for (int i = 0; i < 512; ++i) {
        CHECK(y[i] == doctest::Approx(x[i]).epsilon(1e-5));
        energy += y[i] * y[i];
      }
    }
    CHECK(energy > 0);
  }
}
TEST_CASE("one instrument on two tracks modulates independent slot bases with multiple sources") {
  Engine e;
  auto& p = e.s->project;
  p.tracksCount = 2;
  p.chipsCount = 2;
  for (int t = 0; t < 2; ++t) {
    insertSelect(&p.trackInserts[t][0], insertDistortion);
    p.trackInserts[t][0].values[0] = 40 + 80 * t;
    p.song[0][t] = t;
    p.chains[t].rows[0].phrase = t;
    p.phrases[t].rows[0].note = 48;
    p.phrases[t].rows[0].instrument = 0;
  }
  for (int i = 0; i < 2; ++i) {
    auto& m = p.instruments[0].modulation[i];
    m = {};
    m.type = ModulationType::StickLinear;
    m.destination = getInstrumentFunctions(InstrumentType::Braids).modDestinationsCount + 1 +
                    genericModFirstInsert;
    m.amount = 16;
    m.p1 = i;
  }
  chipnomadSetLiveStickEnabled(1);
  chipnomadSetLiveStickAxes(.5f, .25f, 0, 0);
  chipnomadInitChips(e.s, 48000, nullptr);
  chipnomadQueuePlaybackStartSong(e.s, 0, 0, 1);
  float out[1920];
  chipnomadRender(e.s, out, 960);
  int a = e.s->insertValues[0][0][0], b = e.s->insertValues[1][0][0];
  CHECK(a > 40);
  CHECK(b - a == 80);
  for (int n = 0; n < 10; ++n) {
    chipnomadRender(e.s, out, 960);
    CHECK(e.s->insertValues[0][0][0] == a);
    CHECK(e.s->insertValues[1][0][0] == b);
  }
  CHECK(p.trackInserts[0][0].values[0] == 40);
  CHECK(p.trackInserts[1][0].values[0] == 120);
  chipnomadSetLiveStickEnabled(0);
  chipnomadSetLiveStickAxes(0, 0, 0, 0);
}

TEST_CASE("insert tips return after real key release and categorized chooser selects modules") {
  auto* previous = chipnomadState;
  auto* previousScreen = currentScreen;
  chipnomadState = chipnomadCreate();
  screensInitAll();
  *pSongTrack = 0;
  screenInsertFXOpenTrack();
  insertSelect(&chipnomadState->project.trackInserts[0][0], insertDistortion);
  screenInsertFX.setup(-1);
  screenInsertFX.fullRedraw();
  auto* d = mockScreenData;
  REQUIRE(d);
  d->cursorRow = 1; d->cursorCol = 0;
  d->drawCursor(0, 1);
  std::string hint = screenGetActiveMessage();
  CHECK(hint.find("F11 Input") == 0);
  MainLoopEventData release{};
  release.type = MainLoopEvent::keyUp;
  release.data.input = {InputDeviceType::logical, keyDown};
  appOnEvent(release);
  CHECK(std::string(screenGetActiveMessage()).empty());
  for (int frame = 0; frame < 120; ++frame) {
    screenInsertFX.draw();
    CHECK(std::string(screenGetActiveMessage()) == hint);
  }
  screenMessage(60, "Saved");
  screenInsertFX.draw();
  CHECK(std::string(screenGetActiveMessage()) == "Saved");
  d->cursorRow = 0;
  d->cursorCol = 0;
  auto& insert = chipnomadState->project.trackInserts[0][0];
  insertSelect(&insert, insertOff);
  const int popupOrder[] = {
      insertOff, insertOTT, insertCompressor, insertParametricEQ,
      insertDistortion, insertSaturation, insertBitcrusher, insertDestruction, insertTape,
      insertChorus, insertPhaser, insertFlanger, insertTranceGate,
      insertRotary, insertDoubler, insertBeads, insertFormant,
  };
  for (int i = 1; i < (int)(sizeof(popupOrder) / sizeof(popupOrder[0])); ++i) {
    screenInsertFX.onInput(1, keyEdit | keyRight, 0);
    CHECK(insert.module == popupOrder[i]);
  }
  screenInsertFX.onInput(1, keyEdit | keyRight, 0);
  CHECK(insert.module == insertOff);
  screenInsertFX.onInput(1, keyEdit | keyLeft, 0);
  CHECK(insert.module == insertFormant);
  insertSelect(&insert, insertDistortion);
  screenInsertFX.onInput(1, keyEdit, 1);
  screenInsertFX.onInput(0, 0, 1);
  REQUIRE(currentScreen == &screenSelectionPopup);
  CHECK(selectionPopupIsFullWidth());
  screenSelectionPopup.fullRedraw();
  CHECK(std::string(mockGfxCells[4], 40).find("Character") != std::string::npos);
  CHECK(std::string(mockGfxCells[2], 40).find("Distortion") != std::string::npos);
  // Legacy category choosers keep their two-panel layout.
  SelectionItem items[] = {{"Existing", 0, nullptr, 0, nullptr}};
  selectionPopupSetup("Existing", items, 1, 0, nullptr, nullptr);
  CHECK_FALSE(selectionPopupIsFullWidth());
  screenInsertFXOpenMaster();
  screenInsertFX.setup(-1);
  screenInsertFX.fullRedraw();
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[0], 40).find("MASTER FX") != std::string::npos);
  MainLoopEventData branch{};
  branch.type = MainLoopEvent::keyDown;
  branch.data.input = {InputDeviceType::logical, keyShift | keyRight};
  appOnEvent(branch);
  CHECK(currentScreen == &screenInsertFX);
  screenInsertFX.onInput(1, keyShift | keyDown, 0);
  CHECK(currentScreen == &screenInsertFX);
  screenInsertFX.onInput(1, keyOpt, 0);
  CHECK(currentScreen == &screenMixer);
  screenMessage(0, "");
  chipnomadDestroy(chipnomadState); chipnomadState = previous; currentScreen = previousScreen;
}

TEST_CASE("Beads Shape Freeze has coarse byte edits in both track and master slots") {
  auto* previous = chipnomadState;
  auto* previousScreen = currentScreen;
  chipnomadState = chipnomadCreate();
  screensInitAll();
  *pSongTrack = 0;
  for (bool master : {false, true}) {
    if (master) screenInsertFXOpenMaster();
    else screenInsertFXOpenTrack();
    for (int slot = 0; slot < 2; ++slot) {
      auto& c = master ? chipnomadState->project.masterInserts[slot] :
                        chipnomadState->project.trackInserts[0][slot];
      insertSelect(&c, insertBeads);
      screenInsertFX.fullRedraw();
      auto* d = mockScreenData;
      REQUIRE(d);
      const int row = slot * 5 + 3;
      c.values[6] = 0x78;
      d->onEdit(1, row, CellEditAction::increaseBig);
      CHECK(c.values[6] == 0x88);
      d->onEdit(1, row, CellEditAction::decreaseBig);
      CHECK(c.values[6] == 0x78);
      d->onEdit(1, row, CellEditAction::increase);
      CHECK(c.values[6] == 0x79);
      d->onEdit(1, row, CellEditAction::decrease);
      CHECK(c.values[6] == 0x78);
      c.values[6] = 0xf8;
      d->onEdit(1, row, CellEditAction::increaseBig);
      CHECK(c.values[6] == 0xff);
      c.values[6] = 0x08;
      d->onEdit(1, row, CellEditAction::decreaseBig);
      CHECK(c.values[6] == 0x00);
      // The packed freeze bit must still bypass DSP parameter slew.
      CHECK(insertDescriptor(insertBeads).parameters[6].mapping == InsertMapping::discrete);
      insertSelect(&c, insertCompressor);
      c.values[4] = 0;
      REQUIRE(insertDescriptor(c.module).parameters[4].mapping == InsertMapping::discrete);
      d->onEdit(1, slot * 5 + 1, CellEditAction::increaseBig);
      CHECK(c.values[4] == 1);
      d->onEdit(1, slot * 5 + 1, CellEditAction::decreaseBig);
      CHECK(c.values[4] == 0);
    }
  }
  screenInsertFXOpenTrack();
  chipnomadDestroy(chipnomadState);
  chipnomadState = previous;
  currentScreen = previousScreen;
}
