#include "piano_display.h"
#include "common.h"
#include "corelib_gfx.h"
#include "screens/screens.h"
#include <algorithm>

static Bitmap* piano;

static Bitmap* sized(Bitmap*& bitmap, int columns, int rows) {
  if (bitmap && (bitmap->widthPixels != columns * gfxGetCharWidth() ||
                 bitmap->heightPixels != rows * gfxGetCharHeight())) {
    gfxBitmapFree(bitmap);
    bitmap = nullptr;
  }
  if (!bitmap) bitmap = gfxBitmapCreate(columns, rows);
  if (bitmap) gfxBitmapClear(bitmap);
  return bitmap;
}

// Draw a chunky 32x9 grid in code. These coordinates describe
// stepped key silhouettes, not an image asset or a stretched keyboard.
static constexpr int pianoWidth = 32, pianoHeight = 9;
static int pianoPitchAtNative(int x, int y) {
  if (x < 1 || y < 1 || x >= pianoWidth - 1 || y >= pianoHeight - 1) return -1;
  static const int blackLeft[] = {3, 8, 16, 21, 26};
  static const int blackPitch[] = {1, 3, 6, 8, 10};
  if (y <= 4) {
    for (int i = 0; i < 5; ++i)
      if (x >= blackLeft[i] && x < blackLeft[i] + 3) return blackPitch[i];
  }
  static const int whiteLeft[] = {1, 5, 10, 14, 18, 23, 28};
  static const int whiteRight[] = {4, 9, 13, 17, 22, 27, 31};
  static const int whitePitch[] = {0, 2, 4, 5, 7, 9, 11};
  for (int i = 0; i < 7; ++i)
    if (x >= whiteLeft[i] && x < whiteRight[i]) return whitePitch[i];
  return -1;
}

int monitorPianoPitchAt(int x, int y, int width, int height) {
  const int scale = std::min(width / pianoWidth, height / pianoHeight);
  if (scale < 1) return -1;
  x -= (width - pianoWidth * scale) / 2;
  y -= (height - pianoHeight * scale) / 2;
  if (x < 0 || y < 0 || x >= pianoWidth * scale || y >= pianoHeight * scale) return -1;
  return pianoPitchAtNative(x / scale, y / scale);
}

uint16_t monitorPianoNotes(void) {
  if (!chipnomadState) return 0;
  const PlaybackStatus* playing = chipnomadGetPlaybackStatus(chipnomadState);
  uint16_t notes = 0;
  for (int t = 0; t < chipnomadState->project.tracksCount; ++t) {
    const PlaybackTrackState& track = playing->tracks[t];
    if (!playing->trackEnabled[t] || track.note.pitchFinal >= NOTE_OFF) continue;
    // Chord pitches are already transposed/quantized by the engine.
    if (track.chordVoiceCount > 1) {
      for (int v = 0; v < track.chordVoiceCount && v < CHORD_MAX_VOICES; ++v)
        if (track.chordPitchFinal[v] < NOTE_OFF) notes |= 1u << (track.chordPitchFinal[v] % 12);
    } else notes |= 1u << (track.note.pitchFinal % 12);
  }
  return notes;
}

static void drawPiano() {
  constexpr int col = 35, columns = 5;
  const int availableRows = currentScreen == &screenInstrument || currentScreen == &screenInstrumentPool ||
    currentScreen == &screenModulation ? 3 :
    currentScreen == &screenChain || currentScreen == &screenTable ? 5 : 4;
  const int rows = availableRows % 2 ? 3 : 2;
  const int row = 11 + (availableRows - rows) / 2;
  if (!sized(piano, columns, rows)) return;
  const auto cs = appSettings.colorScheme;
  const int w = piano->widthPixels;
  const int scale = 2 * gfxGetCharHeight() / (pianoHeight + 5);
  if (scale < 1) return;
  const int drawnWidth = std::min(w, pianoWidth * scale);
  const int left = (w - drawnWidth) / 2;
  const int stroke = std::max(1, scale / 2);
  const int top = stroke + (piano->heightPixels - (pianoHeight + 2) * scale - stroke) / 2;
  const uint16_t playing = monitorPianoNotes();
  gfxSetBgColor(cs.background);
  gfxClearRect(col, row, columns, rows);
  // Keep the chunky outer frame; internal strokes are half as thick.
  // Draw at whole screen pixels so the thin dividers remain crisp.
  const int strokeX = std::max(1, drawnWidth / (pianoWidth * 2));
  // Match ordinary Song values for the outline and waveforms for lit notes.
  const int colors[] = {cs.textValue, cs.textValue, cs.background, cs.textInfo, cs.textInfo};
  for (int layer = 0; layer < 5; ++layer) {
    gfxBitmapClear(piano);
    for (int py = -stroke; py < (pianoHeight + 2) * scale; ++py)
      for (int px = 0; px < drawnWidth; ++px) {
        const int x = px * pianoWidth / drawnWidth;
        // Raise a cap above each accidental, restoring its two-pixel border
        // without covering the raised key interior or thickening dividers.
        if (py < 0) {
          int pitch = pianoPitchAtNative(x, 1);
          bool accidental = pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
          if (layer == 0 && accidental) piano->data[(top + py) * w + left + px] = 255;
          continue;
        }
        // Extend two interior bands, leaving every horizontal stroke intact.
        int sourceY = py;
        if (sourceY >= 7 * scale) sourceY = std::max(7 * scale - 1, sourceY - scale);
        if (sourceY >= 3 * scale) sourceY = std::max(3 * scale - 1, sourceY - scale);
        int y = sourceY / scale;
        bool frame = x == 0 || x == pianoWidth - 1 || y == 0 || y == pianoHeight - 1;
        int pitch = pianoPitchAtNative(x, y);
        const int upperPitch = pianoPitchAtNative(x, 1);
        const bool accidentalColumn = upperPitch == 1 || upperPitch == 3 ||
          upperPitch == 6 || upperPitch == 8 || upperPitch == 10;
        // Accidentals reach one stroke into the top frame and one block lower.
        const int accidentalBottom = 7 * scale;
        bool black = accidentalColumn && py >= scale - stroke && py < accidentalBottom;
        if (black) {
          pitch = upperPitch;
          frame = false;
        }
        // Reclaim the right side of each formerly thick natural-key divider.
        if (!frame && pitch < 0 && (px * pianoWidth) % drawnWidth >= strokeX * pianoWidth)
          pitch = pianoPitchAtNative(x + 1, y);
        bool outline = frame || pitch < 0 || (black &&
          (pianoPitchAtNative((px - strokeX) * pianoWidth / drawnWidth, 1) != pitch ||
           pianoPitchAtNative((px + strokeX) * pianoWidth / drawnWidth, 1) != pitch ||
           py >= accidentalBottom - stroke));
        int alpha = 255;
        if (layer == 0) {
          if (!outline) continue;
        } else {
          if (outline) continue;
          bool active = playing & (1u << pitch);
          if (layer != 1 + (active ? 2 : 0) + (black ? 1 : 0)) continue;
          bool shadow = black ? py >= accidentalBottom - stroke - scale : y >= 6;
          // Pressed keys use the waveform color at full intensity, edge to edge.
          if (!active && !black) alpha = shadow ? 12 : 24;
        }
        piano->data[(top + py) * w + left + px] = alpha;
      }
    gfxSetFgColor(colors[layer]);
    gfxDrawBitmap(piano, col, row);
  }
}

void pianoDisplayDraw(void) {
  if (!chipnomadState) return;
  if (currentScreen == &screenSong || currentScreen == &screenChain ||
      currentScreen == &screenPhrase || currentScreen == &screenTable ||
      currentScreen == &screenInstrument || currentScreen == &screenInstrumentPool ||
      currentScreen == &screenModulation || currentScreen == &screenMixer ||
      currentScreen == &screenGroove || currentScreen == &screenSettings ||
      currentScreen == &screenProject) {
    ScreenOverlayCoordinates overlay;
    drawPiano();
  }
}
