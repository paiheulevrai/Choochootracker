#include "doctest.h"
#include "synth/plaits_voice.h"

#include <cmath>
#include <vector>

TEST_CASE("Plaits renders all engines with finite output") {
  PlaitsVoice* voice = new PlaitsVoice();
  voice->init(96000.0f);
  std::vector<float> output(4096);
  for (int engine = 0; engine < 24; ++engine) {
    voice->configure(engine, 16384, 16384, 16384, 0, 60.0f, 1.0f);
    voice->setEnvelope(0.0f, 0.0f, 1.0f, 0.0f);
    voice->noteOn();
    voice->render(output.data(), output.size());
    double energy = 0.0;
    for (float sample : output) {
      REQUIRE(std::isfinite(sample));
      energy += std::fabs(sample);
    }
    CHECK(energy > 0.0);
  }
  delete voice;
}
