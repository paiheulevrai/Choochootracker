#include "screen_instrument.h"
#include "field_style.h"
#include "corelib_gfx.h"
#include "chipnomad_lib.h"
#include "waveform_display.h"

static InstrumentFMAmp* amp() {
  return instrumentFMAmpSettings(&chipnomadState->project.instruments[cInstrument]);
}

static Bitmap* staticWaveformBitmap = nullptr;

void instrumentFMRefreshStaticWaveform() {
  const auto* envelope = amp();
  // SID shares this redraw path but has no optional FM amp or ADSR overlay.
  const int row = 16;
  if (!staticWaveformBitmap) staticWaveformBitmap = gfxBitmapCreate(32, 3);
  gfxClearRect(0, row, 32, 3);
  renderFMPreview(staticWaveformBitmap, &chipnomadState->project.instruments[cInstrument]);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxDrawBitmap(staticWaveformBitmap, 0, row);
  if (envelope && envelope->enabled) {
    instrumentCommonDrawEnvelopePreview(envelope->attack, envelope->decay,
                                        envelope->sustain, envelope->release,
                                        envelope->envelopeShape);
  }
}

void instrumentFMAmpDrawStatic() {
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 12, "Amp env");
  gfxPrint(0, 10, "Bright"); gfxPrint(kColumnNameX[1], 10, "Feedbk.");
  gfxPrint(0, 14, "ADSR");
  gfxPrint(6, 14, "A"); gfxPrint(11, 14, "D");
  gfxPrint(16, 14, "S"); gfxPrint(21, 14, "R");
  gfxPrint(27, 14, "SHP");
  instrumentFMRefreshStaticWaveform();
}

// Bright runs from -63 to +63 and shows as 80 plus the brightness, with a
// bar from the middle; Feedback is the preset's own, or 00-07.
void instrumentFMToneDrawCursor(int col) {
  const auto* tone = instrumentFMToneSettings(&chipnomadState->project.instruments[cInstrument]);
  gfxCursor(kColumnValueX[col], 10, col && !tone->feedback ? 6 : kFieldNumberWidth);
  if (!col) screenMessage(0, "Bright %+d", tone->brightness);
}
void instrumentFMToneDrawField(int col, CellState state) {
  const auto* tone = instrumentFMToneSettings(&chipnomadState->project.instruments[cInstrument]);
  const int x = kColumnValueX[col];
  fieldDrawLabel(kColumnNameX[col], 10, col ? "Feedbk." : "Bright", state);
  gfxClearRect(x, 10, kFieldNumberWidth, 1);
  if (!col) {
    fieldDrawNumber(x, 10, (uint8_t)(0x80 + tone->brightness), state, (tone->brightness + 63) / 126.0f, true);
  } else if (!tone->feedback) {
    gfxSetFgColor(fieldNameColor(state));
    gfxPrint(x, 10, "Preset");
  } else {
    fieldDrawNumber(x, 10, (uint8_t)(tone->feedback - 1), state, (tone->feedback - 1) / 7.0f);
  }
  instrumentFMRefreshStaticWaveform();
}
int instrumentFMToneEdit(int col, CellEditAction action) {
  auto* tone=instrumentFMToneSettings(&chipnomadState->project.instruments[cInstrument]);
  uint8_t value=col?tone->feedback:int(tone->brightness)+63;
  if(!col&&action==CellEditAction::clear)value=63;
  else if(!edit8noLast(action,&value,col?1:8,0,col?8:126))return 0;
  if(col)tone->feedback=value;else tone->brightness=int(value)-63;
  projectModified=1;return 1;
}

void instrumentFMAmpDrawCursor(int col, int row) {
  if (!row) gfxCursor(kColumnValueX[0], 12, amp()->enabled ? 4 : 6);
  else instrumentCommonDrawVoicePostCursor(col, 9);
}

void instrumentFMAmpDrawField(int col, int row, CellState state) {
  instrumentFMRefreshStaticWaveform();
  if (!row) {
    fieldDrawLabel(0, 12, "Amp env", state);
    gfxSetFgColor(fieldNameColor(state));
    gfxClearRect(kColumnValueX[0], 12, kFieldNumberWidth, 1);
    gfxPrint(kColumnValueX[0], 12, amp()->enabled ? "ADSR" : "Bypass");
  } else instrumentCommonDrawVoicePostField(col, 9, state, amp());
}

int instrumentFMAmpEdit(int col, int row, CellEditAction action) {
  auto* p = amp();
  int handled;
  if (!row) {
    handled = edit8noLast(action, &p->enabled, 1, 0, 1);
    if (handled && p->enabled && !(p->attack || p->decay || p->sustain || p->release || p->envelopeShape)) {
      p->sustain = 255; p->release = 32; p->envelopeShape = 128;
    }
  } else handled = instrumentCommonOnEditVoicePost(col, 9, action, p);
  if (handled) { projectModified = 1; currentScreen->fullRedraw(); }
  return handled;
}
