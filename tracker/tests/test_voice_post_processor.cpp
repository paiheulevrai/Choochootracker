#include "doctest.h"

#include "synth/voice_post_processor.h"

TEST_CASE("VoicePostProcessor applies shared VCA lifecycle") {
  VoicePostProcessor<> post;
  post.init(48000.0f);
  post.setGain(0.5f);
  post.setFilter(false, 0, false, 1000.0f, 0.0f);
  post.setEnvelope(true, 0.0f, 0.0f, 1.0f, 0.0f, 0x80);
  post.noteOn();
  CHECK(post.process(1.0f) == doctest::Approx(0.5f));
  post.noteOff();
  CHECK(post.process(1.0f) == doctest::Approx(0.0f));
  CHECK_FALSE(post.envelopeActive());
}
