#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"
#include "selection_popup.h"

static int modelButtonDown;
static InstrumentAChChid* instrument() { return &chipnomadState->project.instruments[cInstrument].chip.achchid; }
static int isBraids() { return instrument()->wave == AChChidWave::braids; }
static void selectModel(int value) { instrument()->model = (uint8_t)value; projectModified = 1; screenSetup(&screenInstrument, cInstrument); }
static void cancelModel() { screenSetup(&screenInstrument, cInstrument); }
static void openModel() { selectionPopupSetup("BRAIDS MODEL", braidsCategories, braidsCategoryCount, instrument()->model, selectModel, cancelModel); screenSetup(&screenSelectionPopup, 0); }
static int columns(int row) { return row < 3 ? instrumentCommonColumnCount(row) : 2; }
static int isCellValid(int col,int row) {
  if (row < 3 || col) return 1;
  return row <= (isBraids() ? 6 : 4);
}
static void drawStatic() {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0,6,"OSCILLATOR"); gfxPrint(20,6,"303 FILTER");
  gfxSetFgColor(appSettings.colorScheme.textDefault); gfxPrint(0,7,"Wave");
  if (isBraids()) { gfxPrint(0,8,"Model"); gfxPrint(0,9,"Timbre"); gfxPrint(0,10,"Color"); } else gfxPrint(0,8,"Fine");
  gfxPrint(20,7,"Cutoff"); gfxPrint(20,8,"Reso"); gfxPrint(20,9,"Env Mod"); gfxPrint(20,10,"Decay"); gfxPrint(20,11,"Accent");
}
// The track status panel starts at column 34; keep values and cursors before it.
static void drawCursor(int col,int row) { if(row<3) return instrumentCommonDrawCursor(col,row); gfxCursor(col ? 28 : 11, row+4, col ? 6 : 8); }
static void drawField(int col,int row,CellState state) {
  if(row<3) return instrumentCommonDrawField(col,row,state);
  InstrumentAChChid* a=instrument();
  gfxSetFgColor(state==CellState::focus?appSettings.colorScheme.textValue:appSettings.colorScheme.textDefault);
  if(!col && row==3) gfxPrint(11,7,a->wave==AChChidWave::square?"Square":a->wave==AChChidWave::saw?"Saw":"Braids");
  else if(!isBraids() && row==4 && !col) gfxPrintf(11,8,"%+03d",a->fineTune);
  else if(isBraids() && row==4 && !col) gfxPrintf(11,8,"%02u %.6s",a->model,modelCatalogName(InstrumentType::Braids,a->model));
  else if(isBraids() && row==5 && !col) gfxPrint(11,9,byteToHex(controlFromRange(a->timbre,32767)));
  else if(isBraids() && row==6 && !col) gfxPrint(11,10,byteToHex(controlFromRange(a->color,32767)));
  else if(col && row==3) gfxPrintf(28,7,"%u Hz",a->cutoff);
  else if(col && row==4) gfxPrint(28,8,byteToHex(controlFromRange(a->resonance,100)));
  else if(col && row==5) gfxPrint(28,9,byteToHex(controlFromRange(a->envMod,100)));
  else if(col && row==6) gfxPrintf(28,10,"%4ums",a->decay);
  else if(col && row==7) gfxPrint(28,11,byteToHex(controlFromRange(a->accent,100)));
}
static int onEdit(int col,int row,CellEditAction action) {
  if(row<3) return instrumentCommonOnEdit(col,row,action);
  InstrumentAChChid* a=instrument(); int ok=0;
  if(row==3 && !col) ok=edit8noLast(action,(uint8_t*)&a->wave,1,0,2);
  else if(!isBraids() && row==4 && !col) ok=editSigned8(action,&a->fineTune,1,-99,99);
  else if(isBraids() && row==4 && !col) ok=edit8noLast(action,&a->model,1,0,46);
  else if(isBraids() && row==5 && !col) ok=editOscillatorParameter(action,&a->timbre);
  else if(isBraids() && row==6 && !col) ok=editOscillatorParameter(action,&a->color);
  else if(col && row==3) ok=edit16withMinMax(action,&a->cutoff,100,200,20000);
  else if(col && row==4) ok=editNormalized8(action,&a->resonance,100);
  else if(col && row==5) ok=editNormalized8(action,&a->envMod,100);
  else if(col && row==6) ok=edit16withMinMax(action,&a->decay,50,200,2000);
  else if(col && row==7) ok=editNormalized8(action,&a->accent,100);
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
