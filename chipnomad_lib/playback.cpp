#include "playback.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include <algorithm>
#include <climits>
#include "playback_internal.h"
#include "synth/sample_voice.h"

///////////////////////////////////////////////////////////////////////////////
//
// Common logic
//

static int moveToNextPhraseRow(PlaybackState* state, int trackIdx);

// ── Pattern-aware song cell access ───────────────────────────────────────────
// Resolves a song cell (chain or pattern) into its phrase / transpose /
// length, based on p->viewMode. The walker and reader use these so the rest
// of the engine stays view-agnostic.
//
//   classic     → cell is chain index; chain has 16 rows
//   simplified  → cell is pattern index; pattern has variable length (1..16)
//
// `chainRow` in PlaybackTrackState is reused as the bar index in simplified
// mode — both are 0..15 and use the same groove-advance logic.

uint16_t playbackResolvePhraseAt(Project* p, int songRow, int trackIdx, int chainRow) {
  if (songRow < 0 || songRow >= PROJECT_MAX_LENGTH) return EMPTY_VALUE_16;
  if (trackIdx < 0 || trackIdx >= PROJECT_MAX_TRACKS) return EMPTY_VALUE_16;
  uint16_t cell = p->song[songRow][trackIdx];
  if (cell == EMPTY_VALUE_16) return EMPTY_VALUE_16;

  if (p->viewMode == ViewMode::simplified) {
    if (cell >= PROJECT_MAX_PATTERNS) return EMPTY_VALUE_16;
    Pattern* pat = &p->patterns[cell];
    int len = pat->length;
    if (len < 1) len = 1;
    if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;
    if (chainRow < 0 || chainRow >= len) return EMPTY_VALUE_16;
    return pat->bars[chainRow].phrase;
  } else {
    if (cell >= PROJECT_MAX_CHAINS) return EMPTY_VALUE_16;
    if (chainRow < 0 || chainRow >= 16) return EMPTY_VALUE_16;
    return p->chains[cell].rows[chainRow].phrase;
  }
}

uint8_t playbackResolveTransposeAt(Project* p, int songRow, int trackIdx, int chainRow) {
  if (songRow < 0 || songRow >= PROJECT_MAX_LENGTH) return 0;
  if (trackIdx < 0 || trackIdx >= PROJECT_MAX_TRACKS) return 0;
  uint16_t cell = p->song[songRow][trackIdx];
  if (cell == EMPTY_VALUE_16) return 0;

  if (p->viewMode == ViewMode::simplified) {
    if (cell >= PROJECT_MAX_PATTERNS) return 0;
    Pattern* pat = &p->patterns[cell];
    int len = pat->length;
    if (len < 1) len = 1;
    if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;
    if (chainRow < 0 || chainRow >= len) return 0;
    // Transpose is preserved as a hidden field (decision #1) — still applied
    // at playback, just not editable in the simplified UI.
    return pat->bars[chainRow].transpose;
  } else {
    if (cell >= PROJECT_MAX_CHAINS) return 0;
    if (chainRow < 0 || chainRow >= 16) return 0;
    return p->chains[cell].rows[chainRow].transpose;
  }
}

int playbackResolveLengthAt(Project* p, int songRow, int trackIdx) {
  if (songRow < 0 || songRow >= PROJECT_MAX_LENGTH) return 16;
  if (trackIdx < 0 || trackIdx >= PROJECT_MAX_TRACKS) return 16;
  uint16_t cell = p->song[songRow][trackIdx];
  if (cell == EMPTY_VALUE_16) return 16;

  if (p->viewMode == ViewMode::simplified) {
    if (cell >= PROJECT_MAX_PATTERNS) return 16;
    int len = p->patterns[cell].length;
    if (len < 1) len = 1;
    if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;
    return len;
  }
  return 16;  // classic chains are always 16 rows
}

static const uint8_t speedNumerator[17] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 4, 8, 3, 5, 6, 7
};
static const uint8_t speedDenominator[17] = {
  7, 6, 5, 3, 32, 16, 8, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1
};

static uint32_t nextConditionRandom(PlaybackTrackState* track) {
  uint32_t x = track->conditionRandom ? track->conditionRandom : 0x6d2b79f5u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  track->conditionRandom = x;
  return x;
}

static int phraseRowConditionsPass(PlaybackTrackState* track, PhraseRow* row, int rowIdx) {
  uint32_t visit = ++track->conditionVisits[rowIdx & 15];
  for (int i = 0; i < 3; ++i) {
    FXId type = row->fx[i].command;
    uint8_t value = row->fx[i][1];
    if (type == fxPRO) {
      if (value > 100) value = 100;
      if (value == 0 || (value < 100 && nextConditionRandom(track) % 100 >= value)) return 0;
    } else if (type == fxMOD) {
      uint8_t a = value >> 4;
      uint8_t b = value & 0x0f;
      if (a == 0 || b < 2 || a > b || ((visit - 1) % b) + 1 != a) return 0;
    }
  }
  return 1;
}

static void resetTrackFXAuxState(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  memset(track->fxAuxState, 0, sizeof(track->fxAuxState));
}

static void resetTableFXAuxState(PlaybackTableState* tableState) {
  memset(tableState->fxAuxState, 0, sizeof(tableState->fxAuxState));
}

FXCell playbackRepeatFX(PlaybackRepeatState* repeat, FXCell cell, int advance) {
  if (cell.command == fxREP) {
    if (advance) {
      repeat->active = cell.value != 0 && repeat->last.command != FX_ID_EMPTY;
      repeat->increment = (int8_t)cell.value;
      repeat->limited = 0;
    }
  } else if (cell.command == fxRTO) {
    if (advance && repeat->active) {
      repeat->limited = 1;
      repeat->limit = cell.value;
    }
  } else if (cell.command != FX_ID_EMPTY) {
    if (advance) {
      repeat->active = 0;
      repeat->limited = 0;
      repeat->last = cell;
    }
    return cell;
  }
  if (!repeat->active) return {FX_ID_EMPTY, 0};
  if (advance) {
    int next = (int)repeat->last.value + repeat->increment;
    if (repeat->limited &&
        ((repeat->increment > 0 && next >= repeat->limit) ||
         (repeat->increment < 0 && next <= repeat->limit))) {
      next = repeat->limit;
      repeat->active = 0;
    }
    repeat->last.value = (uint8_t)next;
  }
  return repeat->last;
}

static PhraseRow resolvePhraseFX(PlaybackTrackState* track, const PhraseRow* row, int advance) {
  PhraseRow resolved = *row;
  for (int i = 0; i < 3; ++i)
    resolved.fx[i] = playbackRepeatFX(&track->repeat[i], row->fx[i], advance);
  return resolved;
}

static void progressEuclidean(PlaybackState* state, int trackIdx) {
  auto& track = state->tracks[trackIdx];
  auto& euclidean = track.euclidean;
  if (!euclidean.active) return;
  if (track.note.pitchBase == EMPTY_VALUE_8 || track.note.instrument == EMPTY_VALUE_8) {
    euclidean.active = 0;
    return;
  }
  euclidean.step = (euclidean.step + 1) % euclidean.length;
  if ((euclidean.step * euclidean.pulses) % euclidean.length < euclidean.pulses)
    playbackRetriggerNote(state, trackIdx);
}

static void resetNoteFX(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  track->note.nativeFM = {};
  track->note.nativeFMCurrent = {};
  track->note.nativeFMRemaining = {};
  for (int i = 0; i < fxTotalCount; i++) {
    track->note.fx[i].isOn = 0;
    track->note.fx[i].counter = 0;
    track->note.fx[i].acc = 0;
    track->note.fx[i].tableSource = 0;
  }
}

static void resetSequencerFX(PlaybackTrackState* track) {
  memset(track->repeat, 0, sizeof(track->repeat));
  for (auto& repeat : track->repeat) repeat.last.command = FX_ID_EMPTY;
  track->euclidean = {};
  for (auto* table : {&track->note.instrumentTable, &track->note.auxTable}) {
    memset(table->repeat, 0, sizeof(table->repeat));
    for (auto& repeat : table->repeat) repeat.last.command = FX_ID_EMPTY;
  }
}

void playbackClearTrackFX(PlaybackState* state, int trackIdx) {
  auto* track = &state->tracks[trackIdx];
  memset(track->note.fx, 0, sizeof(track->note.fx));
  resetSequencerFX(track);
}

static void resetInstrumentFX(PlaybackTrackState* track) {
  for (int i=fxMUV; i<=fxMLV; ++i) track->note.fx[i].isOn=0;
  for (int i=fxFPT; i<=fxFSU; ++i) track->note.fx[i].isOn=0;
  track->note.nativeFM = {};
  track->note.nativeFMCurrent = {};
  track->note.nativeFMRemaining = {};
  for (int i = fxBMD; i <= fxPRS; i++) track->note.fx[i].isOn = 0;
  for (int i = fxRSN; i <= fxTCL; i++) track->note.fx[i].isOn = 0;
  for (int i = fxASL; i <= fxATY; ++i) track->note.fx[i].isOn = 0;
}

static void resetTrack(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];

  // Set specific values that shouldn't be zero
  track->songRow = EMPTY_VALUE_16;
  track->chainRow = 0;
  track->phraseRow = 0;
  track->frameCounter = 0;
  track->midiHeld = track->midiPending = 0;
  track->grooveIdx = 0;
  track->grooveRow = 0;
  track->pendingGrooveIdx = 0;
  track->speedRatio = state->p->signedTrackSpeed ? 0 : 9;
  track->speedPhase = 0;
  track->slewTicks = 0;
  track->achchidGateTicks = 0;
  track->achchidGateCounter = 0;
  for (int i = 0; i < fxTotalCount; ++i) track->slewTarget[i] = -1;
  memset(track->slewCurrent, 0, sizeof(track->slewCurrent));
  memset(track->slewRemaining, 0, sizeof(track->slewRemaining));
  memset(track->conditionVisits, 0, sizeof(track->conditionVisits));
  track->conditionRandom = 0x6d2b79f5u ^ (uint32_t)(trackIdx + 1) * 0x9e3779b9u;
  track->conditionPhrase = EMPTY_VALUE_16;
  resetSequencerFX(track);

  track->note.pitchBase = EMPTY_VALUE_8;
  track->note.pitchFinal = EMPTY_VALUE_8;
  track->note.sequencePitch = 0;
  track->note.pitchOffset = 0;
  track->note.fineOffset = 0;
  track->note.instrument = EMPTY_VALUE_8;
  track->note.volume = 0;
  track->note.sequenceVolume = 0;
  track->note.noteTriggered = 0;
  track->note.noteReleased = 0;
  track->note.noteKilled = 0;
  track->note.accent = 0;
  track->note.volume1 = 0;
  track->note.layerGainActive = 0;
  track->note.volume2 = 0;
  track->note.volume3 = 0;
  track->chordVoiceCount = 1;
  memset(track->chordPitchBase, EMPTY_VALUE_8, sizeof(track->chordPitchBase));
  memset(track->chordPitchFinal, EMPTY_VALUE_8, sizeof(track->chordPitchFinal));

  track->note.instrument = EMPTY_VALUE_8;
  track->note.instrumentTable.tableIdx = EMPTY_VALUE_8;
  track->note.auxTable.tableIdx = EMPTY_VALUE_8;
  track->mode = PlaybackMode::stopped;
  track->queue.liveAction = LiveQueueAction::none;
  track->queue.liveReference = -1;
  track->sliceBypass = 0;
  track->rnsSeeded = 0;
  track->rnsSlice = 0;
  track->rnsLastValue = 0xFF;
  track->rnnSeeded = 0;
  track->rnnPickedNote = EMPTY_VALUE_8;
  track->rnnLastValue = 0xFF;

  resetNoteFX(state, trackIdx);

  resetTrackFXAuxState(state, trackIdx);
  resetTableFXAuxState(&track->note.instrumentTable);
  resetTableFXAuxState(&track->note.auxTable);

  // Clear cached phrase row
  memset(&track->currentPhraseRow, EMPTY_VALUE_8, sizeof(track->currentPhraseRow));

  resetTrackAY(state, trackIdx);
}

int playbackSequenceBypasses(const PlaybackTrackState* track, SequenceSource source) {
  if (!track || track->note.instrument == EMPTY_VALUE_8) return 0;
  for (const PlaybackModState& state : track->note.modulation) {
    const Modulation* mod = state.modulation;
    if (mod && mod->destination != 0 && mod->type == ModulationType::Sequence &&
        mod->p1 == static_cast<uint8_t>(source) &&
        mod->p2 == static_cast<uint8_t>(SequenceMode::bypass)) return 1;
  }
  return 0;
}

void tableInit(PlaybackState* state, int trackIdx, struct PlaybackTableState* table, int tableIdx, int row, int speed) {
  table->tableIdx = tableIdx;
  table->baseSpeed = speed;
  memset(table->heldFX, 0, sizeof(table->heldFX));
  if (tableIdx == EMPTY_VALUE_8) return;

  Project* p = state->p;

  resetTableFXAuxState(table);

  for (int i = 0; i < 4; i++) {
    table->counters[i] = 0;
    table->rows[i] = row;
    table->speed[i] = speed;

    // Check if row 15 has TIC effect for this column
    if (p->tables[tableIdx].rows[15].fx[i][0] == fxTIC) {
      table->speed[i] = p->tables[tableIdx].rows[15].fx[i][1];
    }

    tableReadFX(state, trackIdx, table, i);
  }
}

void hopToTableRow(PlaybackState* state, int trackIdx, PlaybackTableState* table, int tableRow) {
  for (int c = 0; c < 4; c++) {
    table->counters[c] = 0;
    table->rows[c] = tableRow;
    tableReadFX(state, trackIdx, table, c);
  }
}

void tableReadFX(PlaybackState* state, int trackIdx, struct PlaybackTableState* table, int fxIdx, const FXCell* resolved) {
  uint8_t tableIdx = table->tableIdx;
  if (tableIdx == EMPTY_VALUE_8) return;
  Project* p = state->p;

  int tableRow = table->rows[fxIdx];
  FXCell cell = resolved ? *resolved :
    playbackRepeatFX(&table->repeat[fxIdx], p->tables[tableIdx].rows[tableRow].fx[fxIdx]);
  table->currentFX[fxIdx] = cell;
  FXId fxType = cell.command;
  table->heldFX[fxIdx].isOn = 0;
  if (fxType == fxTBL) {
    tableInit(state, trackIdx, &state->tracks[trackIdx].note.instrumentTable, cell.value, 0, 1);
  } else if (fxType == fxTBX) {
    tableInit(state, trackIdx, &state->tracks[trackIdx].note.auxTable, cell.value, 0, 1);
  } else {
    initFX(state, trackIdx, &cell, table, fxIdx);
    if (fxType == fxSTA) fxType = fxSST;
    if (fxType >= fxTotalCount || fxType == fxRET || fxType == fxDEL ||
        fxType == fxOFF || fxType == fxKIL || (fxType >= fxMC1 && fxType <= fxMC4)) return;
    table->heldCommand[fxIdx] = fxType;
    auto& held = table->heldFX[fxIdx];
    if (fxType >= fxOAR && fxType <= fxLEN) {
      held = {};
      held.isOn = 1;
      held.fxValue = cell.value;
    } else {
      held = state->tracks[trackIdx].note.fx[fxType];
    }
  }
}

static void restoreHeldTableFX(PlaybackState* state, int trackIdx, PlaybackTableState* table) {
  if (table->tableIdx == EMPTY_VALUE_8) return;
  for (int lane = 0; lane < 4; ++lane) {
    const auto& held = table->heldFX[lane];
    if (!held.isOn) continue;
    const FXId command = table->heldCommand[lane];
    if (command >= fxOAR && command <= fxLEN) {
      const FXCell cell{command, held.fxValue};
      initFX(state, trackIdx, &cell, table, lane);
    } else {
      state->tracks[trackIdx].note.fx[command] = held;
    }
  }
}

static void tableProgress(PlaybackState* state, int trackIdx, struct PlaybackTableState* table) {
  if (table->tableIdx == EMPTY_VALUE_8) return;
  Project* p = state->p;

  for (int i = 0; i < 4; i++) {
    table->counters[i]++;

    if (table->counters[i] >= table->speed[i]) {
      table->counters[i] = 0;
      uint8_t row = table->rows[i];

      // Check if any column has THO on current row
      int thoTarget = -1;
      for (int col = 0; col < 4; col++) {
        if (p->tables[table->tableIdx].rows[row].fx[col][0] == fxTHO) {
          thoTarget = p->tables[table->tableIdx].rows[row].fx[col][1] & 0xf;
          break;
        }
      }

      if (thoTarget >= 0 && thoTarget == row) {
        // THO pointing to same row - stay here
        tableReadFX(state, trackIdx, table, i);
        continue;
      }

      // Check HOP on current row pointing to same row
      FXCell current = p->tables[table->tableIdx].rows[row].fx[i];
      if (current.command == fxREP || current.command == fxRTO ||
          (current.command == FX_ID_EMPTY && table->repeat[i].active)) current = table->currentFX[i];
      FXId fxType = current.command;
      uint8_t fxValue = current.value;

      if (fxType == fxHOP && (fxValue & 0xf) == row) {
        if (fxValue & 0xf0) {
          // Loop counter
          table->fxAuxState[row][i]++;
          if (table->fxAuxState[row][i] <= ((fxValue & 0xf0) >> 4)) {
            tableReadFX(state, trackIdx, table, i);
            continue;
          }
        } else {
          // Unconditional hop to same row - stay here
          tableReadFX(state, trackIdx, table, i);
          continue;
        }
      }

      // Progress to next row
      table->rows[i] = (row + 1) & 15;

      if (table->rows[i] == 0) {
        // Reset all loop counters for this column
        for (int c = 0; c < 16; c++) {
          table->fxAuxState[c][i] = 0;
        }
      }

      row = table->rows[i];

      // Check if any column has THO on new row
      thoTarget = -1;
      for (int col = 0; col < 4; col++) {
        if (p->tables[table->tableIdx].rows[row].fx[col][0] == fxTHO) {
          thoTarget = p->tables[table->tableIdx].rows[row].fx[col][1] & 0xf;
          break;
        }
      }

      if (thoTarget >= 0) {
        // THO found - hop this column
        table->rows[i] = thoTarget;
        tableReadFX(state, trackIdx, table, i);
        continue;
      }

      // Check HOP on new row
      FXCell cell = playbackRepeatFX(&table->repeat[i], p->tables[table->tableIdx].rows[row].fx[i]);
      fxType = cell.command;
      fxValue = cell.value;

      if (fxType == fxTHO) {
        table->rows[i] = fxValue & 0xf;
        tableReadFX(state, trackIdx, table, i);
        continue;
      }

      if (fxType == fxHOP) {
        uint8_t hopTarget = fxValue & 0xf;
        if (fxValue & 0xf0) {
          // Loop counter
          table->fxAuxState[row][i]++;
          if (table->fxAuxState[row][i] <= ((fxValue & 0xf0) >> 4)) {
            // Reset "nested" loops when hopping back
            if (hopTarget < row) {
              for (int c = hopTarget; c < row; c++) {
                table->fxAuxState[c][i] = 0;
              }
            }
            table->rows[i] = hopTarget;
            tableReadFX(state, trackIdx, table, i);
            continue;
          }
        } else {
          // Unconditional hop
          table->rows[i] = hopTarget;
          tableReadFX(state, trackIdx, table, i);
          continue;
        }
      }

      // No hop - read FX from current row
      tableReadFX(state, trackIdx, table, i, &cell);
    }
  }
}

void handleNoteOff(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  Project* p = state->p;

  track->achchidGateTicks = 0;
  track->achchidGateCounter = 0;
  track->euclidean.active = 0;

  if (track->note.instrument == EMPTY_VALUE_8) return;

  InstrumentType instType = p->instruments[track->note.instrument].type;

  if (instType == InstrumentType::none) return;

  track->note.noteReleased = 1;

  int isModernVoice = instType == InstrumentType::Braids ||
    instType == InstrumentType::SoundFont || instType == InstrumentType::Plaits || instType == InstrumentType::PlaitsAlt || instType == InstrumentType::Sample ||
    instType == InstrumentType::DrumSynth || instType == InstrumentType::DigiSophie || instType == InstrumentType::MME || instType == InstrumentType::Sintered ||
    instType == InstrumentType::AudioIn;

  int hasVolumeADSR = 0;

  if (instType == InstrumentType::AY1) {
    // Handle legacy AY1 volume modulation
    playbackModNoteOff(&track->note.chip.ay.volumeModulation);
    hasVolumeADSR = 1;
  }

  // Common note off for all instruments - send note off to all modulations
  for (int i = 0; i < 4; i++) {
    if (track->note.modulation[i].modulation->destination == 1 && track->note.modulation[i].modulation->type == ModulationType::ADSR) {
      hasVolumeADSR = 1;
    }
    playbackModNoteOff(&track->note.modulation[i]);
  }

  if (isModernVoice) {
    track->note.pitchBase = EMPTY_VALUE_8;
    return;
  }

  if (!hasVolumeADSR) {
    // If no volume ADSR, turn off note immediately
    track->note.pitchBase = EMPTY_VALUE_8;
  }
}

static void initModulations(PlaybackTrackState* track, Project* p, uint8_t oldInstrument, uint8_t newInstrument) {
  if (newInstrument == EMPTY_VALUE_8) return;

  const Modulation* mods = p->instruments[newInstrument].modulation;

  int instrumentChanged = (oldInstrument != newInstrument);

  // Initialize all modulation slots
  for (int i = 0; i < 4; i++) {
    const Modulation* mod = &mods[i];

    // Free and structural LFOs, and rhythmic gates with matching triggers,
    // keep their phase across notes.
    int keepsPhase = mod->type == ModulationType::Audio || (modulationIsTickLFO(mod->type) &&
                      (mod->p2 == static_cast<uint8_t>(LFOTrigger::free) ||
                       mod->p2 == static_cast<uint8_t>(LFOTrigger::phrase) ||
                       mod->p2 == static_cast<uint8_t>(LFOTrigger::chain))) ||
                     (mod->type == ModulationType::FLFO && mod->p2 == static_cast<uint8_t>(LFOTrigger::free)) ||
                     (mod->type == ModulationType::Sequence &&
                      mod->p1 == static_cast<uint8_t>(SequenceSource::tranceGate) &&
                      (sequenceGateTrigger(mod) == SequenceGateTrigger::free ||
                       sequenceGateTrigger(mod) == SequenceGateTrigger::phrase ||
                       sequenceGateTrigger(mod) == SequenceGateTrigger::chain));

    // Initialize modulation if:
    // 1. Instrument changed (always reinit), OR
    // 2. Not an LFO that keeps its phase (retrig/hold/once always reinit)
    if (instrumentChanged || !keepsPhase) {
      playbackModInit(&track->note.modulation[i], (Modulation*)mod);
    }
    playbackModSetAYWavetables(&track->note.modulation[i], p->ayWavetables);
  }
}

static void restartStructuralLFOs(PlaybackState* state, int trackIdx, int enteredPhrase, int enteredChain) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  for (int i = 0; i < 4; ++i) {
    PlaybackModState* mod = &track->note.modulation[i];
    if (!mod->modulation) continue;
    if (mod->modulation->type == ModulationType::Sequence) {
      if (mod->modulation->p1 != static_cast<uint8_t>(SequenceSource::tranceGate)) continue;
      SequenceGateTrigger trigger = sequenceGateTrigger(mod->modulation);
      if ((enteredPhrase && trigger == SequenceGateTrigger::phrase) ||
          (enteredChain && trigger == SequenceGateTrigger::chain)) playbackModRestart(mod);
      continue;
    }
    uint8_t trigger = (uint8_t)clampInt16((int16_t)mod->modulation->p2 + mod->p2Offset, 0, 255);
    if ((enteredPhrase && trigger == static_cast<uint8_t>(LFOTrigger::phrase)) ||
        (enteredChain && trigger == static_cast<uint8_t>(LFOTrigger::chain))) {
      playbackModRestart(mod);
    }
  }

  PlaybackTableState* tables[] = {&track->note.instrumentTable, &track->note.auxTable};
  for (PlaybackTableState* table : tables) {
    if (table->tableIdx == EMPTY_VALUE_8) continue;
    TableRetriggerMode mode = state->p->tables[table->tableIdx].retriggerMode;
    if ((enteredPhrase && mode == TableRetriggerMode::phrase) ||
        (enteredChain && mode == TableRetriggerMode::chain)) {
      tableInit(state, trackIdx, table, table->tableIdx, 0, table->baseSpeed);
    }
  }
}

static int rowHasFX(const PhraseRow* row, FXId fx) {
  for (int i = 0; i < 3; ++i)
    if (row->fx[i].command == fx) return 1;
  return 0;
}

static PhraseRow* phraseRowAt(Project* p, int trackIdx, int songRow, int chainRow, int phraseRow) {
  if (songRow < 0 || songRow >= PROJECT_MAX_LENGTH || chainRow < 0 || chainRow >= 16 || phraseRow < 0 || phraseRow >= 16)
    return NULL;
  uint16_t chain = p->song[songRow][trackIdx];
  if (chain == EMPTY_VALUE_16) return NULL;
  uint16_t phrase = playbackResolvePhraseAt(p, songRow, trackIdx, chainRow);
  return phrase == EMPTY_VALUE_16 ? NULL : &p->phrases[phrase].rows[phraseRow];
}

static PhraseRow* nextPhraseRowForGate(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  Project* p = state->p;
  if (track->mode == PlaybackMode::phraseRow) return NULL;
  if (track->phraseRow < 15)
    return phraseRowAt(p, trackIdx, track->songRow, track->chainRow, track->phraseRow + 1);

  PhraseRow* row = phraseRowAt(p, trackIdx, track->songRow, track->chainRow + 1, 0);
  if (row) return row;
  if (track->mode == PlaybackMode::song)
    return phraseRowAt(p, trackIdx, track->songRow + 1, 0, 0);
  if (track->mode == PlaybackMode::live || (track->mode == PlaybackMode::chain && track->loop))
    return phraseRowAt(p, trackIdx, track->songRow, 0, 0);
  if (track->mode == PlaybackMode::phrase && track->loop)
    return phraseRowAt(p, trackIdx, track->songRow, track->chainRow, 0);
  return NULL;
}

static void scheduleAChChidGate(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  Project* p = state->p;
  PhraseRow* next = nextPhraseRowForGate(state, trackIdx);
  int slideNext = next && next->note != EMPTY_VALUE_8 && next->note != NOTE_OFF && rowHasFX(next, fxASL);
  if (slideNext) return;

  int ticks = p->grooves[track->grooveIdx].speed[track->grooveRow];
  if (ticks == EMPTY_VALUE_8 || ticks < 1) ticks = 1;
  track->achchidGateTicks = track->note.fx[fxATY].isOn ? ticks : ticks / 2;
  if (track->achchidGateTicks < 1) track->achchidGateTicks = 1;
  track->achchidGateCounter = 0;
}

// Pick a random note for RNN within the range encoded by the value's
// low nibble: 0 any note of the pitch table (the project scale, when
// active, snaps the pick afterwards), 1 +3 st, 2 +6, 3 +12, 4 -3,
// 5 -6, 6 -12, 7 +/-3, 8 +/-6, 9 +/-12. Ranges 1-9 center on the
// written note, so the random result stays musically anchored to it.
static int rnnPickNote(uint8_t note, uint8_t range, uint16_t pitchCount) {
  if (range == 0) {
    int limit = pitchCount < 256 ? pitchCount : 256;
    return limit ? (int)(utilsRandom() % limit) : (int)note;
  }
  static const int8_t spans[10] = {0, 3, 6, 12, -3, -6, -12, 3, 6, 12};
  if (range >= 10) return note;
  int span = spans[range];
  if (span == 0) return note;
  int magnitude = span < 0 ? -span : span;
  // Draw a signed offset in [-span, +span]; for one-sided ranges (1-6)
  // the sign is fixed by the span's sign, for two-sided ranges (7-9)
  // both directions are possible.
  int offset;
  if (range <= 6) {
    offset = (int)(utilsRandom() % (magnitude + 1)) * (span < 0 ? -1 : 1);
  } else {
    offset = (int)(utilsRandom() % (2 * magnitude + 1)) - magnitude;
  }
  int picked = (int)note + offset;
  if (picked < 0) picked = 0;
  if (picked > 0xFF) picked = 0xFF;
  return picked;
}

// True when the RNN value changed since the last pick (used by mode 2,
// pick and keep, to re-pick on a value change).
static int rnnLastValueChanged(PlaybackTrackState* track, uint8_t value) {
  return track->rnnLastValue != value;
}

void readPhraseRowDirect(PlaybackState* state, int trackIdx, PhraseRow* phraseRow, int skipDelCheck, int resolved) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  if (track->midiHeld) return;
  Project* p = state->p;
  PhraseRow effective;
  if (!resolved) {
    effective = skipDelCheck ? track->currentPhraseRow : resolvePhraseFX(track, phraseRow, 1);
    phraseRow = &effective;
  }
  if (!skipDelCheck) {
    track->currentPhraseRow = *phraseRow;
    if (phraseRow->note != EMPTY_VALUE_8 || phraseRow->instrument != EMPTY_VALUE_8 ||
        rowHasFX(phraseRow, fxEUC)) track->euclidean.active = 0;
    else progressEuclidean(state, trackIdx);
  }

  uint8_t note = phraseRow->note;
  uint8_t instrument = phraseRow->instrument;
  uint16_t volume = phraseRow->volume;
  uint8_t heldPitch = track->note.pitchBase;

  uint8_t auxTable = EMPTY_VALUE_8;
  uint8_t auxTableRow = EMPTY_VALUE_8;
  uint8_t instrumentTable = EMPTY_VALUE_8;
  uint8_t instrumentTableRow = EMPTY_VALUE_8;
  int hasAuxTableFX = 0;
  int hasInstrumentTableFX = 0;
  uint8_t chordValue = EMPTY_VALUE_8;


  if (instrument != EMPTY_VALUE_8 || note != EMPTY_VALUE_8) {
    track->achchidGateTicks = 0;
    track->achchidGateCounter = 0;
  }

  // Check for pending groove change
  if (track->pendingGrooveIdx != track->grooveIdx) {
    track->grooveIdx = track->pendingGrooveIdx;
    track->grooveRow = 0;
    track->frameCounter = 0;
  }

  // Pre-scan FX for special commands
  for (int i = 0; i < 3; i++) {
    FXId fxType = phraseRow->fx[i].command;
    uint8_t fxValue = phraseRow->fx[i][1];

    if (fxType == fxSCL) {
      // SCL XY: X selects a scale, Y selects C..B. Phrase-only global FX.
      // Inert in Note Lock mode: entry and chords follow the project scale.
      uint8_t preset = fxValue >> 4;
      uint8_t root = fxValue & 0x0f;
      if (!state->scaleFXCommandSeen && p->scaleMode == 0 && preset < scalePresetCount && root < 12) {
        state->scalePreset = (ScalePreset)preset;
        state->scaleRoot = root;
        state->scaleFXCommandSeen = 1;
      }
    } else if (fxType == fxCRD) {
      chordValue = fxValue;
    } else if (fxType == fxSPD && (p->signedTrackSpeed || fxValue <= 0x10)) {
      track->speedRatio = fxValue;
      track->speedPhase = 0;
    } else if (fxType == fxSLE) {
      track->slewTicks = fxValue;
    } else if (!skipDelCheck && (fxType == fxDEL && fxValue != 0)) {
      initFX(state, trackIdx, &phraseRow->fx[i], NULL, -1);
      return;
    } else if (fxType == fxTBL) {
      instrumentTable = fxValue;
      hasInstrumentTableFX = 1;
    } else if (fxType == fxTBX) {
      auxTable = fxValue;
      hasAuxTableFX = 1;
    } else if (fxType == fxTHO) {
      instrumentTableRow = fxValue & 0xf;
    } else if (fxType == fxTXH) {
      auxTableRow = fxValue & 0xf;
    }
  }

  if (hasInstrumentTableFX && instrumentTableRow == EMPTY_VALUE_8) instrumentTableRow = 0;
  if (hasAuxTableFX && auxTableRow == EMPTY_VALUE_8) auxTableRow = 0;

  // Instrument
  if (instrument != EMPTY_VALUE_8) {
    uint8_t oldInstrument = track->note.instrument;
    track->note.instrument = instrument;

    // Turn off all FX on a new instrument
    resetNoteFX(state, trackIdx);
    resetOffsets(state, trackIdx);

    // The historical instrument-triggered aux table stops on an instrument change.
    if (track->note.auxTable.tableIdx == EMPTY_VALUE_8 ||
        p->tables[track->note.auxTable.tableIdx].retriggerMode == TableRetriggerMode::instrument) {
      tableInit(state, trackIdx, &track->note.auxTable, EMPTY_VALUE_8, 0, 1);
    }

    // Initialize modulations
    initModulations(track, p, oldInstrument, instrument);

    // Setup instrument
    setupInstrument(state, trackIdx);
    if (instrumentTable == EMPTY_VALUE_8 &&
        (track->note.instrumentTable.tableIdx == EMPTY_VALUE_8 ||
         p->tables[track->note.instrumentTable.tableIdx].retriggerMode == TableRetriggerMode::instrument)) {
      instrumentTable = instrument;
      hasInstrumentTableFX = 1;
      if (instrumentTableRow == EMPTY_VALUE_8) {
        instrumentTableRow = 0;
      }
    }
  }

  if (instrument == EMPTY_VALUE_8 && (note != EMPTY_VALUE_8 && note != NOTE_OFF)) {
    restartFX(state, trackIdx);
  }

  // Instrument FX hold until the next trig, which restores instrument values.
  if (note != EMPTY_VALUE_8 && note != NOTE_OFF) resetInstrumentFX(track);

  // Init/hop tables
  if (hasInstrumentTableFX) {
    int tableSpeed = instrument != EMPTY_VALUE_8 ? p->instruments[instrument].tableSpeed : 1;
    tableInit(state, trackIdx, &track->note.instrumentTable, instrumentTable, instrumentTableRow, tableSpeed);
  } else if (instrumentTableRow != EMPTY_VALUE_8) {
    hopToTableRow(state, trackIdx, &track->note.instrumentTable, instrumentTableRow);
  }

  if (hasAuxTableFX) {
    tableInit(state, trackIdx, &track->note.auxTable, auxTable, auxTableRow, 1);
  } else if (auxTableRow != EMPTY_VALUE_8) {
    hopToTableRow(state, trackIdx, &track->note.auxTable, auxTableRow);
  }

  if (instrument != EMPTY_VALUE_8 || note != EMPTY_VALUE_8) {
    restoreHeldTableFX(state, trackIdx, &track->note.instrumentTable);
    restoreHeldTableFX(state, trackIdx, &track->note.auxTable);
  }

  // Read new FX
  for (int i = 0; i < 3; i++) {
    if (phraseRow->fx[i][0] != fxDEL && phraseRow->fx[i][0] != fxSLE &&
        phraseRow->fx[i][0] != fxSCL && phraseRow->fx[i][0] != fxCRD) {
      initFX(state, trackIdx, &phraseRow->fx[i], NULL, -1);
    }
  }

  // Note
  if (note != EMPTY_VALUE_8) {
    if (note == NOTE_OFF) {
      handleNoteOff(state, trackIdx);
    } else {
      // RNN replaces the row's note with a random one before any
      // transposition or scale handling. First digit of the value picks
      // the mode: 0 always random (re-picked every row), 1 random per
      // PLAY (held until the track restarts), 2 pick and keep (held
      // until the value changes or the track resets). Second digit
      // limits the randomization range around the written note:
      // 1 +3 st, 2 +6, 3 +12, 4 -3, 5 -6, 6 -12, 7 +/-3, 8 +/-6,
      // 9 +/-12. 0 picks any note of the pitch table; the project
      // scale, when active, snaps the pick afterwards.
      if (track->note.fx[fxRNN].isOn) {
        uint8_t rnnValue = track->note.fx[fxRNN].fxValue;
        uint8_t rnnMode = rnnValue >> 4;
        uint8_t rnnRange = rnnValue & 0x0F;
        if (rnnMode == 2 && rnnLastValueChanged(track, rnnValue)) {
          track->rnnSeeded = 0;
        }
        if (rnnMode == 0 || !track->rnnSeeded) {
          track->rnnPickedNote = (uint8_t)rnnPickNote(note, rnnRange, p->pitchTable.length);
          track->rnnSeeded = 1;
          track->rnnLastValue = rnnValue;
        }
        note = track->rnnPickedNote;
      }
      // RNS picks a random slice for the triggered note (sliced sample
      // instruments only). First digit of the value picks the mode:
      // 0 always random (re-picked every row), 1 random per PLAY (held
      // until the track restarts), 2 pick and keep (held until the
      // value changes or the track resets). The second digit is unused.
      // Picking happens here at note trigger, not in updateSampleVoices,
      // which runs at audio rate and would re-pick thousands of times
      // per second.
      if (track->note.fx[fxRNS].isOn) {
        InstrumentType rnsType = track->note.instrument == EMPTY_VALUE_8
          ? InstrumentType::none : p->instruments[track->note.instrument].type;
        const InstrumentSample* rnsSample = rnsType == InstrumentType::Sample
          ? &p->instruments[track->note.instrument].chip.sample : NULL;
        int rnsSliceCount = rnsType == InstrumentType::Sample &&
          sampleActsAsSliced(rnsSample)
          ? sampleDecodeSliceCount(rnsSample->slice) : 0;
        if (rnsSliceCount) {
          uint8_t rnsValue = track->note.fx[fxRNS].fxValue;
          uint8_t rnsMode = rnsValue >> 4;
          if (rnsMode == 2 && track->rnsLastValue != rnsValue) {
            track->rnsSeeded = 0;
          }
          if (rnsMode == 0 || !track->rnsSeeded) {
            track->rnsSlice = (uint8_t)(utilsRandom() % rnsSliceCount);
            track->rnsSeeded = 1;
            track->rnsLastValue = rnsValue;
          }
        }
      }
      track->note.pitchBase = note;
      // Accent is deliberately derived from this row, never from sticky volume.
      track->note.accent = volume == PHRASE_VOLUME_MAX;
      track->note.noteTriggered = 1;
      if (track->note.instrument != EMPTY_VALUE_8 &&
          p->instruments[track->note.instrument].type == InstrumentType::AChChid)
        scheduleAChChidGate(state, trackIdx);
    }
  }

  // Apply chain transpose (if the instrument allows it)
  if (note != EMPTY_VALUE_8 && note != NOTE_OFF && track->mode != PlaybackMode::phraseRow) {
    uint16_t chainIdx = p->song[track->songRow][trackIdx];
    if (chainIdx != EMPTY_VALUE_16) {
      int8_t transpose = playbackResolveTransposeAt(p, track->songRow, trackIdx, track->chainRow);
      if (p->instruments[track->note.instrument].transposeEnabled) {
        track->note.pitchBase += transpose;
      }
    }
  }

  InstrumentType type = track->note.instrument == EMPTY_VALUE_8 ? InstrumentType::none
    : p->instruments[track->note.instrument].type;
  const InstrumentSample* sample = type == InstrumentType::Sample
    ? &p->instruments[track->note.instrument].chip.sample : NULL;
  int slicedSample = sampleActsAsSliced(sample);

  // Keep phrases chromatic; only the pitch sent to the engine is quantized.
  // Sliced PCM samples map notes to windows, so they stay unquantized.
  // In Note Lock mode entry is already restricted to the scale, so playback
  // quantization of plain notes is bypassed (chords below still quantize).
  int noteLock = p->scaleMode != 0;
  if (note != EMPTY_VALUE_8 && note != NOTE_OFF && p->scaleApply && !noteLock && !slicedSample &&
      (p->scaleTracksMask & (1u << trackIdx)) && p->pitchTable.octaveSize == 12) {
    uint16_t mask = state->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(state->scalePreset);
    track->note.pitchBase = scaleQuantizeNote(track->note.pitchBase, state->scaleRoot, mask, p->pitchTable.length);
  }
  // RNN notes follow the project scale in Note Lock mode too: manual
  // entry is already scale-restricted there (which is why the quantizer
  // path above is bypassed), but random picks are not, so snap them.
  if (track->note.fx[fxRNN].isOn && noteLock && p->scaleApply && !slicedSample &&
      (p->scaleTracksMask & (1u << trackIdx)) && p->pitchTable.octaveSize == 12) {
    uint16_t mask = state->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(state->scalePreset);
    track->note.pitchBase = scaleQuantizeNote(track->note.pitchBase, state->scaleRoot, mask, p->pitchTable.length);
  }

  if (note != EMPTY_VALUE_8 && note != NOTE_OFF) {
    track->chordVoiceCount = 1;
    track->chordPitchBase[0] = track->note.pitchBase;
    bool ay = type == InstrumentType::AY1 || type == InstrumentType::AY2 || type == InstrumentType::AYSample;
    if (chordValue != EMPTY_VALUE_8 && !ay && p->pitchTable.octaveSize == 12) {
      track->chordVoiceCount = chordBuild(track->note.pitchBase, chordValue & 0x0f, chordValue >> 4,
                                          p->pitchTable.length, track->chordPitchBase);
      if (p->scaleApply && !slicedSample && (p->scaleTracksMask & (1u << trackIdx))) {
        uint16_t mask = state->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(state->scalePreset);
        for (int i = 0; i < track->chordVoiceCount; ++i)
          track->chordPitchBase[i] = scaleQuantizeNote(track->chordPitchBase[i], state->scaleRoot, mask, p->pitchTable.length);
      }
    }
  }

  if (note != EMPTY_VALUE_8 && note != NOTE_OFF) {
    track->note.sequencePitch = track->note.pitchBase;
    if (playbackSequenceBypasses(track, SequenceSource::pitch)) {
      int fallback = clampInt(state->p->pitchTable.octaveSize * 4, 0, state->p->pitchTable.length - 1);
      track->note.pitchBase = heldPitch != EMPTY_VALUE_8 ? heldPitch : fallback;
      track->chordVoiceCount = 1;
      track->chordPitchBase[0] = track->note.pitchBase;
    }
  }

  // Volume
  if (volume != EMPTY_VALUE_16) {
    track->note.sequenceVolume = (uint8_t)volume;
    if (!playbackSequenceBypasses(track, SequenceSource::volume)) track->note.volume = (uint8_t)volume;
  }
  for (const auto& fx : phraseRow->fx) {
    if (fx.command != fxEUC) continue;
    auto& euclidean = track->euclidean;
    euclidean.length = (fx.value & 0xf) + 1;
    euclidean.pulses = std::min<int>(fx.value >> 4, euclidean.length);
    euclidean.step = 0;
    euclidean.active = euclidean.pulses != 0 && track->note.pitchBase != EMPTY_VALUE_8 &&
      track->note.instrument != EMPTY_VALUE_8;
    if (euclidean.active && note == EMPTY_VALUE_8) playbackRetriggerNote(state, trackIdx);
  }
}

void readPhraseRow(PlaybackState* state, int trackIdx, int skipDelCheck) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  if (!skipDelCheck) track->frameCounter = 0;
  Project* p = state->p;

  // If using phrase row mode, use cached phrase row
  if (track->mode == PlaybackMode::phraseRow) {
    PhraseRow effective = skipDelCheck ? track->currentPhraseRow :
      resolvePhraseFX(track, &track->currentPhraseRow, 1);
    if (!skipDelCheck && !phraseRowConditionsPass(track, &effective, track->phraseRow)) {
      progressEuclidean(state, trackIdx);
      return;
    }
    readPhraseRowDirect(state, trackIdx, &effective, skipDelCheck, 1);
    return;
  }

  // If nothing is playing, skip it
  if (track->mode == PlaybackMode::stopped || track->songRow == EMPTY_VALUE_16) return;

  uint16_t chainIdx = p->song[track->songRow][trackIdx];
  if (chainIdx != EMPTY_VALUE_16) {
    uint16_t phraseIdx = playbackResolvePhraseAt(p, track->songRow, trackIdx, track->chainRow);
    if (phraseIdx != EMPTY_VALUE_16) {
      if (track->conditionPhrase != phraseIdx) {
        track->conditionPhrase = phraseIdx;
        memset(track->conditionVisits, 0, sizeof(track->conditionVisits));
      }
      int phraseRow = track->phraseRow;
      Phrase* phrase = &p->phrases[phraseIdx];
      PhraseRow* currentRow = &phrase->rows[phraseRow];

      PhraseRow effective = skipDelCheck ? track->currentPhraseRow : resolvePhraseFX(track, currentRow, 1);
      currentRow = &effective;
      if (!skipDelCheck && !phraseRowConditionsPass(track, currentRow, phraseRow)) {
        progressEuclidean(state, trackIdx);
        return;
      }

      // Check for SNG command in Song mode
      if (track->mode == PlaybackMode::song) {
        for (int i = 0; i < 3; i++) {
          if (currentRow->fx[i][0] == fxSNG && currentRow->fx[i][1] != 0) {
            int8_t offset = (int8_t)currentRow->fx[i][1];
            int newSongRow = track->songRow + offset;

            // Check if jump is negative and loop is disabled
            if (offset < 0 && !track->loop) {
              resetTrack(state, trackIdx);
              return;
            }

            // Validate target song position
            if (newSongRow >= 0 && newSongRow < PROJECT_MAX_LENGTH) {
              uint16_t targetChainIdx = p->song[newSongRow][trackIdx];
              if (targetChainIdx != EMPTY_VALUE_16) {
                uint16_t targetPhraseIdx = playbackResolvePhraseAt(p, newSongRow, trackIdx, 0);
                if (targetPhraseIdx != EMPTY_VALUE_16) {
                  // Bounce: SNG jump landing past the end of the selected song
                  // region stops the track instead of playing outside it
                  if (state->stopRange.enabled && state->stopRange.level == 0 &&
                      newSongRow > state->stopRange.endSongRow) {
                    resetTrack(state, trackIdx);
                    return;
                  }

                  // Valid target, perform jump and read from new position
                  track->songRow = newSongRow;
                  track->chainRow = 0;
                  track->phraseRow = 0;
                  resetTrackFXAuxState(state, trackIdx);
                  restartStructuralLFOs(state, trackIdx, 1, 1);
                  readPhraseRow(state, trackIdx, skipDelCheck);
                  return;
                }
              }
            }
            // Invalid target or out of bounds, ignore SNG command and continue normally
            break;
          }
        }
      }

      // Check for HOP command
      for (int i = 0; i < 3; i++) {
        if (currentRow->fx[i][0] == fxHOP) {
          uint8_t hopValue = currentRow->fx[i][1];

          // 0xFF = stop track
          if (hopValue == 0xFF) {
            resetTrack(state, trackIdx);
            return;
          }

          uint8_t targetRow = hopValue & 0x0F;
          uint8_t loopCount = (hopValue & 0xF0) >> 4;

          if (loopCount == 0) {
            // Unconditional jump to next phrase
            track->phraseRow = 15;
            if (moveToNextPhraseRow(state, trackIdx)) {
              return;
            }
            track->phraseRow = targetRow;
            resetTrackFXAuxState(state, trackIdx);
            readPhraseRow(state, trackIdx, skipDelCheck);
            return;
          } else {
            // Conditional jump with loop counter
            track->fxAuxState[phraseRow][i]++;
            if (track->fxAuxState[phraseRow][i] <= loopCount) {
              // Bounce: HOP target outside the selected phrase region stops the track
              if (state->stopRange.enabled && state->stopRange.level == 2 &&
                  track->songRow != EMPTY_VALUE_16 &&
                  (targetRow < state->stopRange.startPhraseRow || targetRow > state->stopRange.endPhraseRow)) {
                resetTrack(state, trackIdx);
                return;
              }

              // Reset nested loop counters when hopping backwards
              if (targetRow < phraseRow) {
                for (int c = targetRow; c < phraseRow; c++) {
                  track->fxAuxState[c][i] = 0;
                }
              }
              track->phraseRow = targetRow;
              effective = resolvePhraseFX(track, &phrase->rows[targetRow], 1);
              currentRow = &effective;
            }
          }
          break;
        }
      }

      readPhraseRowDirect(state, trackIdx, currentRow, skipDelCheck, 1);
    } else {
      // Safeguard for phrase in chain
      resetTrack(state, trackIdx);
    }
  } else if (state->stopRange.enabled && state->stopRange.level == 0 &&
             track->songRow < state->stopRange.endSongRow) {
    // Offline bounce: the track is waiting on an empty song cell for its
    // first chain later in the selected region - stay silent this frame
  } else {
    // Safeguard for chain in song
    resetTrack(state, trackIdx);
  }
}

void resetOffsets(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  track->note.pitchOffset = 0;
  track->note.fineOffset = 0;
  track->note.periodOffset = 0;
  track->note.volumeOffset = 0;

  // Reset modulation offsets
  for (int i = 0; i < 4; i++) {
    track->note.modulation[i].amountOffset = 0;
    track->note.modulation[i].p1Offset = 0;
    track->note.modulation[i].p2Offset = 0;
    track->note.modulation[i].p3Offset = 0;
    track->note.modulation[i].p4Offset = 0;
    track->note.modulation[i].p5Offset = 0;
  }

  // Dispatch to chip-specific offset reset based on instrument type
  if (track->note.instrument != EMPTY_VALUE_8) {
    InstrumentType instType = state->p->instruments[track->note.instrument].type;
    switch (instType) {
      case InstrumentType::AY1:
      case InstrumentType::AY2:
      case InstrumentType::AYSample:
        resetOffsetsAY(state, trackIdx);
        break;
      default:
        break;
    }
  }
}

static void resetLiveStickRate(PlaybackState* state) {
  memset(state->liveStickRate, 0, sizeof(state->liveStickRate));
}

void playbackUpdateLiveStickModulation(PlaybackState* state, const float axes[4], int enabled) {
  int playing = playbackIsPlaying(state);
  if (playing != state->liveStickWasPlaying) {
    resetLiveStickRate(state);
    state->liveStickWasPlaying = playing;
  }

  for (int axis = 0; axis < 4; ++axis) {
    float value = axes[axis];
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;
    state->liveStickAxes[axis] = value;
  }

  if (!enabled) {
    memset(state->liveStickAxes, 0, sizeof(state->liveStickAxes));
    resetLiveStickRate(state);
    return;
  }

  if (!playing) return;

  for (int instrument = 0; instrument < PROJECT_MAX_INSTRUMENTS; ++instrument) {
    for (int slot = 0; slot < 4; ++slot) {
      const Modulation* mod = &state->p->instruments[instrument].modulation[slot];
      if (mod->type != ModulationType::StickRate) {
        state->liveStickRate[instrument][slot] = 0;
        continue;
      }
      int axis = mod->p1 < static_cast<uint8_t>(StickAxis::totalCount) ? mod->p1 : 0;
      int ticks = mod->p2 ? mod->p2 : 1;
      float delta = state->liveStickAxes[axis] * mod->amount * 255.0f / ticks;
      int value = (int)state->liveStickRate[instrument][slot] + (int)roundf(delta);
      state->liveStickRate[instrument][slot] = (int16_t)clampInt(value, -32385, 32385);
    }
  }
}

int16_t playbackLiveStickOutput(const PlaybackState* state, uint8_t instrument,
                                int slot, const Modulation* mod) {
  int axis = mod->p1 < static_cast<uint8_t>(StickAxis::totalCount) ? mod->p1 : 0;
  if (mod->type == ModulationType::StickRate) return state->liveStickRate[instrument][slot];
  int value = (int)roundf(state->liveStickAxes[axis] * mod->amount * 255.0f);
  return (int16_t)clampInt(value, -32385, 32385);
}

int16_t playbackSequenceGateOutput(const Modulation* definition, uint32_t ticks) {
  if (!definition) return 0;
  if (definition->p1 != static_cast<uint8_t>(SequenceSource::tranceGate)) return 0;
  uint32_t step = (uint32_t)(ticks / tranceGateTicksPerStep(sequenceGateRate(definition)));
  uint8_t length = sequenceGateLength(definition);
  if (!tranceGatePatternOpen(sequenceGatePattern(definition), length, step)) return 0;
  int activeCount = 0, activeIndex = 0;
  for (int i = 0; i < length; ++i) {
    if (!tranceGatePatternOpen(sequenceGatePattern(definition), length, i)) continue;
    if ((uint32_t)i == step % length) activeIndex = activeCount;
    ++activeCount;
  }
  int accents = (sequenceGateAccents(definition) * activeCount + 8) / 16;
  return accents && ((activeIndex * accents) % activeCount < accents) ? 32385 : 16192;
}

static int16_t sequenceSourceOutput(const PlaybackState* state, PlaybackTrackState* track,
                                    PlaybackModState* mod) {
  const Modulation* definition = mod->modulation;
  if (!definition) return 0;
  int raw = 0;
  switch (static_cast<SequenceSource>(definition->p1)) {
    case SequenceSource::pitch: {
      int maxPitch = state->p->pitchTable.length - 1;
      if (maxPitch > 0) {
        int pitch = clampInt(track->note.sequencePitch + track->note.pitchOffset, 0, maxPitch);
        raw = (pitch * 2 * 32385 + maxPitch / 2) / maxPitch - 32385;
      }
      break;
    }
    case SequenceSource::volume: {
      int volume = clampInt(track->note.sequenceVolume + track->note.volumeOffset, 0, PHRASE_VOLUME_MAX);
      raw = (volume * 2 * 32385 + PHRASE_VOLUME_MAX / 2) / PHRASE_VOLUME_MAX - 32385;
      break;
    }
    case SequenceSource::tranceGate: {
      raw = playbackSequenceGateOutput(definition, mod->counter);
      ++mod->counter;
      break;
    }
    default:
      break;
  }
  int amount = clampInt((int)definition->amount + mod->amountOffset, -128, 127);
  return (int16_t)clampInt(raw * amount / 127, -32385, 32385);
}

static void processModulationsForTrack(PlaybackState* state, PlaybackTrackState* track, bool advanceClock = true) {
  if (track->note.instrument == EMPTY_VALUE_8) return;

  InstrumentType type = state->p->instruments[track->note.instrument].type;
  // Parameter destinations use the source's previous value. This makes
  // cross-modulation deterministic and avoids recursive evaluation.
  for (int source = 0; source < 4; ++source) {
    PlaybackModState* mod = &track->note.modulation[source];
    if (!mod->modulation || mod->modulation->type == ModulationType::Sequence) continue;
    int generic = instrumentGenericModDestination(type, mod->modulation->destination);
    int target, targetParameter;
    if (generic >= genericModFirstParameter && generic < genericModDestinationCount) {
      int parameter = generic - genericModFirstParameter;
      target = parameter / 4;
      targetParameter = parameter % 4;
    } else if (generic >= genericModFirstP5 && generic < genericModFirstInsert) {
      target = generic - genericModFirstP5;
      targetParameter = 4;
    } else continue;
    if (target == source || !track->note.modulation[target].modulation ||
        track->note.modulation[target].modulation->type == ModulationType::Sequence) continue;
    int16_t offset = playbackModScaleToRange(mod->outValue, 255);
    PlaybackModState* targetMod = &track->note.modulation[target];
    if (targetParameter == 0) targetMod->p1Offset += offset;
    else if (targetParameter == 1) targetMod->p2Offset += offset;
    else if (targetParameter == 2) targetMod->p3Offset += offset;
    else if (targetParameter == 3) targetMod->p4Offset += offset;
    else targetMod->p5Offset += offset;
  }

  for (int i = 0; i < 4; i++) {
    PlaybackModState* mod = &track->note.modulation[i];
    // Skip if modulation not initialized or destination == 0 (no destination)
    if (!mod->modulation || mod->modulation->destination == 0 ||
        mod->modulation->type == ModulationType::FLFO ||
        mod->modulation->type == ModulationType::Audio ||
        mod->modulation->type == ModulationType::Sequence) continue;
    if (advanceClock || !modulationIsTickLFO(mod->modulation->type)) playbackModNext(mod);
    if (modulationIsLiveStick(mod->modulation->type)) {
      mod->outValue = playbackLiveStickOutput(state, track->note.instrument, i, mod->modulation);
    }
  }

  // Sequence sources are sampled after tracker FX and ordinary modulators.
  // Their source/mode fields are deliberately not modulation destinations.
  for (int i = 0; i < 4; ++i) {
    PlaybackModState* mod = &track->note.modulation[i];
    if (!mod->modulation || mod->modulation->type != ModulationType::Sequence) continue;
    if (!modulationSequenceDestinationAllowed(&state->p->instruments[track->note.instrument], mod->modulation,
                                              mod->modulation->destination)) {
      mod->outValue = 0;
      continue;
    }
    if (advanceClock || mod->modulation->p1 != static_cast<uint8_t>(SequenceSource::tranceGate))
      mod->outValue = sequenceSourceOutput(state, track, mod);
  }
}

static void processModulations(PlaybackState* state, int trackIdx, bool advanceClock = true) {
  processModulationsForTrack(state, &state->tracks[trackIdx], advanceClock);
}

static void handleInstrument(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  Project* p = state->p;

  if (track->note.instrument == EMPTY_VALUE_8) return;
  if (track->note.pitchBase == EMPTY_VALUE_8) return;

  InstrumentType instType = p->instruments[track->note.instrument].type;
  switch (instType) {
  case InstrumentType::AY1:
    handleInstrumentAY1(state, trackIdx);
    break;
  case InstrumentType::AY2:
    handleInstrumentAY2(state, trackIdx);
    break;
  case InstrumentType::AYSample:
    handleInstrumentAYSample(state, trackIdx);
    break;
  case InstrumentType::Braids:
  case InstrumentType::Plaits:
  case InstrumentType::PlaitsAlt:
  case InstrumentType::SoundFont:
  case InstrumentType::Sample:
  case InstrumentType::SCWF:
  case InstrumentType::BYOWTBL:
  case InstrumentType::AChChid:
  case InstrumentType::DrumSynth:
  case InstrumentType::DigiSophie:
  case InstrumentType::MME:
  case InstrumentType::Sintered:
  case InstrumentType::AudioIn:
    break;
  case InstrumentType::none:
    break;
  }
}

static void nextFrame(PlaybackState* state, int trackIdx, int chipIdx, bool advanceClock = true) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  Project* p = state->p;

  // Is the channel playing?
  if (track->songRow == EMPTY_VALUE_16) {
    track->note.pitchFinal = EMPTY_VALUE_8;
    return;
  }

  resetOffsets(state, trackIdx);
  if (advanceClock) handleFX(state, trackIdx, chipIdx);
  if (advanceClock && track->achchidGateTicks) {
    if (track->achchidGateCounter >= track->achchidGateTicks) {
      handleNoteOff(state, trackIdx);
    } else {
      track->achchidGateCounter++;
    }
  }
  processModulations(state, trackIdx, advanceClock);
  handleInstrument(state, trackIdx);

  // Final pitch calculation
  if (track->note.pitchBase == EMPTY_VALUE_8) {
    track->note.pitchFinal = EMPTY_VALUE_8;
  } else {
    // Base pitch
    int16_t pitch = track->note.pitchBase;

    // Tables
    int tableIdx = track->note.instrumentTable.tableIdx;
    if (tableIdx != EMPTY_VALUE_8) {
      if (p->tables[tableIdx].rows[track->note.instrumentTable.rows[0]].pitchFlag) {
        pitch = p->tables[tableIdx].rows[track->note.instrumentTable.rows[0]].pitchOffset;
      } else {
        pitch += (int8_t)(p->tables[tableIdx].rows[track->note.instrumentTable.rows[0]].pitchOffset);
      }
    }

    tableIdx = track->note.auxTable.tableIdx;
    if (tableIdx != EMPTY_VALUE_8) {
      if (p->tables[tableIdx].rows[track->note.auxTable.rows[0]].pitchFlag) {
        pitch = p->tables[tableIdx].rows[track->note.auxTable.rows[0]].pitchOffset;
      } else {
        pitch += (int8_t)(p->tables[tableIdx].rows[track->note.auxTable.rows[0]].pitchOffset);
      }
    }

    // Offset from FX
    pitch += track->note.pitchOffset;

    // Clamp pitch to valid range
    pitch = clampInt16(pitch, 0, p->pitchTable.length - 1);

    track->note.pitchFinal = pitch;
    for (int i = 0; i < track->chordVoiceCount; ++i) {
      int chordPitch = pitch + (int)track->chordPitchBase[i] - (int)track->note.pitchBase;
      track->chordPitchFinal[i] = clampInt16(chordPitch, 0, p->pitchTable.length - 1);
    }
  }
}

static void syncLayerTracks(PlaybackState* state, int trackIdx, bool advanceClock = true) {
  PlaybackTrackState& parent = state->tracks[trackIdx];
  const Instrument* layer = parent.note.instrument < PROJECT_MAX_INSTRUMENTS
    ? &state->p->instruments[parent.note.instrument] : nullptr;
  const bool isLayer = layer && layer->type == InstrumentType::Layer;
  const uint8_t trackBit = (uint8_t)(1u << trackIdx);
  if (!isLayer && !(state->layerTrackMask & trackBit)) {
    for (int lane = 0; lane < INSTRUMENT_LAYER_COUNT; ++lane) {
      parent.liveControls[liveSource1 + lane * 2] = 128;
      parent.liveControls[liveSource2 + lane * 2] = 0;
    }
    return;
  }
  state->layerTrackMask &= ~trackBit;
  int layerVolume = layer ? layer->volume : 0;
  for (const auto& mod : parent.note.modulation) {
    if (!mod.modulation || mod.modulation->destination != 1) continue;
    int value = playbackModScaleToRange(mod.outValue, 255);
    layerVolume = modulationIsAdditive(mod.modulation->type) ? layerVolume + value : value;
  }
  layerVolume = clampInt(layerVolume, 0, 255);
  for (int lane = 0; lane < INSTRUMENT_LAYER_COUNT; ++lane) {
    PlaybackTrackState& child = state->layerTracks[trackIdx][lane];
    const uint8_t oldInstrument = child.note.instrument;
    int cents = 0, volume = 0;
    if (isLayer) {
      const int centsFX = fxL1C + lane * 2, volumeFX = fxL1V + lane * 2;
      const uint8_t transpose = parent.note.fx[centsFX].isOn
        ? parent.note.fx[centsFX].fxValue : layer->chip.layer.entries[lane].transpose;
      cents = instrumentLayerTransposeCents(transpose);
      volume = parent.note.fx[volumeFX].isOn
        ? parent.note.fx[volumeFX].fxValue : layer->chip.layer.entries[lane].volume;
      for (const auto& mod : parent.note.modulation) {
        if (!mod.modulation) continue;
        if (mod.modulation->destination == 2 + lane * 2)
          cents += playbackModScaleToRange(mod.outValue, 2400);
        else if (mod.modulation->destination == 3 + lane * 2) {
          int value = playbackModScaleToRange(mod.outValue, 255);
          volume = modulationIsAdditive(mod.modulation->type) ? volume + value : value;
        }
      }
    }
    cents = clampInt(cents, -2400, 2400);
    volume = clampInt(volume, 0, 255);
    parent.liveControls[liveSource1 + lane * 2] = (uint8_t)(((cents + 2400) * 255 + 2400) / 4800);
    parent.liveControls[liveSource2 + lane * 2] = (uint8_t)volume;
    const uint8_t slot = isLayer ? layer->chip.layer.entries[lane].instrument : EMPTY_VALUE_8;
    if (slot >= PROJECT_MAX_INSTRUMENTS || state->p->instruments[slot].type == InstrumentType::none ||
        state->p->instruments[slot].type == InstrumentType::Layer) {
      child.note.instrument = EMPTY_VALUE_8;
      child.liveInstrument = EMPTY_VALUE_8;
      child.note.noteTriggered = child.note.noteReleased = 0;
      child.note.noteKilled = oldInstrument != EMPTY_VALUE_8;
      if (child.note.noteKilled) {
        state->layerTrackMask |= trackBit;
        memset(child.note.modulation, 0, sizeof(child.note.modulation));
        memset(child.midiCCPending, 0, sizeof(child.midiCCPending));
      }
      continue;
    }
    state->layerTrackMask |= trackBit;
    const PlaybackChipNoteState oldChip = child.note.chip;
    const NativeFMValues oldNativeFM = child.note.nativeFM;
    const NativeFMValues oldNativeFMCurrent = child.note.nativeFMCurrent;
    const NativeFMValues oldNativeFMRemaining = child.note.nativeFMRemaining;
    int16_t oldSlewCurrent[fxTotalCount], oldSlewTarget[fxTotalCount];
    uint8_t oldSlewRemaining[fxTotalCount];
    memcpy(oldSlewCurrent, child.slewCurrent, sizeof(oldSlewCurrent));
    memcpy(oldSlewTarget, child.slewTarget, sizeof(oldSlewTarget));
    memcpy(oldSlewRemaining, child.slewRemaining, sizeof(oldSlewRemaining));
    PlaybackFXState oldFX[fxTotalCount];
    memcpy(oldFX, child.note.fx, sizeof(oldFX));
    PlaybackModState modulations[4];
    memcpy(modulations, child.note.modulation, sizeof(modulations));
    child = parent;
    memcpy(child.note.modulation, modulations, sizeof(modulations));

    child.note.instrument = slot;
    if (oldInstrument == slot) {
      child.note.chip = oldChip;
      child.note.nativeFM = oldNativeFM;
      child.note.nativeFMCurrent = oldNativeFMCurrent;
      child.note.nativeFMRemaining = oldNativeFMRemaining;
      memcpy(child.slewCurrent, oldSlewCurrent, sizeof(oldSlewCurrent));
      memcpy(child.slewTarget, oldSlewTarget, sizeof(oldSlewTarget));
      memcpy(child.slewRemaining, oldSlewRemaining, sizeof(oldSlewRemaining));
      for (int fx = 0; fx < fxTotalCount; ++fx) {
        if (child.note.fx[fx].isOn && oldFX[fx].isOn &&
            child.note.fx[fx].fxValue == oldFX[fx].fxValue) {
          const uint8_t value = child.note.fx[fx].fxValue;
          child.note.fx[fx] = oldFX[fx];
          child.note.fx[fx].fxValue = value;
        }
      }
    }
    child.note.fineOffset += cents;
    child.note.volume1 = (uint8_t)((volume * layerVolume + 127) / 255);
    child.note.layerGainActive = 1;
    if (oldInstrument != slot || child.note.noteTriggered)
      initModulations(&child, state->p, oldInstrument, slot);
    if (child.note.noteReleased || child.note.noteKilled)
      for (auto& modulation : child.note.modulation) playbackModNoteOff(&modulation);

    // Engine FX are initialized and advanced against the referenced child,
    // while generic/track FX have already been applied once to the Layer.
    PlaybackTrackState parentTrack = state->tracks[trackIdx];
    state->tracks[trackIdx] = child;
    if (oldInstrument != slot) setupInstrument(state, trackIdx);
    InstrumentType childType = state->p->instruments[slot].type;
    if (childType == InstrumentType::AY1 || childType == InstrumentType::AY2 ||
        childType == InstrumentType::AYSample) resetOffsetsAY(state, trackIdx);
    for (int fx = 0; fx < fxTotalCount; ++fx) {
      if (!child.note.fx[fx].isOn ||
          !instrumentFXAvailableForInstrument(&state->p->instruments[slot], fx)) continue;
      if (oldInstrument != slot || !oldFX[fx].isOn || oldFX[fx].fxValue != child.note.fx[fx].fxValue) {
        const FXCell cell = {(FXId)fx, child.note.fx[fx].fxValue};
        initFX(state, trackIdx, &cell, nullptr, -1);
      }
      if (advanceClock && fxHandlers[fx].handle) {
        fxHandlers[fx].handle(state, &state->tracks[trackIdx], trackIdx, trackIdx,
                              &state->tracks[trackIdx].note.fx[fx]);
        state->tracks[trackIdx].note.fx[fx].counter++;
      }
    }
    processModulationsForTrack(state, &state->tracks[trackIdx], advanceClock);
    handleInstrument(state, trackIdx);
    child = state->tracks[trackIdx];
    state->tracks[trackIdx] = parentTrack;
  }
}

static int applyLiveQueue(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  LiveQueueAction action = track->queue.liveAction;
  if (action == LiveQueueAction::stopNormal || action == LiveQueueAction::stopUrgent) {
    resetTrack(state, trackIdx);
    track->queue.mode = PlaybackMode::none;
    return 1;
  }
  track->mode = PlaybackMode::live;
  track->songRow = track->queue.songRow;
  track->chainRow = 0;
  track->phraseRow = 0;
  track->loop = 1;
  track->queue.mode = PlaybackMode::none;
  track->queue.liveAction = LiveQueueAction::none;
  return 0;
}

static int moveToNextPhraseRow(PlaybackState* state, int trackIdx) {
  int stopped = 0;
  int enteredPhrase = 0;
  int enteredChain = 0;
  struct Project *p = state->p;
  PlaybackTrackState* track = &state->tracks[trackIdx];

  // Check phrase-level loop before incrementing
  if (state->loopRange.enabled && state->loopRange.level == 2 && track->loop &&
      track->songRow == state->loopRange.endSongRow &&
      track->chainRow == state->loopRange.endChainRow &&
      track->phraseRow == state->loopRange.endPhraseRow) {
    track->phraseRow = state->loopRange.startPhraseRow;
    resetTrackFXAuxState(state, trackIdx);
    return stopped;
  }

  // Stop boundary (offline bounce): end of the selected phrase region
  if (state->stopRange.enabled && state->stopRange.level == 2 &&
      track->songRow != EMPTY_VALUE_16 &&
      track->phraseRow == state->stopRange.endPhraseRow) {
    resetTrack(state, trackIdx);
    return 1;
  }

  track->phraseRow++;

  if (track->phraseRow >= 16) {
    track->phraseRow = 0;
    enteredPhrase = 1;
    state->phraseBoundaryMask |= 1u << trackIdx;
    int currentChain = track->songRow == EMPTY_VALUE_16 ? EMPTY_VALUE_16 : p->song[track->songRow][trackIdx];
    if (track->mode == PlaybackMode::phrase || currentChain == EMPTY_VALUE_16 ||
        track->chainRow == 15 ||
        p->chains[currentChain].rows[track->chainRow + 1].phrase == EMPTY_VALUE_16 ||
        (state->loopRange.enabled && state->loopRange.level == 1 && track->loop &&
         track->songRow == state->loopRange.endSongRow &&
         track->chainRow == state->loopRange.endChainRow))
      state->chainBoundaryMask |= 1u << trackIdx;

    if (track->queue.liveAction == LiveQueueAction::urgent ||
        track->queue.liveAction == LiveQueueAction::stopUrgent) {
      stopped = applyLiveQueue(state, trackIdx);
      resetTrackFXAuxState(state, trackIdx);
      if (!stopped) restartStructuralLFOs(state, trackIdx, 1, 1);
      return stopped;
    }

    // Check chain-level loop after phrase overflow
    if (state->loopRange.enabled && state->loopRange.level == 1 && track->loop &&
        track->songRow == state->loopRange.endSongRow &&
        track->chainRow == state->loopRange.endChainRow) {
      track->chainRow = state->loopRange.startChainRow;
      track->phraseRow = state->loopRange.startPhraseRow;
      enteredChain = 1;
      resetTrackFXAuxState(state, trackIdx);
      restartStructuralLFOs(state, trackIdx, 1, 1);
      return stopped;
    }

    // Stop boundary (offline bounce): end of the selected chain region
    if (state->stopRange.enabled && state->stopRange.level == 1 &&
        track->songRow != EMPTY_VALUE_16 &&
        track->chainRow == state->stopRange.endChainRow) {
      resetTrack(state, trackIdx);
      return 1;
    }

    if (track->queue.liveAction == LiveQueueAction::normal ||
        track->queue.liveAction == LiveQueueAction::stopNormal) {
      int chain = track->songRow == EMPTY_VALUE_16 ? EMPTY_VALUE_16 : p->song[track->songRow][trackIdx];
      int next = track->chainRow + 1;
      if (chain == EMPTY_VALUE_16 || next >= playbackResolveLengthAt(p, track->songRow, trackIdx) ||
          playbackResolvePhraseAt(p, track->songRow, trackIdx, next) == EMPTY_VALUE_16) {
        stopped = applyLiveQueue(state, trackIdx);
        resetTrackFXAuxState(state, trackIdx);
        if (!stopped) restartStructuralLFOs(state, trackIdx, 1, 1);
        return stopped;
      }
    }

    // Play mode logic:
    // Song playback
    if (track->mode == PlaybackMode::song) {
      // Next chain row (or pattern bar in simplified view)
      int chain = p->song[track->songRow][trackIdx];
      if (chain != EMPTY_VALUE_16) {
        int chainRow = track->chainRow + 1;
        if (chainRow >= playbackResolveLengthAt(p, track->songRow, trackIdx) ||
            playbackResolvePhraseAt(p, track->songRow, trackIdx, chainRow) == EMPTY_VALUE_16) {
          // Check song-level loop before advancing song row
          if (state->loopRange.enabled && state->loopRange.level == 0 && track->loop &&
              track->songRow == state->loopRange.endSongRow) {
            track->songRow = state->loopRange.startSongRow;
            track->chainRow = state->loopRange.startChainRow;
            track->phraseRow = state->loopRange.startPhraseRow;
            enteredChain = 1;
            resetTrackFXAuxState(state, trackIdx);
            restartStructuralLFOs(state, trackIdx, 1, 1);
            return stopped;
          }

          // Stop boundary (offline bounce): end of the selected song region
          if (state->stopRange.enabled && state->stopRange.level == 0 &&
              track->songRow != EMPTY_VALUE_16 &&
              track->songRow == state->stopRange.endSongRow) {
            resetTrack(state, trackIdx);
            return 1;
          }

          // Next song row
          int songRow = track->songRow + 1;
          track->chainRow = 0;
          // Offline bounce: a track whose first chain is later in the selected
          // region waits silently through empty song rows instead of stopping
          if (state->stopRange.enabled && state->stopRange.level == 0 &&
              songRow < PROJECT_MAX_LENGTH && songRow <= state->stopRange.endSongRow &&
              p->song[songRow][trackIdx] == EMPTY_VALUE_16) {
            track->songRow = songRow;
            enteredChain = 1;
          } else {
            if (songRow >= PROJECT_MAX_LENGTH || p->song[songRow][trackIdx] == EMPTY_VALUE_16) {
              if (track->loop) {
                while (songRow > 0) {
                  songRow--;
                  if (p->song[songRow][trackIdx] == EMPTY_VALUE_16) {
                    songRow++;
                    break;
                  }
                }
              } else {
                songRow = -1;
              }
            }
            if (songRow < 0 || p->song[songRow][trackIdx] == EMPTY_VALUE_16) {
              resetTrack(state, trackIdx);
              stopped = 1;
            } else {
              track->songRow = songRow;
              enteredChain = 1;
            }
          }
        } else {
          track->chainRow = chainRow;
        }
      } else if (state->stopRange.enabled && state->stopRange.level == 0 &&
                 track->songRow < state->stopRange.endSongRow) {
        // Offline bounce: a track sitting on an empty song cell waits silently
        // and moves to the next row, so it can join at its first chain in the
        // selected region
        track->songRow++;
        track->chainRow = 0;
        enteredChain = 1;
      } else {
        resetTrack(state, trackIdx);
      }
    }
    // Chain playback (in simplified view this is "pattern playback").
    // PlayheadMode (Loop: BAR/PAT) applies ONLY in simplified view; classic
    // mode is byte-for-byte unchanged.
    else if (track->mode == PlaybackMode::chain) {
      int chain = p->song[track->songRow][trackIdx];
      if (p->viewMode == ViewMode::simplified) {
        // Simplified: walk the pattern's bars. PlayheadMode decides the loop
        // target once the end of the play range is reached:
        //   loopActiveBar ("Loop: BAR") — the play range is ONLY the queued
        //     bar (mimics classic phrase-view playback on the viewed bar);
        //     it never advances past queue.chainRow.
        //   followPattern ("Loop: PAT") — the play range is the whole
        //     pattern; when its end is reached it loops back to bar 0
        //     (like a chain screen playing from its first phrase).
        int len = playbackResolveLengthAt(p, track->songRow, trackIdx);
        if (p->playheadMode == PlayheadMode::loopActiveBar) {
          int loopBar = track->queue.chainRow;
          if (loopBar < 0 || loopBar >= len) loopBar = 0;
          // One bar is queue.bar..queue.bar+15 phrase rows; at overflow the
          // playhead returns to the queued bar (or stops without loop).
          if (!track->loop) {
            resetTrack(state, trackIdx);
            stopped = 1;
          } else {
            track->chainRow = loopBar;
          }
        } else {
          int chainRow = track->chainRow + 1;
          if (chainRow >= len ||
              playbackResolvePhraseAt(p, track->songRow, trackIdx, chainRow) == EMPTY_VALUE_16) {
            chainRow = track->loop ? 0 : -1;
          }
          if (chainRow < 0 || chainRow >= 16 ||
              playbackResolvePhraseAt(p, track->songRow, trackIdx, chainRow) == EMPTY_VALUE_16) {
            resetTrack(state, trackIdx);
            stopped = 1;
          } else {
            track->chainRow = chainRow;
          }
        }
      } else {
        // Classic: unchanged chain logic.
        int chainRow = track->chainRow + 1;
        if (chainRow >= 16 || p->chains[chain].rows[chainRow].phrase == EMPTY_VALUE_16) {
          chainRow = track->loop ? 0 : -1;
        }
        if (chainRow < 0 || p->chains[chain].rows[chainRow].phrase == EMPTY_VALUE_16) {
          resetTrack(state, trackIdx);
          stopped = 1;
        } else {
          track->chainRow = chainRow;
          enteredChain = chainRow == 0;
        }
      }
    }
    // Phrase playback
    else if (track->mode == PlaybackMode::phrase) {
      if (track->loop) {
        track->chainRow = track->queue.chainRow;
      } else {
        resetTrack(state, trackIdx);
        stopped = 1;
      }
    }
    else if (track->mode == PlaybackMode::live) {
      int chain = p->song[track->songRow][trackIdx];
      int chainRow = track->chainRow + 1;
      int liveLen = playbackResolveLengthAt(p, track->songRow, trackIdx);
      if (chainRow >= liveLen ||
          playbackResolvePhraseAt(p, track->songRow, trackIdx, chainRow) == EMPTY_VALUE_16) {
        track->chainRow = 0;
        enteredChain = 1;
      } else {
        track->chainRow = chainRow;
      }
    }
    // TODO: If in the future I will add NTH command from M8, this logic will need to be updated
    resetTrackFXAuxState(state, trackIdx);
    if (!stopped) restartStructuralLFOs(state, trackIdx, enteredPhrase, enteredChain);
  }

  return stopped;
}

static int skipZeroGrooveRows(PlaybackState* state, int trackIdx) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  Project* p = state->p;

  int curGrooveRow = track->grooveRow;
  while (p->grooves[track->grooveIdx].speed[track->grooveRow] == 0) {
    moveToNextPhraseRow(state, trackIdx);
    track->grooveRow++;
    if (track->grooveRow == 16 || p->grooves[track->grooveIdx].speed[track->grooveRow] == EMPTY_VALUE_8) {
      track->grooveRow = 0;
    }
    if (track->grooveRow == curGrooveRow) {
      // All rows are zero, stop playback
      resetTrack(state, trackIdx);
      return 1;
    }
  }

  return 0;
}


///////////////////////////////////////////////////////////////////////////////
//
// Public interface
//

void playbackInit(PlaybackState* state, Project* project) {
  state->p = project;
  state->layerTrackMask = 0;
  state->phraseBoundaryMask = state->chainBoundaryMask = 0;
  memset(state->liveStickAxes, 0, sizeof(state->liveStickAxes));
  resetLiveStickRate(state);
  state->liveStickWasPlaying = 0;
  state->scaleRoot = project->scaleRoot;
  state->scalePreset = project->scalePreset;
  memset(state->midiCC, 0, sizeof(state->midiCC));

  initFXHandlers();
  initAYSampleTables();

  for (int c = 0; c < PROJECT_MAX_TRACKS; c++) {
    memset(state->layerTracks[c], 0, sizeof(state->layerTracks[c]));
    for (auto& layer : state->layerTracks[c])
      layer.note.instrument = layer.liveInstrument = EMPTY_VALUE_8;
    memset(&state->tracks[c].inserts, 0, sizeof(InsertAutomation));
    state->tracks[c].insertReset = 0;
    state->tracks[c].liveInstrument = EMPTY_VALUE_8;
    resetTrack(state, c);
    state->tracks[c].queue.mode = PlaybackMode::none;
    state->tracks[c].queue.loop = 0;
    state->trackEnabled[c] = 1;
  }

  // Initialize loop range as disabled
  state->loopRange.enabled = 0;
  state->stopRange.enabled = 0;

  // TODO: Properly initialize other global chip states, but for now it's AY only
  for (int c = 0; c < PROJECT_MAX_CHIPS; c++) {
    state->chips[c].ay.envShape = 0;
  }
}

int playbackIsPlaying(PlaybackState* state) {
  for (int trackIdx = 0; trackIdx < state->p->tracksCount; trackIdx++) {
    if (state->tracks[trackIdx].mode != PlaybackMode::stopped) return 1;
  }
  return 0;
}

int playbackTransportIsPlaying(const PlaybackState* state) {
  for (int t = 0; t < state->p->tracksCount; ++t) {
    const auto mode = state->tracks[t].mode;
    if (mode != PlaybackMode::none && mode != PlaybackMode::stopped && mode != PlaybackMode::phraseRow)
      return 1;
  }
  return 0;
}

void playbackSetLoopRange(PlaybackState* state, LoopRange range) {
  state->loopRange = range;
  if (range.enabled)
    for (auto& track : state->tracks) {
      if (track.mode == PlaybackMode::stopped && track.queue.liveAction != LiveQueueAction::none)
        track.queue.mode = PlaybackMode::none;
      track.queue.liveAction = LiveQueueAction::none;
    }
}

void playbackSetStopRange(PlaybackState* state, StopRange range) {
  state->stopRange = range;
}

void playbackClearLoopRange(PlaybackState* state) {
  state->loopRange.enabled = 0;
}

void playbackStartSong(PlaybackState* state, int songRow, int chainRow, int loop) {
  if (playbackIsPlaying(state)) return;
  for (auto& track : state->tracks) {
    memset(&track.inserts, 0, sizeof(track.inserts));
    ++track.insertReset;
  }

  state->scaleRoot = state->p->scaleRoot;
  state->scalePreset = state->p->scalePreset;

  Project* p = state->p;

  for (int trackIdx = 0; trackIdx < p->tracksCount; trackIdx++) {
    PlaybackTrackState* track = &state->tracks[trackIdx];

    if (p->song[songRow][trackIdx] != EMPTY_VALUE_16 &&
        playbackResolvePhraseAt(p, songRow, trackIdx, chainRow) != EMPTY_VALUE_16) {
      track->queue.mode = PlaybackMode::song;
      track->queue.songRow = songRow;
      track->queue.chainRow = chainRow;
      track->queue.phraseRow = 0;
      track->queue.loop = loop;
    }
  }
}

void playbackStartChain(PlaybackState* state, int trackIdx, int songRow, int chainRow, int loop) {
  if (playbackIsPlaying(state)) return;
  for (auto& track : state->tracks) {
    memset(&track.inserts, 0, sizeof(track.inserts));
    ++track.insertReset;
  }

  state->scaleRoot = state->p->scaleRoot;
  state->scalePreset = state->p->scalePreset;

  Project* p = state->p;
  PlaybackTrackState* track = &state->tracks[trackIdx];

  // In simplified view the song cell is a pattern and chainRow is the bar
  // index; in classic it is a chain and chainRow the chain row.
  if (playbackResolvePhraseAt(p, songRow, trackIdx, chainRow) != EMPTY_VALUE_16) {
    track->queue.mode = PlaybackMode::chain;
    track->queue.songRow = songRow;
    track->queue.chainRow = chainRow;
    track->queue.phraseRow = 0;
    track->queue.loop = loop;
  }
}

void playbackStartPhrase(PlaybackState* state, int trackIdx, int songRow, int chainRow, int loop, int startPhraseRow) {
  if (playbackIsPlaying(state)) return;
  for (auto& track : state->tracks) {
    memset(&track.inserts, 0, sizeof(track.inserts));
    ++track.insertReset;
  }

  state->scaleRoot = state->p->scaleRoot;
  state->scalePreset = state->p->scalePreset;

  PlaybackTrackState* track = &state->tracks[trackIdx];

  track->queue.mode = PlaybackMode::phrase;
  track->queue.songRow = songRow;
  track->queue.chainRow = chainRow;
  track->queue.phraseRow = startPhraseRow;
  track->queue.loop = loop;
}

void playbackStartPhraseRow(PlaybackState* state, int trackIdx, PhraseRow* phraseRow) {
  if (!playbackIsPlaying(state)) {
    auto& track = state->tracks[trackIdx];
    memset(&track.inserts, 0, sizeof(track.inserts));
    ++track.insertReset;
  }
  state->scaleRoot = state->p->scaleRoot;
  state->scalePreset = state->p->scalePreset;
  resetTrack(state, trackIdx);

  PlaybackTrackState* track = &state->tracks[trackIdx];

  // Set up phrase row playback
  track->queue.mode = PlaybackMode::phraseRow;
  track->songRow = 0;
  track->currentPhraseRow = *phraseRow;
}

void playbackQueuePhrase(PlaybackState* state, int trackIdx, int songRow, int chainRow) {
  PlaybackTrackState* track = &state->tracks[trackIdx];
  if (track->mode != PlaybackMode::phrase) return;
  if (track->songRow != songRow) return;
  // Ignore queued phrases when ranged loop is enabled
  if (state->loopRange.enabled) return;
  track->queue.mode = PlaybackMode::phrase;
  track->queue.songRow = songRow;
  track->queue.chainRow = chainRow;
  track->queue.phraseRow = 0;
  track->queue.loop = track->loop;
}

static int liveChainValid(const PlaybackState* state, int trackIdx, int songRow) {
  if (trackIdx < 0 || trackIdx >= state->p->tracksCount || songRow < 0 || songRow >= PROJECT_MAX_LENGTH) return 0;
  if (state->p->song[songRow][trackIdx] == EMPTY_VALUE_16) return 0;
  // const-cast-free: resolvers only read the project.
  return playbackResolvePhraseAt(const_cast<Project*>(state->p), songRow, trackIdx, 0) != EMPTY_VALUE_16;
}

float playbackVolumeGain(const PlaybackState* state, const PlaybackTrackState* track) {
  float gain = clampInt(track->note.volume + track->note.volumeOffset, 0, PHRASE_VOLUME_MAX) /
               (float)PHRASE_VOLUME_MAX;
  const PlaybackTableState* tables[] = {&track->note.instrumentTable, &track->note.auxTable};
  for (const PlaybackTableState* table : tables) {
    if (table->tableIdx == EMPTY_VALUE_8) continue;
    uint8_t volume = state->p->tables[table->tableIdx].rows[table->rows[0]].volume;
    if (volume != EMPTY_VALUE_8) gain *= volume / (float)PHRASE_VOLUME_MAX;
  }
  return gain;
}

static int firstActiveLiveReference(const PlaybackState* state) {
  for (int i = 0; i < state->p->tracksCount; ++i) {
    const auto mode = state->tracks[i].mode;
    if (mode == PlaybackMode::song || mode == PlaybackMode::chain ||
        mode == PlaybackMode::phrase || mode == PlaybackMode::live) return i;
  }
  return -1;
}

void playbackStartLiveChain(PlaybackState* state, int trackIdx, int songRow) {
  if (!liveChainValid(state, trackIdx, songRow)) return;
  PlaybackTrackState* track = &state->tracks[trackIdx];
  if (track->mode != PlaybackMode::stopped) return;
  memset(&track->inserts, 0, sizeof(track->inserts));
  ++track->insertReset;
  track->queue.mode = PlaybackMode::live;
  track->queue.songRow = songRow;
  track->queue.chainRow = 0;
  track->queue.phraseRow = 0;
  track->queue.loop = 1;
  track->queue.liveReference = firstActiveLiveReference(state);
  track->queue.liveAction = track->queue.liveReference < 0 ? LiveQueueAction::none : LiveQueueAction::normal;
}

void playbackQueueLiveChain(PlaybackState* state, int trackIdx, int songRow, int urgent) {
  if (trackIdx < 0 || trackIdx >= state->p->tracksCount || state->loopRange.enabled) return;
  PlaybackTrackState* track = &state->tracks[trackIdx];
  if (track->mode == PlaybackMode::stopped) {
    if (songRow >= 0) {
      playbackStartLiveChain(state, trackIdx, songRow);
      if (urgent && track->queue.liveAction != LiveQueueAction::none)
        track->queue.liveAction = LiveQueueAction::urgent;
    } else {
      track->queue.mode = PlaybackMode::none;
      track->queue.liveAction = LiveQueueAction::none;
    }
    return;
  }
  int stop = songRow < 0 || state->p->song[songRow][trackIdx] == EMPTY_VALUE_16;
  if (!stop && !liveChainValid(state, trackIdx, songRow)) return;
  track->queue.mode = stop ? PlaybackMode::stopped : PlaybackMode::live;
  track->queue.songRow = stop ? EMPTY_VALUE_16 : songRow;
  track->queue.chainRow = 0;
  track->queue.phraseRow = 0;
  track->queue.loop = 1;
  track->queue.liveAction = stop ? (urgent ? LiveQueueAction::stopUrgent : LiveQueueAction::stopNormal)
                                  : (urgent ? LiveQueueAction::urgent : LiveQueueAction::normal);
}

void playbackPreviewNote(PlaybackState* state, int trackIdx, uint8_t note, uint8_t instrument) {
  // Create a phrase row for preview
  PhraseRow phraseRow = {0};
  phraseRow.note = note;
  phraseRow.instrument = instrument;
  phraseRow.volume = PHRASE_VOLUME_MAX;

  // Set up empty FX
  for (int i = 0; i < 3; i++) {
    phraseRow.fx[i].command = FX_ID_EMPTY;
    phraseRow.fx[i].value = EMPTY_VALUE_8;
  }

  // Use unified phrase row playback
  playbackStartPhraseRow(state, trackIdx, &phraseRow);
}

void playbackMidiInput(PlaybackState* state, int trackIdx, uint8_t note, uint8_t instrument, uint8_t velocity) {
  if (trackIdx < 0 || trackIdx >= state->p->tracksCount) return;
  auto& track = state->tracks[trackIdx];
  if (!velocity) {
    if (track.midiHeld) track.midiPending = 2;
    return;
  }
  if (note >= state->p->pitchTable.length || instrument >= PROJECT_MAX_INSTRUMENTS ||
      state->p->instruments[instrument].type == InstrumentType::none) return;
  if (track.mode == PlaybackMode::stopped && track.queue.mode == PlaybackMode::none)
    playbackPreviewNote(state, trackIdx, note, instrument);
  track.midiHeld = 1;
  track.midiPending = 1;
  track.midiPitch = note;
  track.midiInstrument = instrument;
  track.midiVelocity = velocity;
}

static void applyMidiInput(PlaybackState* state, int trackIdx) {
  auto& track = state->tracks[trackIdx];
  if (!track.midiPending) return;
  uint8_t pending = track.midiPending;
  track.midiPending = 0;
  track.midiHeld = 0;
  if (pending == 2) {
    handleNoteOff(state, trackIdx);
    return;
  }
  PhraseRow row{};
  row.note = track.midiPitch;
  row.instrument = track.midiInstrument;
  row.volume = track.midiVelocity;
  for (auto& fx : row.fx) fx = {FX_ID_EMPTY, 0};
  readPhraseRowDirect(state, trackIdx, &row, 0, 1);
  track.midiHeld = 1;
}

static void applyQueuedTrack(PlaybackState* state, int trackIdx) {
  auto& track = state->tracks[trackIdx];
  track.mode = track.queue.mode;
  track.songRow = track.queue.songRow;
  track.chainRow = track.queue.chainRow;
  track.phraseRow = track.queue.phraseRow;
  track.loop = track.queue.loop;
  track.queue.mode = PlaybackMode::none;
  track.queue.liveAction = LiveQueueAction::none;
  restartStructuralLFOs(state, trackIdx, track.mode != PlaybackMode::phraseRow,
                        track.mode == PlaybackMode::song || track.mode == PlaybackMode::chain);
  skipZeroGrooveRows(state, trackIdx);
  readPhraseRow(state, trackIdx, 0);
}

uint8_t playbackApplyPendingMidiInput(PlaybackState* state) {
  uint8_t changed = 0;
  int chipIdx = 0, nextChipTrack = projectGetChipTracks(state->p, 0);
  for (int t = 0; t < state->p->tracksCount; ++t) {
    if (t >= nextChipTrack) {
      ++chipIdx;
      nextChipTrack += projectGetChipTracks(state->p, chipIdx);
    }
    auto& track = state->tracks[t];
    if (!track.midiPending) continue;
    if (track.mode == PlaybackMode::stopped && track.queue.mode == PlaybackMode::phraseRow)
      applyQueuedTrack(state, t);
    applyMidiInput(state, t);
    nextFrame(state, t, chipIdx, false);
    syncLayerTracks(state, t, false);
    changed |= 1u << t;
  }
  return changed;
}

void playbackStop(PlaybackState* state) {
  for (auto& track : state->tracks) {
    memset(&track.inserts, 0, sizeof(track.inserts));
    ++track.insertReset;
  }
  for (int c = 0; c < PROJECT_MAX_TRACKS; c++) {
    resetTrack(state, c);
    state->tracks[c].queue.mode = PlaybackMode::none;
  }
  // TODO: Move to AY-specific code when other chip types are added
  // Reset chip states to ensure envelope shapes retrigger on next playback
  for (int c = 0; c < PROJECT_MAX_CHIPS; c++) {
    state->chips[c].ay.envShape = 0;
  }
}

void playbackStopPreview(PlaybackState* state, int trackIdx) {
  if (state->tracks[trackIdx].mode == PlaybackMode::phraseRow) {
    resetTrack(state, trackIdx);
  }
}

int playbackNextFrame(ChipNomadState* chipNomadState) {
  PlaybackState *state = &chipNomadState->playbackState;
  Project* p = &chipNomadState->project;
  int hasActiveTracks = 0;
  state->scaleFXCommandSeen = 0;
  state->phraseBoundaryMask = state->chainBoundaryMask = 0;
  uint8_t waitingTracks = 0;
  for (int i = 0; i < state->p->tracksCount; ++i)
    if (state->tracks[i].mode == PlaybackMode::stopped &&
        state->tracks[i].queue.liveAction != LiveQueueAction::none)
      waitingTracks |= 1u << i;

  int chipIdx = 0;
  int chipTracksCount = projectGetChipTracks(p, chipIdx);
  int nextChipTrackIdx = chipTracksCount;

  // Process all tracker left to right
  for (int trackIdx = 0; trackIdx < state->p->tracksCount; trackIdx++) {
    // Track chip index
    if (trackIdx >= nextChipTrackIdx) {
      chipIdx++;
      chipTracksCount = projectGetChipTracks(p, chipIdx);
      nextChipTrackIdx += chipTracksCount;
    }

    PlaybackTrackState* track = &state->tracks[trackIdx];
    if (track->frameCounter < INT_MAX) ++track->frameCounter;

    // Check queued play event for stopped track or when a track is in phrase row playback mode
    if ((track->mode == PlaybackMode::stopped && track->queue.mode != PlaybackMode::none &&
         !(waitingTracks & (1u << trackIdx))) ||
    (track->mode == PlaybackMode::phraseRow && track->queue.mode == PlaybackMode::phraseRow)) {
      applyQueuedTrack(state, trackIdx);
    }
    // Advance further in the track
    else if (!(waitingTracks & (1u << trackIdx))) {
      tableProgress(state, trackIdx, &track->note.instrumentTable);
      tableProgress(state, trackIdx, &track->note.auxTable);

      // Don't do any playhead movement for phrase row
      if (track->mode != PlaybackMode::phraseRow && track->songRow != EMPTY_VALUE_16) {
        uint8_t grooveValue = p->grooves[track->grooveIdx].speed[track->grooveRow];

        if (grooveValue == EMPTY_VALUE_8) {
          // The current groove row doesn't have a value, stop playback
          resetTrack(state, trackIdx);
        } else {
          uint8_t ratio = track->speedRatio;
          uint32_t threshold;
          if (p->signedTrackSpeed) {
            int speed = (int8_t)ratio;
            if (speed >= 0) {
              track->speedPhase += speed + 1; // 00=x1, 01=x2, ...
              threshold = grooveValue;
            } else {
              track->speedPhase += 1;
              threshold = (uint32_t)grooveValue * (1 - speed); // FF=/2, FE=/3, ...
            }
          } else {
            ratio = ratio <= 0x10 ? ratio : 9;
            track->speedPhase += speedNumerator[ratio];
            threshold = (uint32_t)grooveValue * speedDenominator[ratio];
          }

          if (track->speedPhase >= threshold) {
            // Go to the next groove row
            track->grooveRow++;
            if (track->grooveRow == 16 || p->grooves[track->grooveIdx].speed[track->grooveRow] == EMPTY_VALUE_8) {
              track->grooveRow = 0;
            }

            // Go to the next phrase row
            track->speedPhase -= threshold;
            track->frameCounter = 0;
            moveToNextPhraseRow(state, trackIdx);
            skipZeroGrooveRows(state, trackIdx);
            readPhraseRow(state, trackIdx, 0);
          }
        }
      }
    }

    applyMidiInput(state, trackIdx);
    nextFrame(state, trackIdx, chipIdx);
    syncLayerTracks(state, trackIdx);

    // Check if the track is still playing something
    if (track->songRow == EMPTY_VALUE_16) {
      track->mode = PlaybackMode::stopped;
    } else {
      hasActiveTracks = 1;
    }
  }

  // Service waiting launches after every clock track has advanced, so track
  // order cannot shift a grouped launch by one tick.
  uint8_t readyTracks = 0;
  for (int trackIdx = 0; trackIdx < state->p->tracksCount; ++trackIdx) {
    if (!(waitingTracks & (1u << trackIdx))) continue;
    auto& track = state->tracks[trackIdx];
    int reference = track.queue.liveReference;
    const uint8_t boundary = track.queue.liveAction == LiveQueueAction::urgent
      ? state->phraseBoundaryMask : state->chainBoundaryMask;
    if (reference < 0 || reference >= state->p->tracksCount ||
        (state->tracks[reference].mode == PlaybackMode::stopped &&
         !(boundary & (1u << reference))))
      reference = firstActiveLiveReference(state);
    track.queue.liveReference = reference;
    if (reference >= 0 && !(boundary & (1u << reference))) continue;
    readyTracks |= 1u << trackIdx;
  }
  for (int trackIdx = 0; trackIdx < state->p->tracksCount; ++trackIdx) {
    if (!(readyTracks & (1u << trackIdx))) continue;
    applyLiveQueue(state, trackIdx);
    restartStructuralLFOs(state, trackIdx, 1, 1);
    skipZeroGrooveRows(state, trackIdx);
    readPhraseRow(state, trackIdx, 0);
    int ownerChip = 0, end = projectGetChipTracks(p, 0);
    while (trackIdx >= end && ownerChip + 1 < p->chipsCount)
      end += projectGetChipTracks(p, ++ownerChip);
    nextFrame(state, trackIdx, ownerChip);
    syncLayerTracks(state, trackIdx);
    hasActiveTracks = 1;
  }

  // Output registers for all chips
  for (int chipIdx = 0; chipIdx < p->chipsCount; chipIdx++) {
    outputRegistersAY(chipNomadState, chipIdx * projectGetChipTracks(p, chipIdx), chipIdx);
  }
  for (int trackIdx = 0; trackIdx < p->tracksCount; ++trackIdx) {
    if (!(state->layerTrackMask & (1u << trackIdx))) continue;
    for (int lane = 0; lane < INSTRUMENT_LAYER_COUNT; ++lane) {
      const int owner = PROJECT_MAX_TRACKS + trackIdx * INSTRUMENT_LAYER_COUNT + lane;
      PlaybackTrackState& child = state->layerTracks[trackIdx][lane];
      if (!chipNomadState->layerAYChips[owner] || child.note.instrument >= PROJECT_MAX_INSTRUMENTS) continue;
      InstrumentType type = p->instruments[child.note.instrument].type;
      if (type != InstrumentType::AY1 && type != InstrumentType::AY2 && type != InstrumentType::AYSample) continue;
      PlaybackTrackState parent = state->tracks[trackIdx];
      PlaybackChipState parentChip = state->chips[trackIdx];
      SoundChip* physicalChip = chipNomadState->chips[trackIdx];
      state->tracks[trackIdx] = child;
      state->chips[trackIdx] = state->layerChips[owner];
      chipNomadState->chips[trackIdx] = chipNomadState->layerAYChips[owner];
      outputRegistersAY(chipNomadState, trackIdx, trackIdx);
      child = state->tracks[trackIdx];
      state->layerChips[owner] = state->chips[trackIdx];
      state->tracks[trackIdx] = parent;
      state->chips[trackIdx] = parentChip;
      chipNomadState->chips[trackIdx] = physicalChip;
    }
  }

  return !hasActiveTracks;
}
