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
