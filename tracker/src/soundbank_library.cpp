// clang-format off
// app.h includes common.h inside a legacy C linkage block. Load the C++ API first.
#include "chipnomad_lib.h"
// clang-format on

#include "soundbank_library.h"
#include "soundbank_browser.h"

#include <chrono>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <future>
#include <map>
#include <set>

#include "app.h"
#include "audio_manager.h"
#include "corelib_file.h"
#include "screens/screens.h"
#include "soundbank_runtime.h"
namespace {
struct Result {
  std::vector<soundbank::BankEntry> banks;
  std::shared_ptr<const soundbank::PreparedPreset> preset;
  std::string error;
};
std::vector<std::pair<int,int>> presetOrder;
std::vector<soundbank::BankEntry> entries;
std::future<Result> job;
std::string status, bankId, presetId, expectedBank, expectedPreset;
int target = -1;
int preparationFrames = 0;
bool restoringProject = false;
bool scanned = false;
std::atomic<soundbank::LoadStage> loadingStage{soundbank::LoadStage::reading};
int pendingInstrument = -1;
std::string pendingBank, pendingPreset, pendingExpectedBank, pendingExpectedPreset;
uint32_t pendingExpectedHandle=0;
bool refreshPending = false;
uint32_t expectedHandle = 0;
size_t cacheLimit() {
  const char* value = getenv("CHOOCHOO_SOUNDBANK_CACHE_MB");
  if (!value) return 64 * 1024 * 1024;
  char* end = nullptr;
  long n = strtol(value, &end, 10);
  return n >= 8 && n <= 1024 && end && !*end ? size_t(n) * 1024 * 1024 : 64 * 1024 * 1024;
}
soundbank::SampleCache cache(cacheLimit());
std::map<std::string, std::string> failed;
std::string scanError;
#ifdef __EMSCRIPTEN__
constexpr auto launchPolicy = std::launch::deferred;
#else
constexpr auto launchPolicy = std::launch::async;
#endif
}  // namespace
std::string soundbankRoot() {
  if (const char* root = getenv("CHOOCHOO_SOUNDBANKS"))
    if (*root) return root;
#ifdef __EMSCRIPTEN__
  return "/user/instruments/USER/soundfont";
#endif
  bool external = fileIsRunningFromAppImage();
#if defined(ANDROID_BUILD) || defined(_WIN32)
  external = true;
#endif
  if (external) {
    char directory[1024];
    if (!fileGetDefaultDirectory(directory, sizeof(directory)))
      return std::string(directory) + "/instruments/USER/soundfont";
  }
  return "instruments/USER/soundfont";
}
const std::vector<soundbank::BankEntry>& soundbankEntries() { return entries; }
bool soundbankBusy() { return job.valid(); }
bool soundbankScanned() { return scanned; }
bool soundbankScanning() { return job.valid() && target < 0; }
const std::string& soundbankStatus() { return status; }
std::string soundbankLoadingStatus() {
  if (!soundbankBusy()) return {};
  if (soundbankScanning()) return "Indexing presets...";
  switch (loadingStage.load(std::memory_order_relaxed)) {
    case soundbank::LoadStage::unzipping: return "Unzipping...";
    case soundbank::LoadStage::locating: return "Finding samples...";
    case soundbank::LoadStage::checking: return "Checking samples...";
    case soundbank::LoadStage::loading: return "Loading samples...";
    default: return "Reading preset...";
  }
}
std::string soundbankPresetName(const std::string& bank, const std::string& preset) {
  for (const auto& entry : entries) if (entry.id == bank)
    for (const auto& item : entry.presets) if (item.first == preset)
      return soundbank::browserLabel(item.second);
  return soundbank::browserLabel(preset == "sample" || preset == "sfz" || preset == "exs"
    ? soundbank::bankFolders(bank).back() : preset);
}
std::string soundbankInstrumentError(int instrument) {
  if (!chipnomadState || instrument < 0 || instrument >= PROJECT_MAX_INSTRUMENTS) return {};
  const auto& i = chipnomadState->project.instruments[instrument];
  if (i.type != InstrumentType::SoundFont) return {};
  const auto& s = i.chip.soundfont;
  auto error = failed.find(std::string(s.bank) + "|" + s.preset);
  if (!soundbank::prepared(s.prepared) && error != failed.end()) return error->second;
  return scanError;
}
std::string soundbankInstrumentStatus(int instrument) {
  if (!chipnomadState || instrument < 0 || instrument >= PROJECT_MAX_INSTRUMENTS) return {};
  if (job.valid() && target == instrument) return soundbankLoadingStatus();
  const auto& s = chipnomadState->project.instruments[instrument].chip.soundfont;
  if (soundbank::prepared(s.prepared)) return "Preset ready";
  if (failed.count(std::string(s.bank) + "|" + s.preset)) return {};
  return s.bank[0] ? "Preset not ready" : "Select a preset";
}
std::string soundbankInstrumentWarning(int instrument) {
  if (!chipnomadState || instrument < 0 || instrument >= PROJECT_MAX_INSTRUMENTS) return {};
  const auto* p = soundbank::prepared(chipnomadState->project.instruments[instrument].chip.soundfont.prepared);
  if (!p || p->warnings.empty()) return {};
  for (const auto& warning : p->warnings)
    if (warning.find("custom modulators unsupported") != std::string::npos)
      continue; // Explained in the manual; do not repeat a bank-wide limitation on every preset.
    else return warning;
  return {};
}
std::string soundbankInstrumentRange(int instrument) {
  if(!chipnomadState || instrument<0 || instrument>=PROJECT_MAX_INSTRUMENTS)return {};
  const auto* preset=soundbank::prepared(chipnomadState->project.instruments[instrument].chip.soundfont.prepared);
  if(!preset)return {};
  bool keys[128]={};
  for(const auto& layer:preset->zones) if(!layer.zone.releaseTrigger && layer.zone.hiVel>0)
    for(int k=std::max(0,layer.zone.loKey);k<=std::min(127,layer.zone.hiKey);++k)keys[k]=true;
  int low=0,high=127;while(low<128&&!keys[low])++low;while(high>=low&&!keys[high])--high;
  if(low>high)return "Native keys: none";
  auto note=[](int key){static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return std::string(names[key%12])+std::to_string(key/12-1);};
  bool gaps=false;for(int k=low;k<=high;++k)if(!keys[k])gaps=true;
  return "Native keys: "+note(low)+" to "+note(high)+(gaps?" (gaps)":"");
}
void soundbankRescan(bool force) {
  if (soundbankBusy()) return;
  target = -1;
  status = "Indexing SoundBanks...";
  scanError.clear();
  failed.clear();
  auto root = soundbankRoot();
  preparationFrames = 2;
  job = std::async(launchPolicy, [root, force]() {
    Result r;
    try {
      r.banks = soundbank::BankIndex(root).scan(force);
    } catch (const std::exception& e) {
      r.error = e.what();
    }
    return r;
  });
}
void soundbankLoad(int instrument, const std::string& bank, const std::string& preset,
                   bool restoring) {
  if (!chipnomadState || instrument < 0 || instrument >= PROJECT_MAX_INSTRUMENTS) return;
  auto& i = chipnomadState->project.instruments[instrument];
  if (i.type != InstrumentType::SoundFont) return;
  if (soundbankBusy()) {
    if (!restoring) {
      pendingInstrument=instrument; pendingBank=bank; pendingPreset=preset;
      pendingExpectedBank=i.chip.soundfont.bank; pendingExpectedPreset=i.chip.soundfont.preset;
      pendingExpectedHandle=i.chip.soundfont.prepared;
    }
    return;
  }
  if (bank.size() >= sizeof(i.chip.soundfont.bank) ||
      preset.size() >= sizeof(i.chip.soundfont.preset)) {
    status = "Bank/preset path too long";
    return;
  }
  audioManager.pause();
  chipnomadCollectSoundbanks(chipnomadState);
  audioManager.resume();
  cache.trim();
  target = instrument;
  failed.erase(bank + "|" + preset);
  restoringProject = restoring;
  bankId = bank;
  presetId = preset;
  expectedBank = i.chip.soundfont.bank;
  expectedPreset = i.chip.soundfont.preset;
  expectedHandle = i.chip.soundfont.prepared;
  status = "Preparing preset...";
  loadingStage.store(soundbank::LoadStage::reading, std::memory_order_relaxed);
  auto root = soundbankRoot();
  preparationFrames = 2;
  job = std::async(launchPolicy, [root, bank, preset]() {
    Result r;
    soundbank::LoadProgressScope progress([](soundbank::LoadStage stage) {
      loadingStage.store(stage, std::memory_order_relaxed);
    });
    try {
      auto b = soundbank::parseBank(soundbank::BankIndex(root).resolve(bank));
      r.preset = cache.prepare(b, preset);
    } catch (const std::exception& e) {
      r.error = e.what();
    }
    return r;
  });
}
void soundbankCycle(int instrument, int direction) {
  if (!chipnomadState || instrument < 0 || instrument >= PROJECT_MAX_INSTRUMENTS) return;
  const auto& i = chipnomadState->project.instruments[instrument];
  if (i.type != InstrumentType::SoundFont) return;
  if (!scanned) { if (!soundbankBusy()) soundbankRescan(); return; }
  auto selected = soundbank::cyclePreset(entries, presetOrder,
    pendingInstrument == instrument ? pendingBank : (soundbankBusy() && target == instrument ? bankId : i.chip.soundfont.bank),
    pendingInstrument == instrument ? pendingPreset : (soundbankBusy() && target == instrument ? presetId : i.chip.soundfont.preset), direction);
  if (selected.first >= 0)
    soundbankLoad(instrument, entries[selected.first].id, entries[selected.first].presets[selected.second].first);
}
static void loadPending() {
  int next=pendingInstrument; pendingInstrument=-1;
  if (!chipnomadState || next<0) return;
  const auto& i=chipnomadState->project.instruments[next];
  if (i.type==InstrumentType::SoundFont && pendingExpectedBank==i.chip.soundfont.bank &&
      pendingExpectedPreset==i.chip.soundfont.preset && pendingExpectedHandle==i.chip.soundfont.prepared)
    soundbankLoad(next,pendingBank,pendingPreset);
}
static void failPreset(const std::string& error) {
  auto& i = chipnomadState->project.instruments[target];
  audioManager.pause();
  strcpy(i.chip.soundfont.bank, bankId.c_str());
  strcpy(i.chip.soundfont.preset, presetId.c_str());
  i.chip.soundfont.prepared = 0;
  if (!restoringProject || !i.name[0])
    snprintf(i.name, sizeof(i.name), "%s", soundbankPresetName(bankId, presetId).c_str());
  refreshPending = !chipnomadQueueProjectRefresh(chipnomadState);
  audioManager.resume();
  if (!restoringProject) projectModified = 1;
  failed[bankId + "|" + presetId] = error;
  status = error;
}
void soundbankPoll() {
  if (refreshPending && chipnomadState && chipnomadQueueProjectRefresh(chipnomadState))
    refreshPending = false;
  if (job.valid()) {
    auto ready = job.wait_for(std::chrono::seconds(0));
    if (ready == std::future_status::deferred && preparationFrames-- > 0) return;
    if (ready != std::future_status::ready && ready != std::future_status::deferred) return;
    auto r = job.get();
    // Only the latest requested preset is published when cycling outruns decoding.
    if (pendingInstrument == target && target >= 0) {
      loadPending();
      return;
    }
    if (target < 0) {
      if (!r.error.empty()) { status = scanError = r.error; return; }
      entries = std::move(r.banks);
      presetOrder = soundbank::presetSequence(entries);
      scanned = true;
      status = std::to_string(entries.size()) + " USER banks";
      return;
    }
    if (!chipnomadState) return;
    auto& i = chipnomadState->project.instruments[target];
    if (i.type != InstrumentType::SoundFont || expectedBank != i.chip.soundfont.bank ||
        expectedPreset != i.chip.soundfont.preset || expectedHandle != i.chip.soundfont.prepared) {
      status = "Preset request superseded";
      return;
    }
    if (!r.error.empty()) { failPreset(r.error); return; }
    try {
      audioManager.pause();
      uint32_t handle = soundbank::publish(r.preset);
      strcpy(i.chip.soundfont.bank, bankId.c_str());
      strcpy(i.chip.soundfont.preset, presetId.c_str());
      i.chip.soundfont.prepared = handle;
      if (!restoringProject || !i.name[0])
        snprintf(i.name, sizeof(i.name), "%s", r.preset->name.c_str());
      refreshPending = !chipnomadQueueProjectRefresh(chipnomadState);
      audioManager.resume();
      if (!restoringProject) projectModified = 1;
      status = "Preset ready";
    } catch (const std::exception& e) {
      audioManager.resume();
      failPreset(e.what());
    }
    return;
  }
  if (!chipnomadState) return;
  if (pendingInstrument >= 0) {
    loadPending(); return;
  }
  for (int n = 0; n < PROJECT_MAX_INSTRUMENTS; ++n) {
    auto& i = chipnomadState->project.instruments[n];
    if (i.type != InstrumentType::SoundFont) continue;
    auto& s = i.chip.soundfont;
    if (s.bank[0] && s.preset[0] && !soundbank::prepared(s.prepared) &&
        !failed.count(std::string(s.bank) + "|" + s.preset)) {
      soundbankLoad(n, s.bank, s.preset, true);
      break;
    }
  }
}

void soundbankShutdown() {
  if (job.valid()) job.get();
  pendingInstrument=-1; entries.clear(); scanned=false; status.clear(); scanError.clear();
  failed.clear(); refreshPending=false;
  soundbank::collect([](uint32_t) { return false; });
  cache.trim();
}
