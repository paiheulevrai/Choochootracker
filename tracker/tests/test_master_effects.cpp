#include "doctest.h"
#include "project.h"
#include "synth/master_effects.h"

#include <cmath>
#include <vector>

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
