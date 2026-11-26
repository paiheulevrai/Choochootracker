#pragma once

#include <cmath>

namespace effect_filters {
inline float lowpassCoefficient(float cutoff, float rate) {
  if (cutoff < 20.0f) cutoff = 20.0f;
  if (cutoff > rate * 0.45f) cutoff = rate * 0.45f;
  return 1.0f - expf(-6.283185307179586f * cutoff / rate);
}

inline float highpassCoefficient(float cutoff, float rate) {
  if (cutoff < 20.0f) cutoff = 20.0f;
  if (cutoff > rate * 0.45f) cutoff = rate * 0.45f;
  return expf(-6.283185307179586f * cutoff / rate);
}

inline float lowpass(float input, float coefficient, float& state) {
  state += coefficient * (input - state);
  return state;
}

inline float highpass(float input, float coefficient, float& inputState, float& outputState) {
  const float output = coefficient * (outputState + input - inputState);
  inputState = input;
  return outputState = output;
}
}
