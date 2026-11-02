#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_ops.h"
#include "../../chipnomad_lib/synth/sample_voice.h"

#include <cstdio>
#include <cstring>
#include <vector>

TEST_SUITE("sample_ops") {

// Builds a mono or stereo sample with deterministic content: frame i
// channel c holds value(i, c). Stereo interleaves L/R. The buffer is
// heap-allocated because the ops free() and replace sample->data.
struct OpSample {
  InstrumentSample s;
  std::vector<int16_t> original;

  OpSample() : s{} {}
  ~OpSample() { free(s.data); }

  void init(uint32_t frames, uint8_t channels) {
    memset(&s, 0, sizeof(s));
    s.frameCount = frames;
    s.channels = channels;
    s.sampleRate = 44100;
    s.start = 0;
    s.end = 255;
    if (frames == 0) {
      s.data = NULL;
      return;
      }
    original.resize((size_t)frames * channels);
    for (uint32_t i = 0; i < frames; ++i) {
      for (uint8_t c = 0; c < channels; ++c) {
        original[(size_t)i * channels + c] = value(i, c);
      }
    }
    s.data = (int16_t*)malloc(original.size() * sizeof(int16_t));
    REQUIRE(s.data != NULL);
    memcpy(s.data, original.data(), original.size() * sizeof(int16_t));
  }

  static int16_t value(uint32_t i, uint8_t c) {
    // Deterministic, asymmetric between channels, within int16
    int32_t v = (int32_t)((i * 37 + c * 1000) % 2000) - 1000;
    return (int16_t)v;
  }

  void expectUnchanged() {
    REQUIRE(s.data != NULL);
    CHECK(s.frameCount * s.channels == original.size());
    for (size_t i = 0; i < original.size(); ++i) {
      CHECK(s.data[i] == original[i]);
    }
  }
};

TEST_CASE("Crop, delete and silence") {
SUBCASE("Crop mono keeps the selection and remaps markers") {
  OpSample t;
  t.init(1000, 1);
  // Set markers via the inverse helpers, then crop exactly at the frames
  // those markers resolve to (the 0-255 scale quantizes, so cropping at
  // the resolved frames is what makes start/end land on the new bounds).
  uint32_t selStart = sampleMarkerToStartFrame(1000, sampleFrameToStartMarker(1000, 100));
  uint32_t selEnd = sampleMarkerToEndFrame(1000, sampleFrameToEndMarker(1000, 200));
  t.s.start = sampleFrameToStartMarker(1000, selStart);
  t.s.end = sampleFrameToEndMarker(1000, selEnd);

  int res = sampleOpCrop(&t.s, selStart, selEnd);
  REQUIRE(res == sampleOpOk);
  CHECK(t.s.frameCount == selEnd - selStart);
  for (uint32_t i = 0; i < t.s.frameCount; ++i) {
    CHECK(t.s.data[i] == OpSample::value(i + selStart, 0));
  }
  // Markers that pointed at the selection bounds now span the whole sample
  CHECK(t.s.start == 0);
  CHECK(t.s.end == 255);
}

SUBCASE("Crop stereo preserves channel interleave") {
  OpSample t;
  t.init(100, 2);
  int res = sampleOpCrop(&t.s, 10, 40);
  REQUIRE(res == sampleOpOk);
  CHECK(t.s.frameCount == 30);
  for (uint32_t i = 0; i < 30; ++i) {
    CHECK(t.s.data[i * 2] == OpSample::value(i + 10, 0));
    CHECK(t.s.data[i * 2 + 1] == OpSample::value(i + 10, 1));
  }
}

SUBCASE("Delete removes the span and joins the tails") {
  OpSample t;
  // frameCount=256 makes the 0-255 marker scale map exactly: start marker
  // m -> frame m, end marker m -> frame m+1 (no quantization drift).
  t.init(256, 1);
  // Markers at old frames 80 (start) and 90 (end); delete [20,30)
  t.s.start = sampleFrameToStartMarker(256, 80);
  t.s.end = sampleFrameToEndMarker(256, 90);

  int res = sampleOpDelete(&t.s, 20, 30);
  REQUIRE(res == sampleOpOk);
  CHECK(t.s.frameCount == 246);
  // Join point: frame 20 is old frame 30
  CHECK(t.s.data[19] == OpSample::value(19, 0));
  CHECK(t.s.data[20] == OpSample::value(30, 0));
  CHECK(t.s.data[245] == OpSample::value(255, 0));
  // Marker at old frame 80 shifts to 70; end marker at old frame 90 -> 80
  CHECK(sampleMarkerToStartFrame(246, t.s.start) == 70);
  CHECK(sampleMarkerToEndFrame(246, t.s.end) == 80);
}

SUBCASE("Delete of the whole sample is rejected") {
  OpSample t;
  t.init(50, 1);
  int res = sampleOpDelete(&t.s, 0, 50);
  CHECK(res == sampleOpErrorWholeSample);
  t.expectUnchanged();
}

SUBCASE("Silence zeros exactly the selection") {
  OpSample t;
  t.init(40, 2);
  int res = sampleOpSilence(&t.s, 10, 20);
  REQUIRE(res == sampleOpOk);
  for (uint32_t i = 0; i < 40; ++i) {
    for (uint8_t c = 0; c < 2; ++c) {
      int16_t expected = (i >= 10 && i < 20) ? 0 : OpSample::value(i, c);
      CHECK(t.s.data[(size_t)i * 2 + c] == expected);
    }
  }
}

}

TEST_CASE("Normalize and fades") {
SUBCASE("Normalize scales a known peak to full scale") {
  OpSample t;
  t.init(8, 1);
  for (uint32_t i = 0; i < 8; ++i) t.s.data[i] = (int16_t)(1000 * (int32_t)(i + 1));
  int res = sampleOpNormalize(&t.s, 0, 8);
  REQUIRE(res == sampleOpOk);
  // Peak 8000 -> gain 4 (32767/8000 integer division)
  CHECK(t.s.data[7] == 32000);
  CHECK(t.s.data[0] == 4000);
}

SUBCASE("Normalize silent selection is a no-op") {
  OpSample t;
  t.init(10, 1);
  memset(t.s.data, 0, 10 * sizeof(int16_t));
  int res = sampleOpNormalize(&t.s, 0, 10);
  CHECK(res == sampleOpOk);
  // Silent data stays silent (can't use expectUnchanged: the fixture's
  // original[] holds nonzero pattern values, not zeros).
  for (uint32_t i = 0; i < 10; ++i) CHECK(t.s.data[i] == 0);
}

SUBCASE("Normalize with clean integer gain scales both channels equally") {
  OpSample t;
  t.init(4, 2);
  t.s.data[0] = 100; t.s.data[1] = -200;
  t.s.data[2] = 300; t.s.data[3] = -8192; // peak in right channel
  t.s.data[4] = -50; t.s.data[5] = 400;
  t.s.data[6] = 10;  t.s.data[7] = -20;
  int res = sampleOpNormalize(&t.s, 0, 4);
  REQUIRE(res == sampleOpOk);
  // gain = 32767/8192 = 3 (integer division)
  CHECK(t.s.data[3] == (int16_t)(-8192 * 3));
  CHECK(t.s.data[2] == (int16_t)(300 * 3)); // left scaled by the same gain
  CHECK(t.s.data[0] == (int16_t)(100 * 3));
}

SUBCASE("Fade in ramps from zero and leaves the rest untouched") {
  OpSample t;
  t.init(10, 1);
  int res = sampleOpFade(&t.s, 2, 8, 1);
  REQUIRE(res == sampleOpOk);
  // Exact ramp: data[2+i] = value(2+i) * i / 6 (int32 truncation). The
  // pattern values are negative here, so monotonicity checks would be
  // meaningless - compare exact per-frame results instead.
  for (uint32_t i = 0; i < 6; ++i) {
    CHECK(t.s.data[2 + i] == (int16_t)((int32_t)OpSample::value(2 + i, 0) * (int32_t)i / 6));
  }
  // Outside the selection untouched
  CHECK(t.s.data[0] == OpSample::value(0, 0));
  CHECK(t.s.data[1] == OpSample::value(1, 0));
  CHECK(t.s.data[8] == OpSample::value(8, 0));
  CHECK(t.s.data[9] == OpSample::value(9, 0));
}

SUBCASE("Fade out mirrors fade in") {
  OpSample t;
  t.init(10, 1);
  int res = sampleOpFade(&t.s, 0, 10, 0);
  REQUIRE(res == sampleOpOk);
  // Exact ramp: data[i] = value(i) * (9-i) / 10 (int32 truncation)
  for (uint32_t i = 0; i < 10; ++i) {
    CHECK(t.s.data[i] == (int16_t)((int32_t)OpSample::value(i, 0) * (int32_t)(9 - i) / 10));
  }
}

}

TEST_CASE("Undo, ranges and marker helpers") {
SUBCASE("Undo round-trip restores a cropped sample byte-identically") {
  OpSample t;
  t.init(100, 2);
  SampleUndo slot;
  memset(&slot, 0, sizeof(slot));

  REQUIRE(sampleOpPrepareUndo(&t.s, &slot) == sampleOpOk);
  REQUIRE(sampleOpCrop(&t.s, 10, 40) == sampleOpOk);
  CHECK(t.s.frameCount == 30);

  REQUIRE(sampleOpApplyUndo(&t.s, &slot) == sampleOpOk);
  CHECK(t.s.frameCount == 100);
  CHECK(t.s.channels == 2);
  for (size_t i = 0; i < t.original.size(); ++i) {
    CHECK(t.s.data[i] == t.original[i]);
  }
  // Toggle back to the cropped state
  REQUIRE(sampleOpApplyUndo(&t.s, &slot) == sampleOpOk);
  CHECK(t.s.frameCount == 30);
  sampleOpFreeUndo(&slot);
  CHECK(slot.active == 0);
  CHECK(slot.data == NULL);
}

SUBCASE("Undo round-trip restores an in-place op (silence)") {
  OpSample t;
  t.init(20, 1);
  SampleUndo slot;
  memset(&slot, 0, sizeof(slot));

  REQUIRE(sampleOpPrepareUndo(&t.s, &slot) == sampleOpOk);
  REQUIRE(sampleOpSilence(&t.s, 5, 15) == sampleOpOk);
  REQUIRE(sampleOpApplyUndo(&t.s, &slot) == sampleOpOk);
  t.expectUnchanged();
  sampleOpFreeUndo(&slot);
}

SUBCASE("Undo prepare overwrites the previous slot") {
  OpSample t;
  t.init(10, 1);
  SampleUndo slot;
  memset(&slot, 0, sizeof(slot));

  REQUIRE(sampleOpPrepareUndo(&t.s, &slot) == sampleOpOk);
  int16_t* first = slot.data;
  REQUIRE(sampleOpPrepareUndo(&t.s, &slot) == sampleOpOk);
  CHECK(slot.data != first); // old buffer freed, new one allocated
  sampleOpFreeUndo(&slot);
}

SUBCASE("Apply with an empty slot errors") {
  OpSample t;
  t.init(10, 1);
  SampleUndo slot;
  memset(&slot, 0, sizeof(slot));
  CHECK(sampleOpApplyUndo(&t.s, &slot) == sampleOpErrorNoUndo);
}

SUBCASE("Whole-sample fallback when selStart == selEnd") {
  OpSample t;
  t.init(30, 1);
  // Silence with equal bounds silences everything
  int res = sampleOpSilence(&t.s, 7, 7);
  REQUIRE(res == sampleOpOk);
  for (uint32_t i = 0; i < 30; ++i) CHECK(t.s.data[i] == 0);

  // Normalize with equal bounds on a full-scale sample is a no-op
  OpSample t2;
  t2.init(30, 1);
  for (uint32_t i = 0; i < 30; ++i) t2.s.data[i] = 32767;
  res = sampleOpNormalize(&t2.s, 0, 0);
  REQUIRE(res == sampleOpOk);
  CHECK(t2.s.data[0] == 32767);
}

SUBCASE("Inverted and out-of-bounds ranges are normalized") {
  OpSample t;
  t.init(20, 1);
  // Inverted range: swap on the fly
  int res = sampleOpSilence(&t.s, 15, 5);
  REQUIRE(res == sampleOpOk);
  for (uint32_t i = 5; i < 15; ++i) CHECK(t.s.data[i] == 0);
  CHECK(t.s.data[4] == OpSample::value(4, 0));
  CHECK(t.s.data[15] == OpSample::value(15, 0));

  // Out-of-bounds clamped
  OpSample t2;
  t2.init(20, 1);
  res = sampleOpSilence(&t2.s, 15, 500);
  REQUIRE(res == sampleOpOk);
  for (uint32_t i = 15; i < 20; ++i) CHECK(t2.s.data[i] == 0);
}

SUBCASE("Ops reject empty samples") {
  OpSample t;
  t.init(0, 1);
  t.s.data = NULL;
  CHECK(sampleOpCrop(&t.s, 0, 10) == sampleOpErrorNoSample);
  CHECK(sampleOpNormalize(&t.s, 0, 10) == sampleOpErrorNoSample);
  CHECK(sampleOpDelete(&t.s, 0, 10) == sampleOpErrorNoSample);
  CHECK(sampleOpSilence(&t.s, 0, 10) == sampleOpErrorNoSample);
  CHECK(sampleOpFade(&t.s, 0, 10, 1) == sampleOpErrorNoSample);
  SampleUndo slot;
  memset(&slot, 0, sizeof(slot));
  CHECK(sampleOpPrepareUndo(&t.s, &slot) == sampleOpErrorNoSample);
}

SUBCASE("Marker round-trip helpers hit boundary values") {
  // 0-255 -> frame -> 0-255 round trip on a decent-sized sample.
  // NOTE: int loop counter - a uint8_t would wrap at 255 and never exit.
  for (int m = 0; m <= 255; ++m) {
    uint32_t f = sampleMarkerToStartFrame(100000, (uint8_t)m);
    CHECK(sampleFrameToStartMarker(100000, f) == m);
  }
  for (int m = 0; m <= 255; ++m) {
    uint32_t f = sampleMarkerToEndFrame(100000, (uint8_t)m);
    CHECK(sampleFrameToEndMarker(100000, f) == m);
  }
  // Degenerate sizes
  CHECK(sampleFrameToStartMarker(0, 0) == 0);
  CHECK(sampleFrameToStartMarker(1, 0) == 0);
  CHECK(sampleFrameToEndMarker(0, 0) == 0);
  CHECK(sampleFrameToEndMarker(1, 0) == 0);
  CHECK(sampleFrameToEndMarker(1, 1) == 255);
}

}

TEST_CASE("WAV save") {
SUBCASE("WAV save round-trips through load") {
  const char* path = "test_save_roundtrip.wav";

  // Mono round trip
  {
    OpSample t;
    t.init(257, 1);
    char error[64] = {0};
    REQUIRE(sampleSaveWav16(&t.s, path, error, sizeof(error)) == 0);
    CHECK(error[0] == 0);

    OpSample loaded;
    REQUIRE(sampleLoadWav16(path, &loaded.s, error, sizeof(error)) == 0);
    CHECK(loaded.s.frameCount == 257);
    CHECK(loaded.s.channels == 1);
    CHECK(loaded.s.sampleRate == 44100);
    REQUIRE(loaded.s.data != NULL);
    for (uint32_t i = 0; i < 257; ++i) CHECK(loaded.s.data[i] == OpSample::value(i, 0));
  }

  // Stereo round trip
  {
    OpSample t;
    t.init(64, 2);
    char error[64] = {0};
    REQUIRE(sampleSaveWav16(&t.s, path, error, sizeof(error)) == 0);

    OpSample loaded;
    REQUIRE(sampleLoadWav16(path, &loaded.s, error, sizeof(error)) == 0);
    CHECK(loaded.s.frameCount == 64);
    CHECK(loaded.s.channels == 2);
    CHECK(loaded.s.sampleRate == 44100);
    REQUIRE(loaded.s.data != NULL);
    for (uint32_t i = 0; i < 64 * 2; ++i) CHECK(loaded.s.data[i] == t.original[i]);
  }

  std::remove(path);
}

SUBCASE("WAV save rejects empty samples and overlong paths") {
  OpSample t;
  t.init(0, 1);
  char error[64] = {0};
  CHECK(sampleSaveWav16(&t.s, "test_save_empty.wav", error, sizeof(error)) == 1);
  CHECK(error[0] != 0);

  t.init(10, 1);
  // PROJECT_SAMPLE_PATH_LENGTH is 255; a 256-char path must be rejected
  char longPath[257];
  memset(longPath, 'a', sizeof(longPath) - 1);
  longPath[sizeof(longPath) - 1] = 0;
  CHECK(strlen(longPath) == 256);
  CHECK(sampleSaveWav16(&t.s, longPath, error, sizeof(error)) == 1);
  CHECK(error[0] != 0);
}

}

TEST_CASE("Reverse") {
SUBCASE("Reverse mirrors a mono selection in place") {
  OpSample t;
  t.init(50, 1);
  int res = sampleOpReverse(&t.s, 10, 20);
  REQUIRE(res == sampleOpOk);
  // [10,20) holds the mirrored frames; outside is untouched
  for (uint32_t i = 10; i < 20; ++i) {
    CHECK(t.s.data[i] == OpSample::value(29 - i, 0));
  }
  CHECK(t.s.data[9] == OpSample::value(9, 0));
  CHECK(t.s.data[20] == OpSample::value(20, 0));
  CHECK(t.s.data[49] == OpSample::value(49, 0));
  // Length and markers are unchanged
  CHECK(t.s.frameCount == 50);
  CHECK(t.s.start == 0);
  CHECK(t.s.end == 255);
}

SUBCASE("Reverse preserves stereo channel pairing") {
  OpSample t;
  t.init(40, 2);
  int res = sampleOpReverse(&t.s, 0, 40);
  REQUIRE(res == sampleOpOk);
  for (uint32_t i = 0; i < 40; ++i) {
    CHECK(t.s.data[i * 2] == OpSample::value(39 - i, 0));
    CHECK(t.s.data[i * 2 + 1] == OpSample::value(39 - i, 1));
  }
}

SUBCASE("Reverse of an odd-length selection keeps the middle frame") {
  OpSample t;
  t.init(21, 1);
  int res = sampleOpReverse(&t.s, 0, 21);
  REQUIRE(res == sampleOpOk);
  for (uint32_t i = 0; i < 21; ++i) {
    CHECK(t.s.data[i] == OpSample::value(20 - i, 0));
  }
  // The middle frame (10) maps onto itself
  CHECK(t.s.data[10] == OpSample::value(10, 0));
}

SUBCASE("Reverse with equal bounds falls back to the whole sample") {
  OpSample t;
  t.init(30, 1);
  int res = sampleOpReverse(&t.s, 25, 25);
  REQUIRE(res == sampleOpOk);
  for (uint32_t i = 0; i < 30; ++i) {
    CHECK(t.s.data[i] == OpSample::value(29 - i, 0));
  }
}

SUBCASE("Reverse is an involution and integrates with undo") {
  OpSample t;
  t.init(24, 1);
  REQUIRE(sampleOpReverse(&t.s, 4, 20) == sampleOpOk);
  REQUIRE(sampleOpReverse(&t.s, 4, 20) == sampleOpOk);
  t.expectUnchanged();

  // Undo round trip through the shared slot
  SampleUndo slot;
  memset(&slot, 0, sizeof(slot));
  REQUIRE(sampleOpPrepareUndo(&t.s, &slot) == sampleOpOk);
  REQUIRE(sampleOpReverse(&t.s, 4, 20) == sampleOpOk);
  REQUIRE(sampleOpApplyUndo(&t.s, &slot) == sampleOpOk);
  t.expectUnchanged();
  sampleOpFreeUndo(&slot);
}

SUBCASE("Reverse rejects empty samples and clamps ranges") {
  OpSample t;
  t.init(0, 1);
  t.s.data = NULL;
  CHECK(sampleOpReverse(&t.s, 0, 10) == sampleOpErrorNoSample);

  OpSample t2;
  t2.init(20, 1);
  // Inverted range swaps on the fly; out-of-bounds clamps
  REQUIRE(sampleOpReverse(&t2.s, 15, 5) == sampleOpOk);
  for (uint32_t i = 5; i < 15; ++i) {
    CHECK(t2.s.data[i] == OpSample::value(19 - i, 0));
  }
  CHECK(t2.s.data[4] == OpSample::value(4, 0));
  CHECK(t2.s.data[15] == OpSample::value(15, 0));
}
}

} // TEST_SUITE("sample_ops")
