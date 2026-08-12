// Copyright 2026 Dylan Bolink.
// SPDX-License-Identifier: MIT

#ifndef PLAITS_ALT_GUARD_PLAITS_DSP_ENGINE2_FRESHETS_FORMANT_SHAPES_H_
#define PLAITS_ALT_GUARD_PLAITS_DSP_ENGINE2_FRESHETS_FORMANT_SHAPES_H_

#include <stdint.h>

namespace plaits_alt {

const int kFormantShapeCount = 5;
const float kFormantShapeSize = 128.0f;

extern const int16_t* const kFormantShape[kFormantShapeCount];

}  // namespace plaits_alt

#endif  // PLAITS_DSP_ENGINE2_FRESHETS_FORMANT_SHAPES_H_
