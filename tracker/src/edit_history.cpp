#include "edit_history.h"
#include "audio_manager.h"
#include "app.h"
#include "screens.h"
#include "selection_popup.h"
#include "chipnomad_lib_live_stick.h"
#include "screen_groovebox.h"
#include "screen_instrument.h"
#include "soundbank_runtime.h"
#include <algorithm>
#include <cstring>
#include <array>
#include <memory>
#include <limits>
#include <new>
#include <vector>

namespace {
constexpr size_t historyLimit = 256 * 1024;
constexpr size_t resourceLimit = 8 * 1024 * 1024;
constexpr size_t actionLimit = 64;

struct InstrumentDelete {
  void operator()(Instrument* p) const {
    if (p) { getInstrumentFunctions(p->type).free(p); delete p; }
  }
};
struct Change {
  size_t offset = 0, size = 0, resources = 0;
  int instrument = -1;
  int modelInstrument = -1;
  std::unique_ptr<uint8_t[]> bytes;
  std::unique_ptr<Instrument, InstrumentDelete> patch;
  std::shared_ptr<const soundbank::PreparedPreset> bank;
};
struct Action {
  const char* label = "";
  uint64_t gesture = 0;
  uint32_t midiSequence = 0;
  int modifiedBefore = 0;
  std::vector<Change> changes;
  size_t bytes() const {
    size_t result = changes.capacity() * sizeof(Change);
    for (const auto& c : changes) result += c.size + (c.patch ? sizeof(Instrument) : 0);
    return result;
  }
  size_t resources() const {
    size_t result = 0;
    for (const auto& c : changes) result += c.resources;
    return result;
  }
};
std::array<Action, actionLimit> actions;
Action pending;
size_t cursor = 0, count = 0;
unsigned depth = 0;
bool failed = false, restoring = false;
bool gestureHeld = false, joinGesture = false;
uint64_t gestureId = 0;
uint64_t revision = 0;
ChipNomadState* owner = nullptr;
const AppScreen* menuOwner = nullptr;

int model(const Instrument& i) {
  switch (i.type) {
    case InstrumentType::Braids: return i.chip.braids.model;
    case InstrumentType::Plaits:
    case InstrumentType::PlaitsAlt: return i.chip.plaits.engine;
    case InstrumentType::MME: return int(i.chip.mme.model);
    case InstrumentType::Sintered: return int(i.chip.sintered.model);
    case InstrumentType::DrumSynth:
    case InstrumentType::DigiSophie: return int(i.chip.drumSynth.engine);
    case InstrumentType::AChChid: return int(i.chip.achchid.wave) * 256 + i.chip.achchid.model;
    default: return -1;
  }
}

uint8_t* address(size_t offset) {
  return reinterpret_cast<uint8_t*>(&chipnomadState->project) + offset;
}
size_t totalBytes() {
  size_t result = sizeof(actions) + sizeof(pending) + pending.bytes();
  for (const auto& a : actions) result += a.bytes();
  return result;
}
void dropFirst() {
  for (size_t i = 1; i < count; ++i) actions[i - 1] = std::move(actions[i]);
  actions[--count] = {};
  --cursor;
}
size_t totalResources() {
  size_t result = pending.resources();
  for (const auto& a : actions) result += a.resources();
  return result;
}
bool reserve(size_t bytes, size_t resources) {
  if (failed) return false;
  if (bytes > historyLimit || resources > resourceLimit ||
      pending.bytes() + sizeof(actions) + sizeof(pending) > historyLimit - bytes ||
      pending.resources() > resourceLimit - resources) {
    failed = true;
    screenMessage(MESSAGE_TIME_ERROR, "Undo memory limit: edit cancelled");
    return false;
  }
  while (count && cursor > 0 &&
         (totalBytes() + bytes > historyLimit || totalResources() + resources > resourceLimit)) {
    dropFirst();
  }
  if (totalBytes() + bytes <= historyLimit && totalResources() + resources <= resourceLimit) return true;
  failed = true;
  screenMessage(MESSAGE_TIME_ERROR, "Undo memory limit: edit cancelled");
  return false;
}
bool capture(void* value, size_t size) {
  if (!depth || restoring) return true;
  const size_t offset = static_cast<uint8_t*>(value) - address(0);
  for (const auto& c : pending.changes)
    if (c.instrument < 0 && c.offset == offset && c.size == size) return !failed;
  const size_t growth = pending.changes.size() == pending.changes.capacity() ?
    (pending.changes.capacity() ? pending.changes.capacity() : 1) * sizeof(Change) : 0;
  if (!reserve(size + growth, 0)) return false;
  try {
    Change change;
    change.offset = offset;
    change.size = size;
    change.bytes = std::make_unique<uint8_t[]>(size);
    std::memcpy(change.bytes.get(), value, size);
    pending.changes.push_back(std::move(change));
  } catch (const std::bad_alloc&) {
    failed = true;
    screenMessage(MESSAGE_TIME_ERROR, "Undo allocation failed: edit cancelled");
    return false;
  }
  return true;
}
size_t sampleBytes(const InstrumentSample& s) {
  if (!s.data) return 0;
  if (s.channels && size_t(s.frameCount) > std::numeric_limits<size_t>::max() / (s.channels * sizeof(int16_t)))
    return std::numeric_limits<size_t>::max();
  return size_t(s.frameCount) * s.channels * sizeof(int16_t);
}
size_t instrumentBytes(const Instrument& i) {
  if (i.type == InstrumentType::AYSample) return i.chip.aySample.sampleData ? i.chip.aySample.fileLength : 0;
  if (i.type == InstrumentType::Sample) return sampleBytes(i.chip.sample);
  if (i.type == InstrumentType::SCWF || i.type == InstrumentType::BYOWTBL) {
    const size_t a = sampleBytes(i.chip.scwf.oscillator[0]), b = sampleBytes(i.chip.scwf.oscillator[1]);
    return a > std::numeric_limits<size_t>::max() - b ? std::numeric_limits<size_t>::max() : a + b;
  }
  if (i.type == InstrumentType::SoundFont) {
    const auto* bank = soundbank::prepared(i.chip.soundfont.prepared);
    if (!bank) return 0;
    if (bank->bytes > resourceLimit) return resourceLimit + 1;
    size_t bytes = bank->bytes + sizeof(*bank) + bank->zones.capacity() * sizeof(soundbank::PreparedZone);
    for (const auto& key : bank->keys) bytes += key.capacity() * sizeof(uint16_t);
    return bytes;
  }
  return 0;
}
bool copySample(const InstrumentSample& from, InstrumentSample& to) {
  to.data = nullptr;
  const size_t bytes = sampleBytes(from);
  if (!bytes) return true;
  to.data = static_cast<int16_t*>(malloc(bytes));
  if (!to.data) return false;
  std::memcpy(to.data, from.data, bytes);
  return true;
}
bool prepareInstrument(const Instrument& from, Change& change) {
  change.patch.reset(new Instrument(from));
  change.resources = instrumentBytes(from);
  auto& to = *change.patch;
  if (from.type == InstrumentType::Sample) return copySample(from.chip.sample, to.chip.sample);
  if (from.type == InstrumentType::SCWF || from.type == InstrumentType::BYOWTBL) {
    to.chip.scwf.oscillator[0].data = to.chip.scwf.oscillator[1].data = nullptr;
    return copySample(from.chip.scwf.oscillator[0], to.chip.scwf.oscillator[0]) &&
           copySample(from.chip.scwf.oscillator[1], to.chip.scwf.oscillator[1]);
  }
  if (from.type == InstrumentType::AYSample) {
    to.chip.aySample.sampleData = nullptr;
    if (!change.resources) return true;
    to.chip.aySample.sampleData = static_cast<uint8_t*>(malloc(change.resources));
    if (!to.chip.aySample.sampleData) return false;
    std::memcpy(to.chip.aySample.sampleData, from.chip.aySample.sampleData, change.resources);
  } else if (from.type == InstrumentType::SoundFont) {
    change.bank = soundbank::retain(from.chip.soundfont.prepared);
    return !from.chip.soundfont.prepared || change.bank != nullptr;
  }
  return true;
}
void refreshScreen() {
  const auto* screen = currentScreen;
  if (screen == &screenPhrase || screen == &screenChain || screen == &screenTable || screen == &screenInstrument) {
    screenSetup(screen, screen == &screenInstrument ? cInstrument : -2);
    screenApplyPending();
  } else if (screen && screen->fullRedraw) screen->fullRedraw();
}
void swapChange(Change& change) {
  if (change.patch) {
    auto& current = chipnomadState->project.instruments[change.instrument];
    const size_t resources = instrumentBytes(current);
    auto bank = current.type == InstrumentType::SoundFont ?
      soundbank::retain(current.chip.soundfont.prepared) : nullptr;
    std::swap(current, *change.patch);
    change.resources = resources;
    change.bank = std::move(bank);
  } else {
    auto* current = address(change.offset);
    for (size_t i = 0; i < change.size; ++i) std::swap(current[i], change.bytes[i]);
  }
}
bool apply(bool redo) {
  gestureHeld = joinGesture = false;
  if (!chipnomadState || owner != chipnomadState) {
    editHistoryClear();
    screenMessage(MESSAGE_TIME, "Project changed: undo unavailable");
    return false;
  }
  if ((depth && !pending.changes.empty()) || chipnomadMotionMode() || inputRawCallback || charEditIsOpen() || currentScreen == &screenSelectionPopup) {
    screenMessage(MESSAGE_TIME_ERROR, "Finish current edit/REC before undo");
    return false;
  }
  if (redo ? cursor == count : cursor == 0) {
    screenMessage(MESSAGE_TIME, redo ? "Nothing to redo" : "Nothing to undo");
    return false;
  }
  Action& action = actions[redo ? cursor : cursor - 1];
  size_t resources = totalResources() - action.resources();
  bool instrument = false;
  for (const auto& c : action.changes) if (c.patch) {
    instrument = true;
    const size_t bytes = instrumentBytes(chipnomadState->project.instruments[c.instrument]);
    if (bytes > resourceLimit || resources > resourceLimit - bytes) {
      screenMessage(MESSAGE_TIME_ERROR, "Undo resource limit: unchanged");
      return false;
    }
    resources += bytes;
  }
  if (resources > resourceLimit) {
    screenMessage(MESSAGE_TIME_ERROR, "Undo resource limit: unchanged");
    return false;
  }
  restoring = true;
  if (instrument) audioManager.pause();
  if (redo) {
    for (auto& c : action.changes) swapChange(c);
    ++cursor;
  } else {
    for (auto it = action.changes.rbegin(); it != action.changes.rend(); ++it) swapChange(*it);
    --cursor;
  }
  if (instrument) {
    if (audioManager.reinitializeChips) audioManager.reinitializeChips();
    audioManager.resume();
  }
  projectModified = 1;
  ++revision;
  grooveboxDiscardUndo();
  appPublishProjectEdit();
  refreshScreen();
  restoring = false;
  screenMessage(MESSAGE_TIME, "%s: %s", redo ? "Redo" : "Undo", action.label);
  return true;
}
void menuCancel() { screenSetup(menuOwner, -2); }
void menuSelected(int value) {
  screenSetup(menuOwner, -2);
  screenApplyPending();
  if (value) editHistoryRedo(); else editHistoryUndo();
}
}

EditHistoryScope::EditHistoryScope(const char* label) {
  if (owner != chipnomadState) editHistoryClear();
  if (!depth++) {
    owner = chipnomadState;
    if (joinGesture && count && cursor == count && actions[count - 1].gesture == gestureId) {
      pending = std::move(actions[--count]);
      actions[count] = {};
      --cursor;
    } else {
      pending.label = label;
      pending.gesture = gestureHeld || joinGesture ? gestureId : 0;
      pending.modifiedBefore = projectModified;
    }
    joinGesture = false;
    failed = false;
  }
}
EditHistoryScope::EditHistoryScope(const char* label, uint32_t midiSequence) {
  if (owner != chipnomadState) editHistoryClear();
  if (!depth++) {
    owner = chipnomadState;
    if (midiSequence && count && cursor == count &&
        actions[count - 1].midiSequence == midiSequence) {
      pending = std::move(actions[--count]);
      actions[count] = {};
      --cursor;
    } else {
      pending.label = label;
      pending.midiSequence = midiSequence;
      pending.modifiedBefore = projectModified;
    }
    joinGesture = false;
    failed = false;
  }
}
EditHistoryScope::~EditHistoryScope() {
  if (!depth || --depth) return;
  if (failed) {
    bool instrument = std::any_of(pending.changes.begin(), pending.changes.end(), [](const Change& c) { return bool(c.patch); });
    if (instrument) audioManager.pause();
    for (auto it = pending.changes.rbegin(); it != pending.changes.rend(); ++it) {
      if (it->modelInstrument >= 0) {
        Instrument before;
        std::memcpy(&before, it->bytes.get(), sizeof(before));
        if (model(before) < 0 || std::any_of(pending.changes.begin(), pending.changes.end(),
            [&](const Change& c) { return c.instrument == it->modelInstrument; })) continue;
      }
      if (it->patch && std::memcmp(it->bytes.get(), &chipnomadState->project.instruments[it->instrument], sizeof(Instrument)) == 0) continue;
      swapChange(*it);
    }
    if (instrument) {
      if (audioManager.reinitializeChips) audioManager.reinitializeChips();
      audioManager.resume();
    }
    appPublishProjectEdit();
    projectModified = pending.modifiedBefore;
    refreshScreen();
  } else {
    auto& changes = pending.changes;
    for (const auto& c : changes) if (c.modelInstrument >= 0) {
      Instrument before;
      std::memcpy(&before, c.bytes.get(), sizeof(before));
      const auto& after = chipnomadState->project.instruments[c.modelInstrument];
      const bool typeChange = before.type != after.type &&
        std::none_of(changes.begin(), changes.end(), [&](const Change& owned) { return owned.instrument == c.modelInstrument; });
      if (typeChange || (before.type == after.type && model(before) == model(after) &&
          std::memcmp(&before, &after, sizeof(before)) != 0)) {
        editHistoryBarrier();
        projectModified = 1;
        appPublishProjectEdit();
        return;
      }
    }
    changes.erase(std::remove_if(changes.begin(), changes.end(), [](const Change& c) {
      if (c.modelInstrument >= 0) {
        Instrument before;
        std::memcpy(&before, c.bytes.get(), sizeof(before));
        const auto& after = chipnomadState->project.instruments[c.modelInstrument];
        if (before.type != after.type || model(before) == model(after)) return true;
      }
      return std::memcmp(c.bytes.get(), c.patch ? reinterpret_cast<const uint8_t*>(&chipnomadState->project.instruments[c.instrument]) : address(c.offset), c.size) == 0;
    }), changes.end());
    if (!changes.empty()) {
      while (count > cursor) actions[--count] = {};
      if (count == actionLimit) dropFirst();
      actions[count++] = std::move(pending);
      cursor = count;
      projectModified = 1;
      ++revision;
    }
  }
  pending = {};
}
bool editHistoryCaptureModel(int index) {
  if (!depth || !chipnomadState || index < 0 || index >= PROJECT_MAX_INSTRUMENTS) return true;
  auto& instrument = chipnomadState->project.instruments[index];
  if (!capture(&instrument, sizeof(instrument))) return false;
  for (auto& c : pending.changes)
    if (!c.patch && c.offset == size_t(reinterpret_cast<uint8_t*>(&instrument) - address(0)))
      c.modelInstrument = index;
  return true;
}
bool editHistoryCapture(EditObject object, int index) {
  if (!chipnomadState || !depth) return true;
  auto& p = chipnomadState->project;
  switch (object) {
    case EditObject::song:
      return capture(p.song, sizeof(p.song)) && capture(p.songHighlight, sizeof(p.songHighlight));
    case EditObject::phrase:
      return index < 0 || index >= PROJECT_MAX_PHRASES || capture(&p.phrases[index], sizeof(Phrase));
    case EditObject::chain:
      return index < 0 || index >= PROJECT_MAX_CHAINS || capture(&p.chains[index], sizeof(Chain));
    case EditObject::table:
      return index < 0 || index >= PROJECT_MAX_TABLES || capture(&p.tables[index], sizeof(Table));
    case EditObject::pattern:
      return index < 0 || index >= PROJECT_MAX_PATTERNS ||
        (capture(&p.patterns[index], sizeof(Pattern)) &&
         capture(&p.patternToChainMap[index], sizeof(uint16_t)) &&
         capture(&p.chainToPatternMap[index], sizeof(uint16_t)));
    case EditObject::playhead: return capture(&p.playheadMode, sizeof(p.playheadMode));
  }
  return false;
}
bool editHistoryCapture(Project* project, EditObject object, int index) {
  return !chipnomadState || project != &chipnomadState->project || editHistoryCapture(object, index);
}
bool editHistoryCaptureInstrument(int index) {
  if (!depth || restoring || !chipnomadState) return true;
  if (index < 0 || index >= PROJECT_MAX_INSTRUMENTS) return false;
  for (const auto& c : pending.changes) if (c.instrument == index) return !failed;
  const auto& from = chipnomadState->project.instruments[index];
  const size_t resources = instrumentBytes(from);
  const size_t growth = pending.changes.size() == pending.changes.capacity() ?
    (pending.changes.capacity() ? pending.changes.capacity() : 1) * sizeof(Change) : 0;
  if (!reserve(2 * sizeof(Instrument) + growth, resources)) return false;
  try {
    Change change;
    change.instrument = index;
    change.resources = resources;
    change.size = sizeof(Instrument);
    change.bytes = std::make_unique<uint8_t[]>(sizeof(Instrument));
    std::memcpy(change.bytes.get(), &from, sizeof(Instrument));
    if (!prepareInstrument(from, change)) {
      failed = true;
      screenMessage(MESSAGE_TIME_ERROR, "Undo resource preparation failed: cancelled");
      return false;
    }
    pending.changes.push_back(std::move(change));
  } catch (const std::bad_alloc&) {
    failed = true;
    screenMessage(MESSAGE_TIME_ERROR, "Undo allocation failed: edit cancelled");
    return false;
  }
  return true;
}
bool editHistoryCloneInstrument(int source, int destination) {
  if (!chipnomadState || source < 0 || source >= PROJECT_MAX_INSTRUMENTS ||
      destination < 0 || destination >= PROJECT_MAX_INSTRUMENTS) {
    screenMessage(MESSAGE_TIME_ERROR, "Invalid instrument clone");
    return false;
  }
  if (source == destination) return true;
  auto& p = chipnomadState->project;
  EditHistoryScope history("Instrument clone");
  if (instrumentBytes(p.instruments[source]) > resourceLimit) {
    failed = true;
    screenMessage(MESSAGE_TIME_ERROR, "Undo resource limit: clone cancelled");
    return false;
  }
  if (!editHistoryCaptureInstrument(destination) || !editHistoryCapture(EditObject::table, destination)) return false;
  try {
    Change prepared;
    if (!prepareInstrument(p.instruments[source], prepared)) {
      failed = true;
      screenMessage(MESSAGE_TIME_ERROR, "Instrument clone allocation failed");
      return false;
    }
    audioManager.pause();
    auto& to = p.instruments[destination];
    getInstrumentFunctions(to.type).free(&to);
    to = *prepared.patch;
    *prepared.patch = {};
    p.tables[destination] = p.tables[source];
    if (audioManager.reinitializeChips) audioManager.reinitializeChips();
    audioManager.resume();
  } catch (const std::bad_alloc&) {
    failed = true;
    screenMessage(MESSAGE_TIME_ERROR, "Instrument clone allocation failed");
    return false;
  }
  return true;
}
void editHistoryClear() { for (auto& a : actions) a = {}; pending = {}; count = cursor = depth = 0; failed = gestureHeld = joinGesture = false; owner = chipnomadState; ++revision; }
void editHistoryGesture(bool held) {
  joinGesture = gestureHeld;
  if (!gestureHeld) ++gestureId;
  gestureHeld = held;
}
void editHistoryBarrier() {
  if (restoring) return;
  if (count) screenMessage(MESSAGE_TIME, "Undo cleared: external/resource edit");
  editHistoryClear();
}
bool editHistoryUndo() { return apply(false); }
bool editHistoryRedo() { return apply(true); }
bool editHistoryCanUndo() { return owner == chipnomadState && cursor > 0; }
bool editHistoryCanRedo() { return owner == chipnomadState && cursor < count; }
uint64_t editHistoryRevision() { return revision; }
size_t editHistoryBytes() { return totalBytes(); }
size_t editHistoryResourceBytes() { return totalResources(); }
void editHistoryOpenMenu() {
  if (inputRawCallback) return;
  menuOwner = currentScreen;
  static const SelectionItem items[] = {
    {"Undo", 0, nullptr, 0, "Restore the previous editor action"},
    {"Redo", 1, nullptr, 0, "Restore the next editor action"},
  };
  selectionPopupSetup("EDIT HISTORY", items, 2, 0, menuSelected, menuCancel);
  screenSetup(&screenSelectionPopup, 0);
}
bool editHistoryInput(InputCode input, bool down) {
  if (inputRawCallback) return false;
  static std::array<InputCode, 16> pressed{};
  static size_t pressedCount = 0;
  for (size_t i = 0; i < pressedCount; ++i) {
    if (pressed[i].deviceType != input.deviceType || pressed[i].code != input.code) continue;
    if (!down) pressed[i] = pressed[--pressedCount];
    return true;
  }
  if (input.deviceType == InputDeviceType::none || charEditIsOpen()) return false;
  int command = input.deviceType == InputDeviceType::logical ? input.code : 0;
  for (int i = 0; i < 3; ++i) {
    const auto& undo = appSettings.keyMapping.keyUndo[i];
    if (input.deviceType == undo.deviceType && input.code == undo.code) { command = keyUndo; break; }
  }
  if (command != keyUndo) for (int i = 0; i < 3; ++i) {
    const auto& redo = appSettings.keyMapping.keyRedo[i];
    if (input.deviceType == redo.deviceType && input.code == redo.code) { command = keyRedo; break; }
  }
#if defined(DESKTOP_BUILD) || defined(WEB_BUILD)
  if (inputIsHistoryModifierHeld() && inputIsUndoKey(input)) command = inputIsShiftHeld() ? keyRedo : keyUndo;
#endif
  if (command != keyUndo && command != keyRedo) return false;
  if (down) {
    if (pressedCount == pressed.size()) {
      screenMessage(MESSAGE_TIME_ERROR, "Too many held undo keys");
      return true;
    }
    pressed[pressedCount++] = input;
    if (command == keyUndo) editHistoryUndo(); else editHistoryRedo();
  }
  return true;
}
