// Compile the pinned C source with Choo Choo's C++ toolchain. The only C/C++
// difference is the scope of the anonymous envelope enum inside MonkVoice.
#include "../external/monksynth/voice.h"
static constexpr auto ENV_IDLE = MonkVoice::ENV_IDLE;
static constexpr auto ENV_ATTACK = MonkVoice::ENV_ATTACK;
static constexpr auto ENV_DECAY = MonkVoice::ENV_DECAY;
static constexpr auto ENV_SUSTAIN = MonkVoice::ENV_SUSTAIN;
static constexpr auto ENV_RELEASE = MonkVoice::ENV_RELEASE;
#include "../external/monksynth/voice.c"
