#include "opl_patch.h"
#include "opll_presets.h"
#include "four_op_patch.h"
#include "simple_chip_presets.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <vector>

#include "synth/braids_voice.h"
#include "synth/drum_synth_voice.h"
#include "synth/mme_voice.h"
#include "synth/plaits_alt_voice.h"
#include "synth/plaits_voice.h"
#include "synth/sintered_voice.h"
#include "chips/chips.h"
#include "chipnomad_lib.h"
#include "fm_catalog.h"
#include "user_presets.h"
#include "synth/opll_voice.h"
#include "synth/opl_voice.h"
#include "synth/four_op_voice.h"
#include "synth/dx7_voice.h"
#include "synth/simple_chip_voice.h"
#include "synth/sid_voice.h"
#include <memory>

namespace fs = std::filesystem;

namespace {
constexpr int kSampleRate = 96000;
constexpr size_t kFrames = kSampleRate;  // one second; first/last 100 ms are excluded by the analyser.
constexpr size_t kDrumFrames = kSampleRate / 4;
constexpr uint16_t kHarmonics[] = {2048, 7168, 12288, 17408, 22528};
constexpr uint16_t kTimbres[] = {4096, 12288, 20480};
constexpr uint16_t kMorphs[] = {6144, 16384};
constexpr uint16_t kBraidsTimbres[] = {2048, 8192, 16384, 24576, 30720};
constexpr uint16_t kBraidsColors[] = {2048, 8192, 14336, 20480, 26624, 30720};
constexpr float kNotes[] = {48.0f, 60.0f};
constexpr float kPi = 3.14159265358979323846f;

struct MacroPatch { uint16_t harmonics, timbre, morph; };

MacroPatch macroPatch(size_t index) {
  return {kHarmonics[index / 6], kTimbres[(index / 2) % 3], kMorphs[index % 2]};
}

constexpr size_t kPatchCount = std::size(kHarmonics) * std::size(kTimbres) * std::size(kMorphs);

bool writeWav(const fs::path& path, const std::vector<float>& data) {
  FILE* file = std::fopen(path.string().c_str(), "wb");
  if (!file) return false;
  // Preserve out-of-range peaks for the analyser instead of hiding them in
  // PCM clipping. Eight times full scale still fits the measurement format.
  constexpr float kMeasurementScale = 0.125f;
  const uint32_t dataSize = static_cast<uint32_t>(data.size() * sizeof(int32_t));
  const uint32_t riffSize = 36 + dataSize;
  const uint16_t channels = 1, bits = 32, blockAlign = 4;
  const uint32_t byteRate = kSampleRate * blockAlign;
  auto u16 = [&](uint16_t value) { std::fwrite(&value, sizeof(value), 1, file); };
  auto u32 = [&](uint32_t value) { std::fwrite(&value, sizeof(value), 1, file); };
  std::fwrite("RIFF", 1, 4, file); u32(riffSize); std::fwrite("WAVEfmt ", 1, 8, file);
  u32(16); u16(1); u16(channels); u32(kSampleRate); u32(byteRate); u16(blockAlign); u16(bits);
  std::fwrite("data", 1, 4, file); u32(dataSize);
  for (float sample : data) {
    sample = std::isfinite(sample) ? std::clamp(sample * kMeasurementScale, -1.0f, 1.0f) : 0.0f;
    const int32_t pcm = static_cast<int32_t>(std::llround(sample * 2147483647.0));
    std::fwrite(&pcm, sizeof(pcm), 1, file);
  }
  return std::fclose(file) == 0;
}

// Voice is concrete so Plaits/Plaits-Alt model calibration wrappers are measured.
template <typename Voice>
bool renderPlaitsFamily(const char* family, bool vcaValidation = false) {
  const fs::path directory = fs::path("measurements") / family;
  fs::create_directories(directory);
  std::vector<float> buffer(kFrames);
  for (int engine = 0; engine < 24; ++engine) {
    for (size_t patch = 0; patch < kPatchCount; ++patch) {
      for (size_t note = 0; note < std::size(kNotes); ++note) {
        const MacroPatch settings = macroPatch(patch);
        const bool percussion = engine >= 21;
        const bool vca = vcaValidation && !percussion;
        const size_t frames = percussion ? kDrumFrames : kFrames;
        Voice voice;
        voice.init(kSampleRate);
        // Native TRIG/LPG is the calibration baseline. VCA is a separate
        // diagnostic because internally articulated engines can be silenced by it.
        voice.configure(engine, settings.harmonics, settings.timbre, settings.morph,
                        0, vca ? 2 : 0, vca ? 128 : 255, 255, kNotes[note], 0.9f);
        if (vca) voice.setEnvelope(0.005f, 0.0f, 1.0f, 0.005f);
        voice.noteOn();
        voice.render(buffer.data(), frames);
        char name[80];
        std::snprintf(name, sizeof(name), "%s_%02d_p%zu_n%.0f_%s.wav", family, engine, patch,
                      kNotes[note], vca ? "vca" : "trig");
        if (!writeWav(directory / name, std::vector<float>(buffer.begin(), buffer.begin() + frames))) return false;
      }
    }
  }
  return true;
}

bool renderBraids() {
  const fs::path directory = fs::path("measurements") / "braids";
  fs::create_directories(directory);
  std::vector<float> buffer(kFrames);
  for (int model = 0; model <= braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META; ++model) {
    for (size_t patch = 0; patch < kPatchCount; ++patch) {
      for (size_t note = 0; note < std::size(kNotes); ++note) {
        BraidsVoice voice;
        voice.init();
        if (!voice.setModel(model)) return false;
        voice.setPitch(static_cast<int16_t>(kNotes[note] * 128));
        voice.setParameters(kBraidsTimbres[patch % std::size(kBraidsTimbres)],
                            kBraidsColors[patch / std::size(kBraidsTimbres)]);
        voice.setGain(0.9f);
        voice.setEnvelope(true, 0.005f, 0.0f, 1.0f, 0.005f);
        voice.noteOn();
        voice.render(buffer.data(), buffer.size());
        char name[80];
        std::snprintf(name, sizeof(name), "braids_%02d_p%zu_n%.0f.wav", model, patch, kNotes[note]);
        if (!writeWav(directory / name, buffer)) return false;
      }
    }
  }
  return true;
}

bool renderPcmReference() {
  const fs::path directory = fs::path("measurements") / "pcm";
  fs::create_directories(directory);
  for (float note : kNotes) {
    std::vector<float> buffer(kFrames);
    const float frequency = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
    for (size_t i = 0; i < buffer.size(); ++i) buffer[i] = 0.9f * std::sin(2.0f * kPi * frequency * i / kSampleRate);
    char name[48];
    std::snprintf(name, sizeof(name), "pcm_sine_n%.0f.wav", note);
    if (!writeWav(directory / name, buffer)) return false;
  }
  return true;
}

bool renderAYFamily(bool ym) {
  const char* family = ym ? "ym" : "ay";
  const fs::path directory = fs::path("measurements") / family;
  fs::create_directories(directory);
  for (int source = 0; source < 3; ++source) {
    for (size_t note = 0; note < std::size(kNotes); ++note) {
        ChipSetup setup{};
        setup.ay.clock = 1773400; setup.ay.isYM = ym; setup.ay.stereoMode = StereoModeAY::ABC;
        SoundChipAY chip(kSampleRate, setup);
        const float hz = 440.0f * std::pow(2.0f, (kNotes[note] - 69.0f) / 12.0f);
        const int period = std::max(1, (int)std::lrintf(setup.ay.clock / (16.0f * hz)));
        chip.setRegister(0, period & 255); chip.setRegister(1, period >> 8);
        chip.setRegister(2, period & 255); chip.setRegister(3, period >> 8);
        chip.setRegister(4, period & 255); chip.setRegister(5, period >> 8);
        chip.setRegister(6, 7); chip.setRegister(11, 64); chip.setRegister(12, 0); chip.setRegister(13, 14);
        uint8_t mixer = source == 0 ? 0x38 : source == 1 ? 0x07 : 0x00;
        chip.setRegister(7, mixer);
        chip.setRegister(8, source == 2 ? 0x10 : 0x0f);
        chip.setRegister(9, 0); chip.setRegister(10, 0);
        std::vector<float> stereo(kFrames * 2), mono(kFrames);
        chip.render(stereo.data(), kFrames);
        for (size_t i = 0; i < kFrames; ++i) mono[i] = (stereo[i * 2] + stereo[i * 2 + 1]) * .5f;
        char name[80];
        std::snprintf(name, sizeof(name), "%s_%02d_p0_n%.0f.wav", family, source, kNotes[note]);
        if (!writeWav(directory / name, mono)) return false;
    }
  }
  return true;
}

bool renderBogie() {
  const fs::path directory = fs::path("measurements") / "bogie";
  fs::create_directories(directory);
  const uint8_t values[] = {48, 128, 208};
  for (int model = 0; model < (int)DrumSynthEngine::totalCount; ++model) {
    for (size_t patch = 0; patch < std::size(values); ++patch) {
      for (size_t note = 0; note < std::size(kNotes); ++note) {
        InstrumentDrumSynth d{}; d.engine = (DrumSynthEngine)model; d.decay = 192;
        d.tone = values[patch]; d.sweep = values[patch]; d.noise = values[patch];
        d.fm = values[patch]; d.drive = values[patch];
        DrumSynthVoice voice; voice.init(kSampleRate); voice.configure(&d, kNotes[note] * 100.0f, .9f, 20000, 0); voice.noteOn();
        std::vector<float> buffer(kDrumFrames); voice.render(buffer.data(), buffer.size());
        char name[80]; std::snprintf(name, sizeof(name), "bogie_%02d_p%zu_n%.0f.wav", model, patch, kNotes[note]);
        if (!writeWav(directory / name, buffer)) return false;
      }
    }
  }
  return true;
}

bool renderMME() {
  const fs::path directory = fs::path("measurements") / "mme";
  fs::create_directories(directory);
  const uint8_t shapers[] = {0, 32, 64, 96, 128, 160, 192, 224, 255};
  const uint8_t values[] = {64, 128, 192};
  for (int model = 0; model < (int)MMEModel::totalCount; ++model) {
    for (size_t patch = 0; patch < std::size(values); ++patch) {
      for (uint8_t shaper : shapers) for (size_t note = 0; note < std::size(kNotes); ++note) {
        InstrumentMME m{}; m.model = (MMEModel)model; m.waves = values[patch]; m.interval = 128;
        m.amount = values[patch]; m.flow = values[2 - patch]; m.feedback = values[patch]; m.shaper = shaper; m.sustain = 255;
        MMEVoice voice; voice.init(kSampleRate); voice.configure(&m, kNotes[note] * 100.0f, .9f, 20000, 0); voice.noteOn();
        std::vector<float> buffer(kFrames); voice.render(buffer.data(), buffer.size());
        char name[96]; std::snprintf(name, sizeof(name), "mme_%02d_p%zu_n%.0f_s%03u.wav", model, patch, kNotes[note], shaper);
        if (!writeWav(directory / name, buffer)) return false;
      }
    }
  }
  return true;
}

bool renderSintered() {
  const fs::path directory = fs::path("measurements") / "sintered";
  fs::create_directories(directory);
  const uint8_t values[] = {32, 80, 128, 176, 224};
  for (int model = 0; model < (int)SinteredModel::totalCount; ++model) {
    for (size_t patch = 0; patch < std::size(values); ++patch) {
      for (size_t note = 0; note < std::size(kNotes); ++note) {
        InstrumentSintered s{}; s.model = (SinteredModel)model; s.decay = 224; s.mod = values[patch];
        s.a = values[patch]; s.b = values[2]; s.motion = 128; s.c = values[4 - patch];
        SinteredVoice voice; voice.init(kSampleRate); voice.configure(&s, kNotes[note] * 100.0f, .9f, 20000, 0); voice.noteOn();
        std::vector<float> buffer(kDrumFrames); voice.render(buffer.data(), buffer.size());
        char name[80]; std::snprintf(name, sizeof(name), "sintered_%02d_p%zu_n%.0f.wav", model, patch, kNotes[note]);
        if (!writeWav(directory / name, buffer)) return false;
      }
    }
  }
  return true;
}

// Full factory inventory at the same reference gain/pitches as the other engines.
// Native envelopes remain active; percussion uses the documented 250 ms window.
bool renderNativeChips() {
  auto project=std::make_unique<Project>(); projectInit(project.get()); fillFXNames();
  const std::string folder="packaging/common/instruments/FACTORY";
  for(const auto* catalog:{"catalog.tsv","builtins.tsv"}) {
    std::vector<FMPresetEntry> entries;
    if(!loadFMCatalog((folder+"/"+catalog).c_str(),entries))return false;
    for(size_t index=0;index<entries.size();++index) {
      const auto& entry=entries[index];
      if(!loadFMPreset(folder,entry,project.get(),0))return false;
      const auto& inst=project->instruments[0];
      // OPLL and VRC7 share the same programmable YM2413 voice adapter and gain.
      const std::string family=isOPLL(inst.type)?"opll-vrc7":userPresetFolder(inst.type);
      const auto directory=fs::path("measurements")/family;fs::create_directories(directory);
      for(float note:kNotes) {
        const size_t frames=entry.category=="Percussion"?kDrumFrames:kFrames;
        std::vector<float> mono(frames),stereo(frames*2);
        if(isOPL(inst.type)){OPLVoice voice;voice.init(kSampleRate);voice.configure(inst.type,&inst.chip.opl,note*100,.9f);voice.noteOn();voice.render(stereo.data(),frames);}
        else if(isFourOp(inst.type)){FourOpVoice voice;voice.init(kSampleRate);voice.configure(inst.type,&inst.chip.fourOp,note*100,.9f);voice.noteOn();voice.render(stereo.data(),frames);}
        else if(isOPLL(inst.type)){OPLLVoice voice;voice.init(kSampleRate);voice.configure(&inst.chip.opll,note*100,.9f);voice.noteOn();voice.render(mono.data(),frames);}
        else if(inst.type==InstrumentType::DX7){DX7Part voice;voice.init(kSampleRate);voice.voices[0].configure(&inst.chip.dx7,note*100,.9f);voice.voices[0].noteOn();voice.render(mono.data(),frames);}
        else if(inst.type==InstrumentType::SID){SIDVoice voice;voice.init(kSampleRate);voice.configure(&inst.chip.sid,note*100,.9f);voice.noteOn();voice.render(mono.data(),frames);}
        else{SimpleChipVoice voice;voice.init(kSampleRate);voice.configure(inst.type,&inst.chip.simpleChip,note*100,.9f);voice.noteOn();voice.render(mono.data(),frames);}
        if(isOPL(inst.type)||isFourOp(inst.type))for(size_t i=0;i<frames;++i)mono[i]=(stereo[2*i]+stereo[2*i+1])*.5f;
        char name[160];snprintf(name,sizeof(name),"%s_%02d_p%zu_n%.0f.wav",family.c_str(),int(inst.type),index,note);
        if(!writeWav(directory/name,mono))return false;
      }
    }
  }
  projectFree(project.get());return true;
}
}  // namespace

int main(int argc, char** argv) {
  const char* family = argc == 2 ? argv[1] : "all";
  if (argc > 2 || (std::strcmp(family, "all") && std::strcmp(family, "braids") &&
                   std::strcmp(family, "plaits") && std::strcmp(family, "plaits-alt") &&
                   std::strcmp(family, "plaits-vca") && std::strcmp(family, "plaits-alt-vca") &&
                   std::strcmp(family, "pcm") && std::strcmp(family, "ay") && std::strcmp(family, "ym") &&
                   std::strcmp(family, "bogie") && std::strcmp(family, "mme") && std::strcmp(family, "sintered") && std::strcmp(family, "native-chips"))) {
    std::fputs("Usage: render_engine_measurements [all|braids|plaits|plaits-alt|pcm|ay|ym|bogie|mme|sintered|native-chips]\n", stderr);
    return 2;
  }
  std::printf("Rendering %s measurement WAVs...\n", family);
  const bool all = !std::strcmp(family, "all");
  if ((all && (!renderBraids() || !renderPlaitsFamily<PlaitsVoice>("plaits") ||
               !renderPlaitsFamily<PlaitsAltVoice>("plaits-alt") || !renderPcmReference() ||
               !renderAYFamily(false) || !renderAYFamily(true) || !renderBogie() || !renderMME() || !renderSintered() || !renderNativeChips())) ||
      (!all && !std::strcmp(family, "native-chips") && (!renderPcmReference() || !renderNativeChips())) ||
      (!all && !std::strcmp(family, "braids") && !renderBraids()) ||
      (!all && !std::strcmp(family, "plaits") && !renderPlaitsFamily<PlaitsVoice>("plaits")) ||
      (!all && !std::strcmp(family, "plaits-alt") && !renderPlaitsFamily<PlaitsAltVoice>("plaits-alt")) ||
      (!all && !std::strcmp(family, "plaits-vca") && !renderPlaitsFamily<PlaitsVoice>("plaits-vca", true)) ||
      (!all && !std::strcmp(family, "plaits-alt-vca") && !renderPlaitsFamily<PlaitsAltVoice>("plaits-alt-vca", true)) ||
      (!all && !std::strcmp(family, "pcm") && !renderPcmReference()) ||
      (!all && !std::strcmp(family, "ay") && !renderAYFamily(false)) ||
      (!all && !std::strcmp(family, "ym") && !renderAYFamily(true)) ||
      (!all && !std::strcmp(family, "bogie") && !renderBogie()) ||
      (!all && !std::strcmp(family, "mme") && !renderMME()) ||
      (!all && !std::strcmp(family, "sintered") && !renderSintered())) {
    std::fputs("Could not write measurement WAVs.\n", stderr);
    return 1;
  }
  std::puts("Done: measurements/");
  return 0;
}
