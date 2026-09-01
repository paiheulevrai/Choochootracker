#ifndef CHOOCHOO_AUDIO_MATH_H
#define CHOOCHOO_AUDIO_MATH_H

extern float audioTanhTable[1025];

inline float audioTanh(float x) {
  if (x <= -8.0f) return -1.0f;
  if (x >= 8.0f) return 1.0f;
  const float position = (x + 8.0f) * 64.0f;
  const int index = static_cast<int>(position);
  const float fraction = position - index;
  return audioTanhTable[index] +
         (audioTanhTable[index + 1] - audioTanhTable[index]) * fraction;
}

#endif
