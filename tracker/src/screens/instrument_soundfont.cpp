#include <algorithm>
#include <cstring>
#include <map>

#include "chipnomad_lib.h"
#include "corelib_gfx.h"
#include "screen_instrument.h"
#include "selection_popup.h"
#include "soundbank_library.h"
#include "soundbank_browser.h"
#include "project_utils.h"
#include <iterator>

namespace {
std::vector<soundbank::Collection> browserCollections;
std::vector<soundbank::PresetReference> references;
std::vector<std::vector<SelectionItem>> collectionItems;
std::vector<SelectionItem> choices;
std::string lastStatus;
std::string browserPath;
int buttonDown = 0;
bool lastBusy = false;
bool footerErrorDisplayed = false;
extern const AppScreen screenSoundbankBrowser;
Instrument& current() { return chipnomadState->project.instruments[cInstrument]; }
void back() { chipnomadQueuePlaybackStopPreview(chipnomadState,*pSongTrack); screenSetup(&screenInstrument, cInstrument); }
void show();
void up() {
  if (browserPath.empty()) { back(); return; }
  const auto slash = browserPath.rfind('/');
  browserPath = slash == std::string::npos ? std::string() : browserPath.substr(0, slash);
  show();
}
void select(int value) {
  if(value==-2){up();return;}
  if(value<=-1000) {
    const size_t folder = size_t(-1000-value);
    if(folder<browserCollections.size()) { browserPath=browserCollections[folder].path;show(); }
    return;
  }
  if(value<0 || size_t(value)>=references.size())return;
  const auto& ref=references[value];const auto& bank=soundbankEntries()[ref.bank];
  soundbankLoad(cInstrument,bank.id,bank.presets[ref.preset].first);
}
void preview(int value, bool held) {
  if(!held){chipnomadQueuePlaybackStopPreview(chipnomadState,*pSongTrack);return;}
  if(value<0 || size_t(value)>=references.size() || soundbankBusy())return;
  const auto& ref=references[value];const auto& bank=soundbankEntries()[ref.bank];
  if(bank.id==current().chip.soundfont.bank &&
     bank.presets[ref.preset].first==current().chip.soundfont.preset && current().chip.soundfont.prepared)
    chipnomadQueuePlaybackPreviewNote(chipnomadState,*pSongTrack,
      instrumentFirstNote(&chipnomadState->project,cInstrument),cInstrument);
}
const char* loadingStatus() {
  static std::string message;
  message=soundbankBusy()?soundbankLoadingStatus():soundbankInstrumentError(cInstrument);
  return message.c_str();
}
void show() {
  browserCollections=soundbank::browseCollections(soundbankEntries(),browserPath);
  choices.clear();references.clear();collectionItems.clear();
  collectionItems.resize(browserCollections.size());
  int selected=-2;
  for(size_t g=0;g<browserCollections.size();++g) {
    auto& group=browserCollections[g];auto& items=collectionItems[g];items.reserve(group.presets.size()+1);
    for(const auto& ref:group.presets) {
      int index=int(references.size());references.push_back(ref);
      items.push_back({ref.label.c_str(),index,nullptr,0,ref.label.c_str()});
      const auto& bank=soundbankEntries()[ref.bank];
      if(bank.id==current().chip.soundfont.bank && bank.presets[ref.preset].first==current().chip.soundfont.preset)selected=index;
    }
    choices.push_back({group.name.c_str(),-1000-int(g),items.data(),int(items.size()),group.path.c_str(),group.hasFolders});
  }
  if(browserCollections.empty())choices.push_back({soundbankBusy()?"Scanning...":"No presets",-3,nullptr,0,nullptr});
  else if(selected==-2) selected=-1000;
  selectionPopupSetup("SOUNDFONT PRESETS",choices.data(),int(choices.size()),selected,select,up,false,preview,true,true,loadingStatus,true);
  screenSetup(&screenSoundbankBrowser,0);
}
void setup(int n) { screenSelectionPopup.setup(n);lastStatus=soundbankStatus();lastBusy=soundbankBusy(); }
void draw() {
  const bool busy=soundbankBusy();
  // Keep the browsed selection while preparation is in flight. Re-focus on the
  // active preset only after the load/scan completes.
  if(!busy && (lastBusy || lastStatus!=soundbankStatus())){show();return;}
  lastBusy=busy;
  screenSelectionPopup.draw();
}
int input(int down,int keys,int taps){return screenSelectionPopup.onInput(down,keys,taps);}
ScreenPlaybackLevel level(){return ScreenPlaybackLevel::song;}
const AppScreen screenSoundbankBrowser={nullptr,setup,screenSelectionPopup.fullRedraw,draw,input,level};
void open(){
  const std::string bank=current().chip.soundfont.bank;
  const auto disk=bank.substr(0,bank.find("::"));
  const auto slash=disk.rfind('/');
  browserPath=slash==std::string::npos?std::string():disk.substr(0,slash);
  const auto parent=browserPath.rfind('/');
  browserPath=parent==std::string::npos?std::string():browserPath.substr(0,parent);
  if(!soundbankScanned() && !soundbankBusy())soundbankRescan();show();
}
int columns(int row) { return row < 3 ? instrumentCommonColumnCount(row) : row == 4 ? 2 : 1; }
void drawStatic() {
  instrumentCommonDrawStatic();
}
void drawCursor(int col, int row) {
  if (row < 3)
    instrumentCommonDrawCursor(col, row);
  else
    gfxCursor(row == 4 && col ? 28 : 0, row + 3, row == 4 && col ? 4 : 26);
}
void drawField(int col, int row, CellState state) {
  if (row < 3) {
    instrumentCommonDrawField(col, row, state);
    return;
  }
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue
                                          : appSettings.colorScheme.textDefault);
  if (row == 3) {
    gfxClearRect(0,6,32,1);
    gfxPrintf(0,6,"%s  Rescan",soundbankScanning() ? "Scanning..." : soundbankScanned() ? "Scanned" : "Not scanned");
    gfxSetFgColor(appSettings.colorScheme.textInfo);
    gfxClearRect(0,8,32,3);
    gfxPrintf(0,8,"%.32s",soundbankInstrumentStatus(cInstrument).c_str());
    const auto warning=soundbankInstrumentWarning(cInstrument);
    if(!warning.empty())gfxPrintf(0,9,"%.32s",warning.c_str());
    gfxPrintf(0,warning.empty()?9:10,"%.32s",soundbankInstrumentRange(cInstrument).c_str());
    // Keep errors at the bottom of this page, above the global message row.
    // Two detail rows also keep common decoder errors from being cut mid-word.
    auto error=soundbankInstrumentError(cInstrument);
    if(!error.empty() || footerErrorDisplayed)gfxClearRect(0,16,32,3);
    footerErrorDisplayed=!error.empty();
    if(!error.empty()) {
      gfxPrint(0,16,"Error:");
      // The selected preset is already named above. Prefer the reason over a
      // long filesystem path appended by sample preparation.
      const auto path=error.find(": " + soundbankRoot());
      if(path!=std::string::npos)error.resize(path);
      for(int row=17;row<=18 && !error.empty();++row) {
        size_t count=std::min<size_t>(32,error.size());
        if(error.size()>count) {
          const auto space=error.rfind(' ',count);
          if(space!=std::string::npos && space>0)count=space;
        }
        gfxPrintf(0,row,"%.*s",int(count),error.c_str());
        error.erase(0,count);
        while(!error.empty() && error.front()==' ')error.erase(0,1);
      }
    }
  }
  else if (col) gfxPrint(28, 7, "Edit");
  else {
    const auto name = soundbankPresetName(current().chip.soundfont.bank, current().chip.soundfont.preset);
    gfxClearRect(0, 7, 27, 1);
    gfxPrintf(0, 7, "Preset: %.18s", name.empty() ? "Select..." : name.c_str());
  }
}
int edit(int col, int row, CellEditAction action) {
  if (row == 4 && col == 1) { screenSetup(&screenSoundFontEdit, cInstrument); return 1; }
  return row < 3 ? instrumentCommonOnEdit(col, row, action) : 0;
}
int onInput(int down, int keys, int) {
  if (screenInstrumentSoundFont.cursorRow < 3 ||
      (screenInstrumentSoundFont.cursorRow == 4 && screenInstrumentSoundFont.cursorCol == 1)) {
    buttonDown = 0;
    return 0;
  }
  auto action = popupEditInput(down, keys, &buttonDown);
  if (action == PopupEditInput::cycle) {
    if (screenInstrumentSoundFont.cursorRow==4)
      soundbankCycle(cInstrument, keys==(keyEdit|keyRight) ? 1 : -1);
    return 1;
  }
  if (action == PopupEditInput::hold) return 1;
  if (action == PopupEditInput::open) {
    if (screenInstrumentSoundFont.cursorRow==3) soundbankRescan(true); else open();
    return 1;
  }
  return 0;
}
}  // namespace
ScreenData screenInstrumentSoundFont = {.rows = 5,
                                        .cursorRow = 0,
                                        .cursorCol = 0,
                                        .topRow = 0,
                                        .selectMode = -1,
                                        .selectStartRow = 0,
                                        .selectStartCol = 0,
                                        .selectAnchorRow = 0,
                                        .selectAnchorCol = 0,
                                        .playbackLevel = ScreenPlaybackLevel::none,
                                        .getColumnCount = columns,
                                        .drawStatic = drawStatic,
                                        .drawCursor = drawCursor,
                                        .drawSelection = nullptr,
                                        .drawRowHeader = nullptr,
                                        .drawColHeader = nullptr,
                                        .drawField = drawField,
                                        .onEdit = edit,
                                        .onInput = onInput,
                                        .onRawInput = nullptr,
                                        .isCellValid = nullptr,
                                        .getLoopRange = nullptr};


namespace {
namespace soundfontEditor {
ScreenData data;
struct Field {
  const char* name;
  int16_t SoundbankControls::*member;
  int minimum, maximum, coarse, reset;
  const char* unit;
};
const Field fields[] = {
  {"Tune", &SoundbankControls::tuneCents, -2400, 2400, 100, 0, "ct"},
  {"Cutoff", &SoundbankControls::cutoffHz, 20, 20000, 100, -1, "Hz"},
  {"Resonance", &SoundbankControls::resonanceTenths, 0, 400, 10, -1, "dB"},
  {"Env delay", &SoundbankControls::delayMs, 0, 20000, 100, -1, "ms"},
  {"Attack", &SoundbankControls::attackMs, 0, 20000, 100, -1, "ms"},
  {"Hold", &SoundbankControls::holdMs, 0, 20000, 100, -1, "ms"},
  {"Decay", &SoundbankControls::decayMs, 0, 20000, 100, -1, "ms"},
  {"Sustain", &SoundbankControls::sustainPercent, 0, 100, 10, -1, "%"},
  {"Release", &SoundbankControls::releaseMs, 0, 20000, 100, -1, "ms"},
};
int columns(int) { return 1; }
void drawStatic() {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrintf(0, 0, "SOUNDFONT EDIT %02X", cInstrument);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 1, current().name);
  gfxPrint(0, 16, "Bank = preset; Clear = reset");
  gfxPrint(0, 17, "Atk/Hold: next note; Rel: OFF");
  gfxPrint(0, 18, "Decay needs Sustain below 100%");
}
void rowHeader(int row, CellState) {
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, row + 3, row<9 ? fields[row].name : row==9 ? "Volume" : row==10 ? "Pan" : "Bend step");
}
void field(int, int row, CellState state) {
  if (row==11) {
    gfxSetFgColor(state==CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxClearRect(20,row+3,12,1);
    int step=current().chip.soundfont.controls.bendStepCents;
    if (step) gfxPrintf(20,row+3,"%d ct",step); else gfxPrint(20,row+3,"Stock");
    return;
  }
  if (row>=9) {
    gfxSetFgColor(state==CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxClearRect(20,row+3,12,1);
    gfxPrintf(20,row+3,"%02X",row==9 ? current().volume : current().pan); return;
  }
  const auto& f = fields[row];
  const int v = current().chip.soundfont.controls.*(f.member);
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  gfxClearRect(20, row + 3, 12, 1);
  if (row && v < 0) gfxPrint(20, row + 3, "Bank");
  else if (row == 2) gfxPrintf(20, row + 3, "%d.%d dB", v / 10, v % 10);
  else gfxPrintf(20, row + 3, "%d %s", v, f.unit);
}
void cursor(int, int row) { gfxCursor(20, row + 3, 9); }
void colHeader(int, CellState) {}
int edit(int, int row, CellEditAction action) {
  if (row==11) {
    auto& step=current().chip.soundfont.controls.bendStepCents;
    action=convertMultiAction(action);int value=step;
    if (action==CellEditAction::clear) value=1;
    else if (action==CellEditAction::increase) ++value;
    else if (action==CellEditAction::decrease) --value;
    else if (action==CellEditAction::increaseBig) value+=5;
    else if (action==CellEditAction::decreaseBig) value-=5;
    else return 0;
    value=std::clamp(value,0,100);
    if (value!=step) { step=value; projectModified=1; }
    return 1;
  }
  if (row>=9 && row<=10) {
    auto& stored=row==9 ? current().volume : current().pan;
    int value=stored; action=convertMultiAction(action);
    if (action==CellEditAction::clear) value=row==9 ? 255 : 128;
    else if (action==CellEditAction::increase) ++value;
    else if (action==CellEditAction::decrease) --value;
    else if (action==CellEditAction::increaseBig) value+=16;
    else if (action==CellEditAction::decreaseBig) value-=16;
    else return 0;
    value=std::clamp(value,0,255);
    if (value!=stored) { stored=value; projectModified=1; }
    return 1;
  }
  if (row < 0 || row >= int(std::size(fields))) return 0;
  const auto& f = fields[row];
  auto& stored = current().chip.soundfont.controls.*(f.member);
  int value = stored;
  action = convertMultiAction(action);
  if (action == CellEditAction::clear) value = f.reset;
  else if (action == CellEditAction::increase || action == CellEditAction::increaseBig)
    value = row && value < 0 ? f.minimum : std::min(f.maximum, value + (action == CellEditAction::increase ? 1 : f.coarse));
  else if (action == CellEditAction::decrease || action == CellEditAction::decreaseBig) {
    value -= action == CellEditAction::decrease ? 1 : f.coarse;
    if (value < f.minimum) value = row ? -1 : f.minimum;
  } else return 0;
  if (value != stored) { stored = value; projectModified = 1; }
  return 1;
}
void setup(int) { data.cursorRow = data.cursorCol = data.topRow = 0; }
void redraw() { screenFullRedraw(&data); }
void draw() {}
int input(int down, int keys, int taps) {
  if (!keys) chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  if (keys == keyOpt || keys == (keyLeft | keyShift)) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    screenSetup(&screenInstrument, cInstrument); return 1;
  }
  if (keys == (keyEdit | keyPlay)) {
    if (down && !chipnomadGetPlaybackStatus(chipnomadState)->isPlaying)
      chipnomadQueuePlaybackPreviewNote(chipnomadState, *pSongTrack,
        instrumentFirstNote(&chipnomadState->project, cInstrument), cInstrument);
    return 1;
  }
  return screenInput(&data, down, keys, taps);
}
ScreenPlaybackLevel level() { return ScreenPlaybackLevel::phrase; }
struct Initializer {
  Initializer() {
    data = {}; data.rows = std::size(fields)+3; data.selectMode = -1;
    data.getColumnCount = columns; data.drawStatic = drawStatic;
    data.drawRowHeader = rowHeader; data.drawColHeader = colHeader;
    data.drawField = field; data.drawCursor = cursor; data.onEdit = edit;
  }
} initializer;
} // namespace soundfontEditor
} // namespace
const AppScreen screenSoundFontEdit = {
  nullptr, soundfontEditor::setup, soundfontEditor::redraw, soundfontEditor::draw,
  soundfontEditor::input, soundfontEditor::level
};
