#include "doctest.h"
#include "project.h"
#include "spatial_insert_pool.h"
#include "synth/master_effects.h"

#include <algorithm>
#include <cmath>
#include <vector>

TEST_CASE("spatial DSP matches the corresponding global effect") {
  const int modules[] = {insertCloudsReverb, insertBeadsReverb, insertPingPongDelay,
                         insertTapeEcho, insertDiffusingEchoverb};
  for (int module : modules) {
    INFO("module ", module);
    Project project;
    projectInit(&project);
    project.perceptualEffects = 0;
    project.tickRate = 100.0f;
    project.reverbReturn = project.delayReturn = 100;
    project.delayReverbSend = 0;
    project.reverbAlgorithm = module == insertBeadsReverb ? 1 : 0;
    project.reverbTime = project.beadsReverbTime = 128;
    project.reverbDamping = project.beadsReverbTone = 150;
    project.reverbFilterCutoffHz = project.delayFilterCutoffHz = 3000;
    project.reverbHighpassCutoffHz = project.beadsReverbHighpassCutoffHz =
      project.echoHighpassCutoffHz = 100;
    project.delayAlgorithm = module == insertTapeEcho ? 1 :
      module == insertDiffusingEchoverb ? 2 : 0;
    project.delayTicks = 2;
    project.delayFeedback = 50;
    project.tapeDivision = 0;
    project.tapeHeads = 3;
    project.tapeTone = 150;
    project.tapeFlutter = 20;
    project.tapeSpring = 30;
    project.echoDiffuse = 80;
    project.echoAge = 30;
    MasterEffects effects{};
    effects.init(8000.0f);
    SpatialInsertPool pool;
    auto insert = pool.prepare(0, module, 8000.0f);
    REQUIRE(insert);
    for (int block = 0; block < 100; ++block) {
      if (block == 40) {
        project.tickRate = 123.0f;
        project.echoDiffuse = 220;
        project.echoAge = 180;
      }
      float parameters[8]{};
      if (module == insertCloudsReverb) {
        parameters[0] = 0.2f + 0.78f * project.reverbTime / 255.0f;
        parameters[1] = project.reverbDamping / 255.0f;
        parameters[2] = project.reverbFilterCutoffHz;
        parameters[3] = project.reverbHighpassCutoffHz;
      } else if (module == insertBeadsReverb) {
        parameters[0] = project.beadsReverbTime / 255.0f;
        parameters[1] = project.beadsReverbTone / 255.0f;
        parameters[2] = project.beadsReverbHighpassCutoffHz;
      } else if (module == insertPingPongDelay) {
        parameters[0] = project.delayTicks;
        parameters[1] = project.delayFeedback / 100.0f;
        parameters[2] = project.delayFilterCutoffHz;
      } else if (module == insertTapeEcho) {
        parameters[0] = project.tapeDivision;
        parameters[1] = project.tapeHeads;
        parameters[2] = project.delayFeedback / 100.0f;
        parameters[3] = project.tapeTone / 255.0f;
        parameters[4] = project.tapeFlutter / 100.0f;
        parameters[5] = project.tapeSpring / 100.0f;
      } else {
        parameters[0] = project.delayTicks;
        parameters[1] = project.delayFeedback / 100.0f;
        parameters[2] = project.echoDiffuse / 255.0f;
        parameters[3] = project.echoAge / 255.0f;
        parameters[4] = project.echoHighpassCutoffHz;
        parameters[5] = project.delayFilterCutoffHz;
      }
      float left[128], right[128], bus[256], expected[256]{};
      for (int i = 0; i < 128; ++i) {
        const int position = block * 128 + i;
        left[i] = bus[i * 2] = position < 3000 ? 0.2f * sinf(position * 0.13f) : 0.0f;
        right[i] = bus[i * 2 + 1] = position < 3000 ? 0.1f * cosf(position * 0.19f) : 0.0f;
      }
      const bool reverb = module == insertCloudsReverb || module == insertBeadsReverb;
      effects.process(reverb ? bus : nullptr, reverb ? nullptr : bus, expected, 128, &project);
      insert->process(left, right, 128, parameters, project.tickRate);
      float maximumError = 0.0f;
      for (int i = 0; i < 128; ++i) {
        maximumError = std::max(maximumError, std::abs(left[i] - expected[i * 2]));
        maximumError = std::max(maximumError, std::abs(right[i] - expected[i * 2 + 1]));
      }
      INFO("block ", block);
      CHECK(maximumError < 1.0e-5f);
    }
  }
}

TEST_CASE("Ping-pong delay follows the project tick duration") {
  MasterEffects* effects = new MasterEffects();
  effects->init(1000.0f);

  Project project;
  projectInit(&project);
  project.tickRate = 100.0f;
  project.delayTicks = 1;
  project.delayFeedback = 50;
  project.delayReturn = 100;
  project.delayFilterCutoffHz = 450;

  std::vector<float> bus(48 * 2, 0.0f);
  std::vector<float> output(48 * 2, 0.0f);
  bus[0] = 1.0f;
  effects->process(nullptr, bus.data(), output.data(), 48, &project);

  CHECK(std::fabs(output[10 * 2]) > 0.1f);
  CHECK(std::fabs(output[20 * 2 + 1]) > 0.01f);
  delete effects;
}

TEST_CASE("spatial inserts leave the existing global send processors unchanged") {
  Project sends;
  projectInit(&sends);
  sends.tickRate = 100.0f;
  sends.reverbReturn = 100;
  sends.reverbTime = 180;
  sends.reverbDamping = 150;
  sends.reverbFilterCutoffHz = 18000;
  sends.reverbHighpassCutoffHz = 30;
  sends.delayAlgorithm = 2;
  sends.delayTicks = 1;
  sends.delayFeedback = 55;
  sends.delayReturn = 100;
  sends.delayFilterCutoffHz = 18000;
  sends.echoDiffuse = 160;
  sends.echoAge = 80;
  sends.echoHighpassCutoffHz = 30;
  Project withInserts = sends;
  const int modules[] = {insertCloudsReverb, insertBeadsReverb, insertPingPongDelay,
                         insertTapeEcho, insertDiffusingEchoverb};

  MasterEffects effects;
  effects.init(1000.0f);
  std::vector<float> reverbInput(512 * 2, 0.0f), delayInput(512 * 2, 0.0f);
  reverbInput[0] = 0.5f;
  delayInput[0] = 1.0f;
  std::vector<float> expected(512 * 2, 0.0f);
  effects.process(reverbInput.data(), delayInput.data(), expected.data(), 512, &sends);
  for (int i = 0; i < 5; ++i) {
    insertSelect(&withInserts.masterInserts[0], modules[i]);
    insertSelect(&withInserts.masterInserts[1], modules[(i + 1) % 5]);
    effects.reset();
    auto reverb = std::vector<float>(512 * 2, 0.0f);
    auto delay = std::vector<float>(512 * 2, 0.0f);
    reverb[0] = 0.5f;
    delay[0] = 1.0f;
    std::vector<float> output(512 * 2, 0.0f);
    effects.process(reverb.data(), delay.data(), output.data(), 512, &withInserts);
    CHECK(std::equal(output.begin(), output.end(), expected.begin(),
      [](float a, float b) { return std::abs(a - b) < 1.0e-5f; }));
  }
}

TEST_CASE("Reverb high-pass removes low-frequency send energy") {
  Project project;
  projectInit(&project);
  project.reverbReturn = 100;
  project.reverbTime = 180;
  project.reverbDamping = 255;
  project.reverbFilterCutoffHz = 20000;
  std::vector<float> bus(4096 * 2, 1.0f), lowOutput(4096 * 2, 0.0f), highOutput(4096 * 2, 0.0f);

  MasterEffects low{}, high{};
  low.init(48000.0f);
  high.init(48000.0f);
  project.reverbHighpassCutoffHz = 20;
  low.process(bus.data(), nullptr, lowOutput.data(), 4096, &project);
  project.reverbHighpassCutoffHz = 2000;
  high.process(bus.data(), nullptr, highOutput.data(), 4096, &project);

  float lowEnergy = 0.0f, highEnergy = 0.0f;
  for (int i = 1400 * 2; i < 4096 * 2; ++i) {
    lowEnergy += fabsf(lowOutput[i]);
    highEnergy += fabsf(highOutput[i]);
  }
  CHECK(lowEnergy > highEnergy * 3.0f);
}

TEST_CASE("Beads reverb has its own high-pass send filter") {
  Project project;
  projectInit(&project);
  project.reverbAlgorithm = 1;
  project.reverbReturn = 100;
  project.beadsReverbTime = 180;
  project.beadsReverbTone = 255;
  std::vector<float> bus(12000 * 2, 1.0f), lowOutput(12000 * 2, 0.0f), highOutput(12000 * 2, 0.0f);
  MasterEffects low{}, high{};
  low.init(48000.0f); high.init(48000.0f);
  project.beadsReverbHighpassCutoffHz = 20;
  low.process(bus.data(), nullptr, lowOutput.data(), 12000, &project);
  project.beadsReverbHighpassCutoffHz = 2000;
  high.process(bus.data(), nullptr, highOutput.data(), 12000, &project);
  float lowEnergy = 0.0f, highEnergy = 0.0f;
  for (int i = 7000 * 2; i < 12000 * 2; ++i) { lowEnergy += fabsf(lowOutput[i]); highEnergy += fabsf(highOutput[i]); }
  CHECK(lowEnergy > highEnergy * 3.0f);
}

TEST_CASE("Beads reverb preserves a tail after a short send") {
  Project project;
  projectInit(&project);
  project.reverbAlgorithm = 1;
  project.reverbReturn = 100;
  project.beadsReverbTime = 220;
  project.beadsReverbTone = 200;
  MasterEffects effects;
  effects.init(8000.0f);
  std::vector<float> bus(64 * 2, 0.0f), output(64 * 2, 0.0f);
  bus[0] = 1.0f;
  effects.process(bus.data(), nullptr, output.data(), 64, &project);
  CHECK(effects.reverbTailActive());

  double tailEnergy = 0.0;
  std::fill(bus.begin(), bus.end(), 0.0f);
  for (int block = 0; block < 128; ++block) {
    std::fill(output.begin(), output.end(), 0.0f);
    effects.process(bus.data(), nullptr, output.data(), 64, &project);
    for (float sample : output) tailEnergy += sample * sample;
  }
  CHECK(tailEnergy > 0.000001);
}

TEST_CASE("Reset clears an existing delay tail") {
  MasterEffects effects;
  effects.init(1000.0f);
  Project project;
  projectInit(&project);
  project.tickRate = 100.0f;
  project.delayTicks = 1;
  project.delayFeedback = 50;
  project.delayReturn = 100;
  project.delayFilterCutoffHz = 450;
  std::vector<float> bus(12 * 2, 0.0f), output(12 * 2, 0.0f);
  bus[0] = 1.0f;
  effects.process(nullptr, bus.data(), output.data(), 12, &project);
  CHECK(effects.tailActive());
  effects.reset();
  CHECK_FALSE(effects.tailActive());
  std::fill(bus.begin(), bus.end(), 0.0f);
  std::fill(output.begin(), output.end(), 0.0f);
  effects.process(nullptr, bus.data(), output.data(), 12, &project);
  CHECK(std::fabs(output[10 * 2]) < 0.0001f);
}

TEST_CASE("Tape Echo heads follow their synchronized divisions") {
  MasterEffects effects;
  effects.init(1000.0f);
  Project project;
  projectInit(&project);
  project.tickRate = 100.0f;
  project.delayAlgorithm = 1;
  project.tapeDivision = 0; // 6 ticks = 60 samples at this rate.
  project.tapeHeads = 4; // H1 + H3
  project.tapeTone = 255;
  project.tapeFlutter = 0;
  project.tapeSpring = 0;
  project.delayFeedback = 0;
  project.delayReturn = 100;
  std::vector<float> bus(200 * 2, 0.0f), output(200 * 2, 0.0f);
  bus[0] = 1.0f;
  effects.process(nullptr, bus.data(), output.data(), 200, &project);
  CHECK(std::fabs(output[60 * 2]) > 0.1f);
  CHECK(std::fabs(output[180 * 2]) > 0.1f);
  CHECK(std::fabs(output[120 * 2]) < 0.01f);
}

TEST_CASE("Tape Echo feedback is normalized across selected heads") {
  MasterEffects effects;
  effects.init(1000.0f);
  Project project;
  projectInit(&project);
  project.tickRate = 100.0f;
  project.delayAlgorithm = 1;
  project.tapeDivision = 0;
  project.tapeHeads = 6; // All three heads.
  project.tapeTone = 255;
  project.tapeFlutter = 0;
  project.tapeSpring = 0;
  project.delayFeedback = 95;
  project.delayReturn = 100;
  std::vector<float> bus(200 * 2, 0.0f), output(200 * 2, 0.0f);
  bus[0] = 1.0f;
  effects.process(nullptr, bus.data(), output.data(), 200, &project);

  // At the second head, the direct repeat plus the first-head feedback must
  // stay below 1.5 rather than adding an unbounded 0.95 loop contribution.
  CHECK(std::fabs(output[120 * 2]) < 1.5f);
}

TEST_CASE("Tape Echo Spring is parallel to the tape repeats") {
  MasterEffects dry{}, spring{};
  dry.init(1000.0f); spring.init(1000.0f);
  Project project;
  projectInit(&project);
  project.tickRate = 100.0f;
  project.delayAlgorithm = 1;
  project.tapeDivision = 0;
  project.tapeHeads = 0;
  project.tapeTone = 255;
  project.tapeFlutter = 0;
  project.delayFeedback = 0;
  project.delayReturn = 100;
  std::vector<float> bus(80 * 2, 0.0f), dryOutput(80 * 2, 0.0f), springOutput(80 * 2, 0.0f);
  bus[0] = 1.0f;
  project.tapeSpring = 0;
  dry.process(nullptr, bus.data(), dryOutput.data(), 80, &project);
  project.tapeSpring = 100;
  spring.process(nullptr, bus.data(), springOutput.data(), 80, &project);
  CHECK(std::fabs(dryOutput[31 * 2]) < 0.001f);
  CHECK(std::fabs(springOutput[31 * 2]) > 0.01f);
}

TEST_CASE("Tape Echo Tone and Flutter alter the tape repeats") {
  Project project;
  projectInit(&project);
  project.delayAlgorithm = 1;
  project.tapeDivision = 0;
  project.tapeHeads = 0;
  project.tapeSpring = 0;
  project.delayFeedback = 0;
  project.delayReturn = 100;

  MasterEffects dark{}, bright{};
  dark.init(8000.0f); bright.init(8000.0f);
  project.tickRate = 1000.0f;
  std::vector<float> bus(80 * 2, 0.0f), darkOutput(80 * 2, 0.0f), brightOutput(80 * 2, 0.0f);
  bus[0] = 1.0f;
  project.tapeTone = 0;
  project.tapeFlutter = 0;
  dark.process(nullptr, bus.data(), darkOutput.data(), 80, &project);
  project.tapeTone = 255;
  bright.process(nullptr, bus.data(), brightOutput.data(), 80, &project);
  CHECK(std::fabs(brightOutput[48 * 2]) > std::fabs(darkOutput[48 * 2]) * 1.5f);

  MasterEffects steady{}, flutter{};
  steady.init(8000.0f); flutter.init(8000.0f);
  project.tickRate = 80.0f;
  project.tapeDivision = 6;
  project.tapeTone = 255;
  std::vector<float> longBus(6000 * 2, 0.0f), steadyOutput(6000 * 2, 0.0f), flutterOutput(6000 * 2, 0.0f);
  longBus[0] = 1.0f;
  project.tapeFlutter = 0;
  steady.process(nullptr, longBus.data(), steadyOutput.data(), 6000, &project);
  project.tapeFlutter = 100;
  flutter.process(nullptr, longBus.data(), flutterOutput.data(), 6000, &project);
  float difference = 0.0f;
  for (size_t i = 0; i < steadyOutput.size(); ++i) difference += std::fabs(steadyOutput[i] - flutterOutput[i]);
  CHECK(difference > 0.1f);
}

TEST_CASE("Switching delay algorithms clears a delay tail") {
  MasterEffects effects;
  effects.init(1000.0f);
  Project project;
  projectInit(&project);
  project.tickRate = 100.0f;
  project.delayTicks = 1;
  project.delayFeedback = 0;
  project.delayReturn = 100;
  std::vector<float> bus(12 * 2, 0.0f), output(12 * 2, 0.0f);
  bus[0] = 1.0f;
  effects.process(nullptr, bus.data(), output.data(), 12, &project);
  project.delayAlgorithm = 1;
  std::fill(bus.begin(), bus.end(), 0.0f);
  std::fill(output.begin(), output.end(), 0.0f);
  effects.process(nullptr, bus.data(), output.data(), 12, &project);
  CHECK(std::fabs(output[10 * 2]) < 0.0001f);
}
