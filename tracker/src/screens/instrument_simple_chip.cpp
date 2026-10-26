#include "screen_instrument.h"
#include "field_style.h"
#include "selection_popup.h"
#include "user_preset_browser.h"
#include "simple_chip_presets.h"
#include "project_utils.h"
#include "chipnomad_lib.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "waveform_display.h"
#include <cstring>
namespace {
int buttonDown=0;Bitmap* staticWaveformBitmap=nullptr;
Instrument* current(){return &chipnomadState->project.instruments[cInstrument];}
void refreshStaticWaveform(){int row=16;if(!staticWaveformBitmap)staticWaveformBitmap=gfxBitmapCreate(32,3);gfxClearRect(0,row,32,3);renderSimpleChipPreview(staticWaveformBitmap,current()->type,&current()->chip.simpleChip);gfxSetFgColor(appSettings.colorScheme.textInfo);gfxDrawBitmap(staticWaveformBitmap,0,row);auto& p=current()->chip.simpleChip;instrumentCommonDrawEnvelopePreview(p.attack,p.decay,p.sustain,p.release,p.envelopeShape);}
void open(){instrumentPresetOpenSounds();}
int presetColumns(int row){if(row<3)return instrumentCommonColumnCount(row);if(row==7)return 4;if(row==5)return current()->type==InstrumentType::SegaPSG?1:3;if(row==6)return current()->type==InstrumentType::GBPulse?3:current()->type==InstrumentType::GBNoise?2:1;return 1;}
int valid(int,int){return 1;}
// Names on the screen rows 3 (Bank) to 8 (ADSR), seven letters at most.
const char* label(int screenRow){
 const InstrumentType type=current()->type;
 switch(screenRow){
  case 3:return "Bank";
  case 4:return "Preset";
  case 5:return type==InstrumentType::SegaPSG?"Mode":type==InstrumentType::GBPulse?"Duty":"Width";
  case 6:return type==InstrumentType::SegaPSG?"Noise":"Env";
  case 7:return type==InstrumentType::GBPulse?"Sweep":type==InstrumentType::GBNoise?"Div/Shf":"Bass";
  default:return "ADSR";
 }
}
// Rows with several values place them six columns apart from column 8.
int valueX(int col){return kColumnValueX[0]+col*6;}
bool noiseRateBar(int row){return row==5&&current()->type==InstrumentType::SegaPSG;}
int y(int row){return row==3?7:row+4;}
uint8_t* field(int col,int row,int& max){auto& p=current()->chip.simpleChip;max=255;
 if(row==4){max=current()->type==InstrumentType::SegaPSG?2:current()->type==InstrumentType::GBPulse?3:1;return &p.mode;}
 if(row==5){if(current()->type==InstrumentType::SegaPSG){max=3;return &p.noiseRate;}max=col==0?15:col==1?7:1;return col==0?&p.envelopeInitial:col==1?&p.envelopePeriod:&p.envelopeIncrease;}
 if(row==6){if(current()->type==InstrumentType::SegaPSG){max=1;return &p.segaBassExtension;}if(current()->type==InstrumentType::GBPulse){max=col==2?1:7;return col==0?&p.sweepPeriod:col==1?&p.sweepShift:&p.sweepNegate;}if(current()->type==InstrumentType::GBNoise){max=col==0?7:13;return col==0?&p.noiseDivisor:&p.noiseShift;}return nullptr;}
 if(row==7)return col==0?&p.attack:col==1?&p.decay:col==2?&p.sustain:&p.release;
 return nullptr;
}
void drawStatic(){instrumentCommonDrawStatic();gfxSetFgColor(appSettings.colorScheme.textLabel);for(int row=3;row<=8;++row)gfxPrint(0,row+3,label(row));gfxSetFgColor(appSettings.colorScheme.textInfo);gfxPrint(0,13,current()->type==InstrumentType::SegaPSG?"Bass extends tone / linked noise":"Env: initial / step / rise");gfxPrint(0,14,current()->type==InstrumentType::GBNoise?"Fixed noise rate; keyboard unpitched":"Software ADSR shapes native source");refreshStaticWaveform();}
void presetDrawCursor(int col,int row){if(row<3)instrumentCommonDrawCursor(col,row);else gfxCursor(valueX(col),y(row),row==3?25:row==4?12:noiseRateBar(row)?kFieldNumberWidth:2);}
void presetDrawField(int col,int row,CellState state){if(row<3){instrumentCommonDrawField(col,row,state);return;}refreshStaticWaveform();auto& p=current()->chip.simpleChip;
 // The selected field's label lights up with its value; choices are names.
 fieldDrawLabel(0,y(row),label(row+(row>=3?1:0)),state);
 const bool named=row==4||(row==6&&current()->type==InstrumentType::SegaPSG);
 gfxSetFgColor(named?fieldNameColor(state):fieldValueColor(state));
 gfxClearRect(valueX(col),y(row),row==3?25:row==4||named||noiseRateBar(row)?kFieldNumberWidth+4:5,1);
 if(row==3)gfxPrintf(valueX(0),7,"%.25s",current()->name);
 else if(row==4){const char* sega[]={"Tone","White noise","Periodic"};const char* duty[]={"12.5%","25%","50%","75%"};gfxPrint(valueX(0),8,current()->type==InstrumentType::SegaPSG?sega[p.mode]:current()->type==InstrumentType::GBPulse?duty[p.mode]:p.mode?"7 bit":"15 bit");}
 else if(row==6&&current()->type==InstrumentType::SegaPSG)gfxPrint(valueX(0),10,p.segaBassExtension?"Extended":"Chip");
 else{int max;auto* v=field(col,row,max);if(!v)return;
  if(noiseRateBar(row))fieldDrawNumber(valueX(col),y(row),*v,state,*v/float(max));
  else gfxPrintf(valueX(col),y(row),"%02X",*v);}
}
int presetEdit(int col,int row,CellEditAction action){if(row<3)return instrumentCommonOnEdit(col,row,action);if(row==3){open();return 1;}int max;auto* value=field(col,row,max);if(!value)return 0;
 int ok=edit8noLast(action,value,max<16?1:16,0,max);if(ok)projectModified=1;return ok;}
int mappedRow(int row){return row>3?row-1:row;}
int columns(int row){return row==3?1:presetColumns(mappedRow(row));}
void drawCursor(int col,int row){if(row==3)gfxCursor(kColumnValueX[0],6,25);else presetDrawCursor(col,mappedRow(row));}
void drawField(int col,int row,CellState state){
 if(row!=3){presetDrawField(col,mappedRow(row),state);return;}
 fieldDrawLabel(0,6,"Bank",state);
 gfxSetFgColor(fieldValueColor(state));
 gfxClearRect(kColumnValueX[0],6,25,1);gfxPrintf(kColumnValueX[0],6,"%.25s",instrumentPresetCollectionName());
}
int edit(int col,int row,CellEditAction action){
 if(row==3){instrumentPresetOpenCollections();return 1;}
 return presetEdit(col,mappedRow(row),action);
}
int input(int down,int keys,int){
 int row=screenInstrumentSimpleChip.cursorRow;
 if(row!=3&&row!=4){buttonDown=0;return 0;}
 auto action=popupEditInput(down,keys,&buttonDown);
 if(action==PopupEditInput::cycle){int direction=keys==(keyEdit|keyRight)?1:-1;if(row==3)instrumentPresetCycleCollection(direction);else instrumentPresetCycle(direction);return 1;}
 if(action==PopupEditInput::hold)return 1;
 if(action==PopupEditInput::open){if(row==3)instrumentPresetOpenCollections();else open();return 1;}
 return 0;
}
}
ScreenData screenInstrumentSimpleChip={.rows=9,.cursorRow=0,.cursorCol=0,.topRow=0,.selectMode=-1,.selectStartRow=0,.selectStartCol=0,.selectAnchorRow=0,.selectAnchorCol=0,.playbackLevel=ScreenPlaybackLevel::none,.getColumnCount=columns,.drawStatic=drawStatic,.drawCursor=drawCursor,.drawSelection=nullptr,.drawRowHeader=nullptr,.drawColHeader=nullptr,.drawField=drawField,.onEdit=edit,.onInput=input,.onRawInput=nullptr,.isCellValid=valid,.getLoopRange=nullptr};
