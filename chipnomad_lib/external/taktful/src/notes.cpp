#include <taktful/notes.h>
#include <taktful/api.h>
#include <taktful/chroma.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace taktful {

const char* k_pitch_class_names[12] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

const char* k_note_names[12] = {
    "C", "C#/Db", "D", "D#/Eb", "E", "F",
    "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B"
};

const char* pitch_class_to_string(PitchClass pc) {
    int idx = static_cast<int>(pc);
    if (idx < 0 || idx >= 12) {
        return "Unknown";
    }
    return k_pitch_class_names[idx];
}

const char* pitch_class_to_note_name(PitchClass pc) {
    int idx = static_cast<int>(pc);
    if (idx < 0 || idx >= 12) {
        return "Unknown";
    }
    return k_note_names[idx];
}

const char* const* get_pitch_class_names() {
    return k_pitch_class_names;
}

NotesResult detect_notes(const std::vector<std::vector<float> >& chroma) {
    NotesResult result;
    
    if (chroma.empty()) {
        return result;
    }

    // Aggregate chroma across all frames to get overall note presence
    std::vector<float> aggregated_pcp = aggregate_pcp(chroma);
    
    // Calculate total energy
    float total = 0.0f;
    for (int i = 0; i < 12; ++i) {
        total += aggregated_pcp[static_cast<size_t>(i)];
    }
    result.total_energy = total;

    // Detect individual notes
    result.notes.reserve(12);
    for (int pc = 0; pc < 12; ++pc) {
        NoteInfo info;
        info.pitch_class = static_cast<PitchClass>(pc);
        info.presence = (total > 1.0e-12f) ? (aggregated_pcp[static_cast<size_t>(pc)] / total) : 0.0f;
        
        // Confidence based on how much this note stands out
        // Simple heuristic: confidence = presence * (1 - average_of_others)
        float avg_others = (total - aggregated_pcp[static_cast<size_t>(pc)]) / 11.0f;
        info.confidence = info.presence * (1.0f - avg_others / (aggregated_pcp[static_cast<size_t>(pc)] + 1.0e-8f));
        if (info.confidence < 0.0f) info.confidence = 0.0f;
        if (info.confidence > 1.0f) info.confidence = 1.0f;
        
        result.notes.push_back(info);
    }

    // Sort notes by presence (descending)
    std::sort(result.notes.begin(), result.notes.end(),
        [](const NoteInfo& a, const NoteInfo& b) {
            return a.presence > b.presence;
        });

    // Find dominant pitch class
    if (!result.notes.empty()) {
        result.dominant_pc = static_cast<int>(result.notes[0].pitch_class);
        result.dominant_presence = result.notes[0].presence;
    }

    return result;
}

// WAV-based note detection - defined in the implementation file to avoid circular dependencies
NotesResult detect_notes(const WavData& audio, const Params& params) {
    Params validated_params = params;
    validate_params(validated_params);
    
    NotesResult result;
    if (audio.samples.empty() || audio.sample_rate == 0) {
        return result;
    }

    // Compute intermediate features to get chroma
    IntermediateFeatures feat = compute_features(audio, validated_params);
    
    if (feat.chroma.empty()) {
        return result;
    }

    return detect_notes(feat.chroma);
}

} // namespace taktful
