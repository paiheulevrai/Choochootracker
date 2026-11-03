#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "file_browser.h"
#include "common.h"
#include "synth/pd_voice.h"
#include "utils.h"
#include "audio_manager.h"
#include <string.h>

static int loadButtonDown;
static char checkedPath[256];
static char patchStatus[16];
static char patchError[128];

static InstrumentPDBase* pdBase(Instrument* instrument) {
  return instrument->type == InstrumentType::PDVCO
    ? static_cast<InstrumentPDBase*>(&instrument->chip.pdVco) : &instrument->chip.pdVoice;
}
static bool isVCO() { return chipnomadState->project.instruments[cInstrument].type == InstrumentType::PDVCO; }
static int macroIndex(int row) {
  if (!isVCO()) return row >= 4 && row <= 11 ? row - 4 : -1;
  if (row >= 4 && row <= 8) return row - 4;
  return row >= 10 && row <= 12 ? row - 5 : -1;
}
static int macroY(int index) { return !isVCO() || index < 5 ? 8 + index : 11 + index; }
static void cancelled() { screenSetup(&screenInstrument, cInstrument); }
static void loaded(const char* path) {
  Instrument* instrument = &chipnomadState->project.instruments[cInstrument];
  InstrumentPDBase* pd = pdBase(instrument);
  audioManager.pause();
  PDVoice probe; probe.init(48000); bool ok = probe.load(path);
  if (ok) {
    strncpy(pd->path, path, sizeof(pd->path) - 1); pd->path[sizeof(pd->path) - 1] = 0;
    for (int i = 0; i < 8; ++i) { strncpy(pd->macroName[i], probe.macroName(i), 15); pd->macroName[i][15] = 0; }
    if (instrument->type == InstrumentType::PDVoice) instrument->chip.pdVoice.stereo = probe.stereo();
    if (!instrument->name[0]) { const char* name = strrchr(path, PATH_SEPARATOR); name = name ? name + 1 : path; strncpy(instrument->name, name, PROJECT_INSTRUMENT_NAME_LENGTH); instrument->name[PROJECT_INSTRUMENT_NAME_LENGTH] = 0; char* dot = strrchr(instrument->name, '.'); if (dot) *dot = 0; }
    projectModified = 1; screenMessage(MESSAGE_TIME, "Pd patch loaded");
    strncpy(checkedPath, path, sizeof(checkedPath) - 1); checkedPath[sizeof(checkedPath) - 1] = 0; strcpy(patchStatus, "READY"); patchError[0] = 0;
  } else {
    strncpy(checkedPath, path, sizeof(checkedPath) - 1); checkedPath[sizeof(checkedPath) - 1] = 0; strcpy(patchStatus, "ERROR");
    strncpy(patchError, probe.lastError(), sizeof(patchError) - 1); patchError[sizeof(patchError) - 1] = 0;
    screenMessage(MESSAGE_TIME_ERROR, patchError[0] ? patchError : "Cannot load Pd patch");
  }
  audioManager.resume(); screenSetup(&screenInstrument, cInstrument);
}
static void checkPatch(InstrumentPDBase* pd) {
  if (!strcmp(checkedPath, pd->path)) return;
  strncpy(checkedPath, pd->path, sizeof(checkedPath) - 1); checkedPath[sizeof(checkedPath) - 1] = 0;
  patchError[0] = 0;
  if (!pd->path[0]) { patchStatus[0] = 0; return; }
  audioManager.pause();
  PDVoice probe; probe.init(96000); bool ok = probe.load(pd->path);
  if (ok) strcpy(patchStatus, "READY");
  else { strcpy(patchStatus, "ERROR"); strncpy(patchError, probe.lastError(), sizeof(patchError) - 1); patchError[sizeof(patchError) - 1] = 0; }
  audioManager.resume();
}
static int columns(int row) {
  if (row < 3) return instrumentCommonColumnCount(row);
  if (row == 3) return 1;
  if (isVCO() && row == 9) return 5;
  if (isVCO() && row >= 4 && row <= 8) return 2;
  return 1;
}
static void drawStatic() {
  instrumentCommonDrawStatic(); Instrument* instrument = &chipnomadState->project.instruments[cInstrument];
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0,6,instrument->type == InstrumentType::PDVCO ? "PD-VCO" : "PD-VOICE"); gfxPrint(0,7,"MACROS");
  InstrumentPDBase* pd = pdBase(instrument); gfxSetFgColor(appSettings.colorScheme.textDefault);
  for (int i=0;i<8;++i) gfxPrint(0,macroY(i),pd->macroName[i]);
  checkPatch(pd);
  if (patchStatus[0]) {
    gfxSetFgColor(!strcmp(patchStatus, "READY") ? appSettings.colorScheme.textDefault : appSettings.colorScheme.warning);
    gfxPrint(29, 6, patchStatus);
    if (patchError[0]) { gfxPrint(0, 19, patchError); }
  }
  if (instrument->type == InstrumentType::PDVCO) instrumentCommonDrawVoicePostStatic(1);
}
static void cursor(int col,int row) {
  Instrument* instrument=&chipnomadState->project.instruments[cInstrument];
  if(row<3){instrumentCommonDrawCursor(col,row);return;}
  if(instrument->type==InstrumentType::PDVCO && ((col&&row>=4&&row<=8)||row==9) && instrumentCommonDrawVoicePostCursor(col,row)) return;
  int index = macroIndex(row);
  gfxCursor(row==3?11:12,row==3?6:macroY(index),row==3?18:2);
}
static void field(int col,int row,CellState state) {
  Instrument* instrument=&chipnomadState->project.instruments[cInstrument]; InstrumentPDBase* pd=pdBase(instrument);
  if(row<3){instrumentCommonDrawField(col,row,state);return;}
  if(instrument->type==InstrumentType::PDVCO && ((col&&row>=4&&row<=8)||row==9) && instrumentCommonDrawVoicePostField(col,row,state,&instrument->chip.pdVco)) return;
  gfxSetFgColor(state==CellState::focus?appSettings.colorScheme.textValue:appSettings.colorScheme.textDefault);
  if(row==3){const char* n=strrchr(pd->path,PATH_SEPARATOR);gfxPrint(11,6,pd->path[0]?(n?n+1:pd->path):"Load .pd");}
  else { int index = macroIndex(row); if (index >= 0) gfxPrint(12,macroY(index),byteToHex(pd->macro[index])); }
}
static int edit(int col,int row,CellEditAction action) {
  Instrument* instrument=&chipnomadState->project.instruments[cInstrument];
  if(row<3)return instrumentCommonOnEdit(col,row,action);
  if(instrument->type==InstrumentType::PDVCO && ((col&&row>=4&&row<=8)||row==9)) {
    int result = instrumentCommonOnEditVoicePost(col,row,action,&instrument->chip.pdVco);
    if (result) projectModified = 1;
    return result;
  }
  int index = macroIndex(row);
  if(index>=0&&!col){int ok=edit8noLast(action,&pdBase(instrument)->macro[index],16,0,255);if(ok)projectModified=1;return ok;} return 0;
}
static int input(int down,int keys,int) {
  if(screenInstrumentPD.cursorRow!=3){loadButtonDown=0;return 0;} PopupEditInput in=popupEditInput(down,keys,&loadButtonDown);
  if(in==PopupEditInput::hold)return 1;
  if(in==PopupEditInput::open){fileBrowserSetup("LOAD PD PATCH",".pd",appSettings.instrumentPath,loaded,cancelled);screenSetup(&screenFileBrowser,0);return 1;}
  return 0;
}
static int valid(int col, int row) { return row < 4 || (isVCO() && row == 9) || macroIndex(row) >= 0; }
ScreenData screenInstrumentPD={.rows=13,.cursorRow=0,.cursorCol=0,.topRow=0,.selectMode=-1,.selectStartRow=0,.selectStartCol=0,.selectAnchorRow=0,.selectAnchorCol=0,.playbackLevel=ScreenPlaybackLevel::none,.getColumnCount=columns,.drawStatic=drawStatic,.drawCursor=cursor,.drawSelection=NULL,.drawRowHeader=NULL,.drawColHeader=NULL,.drawField=field,.onEdit=edit,.onInput=input,.onRawInput=NULL,.isCellValid=valid,.getLoopRange=NULL};
