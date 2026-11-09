#include "screen_groovebox.h"
#include "corelib_gfx.h"
#include "project_utils.h"
#include "utils.h"
#include "selection_popup.h"
#include "audio_manager.h"
#include "field_style.h"
#include "copy_paste.h"
#include "synth/sample_voice.h"
#include <memory>
#include <algorithm>

bool grooveboxEditorActive = false;
bool canonicalSimplifiedActive = false;
static int scene, track, bar, step, field, headerField;
static bool editingFX, sharedEditPending, barEditPending;
static uint8_t lastNote = 48, lastInstrument = 0;
static uint16_t lastVolume = PHRASE_VOLUME_MAX;
static FXCell lastFX = {FX_ID_EMPTY, 0};
static bool returningFromTools;
static int lastIndicators[PROJECT_MAX_TRACKS][8];
static uint16_t sceneClipboard[PROJECT_MAX_TRACKS];
static uint8_t sceneHighlight[PROJECT_MAX_TRACKS];
static bool sceneCopied;

struct GrooveboxReferences {
  int chains[PROJECT_MAX_CHAINS] = {};
  int phrases[PROJECT_MAX_PHRASES] = {};
  bool freeChains[PROJECT_MAX_CHAINS] = {};
  bool freePhrases[PROJECT_MAX_PHRASES] = {};
  int chainFree = 0, phraseFree = 0;
  explicit GrooveboxReferences(Project* p) {
    for (const auto& row : p->song)
      for (auto c : row) if (c < PROJECT_MAX_CHAINS) ++chains[c];
    for (auto c : p->patternToChainMap)
      if (c < PROJECT_MAX_CHAINS) ++chains[c];
    for (const auto& chain : p->chains)
      for (const auto& row : chain.rows)
        if (row.phrase < PROJECT_MAX_PHRASES) ++phrases[row.phrase];
    for (const auto& pattern : p->patterns)
      for (const auto& b : pattern.bars)
        if (b.phrase < PROJECT_MAX_PHRASES) ++phrases[b.phrase];
    for (int c = 0; c < PROJECT_MAX_CHAINS; ++c) {
      freeChains[c] = !chains[c] && p->chainToPatternMap[c] == EMPTY_VALUE_16 && chainIsEmpty(p, c);
      chainFree += freeChains[c];
    }
    for (int f = 0; f < PROJECT_MAX_PHRASES; ++f) {
      freePhrases[f] = !phrases[f] && phraseIsEmpty(p, f);
      phraseFree += freePhrases[f];
    }
  }
};

struct GrooveboxSceneUndo {
  int chainCount = 0, phraseCount = 0;
  int chainSlots[PROJECT_MAX_TRACKS], phraseSlots[PROJECT_MAX_TRACKS * 16];
  Chain chains[PROJECT_MAX_TRACKS];
  Phrase phrases[PROJECT_MAX_TRACKS * 16];
};

// One editor-local action, never a second playback arrangement or DSP snapshot.
struct GrooveboxUndo {
  ChipNomadState* owner;
  int scene, chain = -1, count = 0;
  uint16_t song[PROJECT_MAX_TRACKS];
  uint8_t highlights[PROJECT_MAX_TRACKS];
  Chain oldChain;
  int slots[16];
  Phrase phrases[16];
  std::unique_ptr<GrooveboxSceneUndo> preparedScene;
  explicit GrooveboxUndo(int row) : owner(chipnomadState), scene(row) {
    std::copy_n(owner->project.song[row], PROJECT_MAX_TRACKS, song);
    std::copy_n(owner->project.songHighlight[row], PROJECT_MAX_TRACKS, highlights);
  }
  void saveChain(int c) { chain = c; oldChain = owner->project.chains[c]; }
  void savePhrase(int f) {
    slots[count] = f;
    phrases[count++] = owner->project.phrases[f];
  }
};
static std::unique_ptr<GrooveboxUndo> undo;
static void changed() {
  projectModified = true;
  chipnomadQueueProjectRefresh(chipnomadState);
}

static int allocateSlot(bool* free, int size, int start, int bankSize) {
  for (int i = start; i < std::min(start + bankSize, size); ++i)
    if (free[i]) { free[i] = false; return i; }
  if (bankSize == 16)
    for (int i = (start + 128) % 256; i < std::min((start + 128) % 256 + 16, size); ++i)
      if (free[i]) { free[i] = false; return i; }
  for (int i = 0; i < size; ++i)
    if (free[i]) { free[i] = false; return i; }
  return -1;
}

int grooveboxFindFreeChain(Project* p, int col) {
  if (!p || col < 0 || col >= p->tracksCount) return EMPTY_VALUE_16;
  GrooveboxReferences refs(p);
  int slot = allocateSlot(refs.freeChains, PROJECT_MAX_CHAINS, (col + 1) * 16, 16);
  return slot < 0 ? EMPTY_VALUE_16 : slot;
}

bool grooveboxMergePatternsUp(int firstRow, int lastRow, int firstCol, int lastCol) {
  Project* p = &chipnomadState->project;
  if (p->viewMode != ViewMode::classic || firstRow < 0 || lastRow >= PROJECT_MAX_LENGTH ||
      firstRow > lastRow || firstCol < 0 || lastCol >= PROJECT_MAX_TRACKS || firstCol > lastCol) {
    screenMessage(MESSAGE_TIME_ERROR, "Invalid selection: merge cancelled");
    return false;
  }
  GrooveboxReferences refs(p);
  Chain chains[PROJECT_MAX_TRACKS];
  int slots[PROJECT_MAX_TRACKS];
  std::fill_n(slots, PROJECT_MAX_TRACKS, -1);
  for (int col = firstCol; col <= lastCol; ++col) {
    int count = 0, length = 0;
    chainClear(&chains[col]);
    for (int row = firstRow; row <= lastRow; ++row) {
      auto c = p->song[row][col];
      if (c == EMPTY_VALUE_16) continue;
      if (c >= PROJECT_MAX_CHAINS) {
        screenMessage(MESSAGE_TIME_ERROR, "Invalid chain: merge cancelled"); return false;
      }
      int bars = grooveboxTrackLength(p, row, col);
      if (!bars) { screenMessage(MESSAGE_TIME_ERROR, "Empty pattern: add a bar first"); return false; }
      for (int b = bars; b < 16; ++b)
        if (p->chains[c].rows[b].phrase != EMPTY_VALUE_16) {
          screenMessage(MESSAGE_TIME_ERROR, "Retained bars after gap: merge cancelled"); return false;
        }
      if (length + bars > 16) {
        screenMessage(MESSAGE_TIME_ERROR, "Pattern too long to merge"); return false;
      }
      std::copy_n(p->chains[c].rows, bars, chains[col].rows + length);
      length += bars;
      ++count;
    }
    if (count < 2) continue;
    slots[col] = allocateSlot(refs.freeChains, PROJECT_MAX_CHAINS, (col + 1) * 16, 16);
    if (slots[col] < 0) { screenMessage(MESSAGE_TIME_ERROR, "No free chain: merge cancelled"); return false; }
  }
  bool merged = false;
  for (int col = firstCol; col <= lastCol; ++col) {
    if (slots[col] < 0) continue;
    p->chains[slots[col]] = chains[col];
    for (int row = firstRow; row <= lastRow; ++row) p->song[row][col] = EMPTY_VALUE_16;
    p->song[firstRow][col] = slots[col];
    merged = true;
  }
  if (merged) {
    undo.reset();
    changed();
    screenMessage(MESSAGE_TIME, "Merged; new chain IDs may affect FX");
  } else screenMessage(MESSAGE_TIME, "Select at least two patterns in a track");
  return merged;
}

bool grooveboxPrepareScene(int row) {
  auto& p = chipnomadState->project;
  if (p.viewMode != ViewMode::classic || row < 0 || row >= PROJECT_MAX_LENGTH ||
      p.tracksCount < 1 || p.tracksCount > PROJECT_MAX_TRACKS) {
    screenMessage(MESSAGE_TIME_ERROR, "Cannot prepare this Groovebox scene");
    return false;
  }
  const int length = std::clamp((int)appSettings.defaultPatternLength, 1, 16);
  GrooveboxReferences refs(&p);
  for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) {
    int c = p.song[row][t];
    if (c == EMPTY_VALUE_16) continue;
    if (t >= p.tracksCount || c >= PROJECT_MAX_CHAINS || !chainIsEmpty(&p, c) ||
        refs.chains[c] > 1 || p.chainToPatternMap[c] != EMPTY_VALUE_16) return true;
  }
  int chains[PROJECT_MAX_TRACKS], phrases[PROJECT_MAX_TRACKS][16];
  for (int t = 0; t < p.tracksCount; ++t) {
    chains[t] = p.song[row][t] == EMPTY_VALUE_16 ?
      allocateSlot(refs.freeChains, PROJECT_MAX_CHAINS, (t + 1) * 16, 16) : p.song[row][t];
    if (chains[t] < 0) {
      screenMessage(MESSAGE_TIME_ERROR, "No free chains: scene not prepared");
      return false;
    }
    for (int b = 0; b < length; ++b) {
      phrases[t][b] = allocateSlot(refs.freePhrases, PROJECT_MAX_PHRASES, t * 128, 128);
      if (phrases[t][b] < 0) {
        screenMessage(MESSAGE_TIME_ERROR, "No free phrases: scene not prepared");
        return false;
      }
    }
  }
  undo = std::make_unique<GrooveboxUndo>(row);
  undo->preparedScene = std::make_unique<GrooveboxSceneUndo>();
  auto& snapshot = *undo->preparedScene;
  for (int t = 0; t < p.tracksCount; ++t) {
    int c = chains[t], i = snapshot.chainCount++;
    snapshot.chainSlots[i] = c; snapshot.chains[i] = p.chains[c];
    chainClear(&p.chains[c]);
    for (int b = 0; b < length; ++b) {
      int f = phrases[t][b], n = snapshot.phraseCount++;
      snapshot.phraseSlots[n] = f; snapshot.phrases[n] = p.phrases[f];
      phraseClear(&p.phrases[f]);
      p.chains[c].rows[b].phrase = f;
    }
    p.song[row][t] = c;
  }
  changed();
  screenMessage(MESSAGE_TIME, "Empty scene prepared; Tools undo");
  return true;
}

static int chainAt(const Project* p, int row, int col) {
  if (!p || row < 0 || row >= PROJECT_MAX_LENGTH || col < 0 || col >= p->tracksCount)
    return -1;
  int chain = p->song[row][col];
  return chain < PROJECT_MAX_CHAINS ? chain : -1;
}

int grooveboxPhraseAt(const Project* p, int row, int col, int b) {
  int chain = chainAt(p, row, col);
  if (chain < 0 || b < 0 || b >= 16) return -1;
  for (int i = 0; i <= b; ++i)
    if (p->chains[chain].rows[i].phrase >= PROJECT_MAX_PHRASES) return -1;
  return p->chains[chain].rows[b].phrase;
}

int grooveboxTrackLength(const Project* p, int row, int col) {
  int c = chainAt(p, row, col), length = 0;
  if (c >= 0)
    while (length < 16 && p->chains[c].rows[length].phrase < PROJECT_MAX_PHRASES) ++length;
  return length;
}

static int sceneLength() {
  int length = 1;
  for (int t = 0; t < chipnomadState->project.tracksCount; ++t)
    length = std::max(length, grooveboxTrackLength(&chipnomadState->project, scene, t));
  return length;
}

static int displayBar(int t) {
  int length = grooveboxTrackLength(&chipnomadState->project, scene, t);
  return length ? bar % length : -1;
}

bool grooveboxCellShared(const Project* p, int row, int col, int b) {
  int chain = chainAt(p, row, col);
  if (chain < 0) return false;
  for (int r = 0; r < PROJECT_MAX_LENGTH; ++r)
    for (int t = 0; t < PROJECT_MAX_TRACKS; ++t)
      if ((r != row || t != col) && p->song[r][t] == chain) return true;
  int phrase = grooveboxPhraseAt(p, row, col, b);
  if (phrase < 0) return false;
  for (int c = 0; c < PROJECT_MAX_CHAINS; ++c)
    for (int i = 0; i < 16; ++i)
      if ((c != chain || i != b) && p->chains[c].rows[i].phrase == phrase) return true;
  for (int c = 0; c < PROJECT_MAX_PATTERNS; ++c)
    for (int i = 0; i < PROJECT_MAX_PATTERN_LENGTH; ++i)
      if (p->patterns[c].bars[i].phrase == phrase) return true;
  return false;
}

bool grooveboxAction(GrooveboxAction action, int row, int col) {
  auto& p = chipnomadState->project;
  if (p.viewMode != ViewMode::classic) {
    screenMessage(MESSAGE_TIME_ERROR, "Convert to Classic first");
    return false;
  }
  if (action == GrooveboxAction::undo) {
    if (!undo || undo->owner != chipnomadState) {
      screenMessage(MESSAGE_TIME_ERROR, "No Groovebox action to undo");
      return false;
    }
    std::copy_n(undo->song, PROJECT_MAX_TRACKS, p.song[undo->scene]);
    std::copy_n(undo->highlights, PROJECT_MAX_TRACKS, p.songHighlight[undo->scene]);
    if (undo->chain >= 0) p.chains[undo->chain] = undo->oldChain;
    for (int i = 0; i < undo->count; ++i) p.phrases[undo->slots[i]] = undo->phrases[i];
    if (undo->preparedScene) {
      auto& snapshot = *undo->preparedScene;
      for (int i = 0; i < snapshot.chainCount; ++i) p.chains[snapshot.chainSlots[i]] = snapshot.chains[i];
      for (int i = 0; i < snapshot.phraseCount; ++i) p.phrases[snapshot.phraseSlots[i]] = snapshot.phrases[i];
    }
    undo.reset();
    sceneCopied = false;
    changed();
    return true;
  }
  if (p.viewMode != ViewMode::classic || row < 0 || row >= PROJECT_MAX_LENGTH || col < 0 || col >= p.tracksCount) {
    screenMessage(MESSAGE_TIME_ERROR, "Use Classic for this track");
    return false;
  }
  if (action == GrooveboxAction::copyScene) {
    for (auto c : p.song[row])
      if (c != EMPTY_VALUE_16 && c >= PROJECT_MAX_CHAINS) {
        screenMessage(MESSAGE_TIME_ERROR, "Invalid scene: open Classic");
        return false;
      }
    std::copy_n(p.song[row], PROJECT_MAX_TRACKS, sceneClipboard);
    std::copy_n(p.songHighlight[row], PROJECT_MAX_TRACKS, sceneHighlight);
    sceneCopied = true;
    screenMessage(MESSAGE_TIME, "Scene copied (linked references)");
    return true;
  }
  if (action == GrooveboxAction::pasteScene) {
    if (!sceneCopied) { screenMessage(MESSAGE_TIME_ERROR, "Copy a scene first"); return false; }
    for (auto c : p.song[row])
      if (c != EMPTY_VALUE_16) { screenMessage(MESSAGE_TIME_ERROR, "Paste into an empty scene"); return false; }
    undo = std::make_unique<GrooveboxUndo>(row);
    std::copy_n(sceneClipboard, PROJECT_MAX_TRACKS, p.song[row]);
    std::copy_n(sceneHighlight, PROJECT_MAX_TRACKS, p.songHighlight[row]);
    changed();
    return true;
  }
  int c = chainAt(&p, row, col);
  GrooveboxReferences refs(&p);
  int length = grooveboxTrackLength(&p, row, col);
  int reset = std::clamp((int)appSettings.defaultPatternLength, 1, 16);
  if (action != GrooveboxAction::makeIndependent && !canonicalSimplifiedActive &&
      c >= 0 && refs.chains[c] > 1) {
    screenMessage(MESSAGE_TIME_ERROR, "Shared chain: make independent first");
    return false;
  }
  if (action == GrooveboxAction::halveLength &&
      (length < 2 || length % 2 != 0)) {
    screenMessage(MESSAGE_TIME_ERROR, "Can't halve: need an even length");
    return false;
  }
  if (action == GrooveboxAction::resetLength && reset == length) return true;
  if (action == GrooveboxAction::shorten || action == GrooveboxAction::halveLength ||
      (action == GrooveboxAction::resetLength && reset < length)) {
    if (length == 0) { screenMessage(MESSAGE_TIME_ERROR, "Track already has no bars"); return false; }
    undo = std::make_unique<GrooveboxUndo>(row);
    undo->saveChain(c);
    int next = action == GrooveboxAction::shorten ? length - 1 :
      action == GrooveboxAction::halveLength ? length / 2 : reset;
    for (int i = next; i < length; ++i) p.chains[c].rows[i].phrase = EMPTY_VALUE_16;
    changed();
    return true;
  }
  if (action == GrooveboxAction::makeIndependent && c < 0) {
    screenMessage(MESSAGE_TIME_ERROR, "No chain to make independent");
    return false;
  }
  if (c < 0 && p.song[row][col] != EMPTY_VALUE_16) {
    screenMessage(MESSAGE_TIME_ERROR, "Invalid chain: open Classic");
    return false;
  }
  bool clone = action == GrooveboxAction::makeIndependent;
  bool linkedDuplicate = canonicalSimplifiedActive && !clone &&
    (action == GrooveboxAction::doubleLength || action == GrooveboxAction::duplicateBar);
  int target = c;
  if (c < 0 || clone) target = allocateSlot(refs.freeChains, PROJECT_MAX_CHAINS, (col + 1) * 16, 16);
  if (target < 0) { screenMessage(MESSAGE_TIME_ERROR, "No free chain"); return false; }
  int first = clone ? 0 : length;
  int count = clone ? 16 : action == GrooveboxAction::doubleLength ? length :
    action == GrooveboxAction::resetLength ? reset - length : 1;
  if (!clone && (length + count > 16 || count == 0)) {
    screenMessage(MESSAGE_TIME_ERROR, "Length must fit 1..16 bars");
    return false;
  }
  int slots[16], sources[16];
  std::fill_n(slots, 16, -1);
  for (int i = 0; i < count; ++i) {
    int destination = first + i;
    sources[i] = -1;
    if (clone) {
      uint16_t f = p.chains[c].rows[i].phrase;
      if (f == EMPTY_VALUE_16) continue;
      if (f >= PROJECT_MAX_PHRASES) { screenMessage(MESSAGE_TIME_ERROR, "Invalid phrase: open Classic"); return false; }
      sources[i] = f;
    } else {
      if (c >= 0 && p.chains[c].rows[destination].phrase != EMPTY_VALUE_16) {
        screenMessage(MESSAGE_TIME_ERROR, "Retained bars: open Classic");
        return false;
      }
      if (action == GrooveboxAction::duplicateBar || action == GrooveboxAction::doubleLength) {
        int sourceBar = action == GrooveboxAction::doubleLength ? i : bar;
        sources[i] = grooveboxPhraseAt(&p, row, col, sourceBar);
        if (sources[i] < 0) { screenMessage(MESSAGE_TIME_ERROR, "Select an existing source bar"); return false; }
      }
    }
    if (linkedDuplicate) continue;
    slots[i] = allocateSlot(refs.freePhrases, PROJECT_MAX_PHRASES, col * 128, 128);
    if (slots[i] < 0) { screenMessage(MESSAGE_TIME_ERROR, "No free phrase: action cancelled"); return false; }
  }
  undo = std::make_unique<GrooveboxUndo>(row);
  undo->saveChain(target);
  if (clone) p.chains[target] = p.chains[c];
  else if (c < 0) chainClear(&p.chains[target]);
  for (int i = 0; i < count; ++i) {
    if (linkedDuplicate) {
      p.chains[target].rows[first + i] =
        p.chains[c].rows[action == GrooveboxAction::doubleLength ? i : bar];
      continue;
    }
    if (slots[i] < 0) continue;
    undo->savePhrase(slots[i]);
    if (sources[i] >= 0) p.phrases[slots[i]] = p.phrases[sources[i]];
    else {
      phraseClear(&p.phrases[slots[i]]);
      if (action == GrooveboxAction::offBar) p.phrases[slots[i]].rows[0].note = NOTE_OFF;
    }
    auto& b = p.chains[target].rows[first + i];
    b.phrase = slots[i];
    if (!clone) b.transpose = sources[i] < 0 ? 0 :
      p.chains[c].rows[action == GrooveboxAction::doubleLength ? i : bar].transpose;
  }
  p.song[row][col] = target;
  changed();
  return true;
}

static PhraseRow* writableStep(bool recordUndo = true) {
  Project* p = &chipnomadState->project;
  if (p->viewMode != ViewMode::classic || track >= p->tracksCount) {
    screenMessage(MESSAGE_TIME_ERROR, "Use Classic for this track");
    return NULL;
  }
  int trackLength = grooveboxTrackLength(p, scene, track);
  if (trackLength && bar >= trackLength) {
    screenMessage(MESSAGE_TIME_ERROR, "Repeated bar: edit its source bar");
    return NULL;
  }
  if (grooveboxCellShared(p, scene, track, bar)) {
    screenMessage(MESSAGE_TIME_ERROR, "Shared: Tools independence or View");
    sharedEditPending = true;
    return NULL;
  }
  int chain = chainAt(p, scene, track);
  int phrase = grooveboxPhraseAt(p, scene, track, bar);
  if (phrase >= 0) {
    if (recordUndo) {
      undo = std::make_unique<GrooveboxUndo>(scene);
      undo->savePhrase(phrase);
    }
    return &p->phrases[phrase].rows[step];
  }
  int length = 0;
  if (chain >= 0)
    while (length < 16 && p->chains[chain].rows[length].phrase < PROJECT_MAX_PHRASES) ++length;
  if (bar != length || (chain >= 0 && p->chains[chain].rows[bar].phrase != EMPTY_VALUE_16)) {
    screenMessage(MESSAGE_TIME_ERROR, "Complex extension: Tools / View");
    return NULL;
  }
  GrooveboxReferences refs(p);
  if (chain < 0) {
    if (p->song[scene][track] != EMPTY_VALUE_16) {
      screenMessage(MESSAGE_TIME_ERROR, "Invalid chain: use Classic");
      return NULL;
    }
    chain = allocateSlot(refs.freeChains, PROJECT_MAX_CHAINS, (track + 1) * 16, 16);
  }
  phrase = allocateSlot(refs.freePhrases, PROJECT_MAX_PHRASES, track * 128, 128);
  if (chain < 0 || phrase < 0) {
    screenMessage(MESSAGE_TIME_ERROR, "No free chain or phrase");
    return NULL;
  }
  undo = std::make_unique<GrooveboxUndo>(scene);
  undo->saveChain(chain);
  undo->savePhrase(phrase);
  if (p->song[scene][track] == EMPTY_VALUE_16) chainClear(&p->chains[chain]);
  phraseClear(&p->phrases[phrase]);
  p->song[scene][track] = chain;
  p->chains[chain].rows[bar].phrase = phrase;
  p->chains[chain].rows[bar].transpose = 0;
  projectModified = 1;
  chipnomadQueueProjectRefresh(chipnomadState);
  return &p->phrases[phrase].rows[step];
}

// Text detail layout and spatial field navigation adapted from study/groovebox.
static const int detailFields[4][3] = {{0, 3, 4}, {1, 5, 6}, {2, 7, 8}, {9, -1, -1}};
static const int detailX[] = {8, 8, 8, 18, 23, 18, 23, 18, 23, 8};
static const int detailY[] = {13, 14, 15, 13, 13, 14, 14, 15, 15, 16};

static void moveField(int keys) {
  int row = detailY[field] - 13;
  int col = field < 3 || field == 9 ? 0 : field % 2 ? 1 : 2;
  int nextRow = row, nextCol = col;
  if (keys == keyLeft && col > 0) --nextCol;
  if (keys == keyRight && col < 2) ++nextCol;
  if (keys == keyUp && row > 0) --nextRow;
  if (keys == keyDown && row < 3) ++nextRow;
  if (detailFields[nextRow][nextCol] >= 0) field = detailFields[nextRow][nextCol];
}

static void drawDetail() {
  Project* p = &chipnomadState->project;
  int sourceBar = displayBar(track);
  bool repeated = sourceBar >= 0 && sourceBar != bar;
  int phrase = grooveboxPhraseAt(p, scene, track, sourceBar);
  PhraseRow empty = {};
  empty.note = empty.instrument = EMPTY_VALUE_8;
  empty.volume = EMPTY_VALUE_16;
  for (auto& fx : empty.fx) fx.command = FX_ID_EMPTY;
  const PhraseRow* row = phrase < 0 ? &empty : &p->phrases[phrase].rows[step];
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 12, "----------------------------------------");
  const ColorScheme& cs = appSettings.colorScheme;
  const char* labels[] = {"NOTE", "INST", "VOL", "TR"};
  for (int i = 0; i < 4; ++i) {
    int index = i == 3 ? 9 : i;
    fieldDrawLabel(3, 13 + i, labels[i], !headerField && field == index ? CellState::focus : CellState::normal);
  }
  gfxSetFgColor(repeated || row->note == EMPTY_VALUE_8 ? cs.textEmpty :
    fieldNameColor(!headerField && field == 0 ? CellState::focus : CellState::normal));
  gfxPrint(8, 13, noteName(p, row->note));
  gfxSetFgColor(repeated || row->instrument == EMPTY_VALUE_8 ? cs.textEmpty :
    fieldValueColor(!headerField && field == 1 ? CellState::focus : CellState::normal));
  gfxPrint(8, 14, byteToHexOrEmpty(row->instrument));
  gfxSetFgColor(repeated || row->volume == EMPTY_VALUE_16 ? cs.textEmpty :
    fieldValueColor(!headerField && field == 2 ? CellState::focus : CellState::normal));
  gfxPrint(8, 15, volumeToHexOrEmpty(row->volume));
  for (int i = 0; i < 3; ++i) {
    int commandField = 3 + i * 2;
    bool commandFocus = !headerField && field == commandField;
    bool valueFocus = !headerField && field == commandField + 1;
    fieldDrawLabel(13, 13 + i, i == 0 ? "FX1" : i == 1 ? "FX2" : "FX3",
      commandFocus || valueFocus ? CellState::focus : CellState::normal);
    bool emptyFX = row->fx[i].command == FX_ID_EMPTY;
    gfxSetFgColor(repeated || emptyFX ? cs.textEmpty :
      row->fx[i].command >= fxTotalCount ? cs.warning :
      fieldNameColor(commandFocus ? CellState::focus : CellState::normal));
    gfxPrintf(18, 13 + i, "%.3s", row->fx[i].command < fxTotalCount ?
      fxNames[row->fx[i].command].name : "---");
    gfxSetFgColor(repeated || (emptyFX && row->fx[i].value == 0) ? cs.textEmpty :
      fieldValueColor(valueFocus ? CellState::focus : CellState::normal));
    gfxPrintf(23, 13 + i, "%02X", row->fx[i].value);
  }
  int chain = chainAt(p, scene, track);
  gfxSetFgColor(repeated || sourceBar < 0 ? cs.textEmpty :
    fieldValueColor(!headerField && field == 9 ? CellState::focus : CellState::normal));
  gfxPrint(8, 16, sourceBar < 0 ? "--" : byteToHex(p->chains[chain].rows[sourceBar].transpose));
  gfxSetFgColor(cs.textInfo);
  if (sourceBar >= 0) gfxPrintf(11, 16, "(%+d pitch-table steps)", (int8_t)p->chains[chain].rows[sourceBar].transpose);
  else gfxPrint(11, 16, "(no active bar)");
  if (sourceBar >= 0)
    gfxPrintf(0, 17, "BAR %02X%s", sourceBar + 1, repeated ? "  REPEAT (read only)" : "");
  else gfxPrint(0, 17, "No active bars in this track");
  uint8_t instrument = sourceBar < 0 ? EMPTY_VALUE_8 : lookupInstrument(p, scene, sourceBar, step, track);
  if (instrument != EMPTY_VALUE_8)
    gfxPrintf(0, 18, "INST %02X  %.29s", instrument, p->instruments[instrument].name);
  const auto& playback = chipnomadGetPlaybackStatus(chipnomadState)->tracks[track];
  if (playback.mode != PlaybackMode::stopped)
    gfxPrintf(0, 19, "PLAY SC %02X  BAR %02X  STEP %02X", playback.songRow, playback.chainRow + 1, playback.phraseRow);
  else gfxPrint(0, 19, "STOPPED");
  if (!headerField)
    gfxCursor(detailX[field], detailY[field], field == 0 ||
      (field >= 3 && field < 9 && field % 2) ? 3 : 2);
}

static void redraw(bool refreshLocks = true) {
  if (editingFX) { fxEditRedraw(); return; }
  if (chipnomadState->project.viewMode != ViewMode::classic) {
    screenViewSettings.fullRedraw();
    return;
  }
  static int chainUses[PROJECT_MAX_CHAINS], phraseUses[PROJECT_MAX_PHRASES];
  Project* p = &chipnomadState->project;
  if (refreshLocks) {
    GrooveboxReferences refs(p);
    std::copy_n(refs.chains, PROJECT_MAX_CHAINS, chainUses);
    std::copy_n(refs.phrases, PROJECT_MAX_PHRASES, phraseUses);
  }
  gfxSetBgColor(appSettings.colorScheme.background);
  gfxClear();
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "PATTERN");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrintf(10, 0, "%02X", scene);
  {
    fieldDrawLabel(13, 0, "BAR", headerField == 2 ? CellState::focus : CellState::normal);
    gfxSetFgColor(fieldValueColor(headerField == 2 ? CellState::focus : CellState::normal));
    gfxPrintf(17, 0, "%02X", bar + 1);
    fieldDrawLabel(20, 0, "LENGTH", headerField == 1 ? CellState::focus : CellState::normal);
    gfxSetFgColor(fieldValueColor(headerField == 1 ? CellState::focus : CellState::normal));
    gfxPrintf(27, 0, "%02X", grooveboxTrackLength(p, scene, track));
    fieldDrawLabel(32, 0, "TOOLS", headerField == 3 ? CellState::focus : CellState::normal);
  }
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  const PlaybackStatus* status = chipnomadGetPlaybackStatus(chipnomadState);
  int playingSteps[PROJECT_MAX_TRACKS];
  uint16_t headerSteps = 0;
  for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) {
    const auto& playback = status->tracks[t];
    int sourceBar = displayBar(t);
    playingSteps[t] = status->isPlaying && sourceBar >= 0 &&
      playback.mode != PlaybackMode::stopped && playback.songRow == scene &&
      playback.chainRow == sourceBar && playback.phraseRow < 16 ? playback.phraseRow : -1;
    if (playingSteps[t] >= 0) headerSteps |= 1u << playingSteps[t];
  }
  for (int r = 0; r < 16; ++r) {
    gfxSetFgColor(headerSteps & (1u << r) ? appSettings.colorScheme.playMarkers :
      r % 4 == 0 ? appSettings.colorScheme.textTitles : appSettings.colorScheme.textInfo);
    gfxPrintf(2 + r * 2, 2, "%X", r);
  }
  for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) {
    int sourceBar = displayBar(t);
    bool repeated = sourceBar >= 0 && sourceBar != bar;
    int phrase = grooveboxPhraseAt(p, scene, t, sourceBar);
    int c = chainAt(p, scene, t);
    bool locked = t >= p->tracksCount || (c >= 0 && chainUses[c] > 1) ||
      (phrase >= 0 && phraseUses[phrase] > 1);
    gfxSetFgColor(locked ? appSettings.colorScheme.textEmpty :
      t == track ? appSettings.colorScheme.textValue : appSettings.colorScheme.textLabel);
    gfxPrintf(0, 3 + t, "%02X", grooveboxTrackLength(p, scene, t));
    for (int r = 0; r < 16; ++r) {
      uint8_t note = phrase < 0 ? EMPTY_VALUE_8 : p->phrases[phrase].rows[r].note;
      bool playing = playingSteps[t] == r;
      const PhraseRow* event = phrase < 0 ? nullptr : &p->phrases[phrase].rows[r];
      bool data = event && (event->instrument != EMPTY_VALUE_8 || event->volume != EMPTY_VALUE_16);
      if (event)
        for (const auto& fx : event->fx) data |= fx.command != FX_ID_EMPTY || fx.value != 0;
      char label = phrase < 0 ? ' ' : note == NOTE_OFF ? '-' :
        note != EMPTY_VALUE_8 ? 'o' : data ? '+' : '.';
      if (playing) gfxSetFgColor(appSettings.colorScheme.playMarkers);
      else if (repeated) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      else if (phrase >= 0 && r % 4 == 0) gfxSetFgColor(appSettings.colorScheme.textTitles);
      else if (locked) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      else setCellColor(CellState::normal, phrase < 0 || (note == EMPTY_VALUE_8 && !data), note != EMPTY_VALUE_8 || data);
      gfxPrintf(2 + r * 2, 3 + t, "%c", label);
      gfxSetFgColor(appSettings.colorScheme.cursor);
      gfxPrintf(3 + r * 2, 3 + t, "%c", t == track && r == step ? '<' : ' ');
    }
  }
  int phrase = grooveboxPhraseAt(p, scene, track, bar);
  int chain = chainAt(p, scene, track);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  int links = chain >= 0 ? chainUses[chain] : 0;
  if (phrase >= 0) links = std::max(links, phraseUses[phrase]);
  if (links > 1) {
    gfxSetFgColor(appSettings.colorScheme.warning);
    gfxPrintf(0, 11, "T%d STEP %02X LINK x%d", track + 1, step, links);
  }
  else gfxPrintf(0, 11, "T%d STEP %02X", track + 1, step);
  drawDetail();
  if (headerField) {
    gfxCursor(headerField == 1 ? 27 : 17, 0, 2);
  }
}

static void init() {
  scene = track = bar = step = field = headerField = 0;
  editingFX = sharedEditPending = barEditPending = returningFromTools = sceneCopied = false;
  lastNote = 48; lastInstrument = 0; lastVolume = PHRASE_VOLUME_MAX;
  undo.reset();
  for (auto& row : lastIndicators) std::fill_n(row, 8, -1);
}
static void setup(int input) {
  if (chipnomadState->project.viewMode != ViewMode::classic) {
    grooveboxEditorActive = false;
    screenMessage(MESSAGE_TIME_ERROR, "Convert to Classic first");
    screenViewSettings.setup(0);
    return;
  }
  scene = input >= 0 && input < PROJECT_MAX_LENGTH ? input : *pSongRow;
  if (bar != *pChainRow && *pChainRow >= 0 && *pChainRow < 16) bar = *pChainRow;
  track = *pSongTrack;
  if (track < 0 || track >= chipnomadState->project.tracksCount) track = 0;
  editingFX = false;
  sharedEditPending = false;
  barEditPending = false;
  if (!returningFromTools) {
    undo.reset(); sceneCopied = false; headerField = 0;
    if (input >= 0) grooveboxPrepareScene(scene);
    bar = std::min(bar, sceneLength() - 1);
  }
  returningFromTools = false;
  redraw();
}
static void syncPosition() { *pSongRow = scene; *pSongTrack = track; *pChainRow = bar; }

uint8_t grooveboxMidiInstrument(void) {
  return lookupInstrument(&chipnomadState->project, scene, bar, step, track);
}
static void closeTools() {
  returningFromTools = true;
  syncPosition();
  screenSetup(&screenGroovebox, scene);
}
static void pasteStep(int col) {
  if (!canPastePhrase(col)) {
    screenMessage(MESSAGE_TIME_ERROR, "No compatible Phrase clipboard");
    return;
  }
  PhraseRow* row = writableStep();
  if (row) {
    int phrase = grooveboxPhraseAt(&chipnomadState->project, scene, track, bar);
    if (pastePhrase(phrase, col, step) > 0) {
      changed();
      screenMessage(MESSAGE_TIME, "Pasted Phrase clipboard");
    }
  }
}
static void selectTool(int value) {
  if (value == 105) {
    int phrase = grooveboxPhraseAt(&chipnomadState->project, scene, track, bar);
    if (phrase < 0) screenMessage(MESSAGE_TIME_ERROR, "No step to copy");
    else {
      copyPhrase(phrase, 0, step, 8, step, 0);
      screenMessage(MESSAGE_TIME, "Copied step to Phrase clipboard");
    }
    closeTools(); return;
  }
  if (value == 106) {
    pasteStep(0);
    bool blocked = sharedEditPending;
    closeTools();
    sharedEditPending = blocked;
    return;
  }
  if (value == 100) {
    if (track >= chipnomadState->project.tracksCount) {
      screenMessage(MESSAGE_TIME_ERROR, "Track unavailable: open Classic");
      closeTools();
      return;
    }
    int sourceBar = displayBar(track);
    uint8_t instrument = sourceBar < 0 ? EMPTY_VALUE_8 :
      lookupInstrument(&chipnomadState->project, scene, sourceBar, step, track);
    undo.reset(); sceneCopied = false;
    screenSetup(&screenInstrument, instrument == EMPTY_VALUE_8 ? 0 : instrument);
    return;
  }
  if (value == 101) { undo.reset(); sceneCopied = false; screenSetup(&screenMixer, track); return; }
  if (value == 102) { syncPosition(); undo.reset(); sceneCopied = false; screenSetup(&screenViewSettings, 0); return; }
  if (value == 103 || value == 104) {
    chipnomadQueueClearLoopRange(chipnomadState);
    if (!chipnomadQueuePlaybackQueueLiveScene(chipnomadState, scene, value == 104))
      screenMessage(MESSAGE_TIME_ERROR, "Launch failed: invalid data/full queue");
    else screenMessage(MESSAGE_TIME, value == 104 ? "Scene queued at each PHRASE end" : "Scene queued at each CHAIN end");
    closeTools(); return;
  }
  auto action = static_cast<GrooveboxAction>(value);
  int length = grooveboxTrackLength(&chipnomadState->project, scene, track);
  if (grooveboxAction(action, scene, track)) {
    if (action == GrooveboxAction::emptyBar || action == GrooveboxAction::offBar ||
        action == GrooveboxAction::duplicateBar || action == GrooveboxAction::doubleLength) bar = length;
    if (action == GrooveboxAction::shorten)
      bar = std::min(bar, std::max(0, length - 2));
    if (action != GrooveboxAction::copyScene)
      screenMessage(MESSAGE_TIME, action == GrooveboxAction::undo ? "Last Groovebox action undone" : "Applied; undo available in Tools");
  }
  closeTools();
}
static void openTools() {
  static const SelectionItem tools[] = {
    {"Make track independent", (int)GrooveboxAction::makeIndependent, nullptr, 0, "Copies all bars incl. retained data. New IDs can change conditional FX. Undo available."},
    {"Append no-event bar", (int)GrooveboxAction::emptyBar, nullptr, 0, "16 rows with no events; held notes and FX tails may continue. NOT guaranteed silence."},
    {"Append OFF bar", (int)GrooveboxAction::offBar, nullptr, 0, "OFF on row 00, then no events. Releases the note; effect tails are retained."},
    {"Append copy of bar", (int)GrooveboxAction::duplicateBar, nullptr, 0, "Fresh phrase, preserving transpose. New phrase ID can affect conditional FX."},
    {"Double track length", (int)GrooveboxAction::doubleLength, nullptr, 0, "Append independent copies, up to 16 bars. No partial result if slots are full."},
    {"Remove last bar", (int)GrooveboxAction::shorten, nullptr, 0, "Changes playback length. Phrase data remains stored. Undo restores its reference."},
    {"Copy scene (linked)", (int)GrooveboxAction::copyScene, nullptr, 0, "Copies the eight chain references; allocates nothing and does not change IDs."},
    {"Paste into empty scene", (int)GrooveboxAction::pasteScene, nullptr, 0, "Linked scene copy, preserving sharing; never overwrites an occupied scene."},
    {"Undo last editor action", (int)GrooveboxAction::undo, nullptr, 0, "One bounded undo while in this editor. Leaving Groovebox clears this undo."},
    {"Open instrument", 100, nullptr, 0, "Edit the effective instrument at the selected step using the existing CCT screen."},
    {"Open mixer", 101, nullptr, 0, "Existing CCT mixer, sends and FX. Song data remains canonical."},
    {"View settings / change mode", 102, nullptr, 0, "Only an explicit mode change leaves Groovebox. Classic exposes shared or complex arrangements."},
    {"Queue scene at chain end", 103, nullptr, 0, "Explicit live launch of all tracks at their individual chain boundaries. START still starts/stops."},
    {"Queue scene at phrase end", 104, nullptr, 0, "Urgent live launch at each track's next phrase boundary, not a shared bar clock."},
    {"Copy selected step", 105, nullptr, 0, "Copy note, instrument, volume and all FX to the existing Phrase clipboard."},
    {"Paste Phrase clipboard", 106, nullptr, 0, "Paste the existing Phrase clipboard at the selected step. Shared data requires independence."},
  };
  selectionPopupSetup("GROOVEBOX TOOLS", tools, sizeof(tools) / sizeof(tools[0]), 0, selectTool, closeTools, true);
  screenSetup(&screenSelectionPopup, 0);
}
static void sharedEditChoice(int value) {
  if (value == 0 && grooveboxAction(GrooveboxAction::makeIndependent, scene, track))
    screenMessage(MESSAGE_TIME, "Independent: edit this step again");
  closeTools();
}
static void openSharedEditChoice() {
  static const SelectionItem choices[] = {
    {"Make track independent", 0, nullptr, 0, "Shared data cannot be changed here. Copy this track and its bars, then edit. New IDs can affect conditional FX."},
    {"Cancel", 1, nullptr, 0, "Keep all project references and content unchanged."},
  };
  selectionPopupSetup("SHARED GROOVEBOX DATA", choices, 2, 0, sharedEditChoice, closeTools, true);
  screenSetup(&screenSelectionPopup, 0);
}
static void previewStep(const PhraseRow& row) {
  const auto* status = chipnomadGetPlaybackStatus(chipnomadState);
  if (status->isPlaying && status->tracks[track].mode != PlaybackMode::phraseRow) return;
  PhraseRow preview = row;
  if (preview.note != EMPTY_VALUE_8 && preview.note != NOTE_OFF && preview.instrument == EMPTY_VALUE_8)
    preview.instrument = lookupInstrument(&chipnomadState->project, scene, bar, step, track);
  if (!chipnomadQueuePlaybackStartPhraseRow(chipnomadState, track, &preview))
    screenMessage(MESSAGE_TIME_ERROR, "Preview failed: audio queue full");
}
static uint8_t snapNote(uint8_t note, CellEditAction action) {
  Project* p = &chipnomadState->project;
  if (!p->scaleMode || !p->scaleApply || p->pitchTable.octaveSize != 12 ||
      !(p->scaleTracksMask & (1u << track))) return note;
  uint8_t instrument = lookupInstrument(p, scene, bar, step, track);
  if (instrument != EMPTY_VALUE_8 && p->instruments[instrument].type == InstrumentType::Sample &&
      sampleActsAsSliced(&p->instruments[instrument].chip.sample)) return note;
  uint16_t mask = p->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(p->scalePreset);
  bool up = action == CellEditAction::increase || action == CellEditAction::increaseBig;
  return up ? scaleSnapNoteUp(note, p->scaleRoot, mask, p->pitchTable.length) :
    scaleQuantizeNote(note, p->scaleRoot, mask, p->pitchTable.length);
}
static int input(int down, int keys, int taps) {
  if (chipnomadState->project.viewMode != ViewMode::classic)
    return screenViewSettings.onInput(down, keys, taps);
  if (editingFX) {
    PhraseRow* row = writableStep(false);
    if (!row) { editingFX = false; redraw(); return 1; }
    int result = fxEditInput(keys, taps, &row->fx[(field - 3) / 2], &lastFX);
    if (result) {
      projectModified = 1;
      chipnomadQueueProjectRefresh(chipnomadState);
      editingFX = false;
      previewStep(*row);
      redraw();
    }
    return 1;
  }
  if (!down && keys == 0 && sharedEditPending) {
    sharedEditPending = false;
    openSharedEditChoice();
    return 1;
  }
  if (!down && keys == 0 && barEditPending) {
    barEditPending = false;
    bar = (bar + 1) % sceneLength();
    syncPosition(); redraw(); return 1;
  }
  if (!down) return 0;
  barEditPending = false;
  if (keys == keyOpt) return 1;
  if (keys == (keyShift | keyEdit)) {
    if (field == 9) screenMessage(MESSAGE_TIME_ERROR, "Transpose is not a Phrase clipboard field");
    else pasteStep(field);
    redraw();
    return 1;
  }
  if (keys == keyPlay || keys == (keyShift | keyPlay)) { syncPosition(); return 0; }
  if (keys == (keyEdit | keyPlay)) {
    chipnomadQueueClearLoopRange(chipnomadState);
    int accepted = chipnomadQueuePlaybackQueueLiveChain(chipnomadState, track,
        grooveboxTrackLength(&chipnomadState->project, scene, track) ? scene : -1, taps >= 2);
    if (!accepted) screenMessage(MESSAGE_TIME_ERROR, "Launch failed: invalid data/full queue");
    else screenMessage(MESSAGE_TIME, taps >= 2 ? "Queued at each track's PHRASE end" : "Queued at each track's CHAIN end");
    return 1;
  }
  if (keys == (keyOpt | keyShift)) { audioManager.toggleTrackMute(track); redraw(false); return 1; }
  if (keys == (keyOpt | keyPlay)) { audioManager.toggleTrackSolo(track); redraw(false); return 1; }
  if (keys == (keyShift | keyLeft)) {
    syncPosition(); undo.reset(); sceneCopied = false;
    screenSetup(&screenSong, -1); return 1;
  }
  if (keys == (keyShift | keyRight)) {
    syncPosition(); selectTool(100); return 1;
  }
  if (keys == (keyShift | keyUp)) {
    if (!headerField) { headerField = 1; redraw(); return 1; }
    syncPosition(); undo.reset(); sceneCopied = false;
    screenSetup(&screenGroove, 0); return 1;
  }
  if (keys == (keyShift | keyDown)) {
    headerField = 0; redraw(); return 1;
  }
  if (headerField &&
      (keys == keyLeft || keys == keyRight || keys == keyUp || keys == keyDown)) {
    if (keys == keyLeft) headerField = headerField == 3 ? 1 : 2;
    if (keys == keyRight) headerField = headerField == 2 ? 1 : 3;
    redraw(); return 1;
  }
  if (headerField == 3 && (keys & keyEdit)) {
    if (keys == keyEdit) { headerField = 0; openTools(); }
    return 1;
  }
  if (headerField && (keys & keyEdit)) {
    int length = grooveboxTrackLength(&chipnomadState->project, scene, track);
    if (headerField == 2) {
      if (keys == keyEdit) { barEditPending = true; return 1; }
      if (keys == (keyEdit | keyLeft) || keys == (keyEdit | keyDown)) bar = (bar + sceneLength() - 1) % sceneLength();
      else if (keys == (keyEdit | keyRight) || keys == (keyEdit | keyUp))
        bar = (bar + 1) % sceneLength();
      else if (keys == (keyEdit | keyOpt)) bar = 0;
      else return 0;
    } else {
      GrooveboxAction action;
      if (keys == keyEdit || keys == (keyEdit | keyRight)) action = GrooveboxAction::emptyBar;
      else if (keys == (keyEdit | keyLeft)) {
        if (length <= 1) { screenMessage(MESSAGE_TIME_ERROR, "Pattern at min length"); return 1; }
        action = GrooveboxAction::shorten;
      } else if (keys == (keyEdit | keyUp)) action = GrooveboxAction::doubleLength;
      else if (keys == (keyEdit | keyDown)) action = GrooveboxAction::halveLength;
      else if (keys == (keyEdit | keyOpt)) action = GrooveboxAction::resetLength;
      else return 0;
      if (grooveboxAction(action, scene, track))
        bar = std::min(bar, sceneLength() - 1);
    }
    syncPosition(); redraw(); return 1;
  }
  if (keys == (keyOpt | keyLeft)) {
    if (step > 0) --step;
    else { step = 15; bar = (bar + sceneLength() - 1) % sceneLength(); }
  }
  else if (keys == (keyOpt | keyRight)) {
    if (step < 15) ++step;
    else { step = 0; bar = (bar + 1) % sceneLength(); }
  }
  else if (keys == (keyOpt | keyUp)) { if (track > 0) --track; }
  else if (keys == (keyOpt | keyDown)) { if (track < PROJECT_MAX_TRACKS - 1) ++track; }
  else if (keys == keyLeft || keys == keyRight || keys == keyUp || keys == keyDown)
    moveField(keys);
  else if (keys & keyEdit) {
    CellEditAction action;
    if (keys == keyEdit) action = taps == 2 ? CellEditAction::doubleTap : CellEditAction::tap;
    else if (keys == (keyEdit | keyRight)) action = CellEditAction::increase;
    else if (keys == (keyEdit | keyLeft)) action = CellEditAction::decrease;
    else if (keys == (keyEdit | keyUp)) action = CellEditAction::increaseBig;
    else if (keys == (keyEdit | keyDown)) action = CellEditAction::decreaseBig;
    else if (keys == (keyEdit | keyOpt)) action = CellEditAction::clear;
    else return 0;
    PhraseRow* row = writableStep();
    if (!row) return 1;
    int handled = 0;
    if (field == 0) {
      if (action == CellEditAction::clear && row->note == EMPTY_VALUE_8) {
        row->note = NOTE_OFF;
        row->instrument = EMPTY_VALUE_8;
        row->volume = EMPTY_VALUE_16;
        handled = 1;
      }
      else if (action == CellEditAction::clear) {
        handled = edit8withLimit(action, &row->note, &lastNote,
          chipnomadState->project.pitchTable.octaveSize, chipnomadState->project.pitchTable.length - 1);
        edit8withLimit(action, &row->instrument, &lastInstrument, 16, PROJECT_MAX_INSTRUMENTS - 1);
        edit16withLimit(action, &row->volume, &lastVolume, 16, PHRASE_VOLUME_MAX);
      }
      else if (action == CellEditAction::tap && row->note == EMPTY_VALUE_8) {
        row->note = lastNote;
        row->instrument = lastInstrument;
        row->volume = lastVolume;
        handled = 1;
      }
      else if (row->note != NOTE_OFF) {
        handled = edit8withLimit(action, &row->note, &lastNote,
          chipnomadState->project.pitchTable.octaveSize, chipnomadState->project.pitchTable.length - 1);
        if (handled) {
          if (row->instrument != EMPTY_VALUE_8) lastInstrument = row->instrument;
          if (row->volume != EMPTY_VALUE_16) lastVolume = row->volume;
        }
      }
    } else if (field == 1) {
      if (action == CellEditAction::doubleTap) {
        int instrument = findEmptyInstrument(&chipnomadState->project, 0);
        if (instrument == EMPTY_VALUE_8) screenMessage(MESSAGE_TIME_ERROR, "No free instrument");
        else { row->instrument = instrument; handled = 1; }
      } else handled = edit8withLimit(action, &row->instrument, &lastInstrument, 16, PROJECT_MAX_INSTRUMENTS - 1);
      if (handled && row->instrument != EMPTY_VALUE_8)
        screenMessage(0, "%s: %s", byteToHex(row->instrument), instrumentName(&chipnomadState->project, row->instrument));
    }
    else if (field == 2) handled = edit16withLimit(action, &row->volume, &lastVolume, 16, PHRASE_VOLUME_MAX);
    else if (field == 9) {
      int c = chainAt(&chipnomadState->project, scene, track);
      undo->saveChain(c);
      static uint8_t lastTranspose = 0;
      handled = edit8noLimit(action, &chipnomadState->project.chains[c].rows[bar].transpose,
        &lastTranspose, chipnomadState->project.pitchTable.octaveSize);
    } else if (field % 2 == 0) {
      auto& fx = row->fx[(field - 3) / 2];
      if (fx.command != FX_ID_EMPTY) {
        uint8_t inst = lookupInstrument(&chipnomadState->project, scene, bar, step, track);
        handled = editFXValue(action, &fx, &lastFX, 0, inst);
      } else screenMessage(MESSAGE_TIME_ERROR, "Choose FX command first: LEFT then EDIT");
    } else {
      uint8_t inst = lookupInstrument(&chipnomadState->project, scene, bar, step, track);
      auto& fx = row->fx[(field - 3) / 2];
      if (fx.command == FX_ID_EMPTY && lastFX.command == FX_ID_EMPTY &&
          (action == CellEditAction::tap || action == CellEditAction::doubleTap ||
           action == CellEditAction::increase || action == CellEditAction::decrease))
        action = CellEditAction::increaseBig;
      int result = editFX(action, &fx, &lastFX, 0, inst);
      if (result == 1) { editingFX = true; return 1; }
      handled = result == 2;
    }
    if (handled) {
      if (field == 0 && action != CellEditAction::clear)
        row->note = snapNote(row->note, action);
      projectModified = 1;
      chipnomadQueueProjectRefresh(chipnomadState);
      previewStep(*row);
    }
  } else { syncPosition(); return 0; }
  syncPosition(); redraw(); return 1;
}
static void fullRedraw() { redraw(); }
static void draw() {
  if (editingFX) return;
  if (chipnomadState->project.viewMode != ViewMode::classic) { screenViewSettings.draw(); return; }
  const auto* status = chipnomadGetPlaybackStatus(chipnomadState);
  bool update = false;
  for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) {
    const auto& s = status->tracks[t];
    int indicators[] = {s.songRow, s.chainRow, s.phraseRow, s.queue.songRow,
      (int)s.queue.mode, (int)s.mode, (int)s.queue.liveAction, audioManager.trackStates[t]};
    for (int i = 0; i < 8; ++i) {
      update |= lastIndicators[t][i] != indicators[i];
      lastIndicators[t][i] = indicators[i];
    }
  }
  if (update) redraw(false);
}
static ScreenPlaybackLevel playbackLevel() {
  if (chipnomadState->project.viewMode != ViewMode::classic)
    return screenViewSettings.getPlaybackLevel();
  syncPosition();
  return ScreenPlaybackLevel::song;
}
const AppScreen screenGroovebox = {init, setup, fullRedraw, draw, input, playbackLevel};

static void afterConversion(int) {
  grooveboxEditorActive = chipnomadState->project.viewMode == ViewMode::classic;
  screenViewSettings.setup(0);
}
static void conversionViewRedraw() { screenViewSettings.fullRedraw(); }
static void conversionViewDraw() { screenViewSettings.draw(); }
static int conversionViewInput(int down, int keys, int taps) {
  return screenViewSettings.onInput(down, keys, taps);
}
static ScreenPlaybackLevel conversionViewPlaybackLevel() { return screenViewSettings.getPlaybackLevel(); }
const AppScreen screenGrooveboxAfterConversion = {
  NULL, afterConversion, conversionViewRedraw, conversionViewDraw, conversionViewInput, conversionViewPlaybackLevel
};
