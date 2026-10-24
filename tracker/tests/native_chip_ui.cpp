#include <filesystem>
// Developer-only offscreen production UI integration; never shipped.
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include "chipnomad_lib.h"
#include "app.h"
#include "corelib_font.h"
#include "corelib_gfx.h"
#include "screens.h"
#include "screen_instrument.h"
#include "selection_popup.h"
#include "waveform_display.h"
#include "monitor_display.h"
#include "dx7_patch.h"
#include "sid_patch.h"
#include "project_utils.h"
#include "copy_paste.h"
#include "insert_fx.h"
#include "help.h"
#include "help.h"
#include "fm_catalog.h"
#include "user_presets.h"
#include "user_preset_browser.h"
extern SDL_Renderer* renderer;
static const char* output;
static void require(bool condition,const char* why){if(!condition){fprintf(stderr,"FAIL: %s\n",why);exit(2);}}
static void capture(const char* name){appDraw();auto* s=SDL_CreateRGBSurfaceWithFormat(0,640,480,32,SDL_PIXELFORMAT_ARGB8888);require(s,"surface");require(SDL_RenderReadPixels(renderer,nullptr,s->format->format,s->pixels,s->pitch)==0,"pixels");char path[2048];snprintf(path,sizeof(path),"%s/%s.bmp",output,name);require(SDL_SaveBMP(s,path)==0,"capture");SDL_FreeSurface(s);}
static void key(int down,int keys){appDraw();currentScreen->onInput(down,keys,1);appDraw();}
static void tapEdit(){key(1,keyEdit);key(0,0);}
int main(int argc,char** argv){
  if(argc!=2&&argc!=3)return 1;output=argv[1];initDefaultAppSettings();appSettings.screenWidth=640;appSettings.screenHeight=480;fontSetCurrent(fontGetDefault());require(!gfxSetup(&appSettings.screenWidth,&appSettings.screenHeight),"SDL dummy setup");
  chipnomadState=chipnomadCreate();require(chipnomadState,"state");require(!projectLoad(&chipnomadState->project,"projects/gm-midi-demo.cct"),"fixture");
  if(argc==3 && !strcmp(argv[2],"--live-waveform-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();cInstrument=0;
    appSettings.waveformRefreshHz=1; // Exercise cached frames between refreshes.
    auto pixels=[] {
      SDL_Rect region{0,16*gfxGetCharHeight(),32*gfxGetCharWidth(),3*gfxGetCharHeight()};
      std::vector<uint32_t> result(region.w*region.h);
      require(SDL_RenderReadPixels(renderer,&region,SDL_PIXELFORMAT_ARGB8888,
                                   result.data(),region.w*4)==0,"scope pixels");
      return result;
    };
    for(auto type:{InstrumentType::SID,InstrumentType::OPLL,InstrumentType::GenesisFM,
                  InstrumentType::MME,InstrumentType::Braids,InstrumentType::Plaits,
                  InstrumentType::SegaPSG}) {
      auto& instrument=chipnomadState->project.instruments[0];
      getInstrumentFunctions(type).init(&instrument);
      // No audio thread runs in this harness. Feed the UI snapshot a known
      // monitor trace and deliberately expose the inactive render-chunk gap.
      auto& status=chipnomadState->uiPlaybackStatus;
      status.isPlaying=0;status.tracks[0].note.instrument=0;
      memset(chipnomadState->voiceMonitors,0,sizeof(chipnomadState->voiceMonitors));
      auto& monitor=chipnomadState->voiceMonitors[0];
      for(int n=0;n<VOICE_MONITOR_SAMPLES;++n)monitor.samples[n]=std::sin(n*0.083f)*0.7f;
      screenSetup(&screenInstrument,0);appDraw();const auto idle=pixels();
      monitor.active=1;appDraw();const auto live=pixels();
      require(live!=idle,"live trace differs from static preview");
      currentScreen->fullRedraw();appDraw();
      require(pixels()==live,"static redraw cannot show through cached live trace");
      monitor.active=0;appDraw();
      require(pixels()==live,"one inactive monitor read keeps live trace");
      monitor.active=1;appDraw();
      require(pixels()==live,"monitor recovery does not flash static");
      status.isPlaying=1;monitor.active=0;SDL_Delay(300);appDraw();
      const auto rest=pixels();
      require(rest!=idle&&rest!=live,"song rest remains a live silence trace");
      monitor.active=1;appDraw();
      require(pixels()==live,"next note resumes immediately at low refresh rate");
      monitor.active=0;status.isPlaying=0;SDL_Delay(300);appDraw();
      const auto stopped=pixels();
      require(stopped!=live&&stopped!=rest,"sustained idle restores static preview");
      appDraw();require(pixels()==stopped,"idle preview is restored once, not repeatedly regenerated");
      monitor.active=1;appDraw();monitor.active=0;
      screenSetup(&screenInstrument,0);appDraw();
      require(pixels()!=live,"screen reentry cannot retain a stale live trace");
      getInstrumentFunctions(type).init(&chipnomadState->project.instruments[1]);
      monitor.active=1;appDraw();
      currentScreen->onInput(1,keyOpt|keyRight,1);appDraw();
      require(cInstrument==1&&pixels()!=live,"instrument change cannot retain another instrument's live trace");
      printf("Live waveform transitions passed for instrument type %d\n",int(type));
    }
    capture("waveform-returned-to-idle");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--song-waveform-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();
    auto& project=chipnomadState->project;
    *pSongRow=*pChainRow=*pSongTrack=0;
    project.song[0][0]=0;project.chains[0].rows[0].phrase=0;
    getInstrumentFunctions(InstrumentType::OPLL).init(&project.instruments[0]);
    for(auto& mod:project.instruments[0].modulation) {
      mod.type=ModulationType::SLFO;mod.p1=static_cast<uint8_t>(LFOShape::wavetable);
    }
    struct Page { const AppScreen* screen; const char* name; };
    const Page pages[]={{&screenSong,"song"},{&screenChain,"chain"},
      {&screenPhrase,"phrase"},{&screenTable,"table"},{&screenInstrument,"instrument"},
      {&screenModulation,"modulation"},{&screenMixer,"mixer"},
      {&screenGroove,"groove"},{&screenInstrumentPool,"pool"}};
    for(int enabled:{0,1}) {
      appSettings.persistentWaveform=enabled;
      for(const auto& page:pages) {
        // Exercise every transition from the offset SONG page back to an editor.
        screenSetup(&screenSong,0);appDraw();
        screenSetup(page.screen,0);appDraw();
        int offset=enabled && page.screen==&screenSong ? 2 : 0;
        require(gfxGetContentRowOffset()==offset,"Only SONG shifts page content");
        require(screenVisibleRows()==16-offset,"Full editor row capacity");
        if(page.screen==&screenSong) {
          require(screenTouchTap(3,3+offset),"SONG first row touch follows waveform");
          require(*pSongRow==0,"SONG first row selected");
        }
        if(page.screen==&screenPhrase) {
          for(int row:{14,15}) {
            project.phrases[0].rows[row].note=48;
            require(screenTouchTap(3,3+row),"Restored bottom phrase row touch");
            key(1,keyEdit|keyRight);key(0,0);
            require(project.phrases[0].rows[row].note==49,"Bottom row edit reaches correct note");
          }
        }
        if(page.screen==&screenModulation) {
          require(screenTouchTap(7,18),"Last modulation field accessible");
          require(!screenTouchTap(7,10),"Modulation gap is not a field");
        }
        char name[64];snprintf(name,sizeof(name),"%s-waveform-%d",page.name,enabled);
        capture(name);
      }
    }
    printf("Song-only waveform: transitions, full editor rows, bottom Phrase edits and Modulation touch passed in both modes\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--sid-home-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();
    static_assert(sizeof(FXId)==2,"Current lane commands must remain 16-bit");
    struct Control { int x,y,col,row,parameter; FXId command; };
    const Control controls[]={
      {9,9,0,5,sidWave,fxSWV},{24,9,1,5,sidPulse,fxSCP},
      {9,12,0,6,sidFilterMode,fxSFTY},{9,13,0,7,sidCutoff,fxSCT},
      {26,13,1,7,sidResonance,fxSRN},{7,14,0,8,sidAttack,fxSAT},
      {12,14,1,8,sidDecay,fxSDE},{17,14,2,8,sidSustain,fxSSU},
      {22,14,3,8,sidRelease,fxSRL}};
    for(int header:{0,1}) {
      auto& project=chipnomadState->project;auto& instrument=project.instruments[0];
      getInstrumentFunctions(InstrumentType::SID).init(&instrument);
      std::snprintf(instrument.chip.sid.presetName,sizeof(instrument.chip.sid.presetName),"Long preset name beside EDIT");
      project.phrases[0].rows[0].fx[0]={fxSCT,200};
      project.phrases[0].rows[0].fx[1]={FX_ID_EMPTY,17};
      const auto lane=project.phrases[0].rows[0];
      appSettings.persistentWaveform=header;screenSetup(&screenInstrument,0);appDraw();
      require(screenInstrumentOPL.rows==9,"SID logical row count");
      int offset=screenScopeRows(currentScreen);
      for(const auto& c:controls){
        instrument.chip.sid.value[c.parameter]=1;
        require(screenTouchTap(c.x,c.y+offset),"SID field touch");
        require(screenInstrumentOPL.cursorCol==c.col&&screenInstrumentOPL.cursorRow==c.row,"SID exact field mapping");
        require(screenTouchAdjust(c.x,c.y+offset)==touchAdjustCoarse,"SID selected value adjusts");
        require(screenInstrumentOPL.onEdit(c.col,c.row,CellEditAction::increase),"SID field edits");
        require(instrument.chip.sid.value[c.parameter]==2,"SID edit reaches correct native parameter");
        NativeFXInfo info{};require(instrumentNativeFXInfo(&instrument,c.command,&info),"SID lane FX metadata");
        FXCell fx{FX_ID_EMPTY,0},last{FX_ID_EMPTY,0};
        selectInstrumentFX(&fx,c.command,0);
        require(fx.command==c.command&&fx.value==info.preset,"Lane FX starts from saved native value");
        for(int table:{0,1}){
          fx.value=info.maximum;editFXValue(CellEditAction::increase,&fx,&last,table,0);
          require(fx.value==info.maximum,"Phrase/Table native FX upper bound");
          fx.value=info.minimum;editFXValue(CellEditAction::decrease,&fx,&last,table,0);
          require(fx.value==info.minimum,"Phrase/Table native FX lower bound");
        }
      }
      require(!std::memcmp(&lane,&project.phrases[0].rows[0],sizeof(lane)),"Preset edits preserve lane commands, values and 16-bit empty sentinel");
      require(!screenTouchTap(9,11+offset),"Filter heading not editable");
      require(!screenTouchTap(16,13+offset),"Filter column gap not editable");
      require(screenTouchTap(9,13+offset),"Cutoff selected");
      require(screenTouchAdjust(26,13+offset)==touchAdjustNone,"Resonance drag must not change cutoff");
      require(screenTouchTap(30,7+offset),"SID EDIT touch");
      require(screenInstrumentOPL.cursorRow==4&&screenInstrumentOPL.cursorCol==1,"EDIT is beside preset");
      tapEdit();require(currentScreen==&screenSIDEdit,"Real EDIT input opens SID editor, not preset browser");
      key(1,keyOpt);key(0,0);require(currentScreen==&screenInstrument,"SID editor returns home");
      screenMessage(0,"");screenSetup(&screenInstrument,0);appDraw();
      char name[64];snprintf(name,sizeof(name),"sid-home-header-%d",header);capture(name);
    }
    printf("SID home: both headers, nine native controls, exact touch/drag, EDIT routing and 16-bit Phrase/Table FX semantics passed\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--insert-popup-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();
    auto& project=chipnomadState->project;
    *pSongRow=*pChainRow=*pSongTrack=0;
    project.song[0][0]=0;project.chains[0].rows[0].phrase=0;
    phraseClear(&project.phrases[0]);
    getInstrumentFunctions(InstrumentType::Braids).init(&project.instruments[0]);
    project.phrases[0].rows[0].instrument=0;
    for(int header:{0,1}) {
      appSettings.persistentWaveform=header;
      screenSetup(&screenPhrase,-1);appDraw();screenPhrase.init();
      for(int col=0;col<3;++col)key(1,keyRight);
      key(1,keyEdit|keyUp);
      for(int module=0;module<insertModuleCount;++module)for(int slot=0;slot<2;++slot) {
        insertSelect(&project.trackInserts[0][slot],module);
        const auto& descriptor=insertDescriptor(module);
        for(int parameter=0;parameter<8;++parameter) {
          FX fx=FX(fxF11+slot*8+parameter);
          const char* help=helpFXDescription(fx,0);
          require(strstr(help,descriptor.name),"popup names selected insert module");
          require(strstr(help,parameter<descriptor.count?descriptor.parameters[parameter].name:"no effect"),"popup describes active or unused parameter");
          fxEditFullDraw(fx,0,0);
          uint8_t selected[]={uint8_t(fx),91},last[]={0,0};
          fxEditInput(0,1,selected,last);
          if(parameter<descriptor.count) require(selected[0]==fx,"active control selectable");
          else require(selected[0]!=fx,"unused control is not selectable");
          if(selected[0]>=fxF11&&selected[0]<=fxF28) {
            const int a=selected[0]-fxF11;
            require(a%8<insertDescriptor(project.trackInserts[0][a/8].module).count,"fallback is active");
          }
          require(selected[1]==91,"selection preserves automation value");
          if((module==insertRotary && (parameter==0||parameter==4)) ||
             (module==insertCompressor && slot==0 && parameter==5)) {
            char name[100];snprintf(name,sizeof(name),"insert-%d-slot-%d-param-%d-header-%d",module,slot,parameter,header);capture(name);
          }
        }
      }
      screenPhrase.onInput(0,0,1);
    }
    auto eventKey=[](int button,bool down) {
      appDraw(); // Commit pending screenSetup before dispatching the next input.
      MainLoopEventData event{};event.type=down?MainLoopEvent::keyDown:MainLoopEvent::keyUp;
      event.data.input={InputDeviceType::logical,button};appOnEvent(event);
      appDraw(); // Production screen changes are deferred until the next draw.
    };
    struct Route { const AppScreen *from,*left,*right; };
    const Route routes[]={
      {&screenProject,&screenMixer,&screenChain},{&screenSettings,&screenMixer,&screenChain},
      {&screenSynthSettings,&screenMixer,&screenChain},{&screenMixerSettings,&screenMixer,&screenChain},
      {&screenGraphicsSettings,&screenMixer,&screenChain},{&screenTrackVisuals,&screenMixer,&screenChain},
      {&screenMidi,&screenMixer,&screenChain},{&screenMidiChannelMap,&screenMixer,&screenChain},
      {&screenMidiCC,&screenMixer,&screenChain},{&screenGroove,&screenChain,&screenInstrument},
      {&screenModulation,&screenPhrase,&screenTable},{&screenInsertFX,&screenPhrase,&screenTable},
      {&screenAYWavetable,&screenInstrument,&screenTable},{&screenInstrumentPool,&screenPhrase,&screenTable}};
    for(const auto& route:routes)for(int direction:{keyLeft,keyRight}) {
      screenSetup(route.from,0);appDraw();eventKey(keyShift,true);eventKey(direction,true);
      const auto* destination=direction==keyLeft?route.left:route.right;
      require(currentScreen==destination,"Shift leaves branch for expected spine neighbour");
      eventKey(direction,false);require(currentScreen==destination,"direction release does not navigate twice");
      eventKey(keyShift,false);
    }
    screenSetup(&screenMixer,0);eventKey(keyShift,true);eventKey(keyUp,true);eventKey(keyUp,false);
    require(screenMixerGetPage()==1,"reverb page entered");eventKey(keyLeft,true);eventKey(keyLeft,false);eventKey(keyShift,false);
    require(currentScreen==&screenMixer&&screenMixerGetPage()==0,"left edge returns to mixer");
    printf("Insert popup production UI and Shift spine navigation passed: all modules, both slots, active-only selection, waveform off/on\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--user-step-ui-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();
    namespace fs=std::filesystem;
    fs::path dxRoot="instruments/USER/dx7",first=dxRoot/"A-step-test.syx",second=dxRoot/"B-step-test.zip";
    require(!fs::exists(first)&&!fs::exists(second),"isolated step fixtures");
    PresetZip zip;std::vector<uint8_t> bytes;std::string error;
    require(zip.open("../../tests/fixtures/preset-packs/compressed.zip",error)&&zip.read("DX7/Test.syx",bytes,error),"synthetic bank");
    FILE* f=fopen(first.string().c_str(),"wb");require(f,"fixture file");require(fwrite(bytes.data(),1,bytes.size(),f)==bytes.size(),"fixture write");fclose(f);
    fs::copy_file("../../tests/fixtures/preset-packs/compressed.zip",second);
    getInstrumentFunctions(InstrumentType::DX7).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
    instrumentPresetCycleCollection(-1);require(!strcmp(instrumentPresetCollectionName(),"USER"),"USER filter");
    instrumentPresetOpenSounds();tapEdit();key(1,keyUp);key(1,keyUp);tapEdit(); // Back row, then last voice of A.
    require(chipnomadState->project.instruments[0].chip.dx7.sourceProgram==31,"selected A last voice");
    screenInstrumentOPL.cursorRow=4;key(1,keyEdit);key(1,keyEdit|keyRight);key(0,0);
    require(chipnomadState->project.instruments[0].chip.dx7.sourceProgram==0,"A last to zipped B first despite duplicate names");
    instrumentPresetOpenSounds();capture("user-step-dx7");key(1,keyOpt);key(1,keyOpt);
    require(currentScreen==&screenSelectionPopup,"reopened at zipped B bank, not loose A");key(1,keyOpt);key(1,keyOpt);
    screenInstrumentOPL.cursorRow=4;key(1,keyEdit);key(1,keyEdit|keyLeft);key(0,0);
    require(chipnomadState->project.instruments[0].chip.dx7.sourceProgram==31,"B first to A last");
    instrumentPresetOpenSounds();key(1,keyOpt);key(1,keyOpt);require(currentScreen==&screenInstrument,"reverse restored loose A location");
    key(1,keyEdit);for(int n=0;n<33;++n)key(1,keyEdit|keyRight);key(0,0);
    require(chipnomadState->project.instruments[0].chip.dx7.sourceProgram==0,"end of USER wraps to A first");
    key(1,keyEdit);key(1,keyEdit|keyLeft);key(0,0);require(chipnomadState->project.instruments[0].chip.dx7.sourceProgram==31,"reverse wraps to B last");
    fs::remove(first);key(1,keyEdit);key(1,keyEdit|keyRight);key(0,0);
    require(chipnomadState->project.instruments[0].chip.dx7.sourceProgram==0,"missing A bank skipped");fs::remove(second);
    fs::path segaRoot="instruments/USER/sega",a=segaRoot/"A-step-test",b=segaRoot/"B-step-test";
    require(!fs::exists(a)&&!fs::exists(b),"isolated CNI folders");fs::create_directory(a);fs::create_directory(b);
    getInstrumentFunctions(InstrumentType::SegaPSG).init(&chipnomadState->project.instruments[0]);
    strcpy(chipnomadState->project.instruments[0].name,"DUPLICATE");
    chipnomadState->project.instruments[0].chip.simpleChip.mode=0;require(!instrumentSave(&chipnomadState->project,(a/"One.cni").string().c_str(),0),"first CNI");
    chipnomadState->project.instruments[0].chip.simpleChip.mode=1;require(!instrumentSave(&chipnomadState->project,(b/"Two.cni").string().c_str(),0),"second CNI");
    screenSetup(&screenInstrument,0);appDraw();instrumentPresetCycleCollection(-1);instrumentPresetOpenSounds();tapEdit();tapEdit();
    screenInstrumentSimpleChip.cursorRow=4;key(1,keyEdit);key(1,keyEdit|keyRight);key(0,0);
    require(chipnomadState->project.instruments[0].chip.simpleChip.mode==1,"CNI steps across folders with identical names");capture("user-step-sega");
    key(1,keyEdit);key(1,keyEdit|keyRight);key(0,0);require(chipnomadState->project.instruments[0].chip.simpleChip.mode==0,"CNI wraps");
    key(1,keyEdit);key(1,keyEdit|keyLeft);key(0,0);require(chipnomadState->project.instruments[0].chip.simpleChip.mode==1,"CNI reverse wraps");
    fs::remove(a/"One.cni");fs::remove(b/"Two.cni");fs::remove(a);fs::remove(b);
    printf("USER stepping across SYX ZIP and CNI folders, duplicate names, wrap, focus and missing files passed; no audio tests run\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--bank-cycle-ui-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();
    std::vector<FMPresetEntry> factory,builtins;
    require(loadFMCatalog("instruments/FACTORY/catalog.tsv",factory),"catalog");
    require(loadFMCatalog("instruments/FACTORY/builtins.tsv",builtins),"builtins");
    factory.insert(factory.end(),builtins.begin(),builtins.end());
    for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7,InstrumentType::SID,InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}) {
      getInstrumentFunctions(type).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);
      auto& screen=(type==InstrumentType::SegaPSG||type==InstrumentType::GBPulse||type==InstrumentType::GBNoise)?screenInstrumentSimpleChip:screenInstrumentOPL;
      screen.cursorRow=3;screen.cursorCol=0;
      auto original=chipnomadState->project.instruments[0];int modified=projectModified;
      std::vector<std::string> expected={"ALL"};
      for(const auto& group:factoryCollections(factory,type))expected.push_back(&screen==&screenInstrumentSimpleChip?"Factory":group.name);
      expected.push_back("USER");
      // Repeated directions while EDIT stays held, including wraparound.
      key(1,keyEdit);
      for(size_t n=1;n<=expected.size();++n){key(1,keyEdit|keyRight);require(expected[n%expected.size()]==instrumentPresetCollectionName(),"forward bank order");}
      key(0,0);require(currentScreen==&screenInstrument,"release after cycling does not open popup");
      key(1,keyEdit);
      for(size_t n=1;n<=expected.size();++n){key(1,keyEdit|keyLeft);require(expected[(expected.size()-n)%expected.size()]==instrumentPresetCollectionName(),"reverse bank order");}
      key(0,0);require(currentScreen==&screenInstrument,"reverse release stays on instrument");
      require(!memcmp(&original,&chipnomadState->project.instruments[0],sizeof(original))&&modified==projectModified,"bank cycling changes no instrument or dirty flag");
      key(1,keyEdit);key(1,keyEdit|keyLeft);key(0,0);
      if(type==InstrumentType::DX7||type==InstrumentType::SegaPSG){char name[64];snprintf(name,sizeof(name),"bank-cycle-%d",int(type));capture(name);}
      tapEdit();require(currentScreen==&screenSelectionPopup,"EDIT tap still opens chooser");key(1,keyOpt);
    }
    printf("Bank EDIT left/right order, wrap, release and tap passed for all 11 engines; no audio tests run\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--bank-ui-only")) {
    // Structural UI checks only: no audio initialization, playback or audition.
    screensInitAll();waveformDisplayInit();monitorDisplayInit();
    std::vector<FMPresetEntry> factory;
    require(loadFMCatalog("instruments/FACTORY/builtins.tsv",factory),"simple factory catalog");
    for(auto type:{InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}) {
      auto found=std::find_if(factory.begin(),factory.end(),[&](const auto& e){return e.type==int(type);});
      require(found!=factory.end(),"simple factory entry");
      auto path=std::filesystem::path("instruments/USER")/userPresetFolder(type)/"UI-flat-test.cni";
      require(!std::filesystem::exists(path),"isolated user fixture");
      require(loadFMPreset("instruments/FACTORY",*found,&chipnomadState->project,0),"factory fixture load");
      strcpy(chipnomadState->project.instruments[0].name,"UI USER");
      require(!instrumentSave(&chipnomadState->project,path.string().c_str(),0),"user fixture save");
      require(loadFMPreset("instruments/FACTORY",*found,&chipnomadState->project,0),"factory fixture restore");
      screenSetup(&screenInstrument,0);
      char name[64];snprintf(name,sizeof(name),"bank-label-%d",int(type));capture(name);
      instrumentPresetOpenCollections();snprintf(name,sizeof(name),"bank-options-%d",int(type));capture(name);key(1,keyOpt);
      instrumentPresetOpenSounds();require(selectionPopupIsFullWidth(),"ALL has no category panel");
      key(1,keyUp);snprintf(name,sizeof(name),"flat-all-%d",int(type));capture(name);tapEdit();
      require(!strcmp(chipnomadState->project.instruments[0].name,"UI USER"),"ALL includes user preset");
      instrumentPresetOpenCollections();key(1,keyDown);tapEdit();
      require(!strcmp(instrumentPresetCollectionName(),"Factory"),"Factory choice retained");
      instrumentPresetOpenSounds();require(selectionPopupIsFullWidth(),"Factory has no category panel");tapEdit();
      require(strcmp(chipnomadState->project.instruments[0].name,"UI USER"),"Factory excludes user preset");
      instrumentPresetOpenCollections();key(1,keyDown);tapEdit();
      require(!strcmp(instrumentPresetCollectionName(),"USER"),"USER choice retained");
      instrumentPresetOpenSounds();require(selectionPopupIsFullWidth(),"USER folder browser retained");tapEdit();
      require(!strcmp(chipnomadState->project.instruments[0].name,"UI USER"),"USER selects own preset");
      std::filesystem::remove(path);
    }
    getInstrumentFunctions(InstrumentType::DX7).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);
    capture("bank-label-dx7");instrumentPresetOpenSounds();require(!selectionPopupIsFullWidth(),"DX7 keeps category panel");key(1,keyOpt);
    printf("Bank labels and Sega/GB ALL Factory USER navigation passed; no audio tests run\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  chipnomadInitChips(chipnomadState,48000,nullptr);chipnomadReserveRenderBuffers(chipnomadState,1024);screensInitAll();waveformDisplayInit();monitorDisplayInit();
  if(argc==3 && !strcmp(argv[2],"--presets-only")) {
    std::vector<FMPresetEntry> factory, builtins;
    require(loadFMCatalog("instruments/FACTORY/catalog.tsv",factory),"factory catalog");
    require(loadFMCatalog("instruments/FACTORY/builtins.tsv",builtins),"builtin catalog");
    factory.insert(factory.end(),builtins.begin(),builtins.end());
    for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7,InstrumentType::SID,InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}) {
      auto found=std::find_if(factory.begin(),factory.end(),[&](const auto& e){return e.type==int(type);});
      require(found!=factory.end(),"engine factory pack");
      require(loadFMPreset("instruments/FACTORY",*found,&chipnomadState->project,0),"factory ZIP loads on ARM");
      screenSetup(&screenInstrument,0);appDraw();
      char name[64];snprintf(name,sizeof(name),"factory-engine-%d",int(type));capture(name);
      instrumentPresetOpenCollections();snprintf(name,sizeof(name),"collections-engine-%d",int(type));capture(name);
      key(1,keyDown);tapEdit();instrumentPresetOpenSounds();key(1,keyRight);tapEdit();
      require(currentScreen==&screenInstrument,"factory collection selection for every engine");
      openUserPresetBrowser();appDraw();require(currentScreen==&screenSelectionPopup,"USER popup for every engine");
      key(1,keyOpt);require(currentScreen==&screenInstrument,"empty USER back");
    }
    namespace fs=std::filesystem;
    const char* fixture="../../tests/fixtures/preset-packs/compressed.zip";
    const char* oplPack="instruments/USER/opl3/UI-test.zip";
    const char* dxPack="instruments/USER/dx7/UI-test.zip";
    require(!fs::exists(oplPack)&&!fs::exists(dxPack),"isolated ZIP fixtures");
    fs::copy_file(fixture,oplPack);fs::copy_file(fixture,dxPack);
    getInstrumentFunctions(InstrumentType::OPL3).init(&chipnomadState->project.instruments[0]);
    screenSetup(&screenInstrument,0);appDraw();
    screenInstrumentOPL.onEdit(0,3,CellEditAction::tap);key(1,keyUp);tapEdit(); // Collection -> USER
    screenInstrumentOPL.onEdit(0,4,CellEditAction::tap);capture("user-root");
    tapEdit();capture("user-zip-folders");tapEdit();tapEdit();capture("user-zip-presets");
    auto before=std::make_unique<Project>(chipnomadState->project);
    key(1,keyEdit);key(1,keyEdit|keyPlay);
    std::vector<float> audio(2048);double energy=0;
    for(int n=0;n<12;++n){chipnomadRender(chipnomadState,audio.data(),1024);for(float x:audio)energy+=x*x;}
    require(energy>1e-7,"USER ZIP audition audio");
    require(!memcmp(before.get(),&chipnomadState->project,sizeof(Project)),"USER audition leaves song unchanged");
    key(0,keyPlay);tapEdit();require(currentScreen==&screenInstrument,"USER ZIP commit");
    require(!strcmp(chipnomadState->project.instruments[0].name,"Acoustic Grand"),"USER ZIP chosen patch");
    getInstrumentFunctions(InstrumentType::DX7).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);
    openUserPresetBrowser();appDraw();
    UserPresets dxListing;std::string error;
    dxListing.setup("instruments/USER/dx7",InstrumentType::DX7,"instruments/banks/dx7");
    require(dxListing.refresh(error),"DX7 USER listing");
    size_t packIndex=0;
    while(packIndex<dxListing.items().size()&&dxListing.items()[packIndex].name!="UI-test.zip")++packIndex;
    require(packIndex<dxListing.items().size(),"DX7 ZIP listed alongside previous banks folder");
    for(size_t i=0;i<packIndex;++i)key(1,keyDown);
    tapEdit();key(1,keyDown);tapEdit();tapEdit();capture("user-dx7-bank-voices");
    tapEdit();require(currentScreen==&screenInstrument,"zipped DX7 bank voice commit");
    require(!strcmp(chipnomadState->project.instruments[0].name,"TEST VOICE"),"synthetic DX7 bank patch");
    // ALL flattens USER bank voices into Unsorted while USER stays a peer.
    instrumentPresetOpenCollections();capture("dx7-collections");tapEdit();
    require(!strcmp(instrumentPresetCollectionName(),"ALL"),"ALL collection selected");
    instrumentPresetOpenSounds();
    std::vector<std::string> categories={"All"};
    for(const auto& e:factory)if(e.type==int(InstrumentType::DX7)&&std::find(categories.begin(),categories.end(),e.category)==categories.end())categories.push_back(e.category);
    if(std::find(categories.begin(),categories.end(),"Unsorted")==categories.end())categories.push_back("Unsorted");
    auto unsorted=std::find(categories.begin(),categories.end(),"Unsorted")-categories.begin();
    for(int i=0;i<unsorted;++i)key(1,keyDown);
    key(1,keyRight);key(1,keyUp);capture("dx7-all-unsorted");tapEdit();
    require(chipnomadState->project.instruments[0].chip.dx7.sourceProgram==31,"ALL Unsorted loads last USER bank voice");
    require(!strcmp(chipnomadState->project.instruments[0].name,"TEST VOICE"),"ALL Unsorted USER voice name");
    printf("Engine collections, top-level USER and ALL Unsorted bank selection passed\n");
    fs::remove(oplPack);fs::remove(dxPack);
    printf("Factory ZIPs, all 11 USER roots, nested ZIP navigation, audition, selection and DX7 bank voices passed\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--fx-only")) {
    for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
      auto& inst=chipnomadState->project.instruments[0];getInstrumentFunctions(type).init(&inst);
      require(!instrumentFXAvailableForInstrument(&inst,fxFBK+1),"selector removed");
      for(int command=fxOAR;command<=fxLEN;++command) {
        FXCell fx{FX_ID_EMPTY,0};selectInstrumentFX(&fx,command,0);
        require((fx[0]==command)==bool(instrumentFXAvailableForInstrument(&inst,command)),"selection follows compact list");
      }
      *pSongTrack=0;*pSongRow=0;*pChainRow=0;
      chipnomadState->project.song[0][0]=0;
      chipnomadState->project.chains[0].rows[0].phrase=0;
      auto& row=chipnomadState->project.phrases[0].rows[0];
      row.instrument=0;row.note=45;row.fx[0][0]=type==InstrumentType::DX7?fxFBK:fxOMU;
      for(int header:{0,1}) {
        appSettings.persistentWaveform=header;
        screenPhrase.init();screenSetup(&screenPhrase,0);appDraw();
        for(int column=0;column<3;++column)key(1,keyRight);
        key(1,keyEdit|keyUp);
        char name[64];snprintf(name,sizeof(name),"compact-fx-%d-header-%d",int(type),header);capture(name);
      }
    }
    printf("Focused FX selection and popup captures passed\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  // The accepted insert review shares popup controls with native-chip browsing.
  // Check the full-height page and category selection in both waveform modes.
  for (int header : {0, 1}) {
    appSettings.persistentWaveform = header;
    *pSongTrack = 0;
    insertSelect(&chipnomadState->project.trackInserts[0][0], insertDistortion);
    screenSetup(&screenInsertFX, -1); appDraw();
    require(gfxGetContentRowOffset() == 0, "Insert FX keeps accepted full-height layout");
    capture(header ? "insert-header-on" : "insert-header-off");
    key(1, keyEdit); key(0, 0);
    require(currentScreen == &screenSelectionPopup && selectionPopupIsFullWidth(),
            "insert chooser opens full width");
    capture(header ? "insert-chooser-header-on" : "insert-chooser-header-off");
    key(1, keyUp); key(1, keyRight); key(1, keyDown);
    capture("insert-dynamics-ott");
    tapEdit();
    require(currentScreen == &screenInsertFX, "insert choice returns to page");
    require(chipnomadState->project.trackInserts[0][0].module == insertOTT,
            "Dynamics category selects OTT");
    insertSelect(&chipnomadState->project.trackInserts[0][0], insertOff);
  }
  // This harness calls screen input directly; normal app button release clears tips.
  screenMessage(0, "");
  appSettings.persistentWaveform = 0;
  printf("Insert review UI passed: full-height page, grouped chooser, waveform off/on\n");
  // Exercise the real Type popup; direct instrument initialization cannot catch
  // a stale category count hiding otherwise functional instrument pages.
  int fmIndex = 0;
  for (auto type : {InstrumentType::OPLL, InstrumentType::VRC7, InstrumentType::OPL2,
                   InstrumentType::OPL3, InstrumentType::GenesisFM,
                   InstrumentType::ArcadeFM, InstrumentType::DX7}) {
    getInstrumentFunctions(InstrumentType::AY1).init(&chipnomadState->project.instruments[0]);
    screenSetup(&screenInstrument, 0);
    appDraw();
    key(1, keyEdit); key(0, 0);
    require(currentScreen == &screenSelectionPopup, "Type popup opens on EDIT release");
    key(1, keyUp); // CHIP wraps to the FM category.
    key(1, keyRight);
    for (int i = 0; i < fmIndex; ++i) key(1, keyDown);
    if (type == InstrumentType::DX7) capture("type-fm-dx7");
    tapEdit();
    require(currentScreen == &screenInstrument, "Type popup selection returns to instrument");
    require(chipnomadState->project.instruments[0].type == type, "all seven FM types selectable through popup");
    key(1, keyEdit); key(0, 0); key(1, keyRight); tapEdit();
    require(chipnomadState->project.instruments[0].type == type, "Type popup reopens on selected FM type");
    ++fmIndex;
  }
  printf("Type popup passed: all seven FM types selected and reopened\n");
  for(auto type:{InstrumentType::SID,InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}){
    getInstrumentFunctions(type).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();if(type==InstrumentType::SegaPSG)require(screenInstrumentSimpleChip.isCellValid(0,6),"Sega bass extension selectable");char name[40];snprintf(name,sizeof(name),"instrument-%d",int(type));capture(name);
  }
  require(!strcmp(instrumentTypeName(InstrumentType::SID),"SID"),"SID instrument and browser titles identify the engine");
  // Real SDL pixel regression: incremental ADSR edits must match a fresh draw.
  auto pixels=[](){std::vector<uint32_t> p(640*480);require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,p.data(),640*4)==0,"read graph pixels");return p;};
  // A bank stays selected within its engine, but switching engines must return
  // to All, even between OPL2 and OPL3 which share compatible bank IDs.
  appSettings.persistentWaveform=0;
  for(auto type:{InstrumentType::SID,InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
    getInstrumentFunctions(type).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
    screenInstrumentOPL.onEdit(0,3,CellEditAction::tap);appDraw();auto allBanks=pixels();
    key(1,keyDown);tapEdit();screenInstrumentOPL.onEdit(0,3,CellEditAction::tap);appDraw();
    require(pixels()!=allBanks,"bank selection persists within engine");key(1,keyOpt);
    getInstrumentFunctions(InstrumentType::GBPulse).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
    getInstrumentFunctions(type).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
    screenInstrumentOPL.onEdit(0,3,CellEditAction::tap);appDraw();require(pixels()==allBanks,"engine round trip resets bank to All");
    char name[64];snprintf(name,sizeof(name),"banks-%d-all",int(type));capture(name);key(1,keyOpt);
    if(type==InstrumentType::OPL3) {
      getInstrumentFunctions(InstrumentType::OPL2).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
      screenInstrumentOPL.onEdit(0,3,CellEditAction::tap);key(1,keyDown);tapEdit();
      getInstrumentFunctions(type).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
      screenInstrumentOPL.onEdit(0,3,CellEditAction::tap);appDraw();require(pixels()==allBanks,"compatible OPL bank cannot carry across engines");key(1,keyOpt);
    }
  }
  for(int fx=fxFBR;fx<=fxFBK;++fx) {
    const char* description=helpFXDescription((FX)fx,0);
    require(description&&description[0]&&strchr(description,'\n'),"native FX has title and description");
    uint8_t value[]={uint8_t(fx),0};require(helpFXHint(value,0,0)[0],"native FX has value hint");
  }
  for(auto type:{InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}) {
    getInstrumentFunctions(type).init(&chipnomadState->project.instruments[0]);
    require(strstr(helpFXDescription(fxCMD,0),type==InstrumentType::SegaPSG?"Tone / Noise":type==InstrumentType::GBPulse?"Pulse Duty":"Noise Width"),"chip mode description follows engine");
  }
  printf("Bank reset and native FX description regressions passed\n");
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7,InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise,InstrumentType::SID}) {
    auto& inst=chipnomadState->project.instruments[0];getInstrumentFunctions(type).init(&inst);
    for(int command=fxFBR;command<fxTotalCount;++command) {
      NativeFXInfo info{};if(!instrumentFXAvailableForInstrument(&inst,command)||!instrumentNativeFXInfo(&inst,command,&info))continue;
      FXCell fx{FX_ID_EMPTY,255},last{FXId(command),255};
      selectInstrumentFX(&fx,command,0);require(fx[1]==info.preset,"new native FX starts at instrument value");
      fx[1]=info.minimum;selectInstrumentFX(&fx,command,0);require(fx[1]==info.minimum,"existing native FX value preserved");
      for(auto action:{CellEditAction::increase,CellEditAction::increaseBig,CellEditAction::multiIncreaseBig}) {
        fx[1]=info.maximum;editFXValue(action,&fx,&last,0,0);require(fx[1]==info.maximum,"native upper bound");
      }
      for(auto action:{CellEditAction::decrease,CellEditAction::decreaseBig}) {
        fx[1]=info.minimum;editFXValue(action,&fx,&last,1,0);require(fx[1]==info.minimum,"native lower bound in table");
      }
      fx[1]=255;editFXValue(CellEditAction::tap,&fx,&last,0,0);require(fx[1]<=info.maximum,"out of range cached FX repaired on edit");
    }
  }
  getInstrumentFunctions(InstrumentType::DX7).init(&chipnomadState->project.instruments[0]);
  chipnomadState->project.instruments[0].chip.dx7.voice[5*21+16]=42;
  auto& fxProject=chipnomadState->project;
  auto phraseBackup=fxProject.phrases[0];auto chainBackup=fxProject.chains[0];auto songBackup=fxProject.song[0][0];
  *pSongRow=*pChainRow=*pSongTrack=0;fxProject.song[0][0]=0;fxProject.chains[0].rows[0].phrase=0;
  phraseClear(&fxProject.phrases[0]);fxProject.phrases[0].rows[0].instrument=0;
  getInstrumentFunctions(InstrumentType::OPL2).init(&fxProject.instruments[1]);
  fxProject.instruments[1].chip.opl.operators[0].level=20;
  fxProject.phrases[0].rows[1].instrument=1;
  require(lookupInstrument(&fxProject,0,0,0,0)==0&&lookupInstrument(&fxProject,0,0,2,0)==1,"FX lookup follows explicit and inherited I column");
  FXCell inherited{FX_ID_EMPTY,255};selectInstrumentFX(&inherited,fxOL1,lookupInstrument(&fxProject,0,0,2,0));
  require(inherited[1]==43,"inherited instrument supplies its own operator level");
  for(int header:{0,1}) {
    appSettings.persistentWaveform=header;screenSetup(&screenPhrase,-1);appDraw();
    fxProject.phrases[0].rows[0].fx[0].command=FX_ID_EMPTY;
    screenPhrase.init();
    for(int col=0;col<3;++col)key(1,keyRight);
    key(1,keyEdit|keyUp);
    fxEditFullDraw(fxOL1,0,0);capture(header?"fx-native-dx7-header-on":"fx-native-dx7-header-off");
    fxEditFullDraw(fxFBK,0,0);capture(header?"fx-native-last-header-on":"fx-native-last-header-off");
    fxEditFullDraw(fxOL1,0,0);
    screenPhrase.onInput(0,0,1);
    auto& fx=fxProject.phrases[0].rows[0].fx[0];require(fx[0]==fxOL1&&fx[1]==42,"popup commit reads instrument preset");
  }
  getInstrumentFunctions(InstrumentType::GBPulse).init(&chipnomadState->project.instruments[0]);
  screenSetup(&screenPhrase,-1);appDraw();screenPhrase.init();
  for(int col=0;col<3;++col)key(1,keyRight);
  key(1,keyEdit|keyUp);
  fxEditFullDraw(fxCMD,0,0);capture("fx-native-duty");
  screenPhrase.onInput(0,0,1);
  fxProject.phrases[0]=phraseBackup;fxProject.chains[0]=chainBackup;fxProject.song[0][0]=songBackup;
  printf("Native FX popup preset values, selection defaults and engine bounds passed\n");
  for(int header:{0,1}){
    appSettings.persistentWaveform=header;getInstrumentFunctions(InstrumentType::SID).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
    for(int row=5;row<=8;++row)for(int col=0;col<screenInstrumentOPL.getColumnCount(row);++col){screenInstrumentOPL.onEdit(col,row,CellEditAction::increase);screenInstrumentOPL.drawField(col,row,CellState::normal);appDraw();}
    auto incremental=pixels();currentScreen->fullRedraw();appDraw();auto clean=pixels();int start=(8+gfxGetContentRowOffset())*gfxGetCharHeight()*640;
    require(std::equal(incremental.begin()+start,incremental.end(),clean.begin()+start),"SID controls incremental pixels match full redraw");capture(header?"sid-controls-header-on":"sid-controls-header-off");
    require(screenInstrumentOPL.rows==9,"SID has only implemented synthesis controls");
  }

  printf("SID controls passed: native waveform, pulse width, filter and ADSR; no alternate model options\n");
  for(int header:{0,1})for(auto type:{InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise,InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
    appSettings.persistentWaveform=header;getInstrumentFunctions(type).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
    auto* amp=instrumentFMAmpSettings(&chipnomadState->project.instruments[0]);
    if(amp){instrumentFMAmpEdit(0,0,CellEditAction::increase);require(amp->enabled,"FM amp toggle");instrumentFMToneEdit(0,CellEditAction::increase);instrumentFMToneEdit(1,CellEditAction::increase);appDraw();}
    for(int edit=0;edit<12;++edit){
      int col=edit%4;
      if(amp){instrumentFMAmpEdit(col,1,CellEditAction::increaseBig);instrumentFMAmpDrawField(col,1,CellState::normal);}
      else{screenInstrumentSimpleChip.onEdit(col,8,CellEditAction::increaseBig);screenInstrumentSimpleChip.drawField(col,8,CellState::normal);}
      appDraw();
    }
    auto incremental=pixels();currentScreen->fullRedraw();appDraw();auto clean=pixels();
    // Below row 6 excludes the independently refreshed persistent waveform.
    int start=(6+gfxGetContentRowOffset())*gfxGetCharHeight()*640;
    require(std::equal(incremental.begin()+start,incremental.end(),clean.begin()+start),"ADSR incremental pixels match full redraw");
    char name[64];snprintf(name,sizeof(name),"adsr-%d-header-%d",int(type),header);capture(name);
  }
  printf("ADSR pixel regression passed: ten engines, header off/on, twelve edits each\n");
  appSettings.persistentWaveform=1;getInstrumentFunctions(InstrumentType::DX7).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
  auto before=std::make_unique<Project>(chipnomadState->project);screenInstrumentOPL.onEdit(0,4,CellEditAction::tap);appDraw();require(currentScreen==&screenSelectionPopup,"shared FM browser");capture("dx7-categories");key(1,keyRight);capture("dx7-presets");
  auto popupPixels = pixels();
  screenSelectionPopup.fullRedraw();
  require(popupPixels == pixels(), "app overlays must not cover native preset lists or footer");
  key(1,keyEdit);require(currentScreen==&screenSelectionPopup,"EDIT waits to permit preview chord");key(1,keyEdit|keyPlay);
  std::vector<float> audio(2048);double energy=0;for(int n=0;n<12;++n){chipnomadRender(chipnomadState,audio.data(),1024);for(float x:audio)energy+=x*x;}require(energy>1e-5,"audition audio");require(!memcmp(before.get(),&chipnomadState->project,sizeof(Project)),"audition mutation");key(0,keyPlay);key(1,keyOpt);require(!memcmp(before.get(),&chipnomadState->project,sizeof(Project)),"cancel mutation");
  screenInstrumentOPL.onEdit(0,4,CellEditAction::tap);appDraw();key(1,keyRight);tapEdit();require(currentScreen==&screenInstrument,"confirm returns");require(!memcmp(before->tables,chipnomadState->project.tables,sizeof(before->tables)),"table changed");require(!memcmp(before->trackInserts,chipnomadState->project.trackInserts,sizeof(before->trackInserts)),"insert changed");capture("dx7-loaded");
  // Local SysEx opens the same transactional browser from another instrument.
  InstrumentDX7 patch{};initDX7Patch(&patch);char path[2048];snprintf(path,sizeof(path),"%s/original-test.syx",output);FILE* f=fopen(path,"wb");require(f,"syx fixture");uint8_t h[]={240,67,0,0,1,27};fwrite(h,1,6,f);fwrite(patch.voice,1,155,f);unsigned sum=0;for(auto b:patch.voice)sum+=b;fputc((-sum)&127,f);fputc(247,f);fclose(f);
  // A bank dropped in the persistent hierarchy appears without Load Instrument.
  const char* libraryFile="instruments/USER/dx7/UI-test.syx";
  require(!std::filesystem::exists(libraryFile),"isolated persistent library fixture");
  std::filesystem::copy_file(path,libraryFile);
  getInstrumentFunctions(InstrumentType::DX7).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
  screenInstrumentOPL.onEdit(0,3,CellEditAction::tap);appDraw();capture("dx7-persistent-banks");
  // USER is the final top-level collection; the single voice loads directly.
  key(1,keyUp);tapEdit();
  screenInstrumentOPL.onEdit(0,4,CellEditAction::tap);tapEdit();
  require(!memcmp(patch.voice,chipnomadState->project.instruments[0].chip.dx7.voice,155),"persistent bank voice exact");
  auto savedLibraryPatch=chipnomadState->project.instruments[0];
  std::filesystem::remove(libraryFile);
  screenInstrumentOPL.onEdit(0,3,CellEditAction::tap);key(1,keyOpt);
  require(!memcmp(&savedLibraryPatch,&chipnomadState->project.instruments[0],sizeof(Instrument)),"rescan preserves selected patch after file removal");
  printf("Persistent DX7 folder discovery, selection and removal passed\n");
  getInstrumentFunctions(InstrumentType::MME).init(&chipnomadState->project.instruments[0]);*before=chipnomadState->project;instrumentFMImportSysEx(path);appDraw();require(currentScreen==&screenSelectionPopup,"local import browser");require(!memcmp(before.get(),&chipnomadState->project,sizeof(Project)),"import must not commit");capture("dx7-user-bank");key(1,keyRight);tapEdit();require(chipnomadState->project.instruments[0].type==InstrumentType::DX7,"import commit type");require(!memcmp(patch.voice,chipnomadState->project.instruments[0].chip.dx7.voice,155),"import patch");
  auto original=chipnomadState->project.instruments[0];copyInstrument(0);pasteInstrument(3);require(!memcmp(&original,&chipnomadState->project.instruments[3],sizeof(Instrument)),"DX7 paste");
  require(cloneInstrument(0,4),"DX7 clone");require(!memcmp(&original,&chipnomadState->project.instruments[4],sizeof(Instrument)),"DX7 clone payload");
  for(auto type:{InstrumentType::SID,InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::GenesisFM,InstrumentType::ArcadeFM}){getInstrumentFunctions(type).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();screenInstrumentOPL.onEdit(0,4,CellEditAction::tap);appDraw();require(currentScreen==&screenSelectionPopup,"four-op browser");key(1,keyRight);key(1,keyEdit);key(1,keyEdit|keyPlay);for(int n=0;n<8;++n)chipnomadRender(chipnomadState,audio.data(),1024);key(0,keyPlay);tapEdit();require(currentScreen==&screenInstrument,"four-op confirm");char name[64];snprintf(name,sizeof(name),"four-op-%d-loaded",int(type));capture(name);}
  // Actual sequencer playback with UI rendering and the existing song sends.
  for(auto& i:chipnomadState->project.instruments)if(i.type==InstrumentType::Midi)getInstrumentFunctions(InstrumentType::DX7).init(&i);
  require(chipnomadQueueProjectRefresh(chipnomadState),"snapshot");chipnomadQueuePlaybackStartSong(chipnomadState,0,0,1);energy=0;
  for(int n=0;n<24;++n){chipnomadRender(chipnomadState,audio.data(),1024);appDraw();SDL_RenderFlush(renderer);for(float x:audio)energy+=x*x;}
  require(energy>0.001,"sequencer playback");capture("dx7-song-playing");printf("UI smoke passed; sequencer playback and drawing verified\n");
  screenSetup(&screenInstrument,0);appDraw();screenInstrumentOPL.onEdit(0,4,CellEditAction::tap);appDraw();
  require(currentScreen==&screenSelectionPopup,"browser during playback");capture("fm-presets-during-playback");
  key(1,keyRight);key(1,keyEdit);key(1,keyEdit|keyPlay);chipnomadRender(chipnomadState,audio.data(),1024);
  require(chipnomadState->opllPreviewTrack<0,"playback cannot admit extra preview voice");key(0,keyPlay);key(1,keyOpt);
  // Drive the app event path, including a tick between press and release: the
  // press snapshot must not swallow the preset committed by the release.
  getInstrumentFunctions(InstrumentType::DX7).init(&chipnomadState->project.instruments[0]);screenSetup(&screenInstrument,0);appDraw();
  require(chipnomadQueueProjectRefresh(chipnomadState),"initial live patch snapshot");chipnomadRender(chipnomadState,audio.data(),1024);
  screenInstrumentOPL.onEdit(0,4,CellEditAction::tap);key(1,keyRight);key(1,keyDown);
  MainLoopEventData event{};event.type=MainLoopEvent::keyDown;event.data.input={InputDeviceType::logical,keyEdit};appOnEvent(event);appDraw();
  event.type=MainLoopEvent::tick;appOnEvent(event);chipnomadRender(chipnomadState,audio.data(),1024);
  auto oldVoice=chipnomadState->audioProject.instruments[0].chip.dx7;
  event.type=MainLoopEvent::keyUp;event.data.input={InputDeviceType::logical,keyEdit};appOnEvent(event);appDraw();
  require(currentScreen==&screenInstrument,"app release commits preset");
  require(memcmp(oldVoice.voice,chipnomadState->project.instruments[0].chip.dx7.voice,155)!=0,"live preset choice changes patch");
  event.type=MainLoopEvent::tick;appOnEvent(event);chipnomadRender(chipnomadState,audio.data(),1024);
  require(!memcmp(&chipnomadState->audioProject.instruments[0],&chipnomadState->project.instruments[0],sizeof(Instrument)),"release commit reaches audio without another input or phrase restart");
  require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying,"preset change preserves playback");
  printf("Playing preset release refresh regression passed\n");
  chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
}
