#ifndef TAKTFUL_NOTES_H
#define TAKTFUL_NOTES_H

#include <taktful/api.h>
#include <vector>

namespace taktful {

enum class PitchClass {
    C = 0, Cs, D, Ds, E, F,
    Fs, G, Gs, A, As, B,
};

struct NoteInfo {
    PitchClass pitch_class = PitchClass::C;
    float presence = 0.0f;
    float confidence = 0.0f;
};

struct NotesResult {
    float total_energy = 0.0f;
    int dominant_pc = -1;
    float dominant_presence = 0.0f;
    std::vector<NoteInfo> notes;
};

NotesResult detect_notes(const std::vector<std::vector<float> >& chroma);
NotesResult detect_notes(const WavData& audio, const Params& params);

const char* pitch_class_to_string(PitchClass pc);
const char* pitch_class_to_note_name(PitchClass pc);
const char* const* get_pitch_class_names();

extern const char* k_pitch_class_names[12];
extern const char* k_note_names[12];

} // namespace taktful

#endif
