#include "project.h"

static const char* const scaleNames[scalePresetCount] = {
  "Chromatic", "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian",
  "Locrian", "Major Pent", "Minor Pent", "Blues", "Whole Tone", "Custom"
};

static const uint16_t scaleMasks[scalePresetCount] = {
  0x0fff, // Chromatic
  0x0ab5, // Major
  0x05ad, // Minor
  0x06ad, // Dorian
  0x05ab, // Phrygian
  0x0ad5, // Lydian
  0x06b5, // Mixolydian
  0x056b, // Locrian
  0x0295, // Major pentatonic
  0x04a9, // Minor pentatonic
  0x04e9, // Blues
  0x0555, // Whole tone
  0
};

const char* scalePresetName(ScalePreset preset) {
  return preset < scalePresetCount ? scaleNames[preset] : "Chromatic";
}

uint16_t scalePresetMask(ScalePreset preset) {
  return preset < scalePresetCount ? scaleMasks[preset] : scaleMasks[scaleChromatic];
}

uint8_t scaleQuantizeNote(uint8_t note, uint8_t root, uint16_t mask, uint16_t pitchCount) {
  if (pitchCount == 0 || note >= pitchCount || mask == 0) return note;
  root %= 12;
  for (int candidate = note; candidate >= 0; --candidate) {
    int degree = (candidate - root) % 12;
    if (degree < 0) degree += 12;
    if (mask & (1u << degree)) return (uint8_t)candidate;
  }
  return 0;
}

// Mirror of scaleQuantizeNote that snaps upward. When no scale note exists
// above, the note settles on the closest scale note below so the result is
// always in scale.
uint8_t scaleSnapNoteUp(uint8_t note, uint8_t root, uint16_t mask, uint16_t pitchCount) {
  if (pitchCount == 0 || note >= pitchCount || mask == 0) return note;
  root %= 12;
  for (int candidate = note; candidate < pitchCount; ++candidate) {
    int degree = (candidate - root) % 12;
    if (degree < 0) degree += 12;
    if (mask & (1u << degree)) return (uint8_t)candidate;
  }
  return scaleQuantizeNote(note, root, mask, pitchCount);
}
