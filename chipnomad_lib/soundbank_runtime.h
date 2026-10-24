#pragma once
#include <functional>

#include "soundbank.h"
namespace soundbank {
// Registry mutations and collection require the audio device to be paused.
// Handles, rather than owning pointers, remain safe in copied/old projects.
uint32_t publish(std::shared_ptr<const PreparedPreset> preset);
const PreparedPreset* prepared(uint32_t handle);
void collect(const std::function<bool(uint32_t)>& inUse);
}  // namespace soundbank
