#include "screen_instrument.h"
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
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 12, "Amp env");
  gfxPrint(0, 10, "Bright"); gfxPrint(19, 10, "Feedback");
  gfxPrint(0, 14, "ADSR");
  gfxPrint(6, 14, "A"); gfxPrint(11, 14, "D");
  gfxPrint(16, 14, "S"); gfxPrint(21, 14, "R");
  gfxPrint(27, 14, "SHP");
  instrumentFMRefreshStaticWaveform();
}

void instrumentFMToneDrawCursor(int col) { gfxCursor(col ? 28 : 9, 10, col ? 6 : 4); }
void instrumentFMToneDrawField(int col, CellState state) {
  const auto* tone=instrumentFMToneSettings(&chipnomadState->project.instruments[cInstrument]);
  gfxSetFgColor(state==CellState::focus?appSettings.colorScheme.textValue:appSettings.colorScheme.textDefault);
  gfxClearRect(col?28:9,10,7,1);
  if(!col)gfxPrintf(9,10,"%+03d",tone->brightness);
  else if(!tone->feedback)gfxPrint(28,10,"Preset");
  else gfxPrintf(28,10,"%u",tone->feedback-1);
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
  if (!row) gfxCursor(9, 12, amp()->enabled ? 4 : 6);
  else instrumentCommonDrawVoicePostCursor(col, 9);
}

void instrumentFMAmpDrawField(int col, int row, CellState state) {
  instrumentFMRefreshStaticWaveform();
  if (!row) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxClearRect(9, 12, 8, 1);
    gfxPrint(9, 12, amp()->enabled ? "ADSR" : "Bypass");
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
