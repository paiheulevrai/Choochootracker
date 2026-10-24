#pragma once
#include <cstdint>
#include "user_presets.h"
// Opens the current native instrument's USER library; remembers each engine's location.
void openUserPresetBrowser();
void cycleUserPreset(int direction);
void rememberUserPreset(const UserPresets::Reference& preset);
enum class InstrumentType : uint8_t;
bool setupUserPresetLibrary(UserPresets& library, InstrumentType type, bool factory=false);
