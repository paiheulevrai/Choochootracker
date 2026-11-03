#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "project.h"
#include "corelib_gfx.h"
#include "screen_instrument.h"
#include "mocks/app_ui_mock.h"
#include "synth/instrument_eq.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

float filteredRms(const InstrumentEQ& eq, float hz) {
  InstrumentEQFilter filter;
  filter.init(48000.0f);
  filter.configure(eq);
  std::vector<float> audio(2 * 48000);
  for (int i = 0; i < 48000; ++i) audio[i * 2] = audio[i * 2 + 1] = 0.1f * sinf(6.2831853f * hz * i / 48000.0f);
  for (int offset = 0; offset < 48000; offset += 480) filter.process(audio.data() + offset * 2, 480);
  double sum = 0.0;
  for (int i = 24000; i < 48000; ++i) sum += audio[i * 2] * audio[i * 2];
  return (float)sqrt(sum / 24000.0);
}

float db(float ratio) { return 20.0f * log10f(ratio); }

InstrumentEQ bell(int8_t gain) {
  InstrumentEQ eq;
  instrumentEQDefaults(&eq);
  eq.bands[1].gain = gain;
  return eq;
}

// Bank 5 holds eq; the Braids instrument uses it unless bank is EMPTY_VALUE_8.
double previewEnergy(const InstrumentEQ& eq, std::vector<float>* capture = nullptr, uint8_t bank = 5,
                     const InstrumentEQ* master = nullptr, uint8_t* trackActive = nullptr,
                     uint8_t* masterActive = nullptr) {
  auto* s = chipnomadCreate();
  auto& p = s->project;
  p.chipsCount = 1; p.tracksCount = 1; p.tickRate = 50; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  getInstrumentFunctions(InstrumentType::Braids).init(&p.instruments[0]);
  p.eqBanks[5] = eq;
  p.instruments[0].eq = bank;
  if (master) p.masterEQ = *master;
  chipnomadInitChips(s, 48000, nullptr);
  REQUIRE(chipnomadQueueProjectRefresh(s));
  chipnomadQueuePlaybackPreviewNote(s, 0, 69, 0);
  double energy = 0.0;
  float audio[1920];
  for (int block = 0; block < 25; ++block) {
    chipnomadRender(s, audio, 960);
    // Braids has a small DC offset, which a high cut rightly keeps.
    for (int channel = 0; channel < 2; ++channel) {
      double mean = 0.0;
      for (int i = 0; i < 960; ++i) mean += audio[i * 2 + channel] / 960.0;
      for (int i = 0; i < 960; ++i) energy += (audio[i * 2 + channel] - mean) * (audio[i * 2 + channel] - mean);
    }
    if (capture) capture->insert(capture->end(), audio, audio + 1920);
  }
  if (trackActive) *trackActive = s->trackEQActive[0];
  if (masterActive) *masterActive = s->masterEQActive;
  chipnomadDestroy(s);
  return energy;
}

}  // namespace

TEST_CASE("Instruments start without an EQ, and EQ banks start flat") {
  Instrument instrument;
  getInstrumentFunctions(InstrumentType::Braids).init(&instrument);
  CHECK(instrument.eq == EMPTY_VALUE_8);
  auto project = std::make_unique<Project>();
  projectInit(project.get());
  for (const auto& eq : project->eqBanks) {
    CHECK(instrumentEQIsDefault(eq));
    CHECK_FALSE(instrumentEQAudible(eq));  // every band is at 0 dB
  }
  InstrumentEQ eq = project->eqBanks[0];
  CHECK(eq.bands[0].shape == EQShape::lowShelf);
  CHECK(eq.bands[1].shape == EQShape::bell);
  CHECK(eq.bands[2].shape == EQShape::highShelf);
  CHECK(instrumentEQQ(42) == doctest::Approx(0.7071f).epsilon(0.001));
  CHECK(instrumentEQQ(48) == doctest::Approx(1.0f));
  eq.bands[0].shape = EQShape::lowCut;
  CHECK(instrumentEQAudible(eq));  // a cut works at any gain
}

TEST_CASE("An unused EQ bank is one no instrument uses, flat ones first") {
  auto project = std::make_unique<Project>();
  projectInit(project.get());
  Project& p = *project;
  CHECK(instrumentEQUnusedBank(&p, -1) == 0);
  getInstrumentFunctions(InstrumentType::Braids).init(&p.instruments[0]);
  getInstrumentFunctions(InstrumentType::Braids).init(&p.instruments[1]);
  p.instruments[0].eq = 0;
  p.eqBanks[1].bands[1].gain = 6;  // changed but unused
  CHECK(instrumentEQUnusedBank(&p, -1) == 2);
  CHECK(instrumentEQUnusedBank(&p, 0) == 0);  // instrument 0's own bank counts as free for it
  p.instruments[1].eq = 0;                     // sharing bank 0
  CHECK(instrumentEQUnusedBank(&p, 0) == 2);
}

TEST_CASE("NEW EQ presets seed twenty editable banks without assigning or changing the master") {
  auto project = std::make_unique<Project>();
  projectInit(project.get());
  getInstrumentFunctions(InstrumentType::Braids).init(&project->instruments[0]);
  project->masterEQ.bands[1].gain = -2;
  const InstrumentEQ master = project->masterEQ;
  instrumentEQInitPresets(project.get());
  CHECK(instrumentEQSame(master, project->masterEQ));
  CHECK(project->instruments[0].eq == EMPTY_VALUE_8);
  const int frequencies[] = {25, 80, 180, 8000, 300, 650, 3200, 10000,
                            90, 30, 65, 100, 100, 250, 250, 350, 120, 250, 120, 1000};
  using S = EQShape;
  const S shapes[] = {S::lowCut, S::lowCut, S::lowCut, S::highCut, S::bell, S::bell, S::bell, S::highShelf,
                      S::lowCut, S::lowCut, S::bell, S::lowCut, S::lowShelf, S::lowShelf, S::lowCut,
                      S::lowCut, S::lowShelf, S::lowShelf, S::highCut, S::bell};
  const int gains[] = {0, 0, 0, 0, -6, -6, -6, 4, 0, 0, 4, 0, 4, 3, 0, 0, -6, -3, 0, -18};
  const int qs[] = {42, 42, 42, 42, 48, 54, 48, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 84};
  const int activeBands[] = {1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 2, 3, 3, 1, 2, 1, 1};
  for (int bank = 0; bank < PROJECT_EQ_BANKS; ++bank) {
    CAPTURE(bank);
    const auto& eq = project->eqBanks[bank];
    if (bank < 0x10 || bank > 0x23) {
      CHECK(instrumentEQIsDefault(eq));
      continue;
    }
    CHECK(instrumentEQAudible(eq));
    CHECK(eq.bands[0].frequency == frequencies[bank - 0x10]);
    CHECK(eq.bands[0].shape == shapes[bank - 0x10]);
    CHECK(eq.bands[0].gain == gains[bank - 0x10]);
    CHECK(eq.bands[0].q == qs[bank - 0x10]);
    int active = 0;
    for (const auto& band : eq.bands) {
      if (band.shape != EQShape::off) ++active;
      CHECK(band.frequency >= kEQFrequencyMin);
      CHECK(band.frequency <= kEQFrequencyMax);
      CHECK(band.gain >= kEQGainMin);
      CHECK(band.gain <= kEQGainMax);
      CHECK(band.q >= kEQQMin);
      CHECK(band.q <= kEQQMax);
    }
    struct RemainingBands {
      int bank;
      InstrumentEQBand second, third;
    };
    const InstrumentEQBand off = {S::off, 1000, 0, 48};
    const RemainingBands remaining[] = {
      {0x18, {S::bell, 300, -4, 48}, {S::bell, 2500, 4, 42}},
      {0x19, {S::bell, 250, -4, 48}, {S::bell, 900, 4, 42}},
      {0x1a, {S::bell, 300, -6, 48}, {S::bell, 3000, 4, 48}},
      {0x1b, {S::bell, 200, 4, 48}, {S::bell, 5000, 4, 42}},
      {0x1c, {S::bell, 800, -3, 42}, {S::highShelf, 8000, 4, 42}},
      {0x1d, {S::highShelf, 2500, -3, 42}, off},
      {0x1e, {S::bell, 1200, 10, 60}, {S::highCut, 6000, 0, 42}},
      {0x1f, {S::bell, 1500, 4, 48}, {S::highCut, 3500, 0, 42}},
      {0x21, {S::highShelf, 2500, 3, 42}, off},
    };
    for (const auto& expected : remaining) {
      CAPTURE(expected.bank);
      const auto& eq = project->eqBanks[expected.bank];
      for (int band = 1; band < INSTRUMENT_EQ_BANDS; ++band) {
        const auto& value = band == 1 ? expected.second : expected.third;
        CHECK(eq.bands[band].shape == value.shape);
        CHECK(eq.bands[band].frequency == value.frequency);
        CHECK(eq.bands[band].gain == value.gain);
        CHECK(eq.bands[band].q == value.q);
      }
    }
    CHECK(active == activeBands[bank - 0x10]);
    for (int previous = 0x10; previous < bank; ++previous)
      CHECK_FALSE(instrumentEQSame(eq, project->eqBanks[previous]));
  }
  CHECK(project->eqBanks[0x1c].bands[1].gain == -3); // -1.5 dB, not -3 dB.
  CHECK(project->eqBanks[0x23].bands[0].q == 84);
  project->eqBanks[0x23].bands[0].frequency = 1700;
  CHECK(project->eqBanks[0x23].bands[0].frequency == 1700);
}

TEST_CASE("NEW EQ preset responses perform their intended utility") {
  auto project = std::make_unique<Project>();
  projectInit(project.get());
  instrumentEQInitPresets(project.get());
  for (float rate : {32000.0f, 44100.0f, 48000.0f}) {
    CAPTURE(rate);
    auto response = [&](int bank, float hz) { return instrumentEQResponseDb(project->eqBanks[bank], hz, rate); };
    CHECK(fabsf(response(0x10, 25) + 3.01f) < 0.03f);
    CHECK(response(0x10, 100) > -0.1);
    CHECK(response(0x11, 20) < -23);
    CHECK(response(0x12, 45) < -23);
    CHECK(fabsf(response(0x13, 8000) + 3.01f) < 0.03f);
    CHECK(fabsf(response(0x14, 300) + 3) < 0.01f);
    CHECK(fabsf(response(0x15, 650) + 3) < 0.01f);
    CHECK(fabsf(response(0x16, 3200) + 3) < 0.01f);
    CHECK(fabsf(response(0x17, 10000) - 1) < 0.02f);
    CHECK(response(0x18, 30) < -19);
    CHECK(response(0x18, 2500) > 1.8);
    CHECK(response(0x19, 900) > 1.7);
    CHECK(response(0x1a, 300) < -2.6);
    CHECK(response(0x1b, 5000) > 1.9);
    CHECK(response(0x1c, 800) < -1.3);
    CHECK(response(0x1c, 50) > 1.8);
    CHECK(response(0x1d, 50) > 1.4);
    CHECK(response(0x1d, 10000) < -1.4);
    CHECK(response(0x1e, 1200) > 4.9);
    CHECK(response(0x1f, 50) < -33);
    CHECK(response(0x1f, 1500) > 1.6);
    CHECK(response(0x20, 20) < -2.9);
    CHECK(response(0x20, 1000) > -0.1);
    CHECK(response(0x21, 50) < -1.4);
    CHECK(response(0x21, 10000) > 1.4);
    CHECK(response(0x22, 30) > -0.1);
    CHECK(response(0x22, 480) < -23);
    CHECK(fabsf(response(0x23, 1000) + 9) < 0.01f);
    CHECK(response(0x23, 500) > -0.15);
    for (int bank = 0x10; bank <= 0x23; ++bank) {
      InstrumentEQFilter filter;
      filter.init(rate);
      filter.configure(project->eqBanks[bank]);
      std::vector<float> audio(8192);
      audio[0] = audio[1] = 0.1f;
      filter.process(audio.data(), int(audio.size() / 2));
      for (float sample : audio) REQUIRE(std::isfinite(sample));
    }
  }
}

TEST_CASE("Unused flat EQ banks are allocated before NEW presets") {
  auto project = std::make_unique<Project>();
  projectInit(project.get());
  instrumentEQInitPresets(project.get());
  for (int i = 0; i < 16; ++i) {
    getInstrumentFunctions(InstrumentType::Braids).init(&project->instruments[i]);
    project->instruments[i].eq = i;
  }
  CHECK(instrumentEQUnusedBank(project.get(), -1) == 0x24);
  const InstrumentEQ original = project->eqBanks[0x10];
  InstrumentEQ imported = bell(-8);
  CHECK(instrumentEQAdopt(project.get(), imported, 16) == 0x24);
  CHECK(instrumentEQSame(original, project->eqBanks[0x10]));
  CHECK(instrumentEQAdopt(project.get(), original, 16) == 0x10);
}

TEST_CASE("NEW EQ presets and edits round trip while old projects retain flat banks") {
  auto saved = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  fillFXNames();
  projectInit(saved.get());
  projectInit(loaded.get());
  REQUIRE(projectLoad(saved.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  instrumentEQInitPresets(saved.get());
  saved->eqBanks[0x23].bands[0].frequency = 1700;
  instrumentEQDefaults(&saved->eqBanks[0x12]); // Clearing a preset must stay flat after loading.
  const char* path = "test_new_eq_presets.cct";
  REQUIRE(projectSave(saved.get(), path) == 0);
  REQUIRE(projectLoad(loaded.get(), path) == 0);
  for (int bank = 0; bank < PROJECT_EQ_BANKS; ++bank)
    CHECK(instrumentEQSame(saved->eqBanks[bank], loaded->eqBanks[bank]));
  CHECK(instrumentEQIsDefault(loaded->masterEQ));
  REQUIRE(projectLoad(loaded.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  for (const auto& eq : loaded->eqBanks) CHECK(instrumentEQIsDefault(eq));
  std::remove(path);
  projectFree(saved.get());
  projectFree(loaded.get());
}

TEST_CASE("Instrument EQ response matches the filter") {
  const InstrumentEQ boost = bell(12);  // +6 dB at 1 kHz
  CHECK(instrumentEQResponseDb(boost, 1000.0f, 48000.0f) == doctest::Approx(6.0f).epsilon(0.01));
  CHECK(fabsf(instrumentEQResponseDb(boost, 50.0f, 48000.0f)) < 0.2f);
  CHECK(db(filteredRms(boost, 1000.0f) / filteredRms(InstrumentEQ{}, 1000.0f)) == doctest::Approx(6.0f).epsilon(0.02));

  InstrumentEQ cut;
  instrumentEQDefaults(&cut);
  cut.bands[0] = {EQShape::lowCut, 200, 0, 42};
  CHECK(instrumentEQResponseDb(cut, 200.0f, 48000.0f) == doctest::Approx(-3.0f).epsilon(0.02));
  CHECK(instrumentEQResponseDb(cut, 50.0f, 48000.0f) < -20.0f);
  CHECK(db(filteredRms(cut, 50.0f) / 0.0707f) == doctest::Approx(instrumentEQResponseDb(cut, 50.0f, 48000.0f)).epsilon(0.05));

  InstrumentEQ shelf;
  instrumentEQDefaults(&shelf);
  shelf.bands[2].gain = -12;  // -6 dB above 8 kHz, even with the EQ switched off
  CHECK(instrumentEQResponseDb(shelf, 16000.0f, 48000.0f) == doctest::Approx(-6.0f).epsilon(0.05));
  CHECK(fabsf(instrumentEQResponseDb(shelf, 200.0f, 48000.0f)) < 0.1f);
}

TEST_CASE("EQ banks and instrument EQ numbers are saved only when used or changed") {
  auto a = std::make_unique<Project>(), b = std::make_unique<Project>();
  fillFXNames();
  projectInit(a.get());
  projectInit(b.get());
  REQUIRE(projectLoad(a.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  getInstrumentFunctions(InstrumentType::Braids).init(&a->instruments[3]);
  getInstrumentFunctions(InstrumentType::Braids).init(&a->instruments[4]);
  getInstrumentFunctions(InstrumentType::Braids).init(&a->instruments[6]);
  a->instruments[3].eq = 0x12;
  a->instruments[6].eq = 0x12;  // sharing a bank
  InstrumentEQ& eq = a->eqBanks[0x12];
  eq.bands[0] = {EQShape::lowCut, 85, 0, 60};
  eq.bands[1].gain = -17;
  eq.bands[2] = {EQShape::highCut, 12500, 4, 30};
  a->masterEQ.bands[1] = {EQShape::bell, 2500, 8, 54};
  REQUIRE(projectSave(a.get(), "test_instrument_eq.cct") == 0);
  REQUIRE(projectLoad(b.get(), "test_instrument_eq.cct") == 0);
  CHECK(b->instruments[3].eq == 0x12);
  CHECK(b->instruments[6].eq == 0x12);
  CHECK(b->instruments[4].eq == EMPTY_VALUE_8);
  CHECK(instrumentEQSame(b->eqBanks[0x12], eq));
  CHECK(instrumentEQSame(b->masterEQ, a->masterEQ));
  for (int bank = 0; bank < PROJECT_EQ_BANKS; ++bank)
    if (bank != 0x12) CHECK(instrumentEQIsDefault(b->eqBanks[bank]));
  FILE* file = fopen("test_instrument_eq.cct", "rb");
  REQUIRE(file);
  std::string text;
  char buffer[4096];
  for (size_t n; (n = fread(buffer, 1, sizeof(buffer), file)) > 0;) text.append(buffer, n);
  fclose(file);
  // Two "- EQ: 18" instrument lines, and one changed bank.
  size_t lines = 0;
  for (size_t at = 0; (at = text.find("- EQ: ", at)) != std::string::npos; ++at) ++lines;
  CHECK(lines == 2);
  CHECK(text.find("## EQ banks\n\n12 1,85,0,60,3,1000,-17,48,5,12500,4,30\n") != std::string::npos);
  CHECK(text.find("FE 2,100,0,42,3,2500,8,54,4,8000,0,42\n") != std::string::npos);
  std::remove("test_instrument_eq.cct");
  projectFree(a.get());
  projectFree(b.get());
}

TEST_CASE("A saved EQ with a Q beyond the range loads with the narrowest Q") {
  auto a = std::make_unique<Project>(), b = std::make_unique<Project>();
  fillFXNames();
  projectInit(a.get());
  projectInit(b.get());
  REQUIRE(projectLoad(a.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  a->eqBanks[0x05].bands[1] = {EQShape::bell, 1000, -12, 60};
  REQUIRE(projectSave(a.get(), "test_instrument_eq_q.cct") == 0);
  // An earlier build allowed Q codes up to 120 (Q 64).
  std::string text;
  {
    FILE* file = fopen("test_instrument_eq_q.cct", "rb");
    REQUIRE(file);
    char buffer[4096];
    for (size_t n; (n = fread(buffer, 1, sizeof(buffer), file)) > 0;) text.append(buffer, n);
    fclose(file);
  }
  const std::string saved = "05 2,100,0,42,3,1000,-12,60,";
  const size_t at = text.find(saved);
  REQUIRE(at != std::string::npos);
  text.replace(at, saved.size(), "05 2,100,0,42,3,1000,-12,110,");
  {
    FILE* file = fopen("test_instrument_eq_q.cct", "wb");
    REQUIRE(file);
    fwrite(text.data(), 1, text.size(), file);
    fclose(file);
  }
  REQUIRE(projectLoad(b.get(), "test_instrument_eq_q.cct") == 0);
  CHECK(b->eqBanks[0x05].bands[1].gain == -12);
  CHECK(b->eqBanks[0x05].bands[1].q == kEQQMax);
  CHECK(instrumentEQQ(kEQQMax) == doctest::Approx(16.0f));
  std::remove("test_instrument_eq_q.cct");
  projectFree(a.get());
  projectFree(b.get());
}

TEST_CASE("An instrument file brings its EQ into a free bank of the project") {
  auto a = std::make_unique<Project>(), b = std::make_unique<Project>();
  fillFXNames();
  projectInit(a.get());
  projectInit(b.get());
  REQUIRE(projectLoad(a.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  REQUIRE(projectLoad(b.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  getInstrumentFunctions(InstrumentType::Braids).init(&a->instruments[2]);
  a->instruments[2].eq = 0;
  a->eqBanks[0].bands[1].gain = 10;
  REQUIRE(instrumentSave(a.get(), "test_instrument_eq.cni", 2) == 0);

  // Bank 0 of the other project is taken by one of its instruments.
  getInstrumentFunctions(InstrumentType::Braids).init(&b->instruments[0]);
  b->instruments[0].eq = 0;
  REQUIRE(instrumentLoad(b.get(), "test_instrument_eq.cni", 9) == 0);
  const uint8_t bank = b->instruments[9].eq;
  CHECK(bank == 1);
  CHECK(instrumentEQSame(b->eqBanks[bank], a->eqBanks[0]));
  CHECK(instrumentEQIsDefault(b->eqBanks[0]));
  // Loading it again finds the same EQ instead of using another bank.
  REQUIRE(instrumentLoad(b.get(), "test_instrument_eq.cni", 10) == 0);
  CHECK(b->instruments[10].eq == bank);
  std::remove("test_instrument_eq.cni");
  projectFree(a.get());
  projectFree(b.get());
}

TEST_CASE("Instrument EQ filters the track the instrument plays on") {
  InstrumentEQ flat;
  instrumentEQDefaults(&flat);
  std::vector<float> dry, withFlat;
  const double reference = previewEnergy(flat, &dry, EMPTY_VALUE_8);
  REQUIRE(reference > 0.0);

  // A flat EQ leaves the track exactly as it was.
  previewEnergy(flat, &withFlat);
  CHECK(withFlat == dry);

  // A low high cut removes nearly everything above it.
  InstrumentEQ dark = flat;
  dark.bands[2] = {EQShape::highCut, 40, 0, 42};
  CHECK(previewEnergy(dark) < reference * 0.01);

  // The same EQ does nothing when the instrument doesn't use it.
  CHECK(previewEnergy(dark, nullptr, EMPTY_VALUE_8) == doctest::Approx(reference));
}

TEST_CASE("Master EQ filters the complete mix") {
  InstrumentEQ flat, dark;
  instrumentEQDefaults(&flat);
  dark = flat;
  dark.bands[2] = {EQShape::highCut, 40, 0, 42};
  uint8_t trackActive = 1, masterActive = 1;
  const double reference = previewEnergy(flat, nullptr, 5, nullptr, &trackActive, &masterActive);
  CHECK(trackActive == 0);
  CHECK(masterActive == 0);
  CHECK(previewEnergy(flat, nullptr, EMPTY_VALUE_8, &flat) == doctest::Approx(reference));
  CHECK(previewEnergy(flat, nullptr, EMPTY_VALUE_8, &dark) < reference * 0.01);
}

namespace {

struct EQPage {
  ChipNomadState* previous = chipnomadState;
  const AppScreen* previousScreen = currentScreen;
  AppSettings settings = appSettings;
  int previousInstrument = cInstrument;
  int previousModified = projectModified;
  explicit EQPage(InstrumentType type) {
    chipnomadState = chipnomadCreate();
    initDefaultAppSettings();
    screensInitAll();
    fillFXNames();
    cInstrument = 0;
    getInstrumentFunctions(type).init(&instrument());
  }
  ~EQPage() {
    chipnomadDestroy(chipnomadState);
    chipnomadState = previous;
    currentScreen = previousScreen;
    appSettings = settings;
    cInstrument = previousInstrument;
    projectModified = previousModified;
  }
  Instrument& instrument() { return chipnomadState->project.instruments[0]; }
};

}  // namespace

TEST_CASE("EQ screen edits move the band and OPT returns to the instrument") {
  EQPage e(InstrumentType::Sample);
  e.instrument().eq = 7;
  currentScreen = &screenInstrumentEQ;
  screenInstrumentEQ.setup(7);
  screenInstrumentEQ.fullRedraw();
  ScreenData* eqPage = mockScreenData;
  REQUIRE(eqPage);
  InstrumentEQBand& low = chipnomadState->project.eqBanks[7].bands[0];
  // Band row: sideways is frequency, up and down is gain.
  eqPage->onEdit(0, 1, CellEditAction::increaseBig);
  CHECK(low.gain == 2);
  eqPage->onEdit(0, 1, CellEditAction::increase);
  eqPage->onEdit(0, 1, CellEditAction::decrease);
  CHECK(low.frequency == 100);
  eqPage->onEdit(0, 3, CellEditAction::increaseBig);  // Freq: half an octave
  CHECK(low.frequency == 141);
  eqPage->onEdit(0, 2, CellEditAction::decrease);     // Shape: Lo shelf to Lo cut
  CHECK(low.shape == EQShape::lowCut);
  CHECK(eqPage->onEdit(0, 4, CellEditAction::increase) == 0);  // cuts have no gain
  eqPage->onEdit(0, 1, CellEditAction::increaseBig);  // a cut's dot moves its resonance
  CHECK(low.q == 44);
  eqPage->onEdit(0, 1, CellEditAction::clear);
  CHECK(low.shape == EQShape::lowShelf);
  CHECK(low.gain == 0);
  screenInstrumentEQ.onInput(1, keyOpt, 1);
  CHECK(currentScreen == &screenInstrument);
}

TEST_CASE("Master EQ FE screen returns to the mixer") {
  EQPage e(InstrumentType::Sample);
  currentScreen = &screenInstrumentEQ;
  screenInstrumentEQ.setup(PROJECT_MASTER_EQ_BANK);
  screenInstrumentEQ.fullRedraw();
  REQUIRE(mockScreenData);
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[0], 5) == "EQ FE");
  CHECK(std::string(mockGfxCells[1], 13) == "Master output");
  screenInstrumentEQ.onInput(1, keyOpt, 1);
  CHECK(currentScreen == &screenMixer);
}

TEST_CASE("EQ screen EDIT arrows assign banks only to the current instrument") {
  EQPage e(InstrumentType::Braids);
  auto& project = chipnomadState->project;
  instrumentEQInitPresets(&project);
  e.instrument().eq = 0x16;
  getInstrumentFunctions(InstrumentType::Braids).init(&project.instruments[1]);
  project.instruments[1].eq = 0x16;
  InstrumentEQ original[PROJECT_EQ_BANKS];
  memcpy(original, project.eqBanks, sizeof(original));
  currentScreen = &screenInstrumentEQ;
  screenInstrumentEQ.setup(0x16);
  screenInstrumentEQ.fullRedraw();
  REQUIRE(mockScreenData);
  mockScreenData->cursorCol = 2;
  mockScreenData->cursorRow = 3;
  projectModified = 0;
  CHECK(screenInstrumentEQ.onInput(1, keyOpt | keyRight, 1) == 1);
  CHECK(e.instrument().eq == 0x16);
  CHECK(projectModified == 0);
  mockScreenData->cursorCol = 0;
  mockScreenData->cursorRow = 0;
  CHECK(mockScreenData->getColumnCount(0) == 1);
  CHECK(mockScreenData->onEdit(0, 0, CellEditAction::clear) == 0);
  CHECK(screenInstrumentEQ.onInput(0, keyOpt | keyRight, 1) == 1);
  CHECK(e.instrument().eq == 0x16);
  CHECK(projectModified == 0);
  CHECK(mockScreenData->onEdit(0, 0, CellEditAction::increase) == 1);
  CHECK(e.instrument().eq == 0x17);
  CHECK(projectModified == 1);
  CHECK(currentScreen == &screenInstrumentEQ);
  CHECK(mockScreenData->cursorCol == 0);
  CHECK(mockScreenData->cursorRow == 0);
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[0], 5) == "EQ 17");
  CHECK(std::string(mockGfxCells[1], 10) == "Used by 00");
  mockScreenData->onEdit(0, 0, CellEditAction::decrease);
  CHECK(e.instrument().eq == 0x16);
  CHECK(project.instruments[1].eq == 0x16);
  mockScreenData->onEdit(0, 4, CellEditAction::decrease);
  CHECK(project.eqBanks[0x16].bands[0].gain == original[0x16].bands[0].gain - 1);
  project.eqBanks[0x16] = original[0x16];
  for (int bank = 0; bank < PROJECT_EQ_BANKS; ++bank)
    CHECK(instrumentEQSame(project.eqBanks[bank], original[bank]));
  CHECK(mockScreenData->onEdit(0, 0, CellEditAction::increase) == 1);
  CHECK(e.instrument().eq == 0x17);
  CHECK(mockScreenData->onEdit(0, 0, CellEditAction::increaseBig) == 1);
  CHECK(e.instrument().eq == 0x27);
  CHECK(mockScreenData->onEdit(0, 0, CellEditAction::decreaseBig) == 1);
  CHECK(e.instrument().eq == 0x17);
  screenInstrumentEQ.onInput(1, keyOpt, 1);
  CHECK(currentScreen == &screenInstrument);
}

TEST_CASE("EQ screen bank browsing stops at 00 and 7F and leaves master FE alone") {
  EQPage e(InstrumentType::Sample);
  for (int bank : {0, PROJECT_EQ_BANKS - 1, PROJECT_MASTER_EQ_BANK}) {
    CAPTURE(bank);
    currentScreen = &screenInstrumentEQ;
    e.instrument().eq = bank == PROJECT_MASTER_EQ_BANK ? 7 : bank;
    screenInstrumentEQ.setup(bank);
    screenInstrumentEQ.fullRedraw();
    mockScreenData->cursorRow = 0;
    projectModified = 0;
    mockScreenData->onEdit(0, 0, bank == 0 ? CellEditAction::decrease : CellEditAction::increase);
    CHECK(projectModified == 0);
    CHECK(e.instrument().eq == (bank == PROJECT_MASTER_EQ_BANK ? 7 : bank));
    CHECK(currentScreen == &screenInstrumentEQ);
    if (bank == PROJECT_MASTER_EQ_BANK) {
      CHECK(mockScreenData->onEdit(0, 0, CellEditAction::decrease) == 0);
      CHECK(e.instrument().eq == 7);
      mockScreenData->drawStatic();
      CHECK(std::string(mockGfxCells[0], 5) == "EQ FE");
    }
  }
}
