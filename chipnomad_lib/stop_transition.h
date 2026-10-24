#pragma once

// Audio-thread-only correction for a transport stop or mute/solo change.
// Normal playback passes through unchanged; a discontinuity is removed over
// three milliseconds.
struct StopTransition {
  float last[2], offset[2];
  int remaining, length;
  bool pending;

  void begin(int sampleRate) {
    length = sampleRate * 3 / 1000;
    if (length < 2) length = 2;
    pending = true;
  }
  void process(float* audio, int frames) {
    if (!pending && !remaining) {
      if (frames > 0) {
        last[0] = audio[(frames - 1) * 2];
        last[1] = audio[(frames - 1) * 2 + 1];
      }
      return;
    }
    for (int i = 0; i < frames; ++i) {
      if (pending) {
        for (int c = 0; c < 2; ++c) offset[c] = last[c] - audio[i * 2 + c];
        remaining = length;
        pending = false;
      }
      const float weight = remaining ? float(remaining - 1) / (length - 1) : 0;
      for (int c = 0; c < 2; ++c) {
        audio[i * 2 + c] += offset[c] * weight;
        last[c] = audio[i * 2 + c];
      }
      if (remaining) --remaining;
    }
  }
};
