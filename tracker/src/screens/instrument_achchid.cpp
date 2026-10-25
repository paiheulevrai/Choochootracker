#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"
#include "selection_popup.h"
#include "waveform_display.h"
#include "synth/multimode_filter.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static int modelButtonDown;
static Bitmap* previewBitmap;
static InstrumentAChChid* instrument() { return &chipnomadState->project.instruments[cInstrument].chip.achchid; }
static int isBraids() { return instrument()->wave == AChChidWave::braids; }
static int isSID() { return instrument()->wave == AChChidWave::sid; }
static const char* const sidShapes[] = {"off", "tri", "saw", "TS", "pls", "TP", "SP", "TSP", "noi"};
static const char* waveName(AChChidWave wave) {
  switch (wave) { case AChChidWave::square: return "Square"; case AChChidWave::saw: return "Saw";
    case AChChidWave::braids: return "Braids"; case AChChidWave::sid: return "SID"; }
  return "Saw";
}
static int editLogarithmic16(CellEditAction action, uint16_t* value, uint16_t min, uint16_t max) {
  uint16_t bounded = *value < min ? min : *value > max ? max : *value;
  float ratio = (float)max / min;
  uint8_t control = (uint8_t)(255.0f * logf((float)bounded / min) / logf(ratio) + .5f);
  int handled = edit8noLast(action, &control, 16, 0, 255);
  if (handled) *value = (uint16_t)(min * powf(ratio, control / 255.0f) + .5f);
  return handled;
}
static void drawPreview(const InstrumentAChChid* a) {
  if (!previewBitmap) previewBitmap = gfxBitmapCreate(32, 3);
  renderAChChidPreview(previewBitmap, a);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxDrawBitmap(previewBitmap, 0, 16);
}
static void selectModel(int value) { instrument()->model = (uint8_t)value; projectModified = 1; screenSetup(&screenInstrument, cInstrument); }
static void cancelModel() { screenSetup(&screenInstrument, cInstrument); }
static void openModel() { selectionPopupSetup("BRAIDS MODEL", braidsCategories, braidsCategoryCount, instrument()->model, selectModel, cancelModel); screenSetup(&screenSelectionPopup, 0); }
static int columns(int row) { return row < 3 ? instrumentCommonColumnCount(row) : 2; }
static int isCellValid(int col,int row) {
  if (row < 3 || col) return 1;
  return row <= (isBraids() ? 7 : isSID() ? 6 : 4);
}
static void drawStatic() {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0,6,"OSCILLATOR"); gfxPrint(20,6,"303 FILTER");
  gfxSetFgColor(appSettings.colorScheme.textLabel); gfxPrint(0,7,"Wave");
  if (isBraids()) { gfxPrint(0,8,"Model"); gfxPrint(0,9,"Timbre"); gfxPrint(0,10,"Color"); gfxPrint(0,11,"Shaper"); }
  else if (isSID()) { gfxPrint(0,8,"Shape"); gfxPrint(0,9,"Width"); gfxPrint(0,10,"Shaper"); }
  else gfxPrint(0,8,"Fine");
  gfxPrint(20,7,"Cutoff"); gfxPrint(20,8,"Reso"); gfxPrint(20,9,"Env Mod"); gfxPrint(20,10,"Decay"); gfxPrint(20,11,"Accent");
  drawPreview(instrument());
}
// The track status panel starts at column 34; keep values and cursors before it.
static void drawCursor(int col,int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  InstrumentAChChid* a = instrument();
  if (!col) {
    if (row == 3) gfxCursor(11, 7, strlen(waveName(a->wave)));
    else if (row == 4) gfxCursor(11, 8, isBraids() ? 3 + min((int)strlen(modelCatalogName(InstrumentType::Braids, a->model)), 6) : isSID() ? 3 + strlen(sidShapes[a->sidShape >= 1 && a->sidShape <= 8 ? a->sidShape : 4]) : 3);
    else if (row >= 5 && row <= 7) gfxCursor(11, row + 4, 2);
  } else {
    char value[12];
    snprintf(value, sizeof(value), "%u Hz", a->cutoff);
    int width = row == 3 ? (int)strlen(value) : row == 6 ? 6 : 2;
    gfxCursor(28, row + 4, width);
  }
}
static void drawField(int col,int row,CellState state) {
  if(row<3) return instrumentCommonDrawField(col,row,state);
  InstrumentAChChid* a=instrument();
  gfxSetFgColor(state==CellState::focus?appSettings.colorScheme.textValue:appSettings.colorScheme.textDefault);
  if(!col && row==3) gfxPrint(11,7,waveName(a->wave));
  else if(!isBraids() && !isSID() && row==4 && !col) gfxPrintf(11,8,"%+03d",a->fineTune);
  else if(isBraids() && row==4 && !col) gfxPrintf(11,8,"%02u %.6s",a->model,modelCatalogName(InstrumentType::Braids,a->model));
  else if(isBraids() && row==5 && !col) gfxPrint(11,9,byteToHex(controlFromRange(a->timbre,32767)));
  else if(isBraids() && row==6 && !col) gfxPrint(11,10,byteToHex(controlFromRange(a->color,32767)));
  else if(isBraids() && row==7 && !col) gfxPrint(11,11,byteToHex(a->saturation));
  else if(isSID() && row==4 && !col) gfxPrintf(11,8,"%02u %s",a->sidShape,sidShapes[a->sidShape >= 1 && a->sidShape <= 8 ? a->sidShape : 4]);
  else if(isSID() && row==5 && !col) gfxPrint(11,9,byteToHex(controlFromRange(a->sidPulseWidth,4095)));
  else if(isSID() && row==6 && !col) gfxPrint(11,10,byteToHex(a->saturation));
  else if(col && row==3) gfxPrintf(28,7,"%u Hz",a->cutoff);
  else if(col && row==4) gfxPrint(28,8,byteToHex(controlFromRange(a->resonance,100)));
  else if(col && row==5) gfxPrint(28,9,byteToHex(controlFromRange(a->envMod,100)));
  else if(col && row==6) gfxPrintf(28,10,"%4ums",a->decay);
  else if(col && row==7) gfxPrint(28,11,byteToHex(controlFromRange(a->accent,100)));
}
static int onEdit(int col,int row,CellEditAction action) {
  if(row<3) return instrumentCommonOnEdit(col,row,action);
  InstrumentAChChid* a=instrument(); int ok=0;
  if(row==3 && !col) ok=edit8noLast(action,(uint8_t*)&a->wave,1,0,3);
  else if(!isBraids() && !isSID() && row==4 && !col) ok=editSigned8(action,&a->fineTune,1,-99,99);
  else if(isBraids() && row==4 && !col) ok=edit8noLast(action,&a->model,1,0,46);
  else if(isBraids() && row==5 && !col) ok=editOscillatorParameter(action,&a->timbre);
  else if(isBraids() && row==6 && !col) ok=editOscillatorParameter(action,&a->color);
  else if(isBraids() && row==7 && !col) ok=edit8noLast(action,&a->saturation,16,0,255);
  else if(isSID() && row==4 && !col) ok=edit8noLast(action,&a->sidShape,1,1,8);
  else if(isSID() && row==5 && !col) { uint8_t width=controlFromRange(a->sidPulseWidth,4095);
    ok=edit8noLast(action,&width,16,0,255); if(ok) a->sidPulseWidth=(uint16_t)(width*4095u/255u); }
  else if(isSID() && row==6 && !col) ok=edit8noLast(action,&a->saturation,16,0,255);
  else if(col && row==3) ok=editLogarithmic16(action,&a->cutoff,200,FILTER_CUTOFF_MAX_HZ);
  else if(col && row==4) ok=edit8noLast(action,&a->resonance,6,0,100);
  else if(col && row==5) ok=edit8noLast(action,&a->envMod,6,0,100);
  else if(col && row==6) ok=editLogarithmic16(action,&a->decay,200,2000);
  else if(col && row==7) ok=edit8noLast(action,&a->accent,6,0,100);
  if(ok){projectModified=1;screenFullRedraw(&screenInstrumentAChChid);} return ok;
}
static int onInput(int down,int keys,int taps) {
  if(screenInstrumentAChChid.cursorRow!=4||screenInstrumentAChChid.cursorCol||!isBraids()){modelButtonDown=0;return 0;}
  PopupEditInput input=popupEditInput(down,keys,&modelButtonDown);
  if(input==PopupEditInput::cycle) {
    cycle8(&instrument()->model,keys==(keyEdit|keyRight)?1:-1,0,46,0);
    projectModified=1; screenFullRedraw(&screenInstrumentAChChid); return 1;
  }
  if(input==PopupEditInput::hold) return 1;
  if(input==PopupEditInput::open){openModel();return 1;}
  return 0;
}
ScreenData screenInstrumentAChChid={.rows=8,.cursorRow=0,.cursorCol=0,.topRow=0,.selectMode=-1,.playbackLevel=ScreenPlaybackLevel::none,.getColumnCount=columns,.drawStatic=drawStatic,.drawCursor=drawCursor,.drawSelection=NULL,.drawRowHeader=NULL,.drawColHeader=NULL,.drawField=drawField,.onEdit=onEdit,.onInput=onInput,.onRawInput=NULL,.isCellValid=isCellValid,.getLoopRange=NULL};
