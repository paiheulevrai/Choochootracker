#include "playback_blend.h"
#include <algorithm>

static_assert(PROJECT_MAX_LENGTH * 16 * 16 <= 65536, "Blend row distances need wider storage");

uint32_t blendPhraseKey(int track, int column, int songRow, int chainRow, int row) {
  return (uint32_t(track * 3 + column) << 16) |
         (uint32_t(songRow) << 8) | (uint32_t(chainRow) << 4) | uint32_t(row);
}

uint32_t blendTableKey(int table, int column, int row) {
  return 0x80000000u | uint32_t((table * 4 + column) * 16 + row);
}

const BlendSegment* PreparedBlends::find(uint32_t begin) const {
  const auto found = std::lower_bound(segments.begin(), segments.end(), begin,
    [](const BlendSegment& segment, uint32_t key) { return segment.begin < key; });
  return found != segments.end() && found->begin == begin ? &*found : nullptr;
}

void PreparedBlends::prepare(const Project& p) {
  segments.clear();
  struct Pending {
    uint32_t key = 0, row = 0;
    bool active = false;
  };
  auto visit = [&](Pending& pending, FXCell cell, uint32_t key, uint32_t row) {
    if (cell.command == fxBEG) pending = {key, row, true};
    else if (cell.command == fxEND) {
      if (pending.active)
        segments.push_back({pending.key, key, uint16_t(row - pending.row), cell.value});
      pending.active = false;
    } else if (cell.command != FX_ID_EMPTY) pending.active = false;
  };
  // Skip arrangement expansion entirely for projects without phrase markers.
  bool hasBegin = false;
  for (const auto& phrase : p.phrases)
    for (const auto& row : phrase.rows)
      for (const auto& cell : row.fx)
        hasBegin |= cell.command == fxBEG;
  if (hasBegin) {
    for (int track = 0; track < p.tracksCount; ++track) {
      Pending pending[3];
      uint32_t ordinal = 0;
      for (int song = 0; song < PROJECT_MAX_LENGTH; ++song) {
        const auto cell = p.song[song][track];
        if (cell >= PROJECT_MAX_CHAINS) {
          for (auto& item : pending) item.active = false;
          continue;
        }
        const int length = p.viewMode == ViewMode::simplified ?
          std::min(int(p.patterns[cell].length), 16) : 16;
        for (int bar = 0; bar < length; ++bar) {
          const auto phrase = p.viewMode == ViewMode::simplified ?
            p.patterns[cell].bars[bar].phrase : p.chains[cell].rows[bar].phrase;
          if (phrase >= PROJECT_MAX_PHRASES) {
            if (bar == 0) for (auto& item : pending) item.active = false;
            break;
          }
          for (int row = 0; row < 16; ++row, ++ordinal)
            for (int lane = 0; lane < 3; ++lane)
              visit(pending[lane], p.phrases[phrase].rows[row].fx[lane],
                    blendPhraseKey(track, lane, song, bar, row), ordinal);
        }
      }
    }
  }
  for (int table = 0; table < PROJECT_MAX_TABLES; ++table) {
    Pending pending[4];
    for (int row = 0; row < 16; ++row)
      for (int lane = 0; lane < 4; ++lane)
        visit(pending[lane], p.tables[table].rows[row].fx[lane],
              blendTableKey(table, lane, row), row);
  }
  std::sort(segments.begin(), segments.end(),
    [](const BlendSegment& a, const BlendSegment& b) { return a.begin < b.begin; });
}

int blendCurve(int progress, uint8_t curve) {
  progress = std::clamp(progress, 0, 65536);
  const int bow = int(int64_t(progress) * (65536 - progress) / 65536);
  const int amount = 128 - int(curve);
  return progress + bow * amount / (amount >= 0 ? 128 : 127);
}

int blendValue(FXId command, uint8_t value) {
  return command == fxSPT ? int(int8_t(value)) : int(value);
}

bool blendDestination(FXId command) {
  if ((command >= fxF11 && command <= fxF28) ||
      (command >= fxOAR && command <= fxOSL) || command == fxOFI ||
      (command >= fxOE1 && command <= fxOE4) ||
      (command >= fxLFR && command <= fxLPS)) return true;
  switch (command) {
    case fxPAN: case fxTPN:
    case fxBTM: case fxBCL: case fxBCF: case fxBRS:
    case fxSPT: case fxSST: case fxSTA: case fxSEN: case fxSVL:
    case fxSCF: case fxSRS: case fxSSP:
    case fxPHA: case fxPTM: case fxPMO: case fxPAX: case fxPCF: case fxPRS:
    case fxSDT: case fxSMX: case fxSCF2: case fxSRS2: case fxBIA: case fxBIB:
    case fxRSN: case fxDSN: case fxEAT: case fxEDC: case fxESU: case fxERL:
    case fxESH: case fxTDC: case fxTCL:
    case fxADC: case fxATM: case fxACL: case fxACF: case fxARS: case fxAEM:
    case fxDDC: case fxDTO: case fxDSW: case fxDNO: case fxDFM: case fxDDR:
    case fxDCF: case fxDRS:
    case fxMWV: case fxMIN: case fxMAM: case fxMFL: case fxMFB: case fxMSH:
    case fxMCF: case fxMRS:
    case fxSDC: case fxSMD: case fxSA: case fxSB: case fxSMO: case fxSC:
    case fxSCF3: case fxSRS3:
    case fxSCP: case fxSCT: case fxSRN: case fxSAT: case fxSDE:
    case fxSSU: case fxSRL:
    case fxFBR: case fxFFB: case fxFBK:
    case fxFET: case fxFTD: case fxFDT: case fxFHR: case fxFLR: case fxFLD:
    case fxCSP: case fxCSS: case fxCEP:
    case fxOL1: case fxOL2: case fxOL3: case fxOL4: case fxOL5: case fxOL6:
    case fxMDT: case fxMVS: case fxMDM: case fxMDR: case fxMLV:
    case fxFPT: case fxFVL: case fxFPN: case fxFCF: case fxFRS:
    case fxFAT: case fxFDC: case fxFRL: case fxFHD: case fxFSU:
    case fxL1C: case fxL1V: case fxL2C: case fxL2V: case fxL3C: case fxL3V:
    case fxBSL: case fxBDR: case fxGLD: case fxB80CF: case fxB80RS:
      return true;
    default:
      return false;
  }
}
