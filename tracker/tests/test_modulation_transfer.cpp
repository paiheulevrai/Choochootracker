#include "doctest.h"
#include "project.h"
#include <cstring>

namespace {
Instrument makeInstrument(InstrumentType type) {
  Instrument instrument{};
  getInstrumentFunctions(type).init(&instrument);
  return instrument;
}

int genericDestination(const Instrument& instrument, int generic) {
  return getInstrumentFunctions(instrument.type).modDestinationsCount + 1 + generic;
}

void checkSourcePreserved(const Modulation& before, const Modulation& after) {
  Modulation expected = before;
  expected.destination = after.destination;
  CHECK(std::memcmp(&expected, &after, sizeof(expected)) == 0);
}
}

TEST_CASE("modulation transfer preserves sources and common destinations") {
  Instrument source = makeInstrument(InstrumentType::Braids);
  Instrument target = makeInstrument(InstrumentType::MME);
  source.modulation[0] = {ModulationType::ADSR, 5, -63, 10, 20, 190, 40, 0};
  source.modulation[1] = {ModulationType::SLFO, 6, 79, 9, 2, 36, 4, 187};
  source.modulation[2] = {ModulationType::StickRate,
    uint8_t(genericDestination(source, genericModFirstInsert + 12)), -91, 3, 32, 0, 0, 0};
  source.modulation[3] = {ModulationType::Sequence,
    uint8_t(genericDestination(source, genericModFirstParameter + 14)), 100, 2, 1, 21, 249, 64};
  CHECK(instrumentTransferModulations(&source, &target) == 0);
  CHECK(target.modulation[0].destination == 9); // Cutoff, not macro 3.
  CHECK(target.modulation[1].destination == 10);
  CHECK(target.modulation[2].destination == genericDestination(target, genericModFirstInsert + 12));
  CHECK(target.modulation[3].destination == genericDestination(target, genericModFirstParameter + 14));
  for (int i = 0; i < 4; ++i) checkSourcePreserved(source.modulation[i], target.modulation[i]);

  Instrument audio = makeInstrument(InstrumentType::AudioIn);
  source.modulation[0].destination = 2;
  instrumentTransferModulations(&source, &audio);
  CHECK(audio.modulation[0].destination == 0); // AudioIn destination 2 is Cutoff, not Pitch.
  audio.modulation[0].destination = 2;
  instrumentTransferModulations(&audio, &target);
  CHECK(target.modulation[0].destination == 9);
}

TEST_CASE("modulation transfer follows logical macro order rather than legacy IDs") {
  Instrument source = makeInstrument(InstrumentType::BYOWTBL);
  Instrument target = makeInstrument(InstrumentType::MME);
  const int oldDestinations[] = {5, 6, 3, 4}; // Pos A, Pos B, Detune, Mix.
  for (int i = 0; i < 4; ++i) source.modulation[i].destination = oldDestinations[i];
  CHECK(instrumentTransferModulations(&source, &target) == 4);
  for (int i = 0; i < 4; ++i) CHECK(target.modulation[i].destination == 3 + i);
  instrumentTransferModulations(&target, &source);
  for (int i = 0; i < 4; ++i) CHECK(source.modulation[i].destination == oldDestinations[i]);

  Instrument acid = makeInstrument(InstrumentType::AChChid);
  Instrument braids = makeInstrument(InstrumentType::Braids);
  braids.modulation[0].destination = 3;
  braids.modulation[1].destination = 4;
  instrumentTransferModulations(&braids, &acid);
  CHECK(acid.modulation[0].destination == 8); // Timbre, not EnvMod.
  CHECK(acid.modulation[1].destination == 9);
}

TEST_CASE("modulation transfer clamps macro rank and disables missing target families") {
  Instrument source = makeInstrument(InstrumentType::Sintered);
  Instrument target = makeInstrument(InstrumentType::Braids);
  source.modulation[0].destination = 8;
  source.modulation[1].destination = genericDestination(source, genericModReverbSend);
  source.modulation[2].destination = genericDestination(source, genericModInstrumentPan);
  source.modulation[3].destination = 0;
  CHECK(instrumentTransferModulations(&source, &target) == 1);
  CHECK(target.modulation[0].destination == 4); // Last available macro, Color.
  CHECK(target.modulation[1].destination == genericDestination(target, genericModReverbSend));
  CHECK(target.modulation[2].destination == genericDestination(target, genericModInstrumentPan));
  CHECK(target.modulation[3].destination == 0);
  Instrument midi = makeInstrument(InstrumentType::Midi);
  instrumentTransferModulations(&source, &midi);
  CHECK(midi.modulation[0].destination == 0);
  CHECK(midi.modulation[1].destination == genericDestination(midi, genericModReverbSend));
  CHECK(midi.modulation[2].destination == 0);
  for (int i = 0; i < 4; ++i) checkSourcePreserved(source.modulation[i], midi.modulation[i]);
}

TEST_CASE("modulation transfer handles SID shared controls and FM operator limits") {
  Instrument sid = makeInstrument(InstrumentType::SID);
  Instrument plaits = makeInstrument(InstrumentType::Plaits);
  sid.modulation[0].destination = genericDestination(sid, genericModSIDCutoff);
  sid.modulation[1].destination = genericDestination(sid, genericModSIDResonance);
  sid.modulation[2].destination = genericDestination(sid, genericModSIDAttack);
  sid.modulation[3].destination = genericDestination(sid, genericModSIDRelease);
  CHECK(instrumentTransferModulations(&sid, &plaits) == 0);
  CHECK(plaits.modulation[0].destination == 7);
  CHECK(plaits.modulation[1].destination == 8);
  CHECK(plaits.modulation[2].destination == genericDestination(plaits, genericModEnvelopeAttack));
  CHECK(plaits.modulation[3].destination == genericDestination(plaits, genericModEnvelopeRelease));
  instrumentTransferModulations(&plaits, &sid);
  CHECK(sid.modulation[2].destination == genericDestination(sid, genericModSIDAttack));
  CHECK(sid.modulation[3].destination == genericDestination(sid, genericModSIDRelease));

  Instrument dx7 = makeInstrument(InstrumentType::DX7);
  Instrument opl2 = makeInstrument(InstrumentType::OPL2);
  dx7.modulation[0].destination = genericDestination(dx7, genericModFMOperator2);
  dx7.modulation[1].destination = genericDestination(dx7, genericModFMOperator6);
  dx7.modulation[2].destination = genericDestination(dx7, genericModFirstDirectFM + 60);
  dx7.modulation[3].destination = genericDestination(dx7, genericModFirstDirectFM + 12);
  instrumentTransferModulations(&dx7, &opl2);
  CHECK(opl2.modulation[0].destination == genericDestination(opl2, genericModFMOperator2));
  CHECK(opl2.modulation[3].destination == genericDestination(opl2, genericModFirstDirectFM + 12));
  for (const auto& modulation : opl2.modulation)
    CHECK(modulationSequenceDestinationAllowed(&opl2, &modulation, modulation.destination));
}

TEST_CASE("modulation transfer keeps every engine pair routable without changing sources") {
  for (int from = 1; from < int(InstrumentType::totalCount); ++from) {
    Instrument source = makeInstrument(InstrumentType(from));
    if (source.type == InstrumentType::none) continue;
    for (int to = 1; to < int(InstrumentType::totalCount); ++to) {
      Instrument target = makeInstrument(InstrumentType(to));
      if (target.type == InstrumentType::none) continue;
      CAPTURE(from); CAPTURE(to);
      for (int d = 0; d <= getInstrumentFunctions(source.type).modDestinationsCount + genericModTotalCount; ++d) {
        if (!instrumentModDestinationAvailable(&source, d)) continue;
        CAPTURE(d);
        source.modulation[0] = {ModulationType::ADSR, uint8_t(d), -127, 11, 22, 33, 44, 55};
        source.modulation[1] = {ModulationType::FLFO, uint8_t(d), 127, 9, 2, 221, 88, 192};
        source.modulation[2] = {ModulationType::StickRate, uint8_t(d), -53, 3, 48, 77, 66, 55};
        source.modulation[3] = {ModulationType::Sequence, uint8_t(d), 89,
          uint8_t(SequenceSource::pitch), 1, 33, 44, 55};
        Instrument before = target;
        instrumentTransferModulations(&source, &target);
        for (int i = 0; i < 4; ++i) {
          checkSourcePreserved(source.modulation[i], target.modulation[i]);
          CHECK(modulationSequenceDestinationAllowed(&target, &target.modulation[i],
                                                     target.modulation[i].destination));
          if (from == to && modulationSequenceDestinationAllowed(&source, &source.modulation[i], d))
            CHECK(target.modulation[i].destination == d);
        }
        std::memcpy(before.modulation, target.modulation, sizeof(before.modulation));
        CHECK(std::memcmp(&before, &target, sizeof(target)) == 0);
      }
    }
  }
}

TEST_CASE("modulation transfer leaves clearing and fresh initialization alone") {
  Instrument source = makeInstrument(InstrumentType::Braids);
  Instrument empty = makeInstrument(InstrumentType::none);
  source.modulation[0] = {ModulationType::SlowLFO, 3, 100, 9, 1, 200, 0, 87};
  CHECK(instrumentTransferModulations(&source, &empty) == 0);
  CHECK(empty.modulation[0].destination == 0);
  Instrument target = makeInstrument(InstrumentType::MME);
  Instrument expected = target;
  CHECK(instrumentTransferModulations(&empty, &target) == 0);
  CHECK(std::memcmp(&expected, &target, sizeof(target)) == 0);
}
