#include "doctest.h"

#include "synth/voice_post_processor.h"
#include <initializer_list>

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

TEST_CASE("VoicePostProcessor retrigger keeps the current envelope level") {
  VoicePostProcessor<> post;
  post.init(48000.0f);
  post.setFilter(false, 0, false, 1000.0f, 0.0f);
  post.setEnvelope(true, 0.0f, 0.0f, 1.0f, 0.0f, 0x80);
  post.noteOn();
  CHECK(post.process(1.0f) == doctest::Approx(1.0f));

  post.setEnvelope(true, 0.1f, 0.0f, 1.0f, 0.0f, 0x80);
  post.noteOn();
  CHECK(post.process(1.0f) == doctest::Approx(1.0f));
}

TEST_CASE("VoicePostProcessor block paths preserve filter and envelope transitions exactly") {
  VoicePostProcessor<> reference, selected;
  reference.init(48000.0f);
  selected.init(48000.0f);
  for (int block = 0; block < 16; ++block) {
    for (auto* post : {&reference, &selected}) {
      if (block == 0 || block == 10) {
        post->setGain(0.8f);
        post->setEnvelope(true, 0.0f, 0.0f, 1.0f, 0.002f, 0x80);
        post->noteOn();
      }
      if (block == 2 || block == 6) post->setFilter(true, 1, 0, true, 4200.0f, 0.6f);
      if (block == 4) post->setFilter(false, 1, 0, true, 4200.0f, 0.6f);
      if (block == 7) post->setFilter(true, 0, 0, true, 4200.0f, 0.6f);
      if (block == 8 || block == 12) post->noteOff();
      if (block == 14) post->setEnvelope(false, 0, 0, 1, 0, 0x80);
    }
    const bool filter = selected.filterActive();
    const bool unity = selected.unityEnvelope();
    for (int i = 0; i < 64; ++i) {
      float a[2] = {(float)(i - 31) / 32.0f, (float)(29 - i) / 32.0f};
      float b[2] = {a[0], a[1]};
      reference.processStereo(a);
      if (filter) {
        if (unity) selected.processStereo<true, true>(b);
        else selected.processStereo<true, false>(b);
      } else {
        if (unity) selected.processStereo<false, true>(b);
        else selected.processStereo<false, false>(b);
      }
      CHECK(a[0] == b[0]);
      CHECK(a[1] == b[1]);
      CHECK(reference.envelopeActive() == selected.envelopeActive());
    }
  }
}
