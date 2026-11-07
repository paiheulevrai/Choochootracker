#include <filesystem>
// Developer-only offscreen production UI integration; never shipped.
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include "chipnomad_lib.h"
#include "app.h"
#include "screen_project.h"
#include "screen_groovebox.h"
#include "corelib_font.h"
#include "corelib_gfx.h"
#include "screens.h"
#include "screen_instrument.h"
#include "field_style.h"
#include "screen_layout.h"
#include "selection_popup.h"
#include "waveform_display.h"
#include "monitor_display.h"
#include "background.h"
#include "audio_input_manager.h"
#include "screen_audio_input.h"
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
#include "soundbank_library.h"
#include "soundbank_runtime.h"
#include "synth/sample_voice.h"
#include "synth/instrument_eq.h"
#include "synth/sample_transient.h"
#include "synth/sr_wavetable_loader.h"
#include "audio_manager.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <cctype>
#include "pattern_utils.h"
#include "external/mutable/stmlib/utils/random.h"
extern SDL_Renderer* renderer;
static const char* output;
#ifdef BMP_STREAM_TEST
static const char* virtualBMPPath = "/ChooChooTracker/__bmp_stream_test__.bmp";
static std::string virtualBMPBacking;
static int virtualBMPOpens;
extern "C" FILE* __real_fopen(const char*, const char*);
extern "C" FILE* __wrap_fopen(const char* path, const char* mode) {
  if (!virtualBMPBacking.empty() && !strcmp(path, virtualBMPPath)) {
    ++virtualBMPOpens;
    return __real_fopen(virtualBMPBacking.c_str(), mode);
  }
  return __real_fopen(path, mode);
}
#endif
static void require(bool condition,const char* why){if(!condition){fprintf(stderr,"FAIL: %s\n",why);exit(2);}}
static std::vector<uint32_t> framePixels(){std::vector<uint32_t> pixels(640*480);require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),640*4)==0,"frame pixels");return pixels;}
static size_t trackerInkPixels(const std::vector<uint32_t>& pixels){size_t count=0;for(uint32_t p:pixels){int r=(p>>16)&255,g=(p>>8)&255,b=p&255;int hi=std::max(r,std::max(g,b)),lo=std::min(r,std::min(g,b));if(hi-lo>40||lo>120)++count;}return count;}
static void capture(const char* name){appDraw();auto* s=SDL_CreateRGBSurfaceWithFormat(0,640,480,32,SDL_PIXELFORMAT_ARGB8888);require(s,"surface");require(SDL_RenderReadPixels(renderer,nullptr,s->format->format,s->pixels,s->pitch)==0,"pixels");char path[2048];snprintf(path,sizeof(path),"%s/%s.bmp",output,name);require(SDL_SaveBMP(s,path)==0,"capture");SDL_FreeSurface(s);}
static void key(int down,int keys){appDraw();currentScreen->onInput(down,keys,1);appDraw();}
static void tapEdit(){key(1,keyEdit);key(0,0);}
static void appButton(int button, bool down) {
  MainLoopEventData event = {};
  event.type = down ? MainLoopEvent::keyDown : MainLoopEvent::keyUp;
  event.data.input = {InputDeviceType::logical, button};
  appOnEvent(event);
  float audio[256 * 2];
  chipnomadRender(chipnomadState, audio, 256);
  appDraw();
}
static void appChord(int modifier, int button) {
  // Independent gestures model an expired double-tap window, not instant consecutive taps.
  appResetInputState();
  if (modifier) appButton(modifier, true);
  appButton(button, true);
  appButton(button, false);
  if (modifier) appButton(modifier, false);
}
static void useTheme(const std::string& filename) {
  const std::string path = "themes/" + filename;
  require(loadTheme(path.c_str()) == 0, "theme loaded");
  gfxSetBgColor(appSettings.colorScheme.background);
}
static Instrument& prepareHomepageInstrument(InstrumentType type, const char* name) {
  Instrument& instrument = chipnomadState->project.instruments[0];
  getInstrumentFunctions(instrument.type).free(&instrument);
  memset(&instrument, 0, sizeof(instrument));
  getInstrumentFunctions(type).init(&instrument);
  snprintf(instrument.name, sizeof(instrument.name), "%s", name);
  instrument.volume = 224;
  instrument.pan = 128;
  cInstrument = 0;
  return instrument;
}
static void prepareHomepageSoundbank(const char* name) {
  namespace fs = std::filesystem;
  const fs::path bankDirectory = "instruments/USER/soundfont";
  const fs::path sfzPath = bankDirectory / "Soft Keys.sfz";
  const fs::path sfzSample = bankDirectory / "SoftKeys.wav";
  fs::create_directories(bankDirectory);
  fs::copy_file("samples/ST-01/EPiano.wav", sfzSample, fs::copy_options::overwrite_existing);
  std::ofstream sfz(sfzPath, std::ios::binary | std::ios::trunc);
  require(bool(sfz), "temporary homepage SFZ created");
  sfz << "<region> sample=SoftKeys.wav key=60 lokey=0 hikey=127\n";
  sfz.close();
  auto& instrument = prepareHomepageInstrument(InstrumentType::SoundFont, name);
  const auto bank = soundbank::parseBank({fs::absolute(sfzPath).string(), "", ""});
  require(!bank.presets.empty(), "homepage SFZ parsed");
  soundbank::SampleCache cache;
  const auto preset = cache.prepare(bank, bank.presets.front().id);
  audioManager.pause();
  instrument.chip.soundfont.prepared = soundbank::publish(preset);
  audioManager.resume();
  snprintf(instrument.chip.soundfont.bank, sizeof(instrument.chip.soundfont.bank), "Soft Keys.sfz");
  snprintf(instrument.chip.soundfont.preset, sizeof(instrument.chip.soundfont.preset), "%s",
           bank.presets.front().id.c_str());
  chipnomadQueueProjectRefresh(chipnomadState);
  require(instrument.chip.soundfont.prepared != 0, "homepage SoundFont preset prepared");
  fs::remove(sfzPath);
  fs::remove(sfzSample);
}
static void captureHomepage() {
  namespace fs = std::filesystem;
  std::string baseTheme;
  std::string sidTheme;
  std::string soundfontTheme;
  std::string mmeTheme;
  std::string monkTheme;
  std::string scwfTheme;
  std::string wavetableTheme;
  std::string sampleEditorTheme;
  std::string sampleSlicesTheme;
  std::string insertTheme;
  std::string reverbTheme;
  std::ofstream themeManifest(fs::path(output) / "homepage-theme-manifest.txt", std::ios::trunc);
  require(bool(themeManifest), "homepage theme manifest opened");
  for (const auto& entry : fs::recursive_directory_iterator("themes")) {
    if (!entry.is_regular_file() || entry.path().extension() != ".cth") continue;
    const std::string file = entry.path().lexically_relative("themes").generic_string();
    std::string lower = file;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (lower == "bb22200.cth") baseTheme = file;
    else if (lower == "synths/nutable.cth") sidTheme = file;
    else if (lower == "synths/dixiseven.cth") soundfontTheme = file;
    else if (lower == "idecolorthemes/nordlight.cth") mmeTheme = file;
    else if (lower == "idecolorthemes/wlterminalgreenlight.cth") monkTheme = file;
    else if (lower == "idecolorthemes/catpuccinlight.cth") scwfTheme = file;
    else if (lower == "idecolorthemes/cyberpunkdark.cth") wavetableTheme = file;
    else if (lower == "wood.cth") sampleEditorTheme = file;
    else if (lower == "dubwise.cth") sampleSlicesTheme = file;
    else if (lower == "idecolorthemes/nostromoamberdark.cth") insertTheme = file;
    else if (lower == "idecolorthemes/draculadark.cth") reverbTheme = file;
  }
  require(!baseTheme.empty() && !sampleEditorTheme.empty() && !sampleSlicesTheme.empty() && !reverbTheme.empty() &&
          !sidTheme.empty() && !soundfontTheme.empty() && !mmeTheme.empty() && !monkTheme.empty() &&
          !scwfTheme.empty() && !wavetableTheme.empty() && !insertTheme.empty(),
          "BB22200, nested themes, and distinct capture themes available");
  const auto themeFor = [&](int pageIndex) -> std::string {
    if (pageIndex < 10) return baseTheme;
    switch (pageIndex) {
      case 10: return sidTheme;
      case 11: return soundfontTheme;
      case 12: return mmeTheme;
      case 13: return monkTheme;
      case 14: return scwfTheme;
      case 15: return wavetableTheme;
      case 16: return sampleEditorTheme;
      case 17: return sampleSlicesTheme;
      case 18: return insertTheme;
      case 19: return reverbTheme;
      default: return baseTheme;
    }
  };
  const auto show = [&](int pageIndex, const char* name, const AppScreen* screen, int input = 0) {
    const std::string theme = themeFor(pageIndex);
    useTheme(theme);
    screenMessage(0, "");
    screenSetup(screen, input);
    capture(name);
    themeManifest << name << " theme=" << theme << '\n';
    themeManifest.flush();
    printf("%02d %s theme=%s\n", pageIndex + 1, name, theme.c_str());
  };

  require(!projectLoad(&chipnomadState->project, "projects/ChooBreaks01.cct"), "ChooBreaks project loaded");
  appSettings.persistentWaveform = 1;
  show(0, "song", &screenSong);
  Instrument& achchid = prepareHomepageInstrument(InstrumentType::AChChid, "Acid Bass");
  achchid.chip.achchid.wave = AChChidWave::braids; achchid.chip.achchid.model = 10;
  achchid.chip.achchid.timbre = 19200; achchid.chip.achchid.color = 22400;
  achchid.chip.achchid.saturation = 88; achchid.chip.achchid.cutoff = 3100;
  achchid.chip.achchid.resonance = 92; achchid.chip.achchid.envMod = 76;
  achchid.chip.achchid.decay = 188; achchid.chip.achchid.accent = 126;
  show(1, "engine-achchid", &screenInstrument);
  show(2, "mixer", &screenMixer);

  require(!projectLoad(&chipnomadState->project, "projects/ChooTest_01.cct"), "ChooTest project loaded");
  screenSetup(&screenSong, 0);
  screenSetup(&screenChain, 0);
  show(3, "phrase", &screenPhrase);
  // Track 8, row 0 points to ChooTest's most varied populated chain (80).
  screenSetup(&screenSong, 0);
  *pSongTrack = 7;
  show(4, "chain", &screenChain);

  Instrument& modulationInstrument = prepareHomepageInstrument(InstrumentType::Braids, "Modulated Bass");
  modulationInstrument.chip.braids.model = 4;
  modulationInstrument.chip.braids.timbre = 17600;
  modulationInstrument.chip.braids.color = 9200;
  modulationInstrument.modulation[0] = {ModulationType::ADSR, 1, 72, 20, 82, 190, 54, 0};
  modulationInstrument.modulation[1] = {ModulationType::LFO, 2, -48, 1, 1, 36, 0, 0};
  show(5, "modulation", &screenModulation);

  Instrument& braids = prepareHomepageInstrument(InstrumentType::Braids, "Bells in Rain");
  braids.chip.braids.model = 14; braids.chip.braids.timbre = 20900; braids.chip.braids.color = 14300;
  show(6, "engine-braids", &screenInstrument);
  Instrument& plaits = prepareHomepageInstrument(InstrumentType::Plaits, "Wavetable Pluck");
  plaits.chip.plaits.engine = 8; plaits.chip.plaits.harmonics = 16300; plaits.chip.plaits.timbre = 22100;
  plaits.chip.plaits.morph = 12700; plaits.chip.plaits.auxMix = 62;
  show(7, "engine-plaits", &screenInstrument);
  Instrument& bogie = prepareHomepageInstrument(InstrumentType::DrumSynth, "Round Kick");
  bogie.chip.drumSynth.engine = DrumSynthEngine::kick; bogie.chip.drumSynth.decay = 174;
  bogie.chip.drumSynth.tone = 164; bogie.chip.drumSynth.sweep = 126; bogie.chip.drumSynth.drive = 58;
  show(8, "engine-bogie", &screenInstrument);
  Instrument& sintered = prepareHomepageInstrument(InstrumentType::Sintered, "Shifting Metal");
  sintered.chip.sintered.model = SinteredModel::comb; sintered.chip.sintered.decay = 122;
  sintered.chip.sintered.mod = 174; sintered.chip.sintered.a = 198; sintered.chip.sintered.b = 76;
  sintered.chip.sintered.motion = 152; sintered.chip.sintered.c = 104;
  show(9, "engine-sintered", &screenInstrument);
  Instrument& sid = prepareHomepageInstrument(InstrumentType::SID, "Filter Bass");
  snprintf(sid.chip.sid.presetName, sizeof(sid.chip.sid.presetName), "Filter Bass");
  show(10, "engine-sid", &screenInstrument);

  prepareHomepageSoundbank("Soft Keys");
  show(11, "engine-soundfont", &screenInstrument);
  Instrument& mme = prepareHomepageInstrument(InstrumentType::MME, "Folded Saw");
  mme.chip.mme.model = MMEModel::fold; mme.chip.mme.waves = 72; mme.chip.mme.interval = 144;
  mme.chip.mme.amount = 190; mme.chip.mme.flow = 124; mme.chip.mme.feedback = 78; mme.chip.mme.shaper = 142;
  show(12, "engine-mme", &screenInstrument);
  Instrument& monk = prepareHomepageInstrument(InstrumentType::Monk, "Soft Choir");
  monk.chip.monk.attack = 26; monk.chip.monk.decay = 92; monk.chip.monk.sustain = 188;
  monk.chip.monk.release = 76; monk.chip.monk.vowel = 142; monk.chip.monk.character = 168;
  monk.chip.monk.breath = 86; monk.chip.monk.glide = 24; monk.chip.monk.vibrato = 62;
  monk.chip.monk.rate = 80; monk.chip.monk.unison = 2; monk.chip.monk.detune = 54;
  monk.chip.monk.voiceSpread = 144; monk.chip.monk.delayMix = 48; monk.chip.monk.delayRate = 92;
  monk.chip.monk.level = 210;
  show(13, "engine-monk", &screenInstrument);

  char error[128] = {};
  Instrument& scwf = prepareHomepageInstrument(InstrumentType::SCWF, "Chello Glass");
  require(!sampleLoadWav16("waveforms/AKWF/AKWF_cello_0001.wav", &scwf.chip.scwf.oscillator[0], error, sizeof(error)), "SCWF oscillator one loaded");
  require(!sampleLoadWav16("waveforms/AKWF/AKWF_cello_0002.wav", &scwf.chip.scwf.oscillator[1], error, sizeof(error)), "SCWF oscillator two loaded");
  scwf.chip.scwf.detune = 37; scwf.chip.scwf.mix = 94;
  show(14, "engine-scwf", &screenInstrument);
  Instrument& table = prepareHomepageInstrument(InstrumentType::BYOWTBL, "Acid Wavetable");
  require(!srWavetableLoadWav("SR_wavetables/WaveEdit/ACID_RIN.WAV", &table.chip.byowtbl.oscillator[0],
    &table.chip.byowtbl.frameSize[0], &table.chip.byowtbl.tableFrames[0], error, sizeof(error)), "wavetable oscillator one loaded");
  require(!srWavetableLoadWav("SR_wavetables/WaveEdit/BASIC_WA.WAV", &table.chip.byowtbl.oscillator[1],
    &table.chip.byowtbl.frameSize[1], &table.chip.byowtbl.tableFrames[1], error, sizeof(error)), "wavetable oscillator two loaded");
  table.chip.byowtbl.frameIndex[0] = 2; table.chip.byowtbl.frameIndex[1] = 5;
  table.chip.byowtbl.detune = 30; table.chip.byowtbl.mix = 104;
  show(15, "engine-byowtbl", &screenInstrument);

  require(!projectLoad(&chipnomadState->project, "projects/ChooBreaks01.cct"), "one-shot source project loaded");
  Instrument& oneShot = prepareHomepageInstrument(InstrumentType::Sample, "909 Kick");
  require(!sampleLoadWav16("samples/909/BT0A0A7.WAV", &oneShot.chip.sample, error, sizeof(error)),
          "bundled 909 one-shot loaded");
  show(16, "sample-editor", &screenSampleSettings, 0);

  Instrument& breakInstrument = prepareHomepageInstrument(InstrumentType::Sample, "AMEN.WAV");
  require(!sampleLoadWav16("SR_wavetables/WaveEdit/AMEN.WAV", &breakInstrument.chip.sample, error, sizeof(error)), "AMEN break loaded");
  auto& sample = breakInstrument.chip.sample;
  sample.autoSensitivity = 55;
  uint8_t sliceCount = 0;
  uint32_t bounds[PROJECT_SAMPLE_MAX_SLICES] = {};
  if (!sampleDetectTransients(&sample, 0, sample.frameCount, 16, sample.autoSensitivity, bounds, &sliceCount) && sliceCount > 1) {
    sample.slice = sampleEncodeSlice(sliceModeAuto, sliceCount);
    memcpy(sample.sliceBounds, bounds, sliceCount * sizeof(bounds[0]));
  } else {
    sample.slice = sampleEncodeSlice(sliceModeEqual, 16);
  }
  show(17, "sample-slices", &screenSampleSettings, 0);

  auto& insert = chipnomadState->project.trackInserts[0][0];
  insertSelect(&insert, insertOTT); insertEdit(&insert, 0, 186); insertEdit(&insert, 1, 170);
  screenSetup(&screenSong, 0);
  show(18, "insert-fx", &screenInsertFX, -1);
  require(!projectLoad(&chipnomadState->project, "projects/ChooBreaks01.cct"), "effect project loaded");
  screenSetup(&screenSong, 0);
  for (int track = 0; track < PROJECT_MAX_TRACKS; ++track) {
    chipnomadState->project.trackReverbSend[track] = track < 4 ? (uint8_t)(34 + track * 7) : 0;
    chipnomadState->project.trackDelaySend[track] = track == 0 || track == 2 ? 28 : 0;
  }
  show(19, "reverb", &screenMixer);
  key(1, keyShift | keyUp);
  capture("reverb");
  screenMessage(0, "");
  soundbankShutdown();
  printf("Captured 20 homepage screens as 640x480 BMPs.\n");
}
int main(int argc,char** argv){
  if(argc!=2&&argc!=3) return 1;
  output=argv[1];initDefaultAppSettings();appSettings.screenWidth=640;appSettings.screenHeight=480;fontSetCurrent(fontGetDefault());require(!gfxSetup(&appSettings.screenWidth,&appSettings.screenHeight),"SDL dummy setup");
#ifdef BMP_STREAM_TEST
  if(argc==3 && !strcmp(argv[2],"--bmp-stream-only")) {
    const std::string bmp = (std::filesystem::path(output) / "bmp-stream-valid.bmp").string();
    const std::string invalid = (std::filesystem::path(output) / "bmp-stream-invalid.bmp").string();
    SDL_Surface* fixture = SDL_CreateRGBSurfaceWithFormat(0, 2, 2, 32, SDL_PIXELFORMAT_ARGB8888);
    require(fixture != nullptr, "BMP fixture surface");
    require(SDL_FillRect(fixture, nullptr, 0xff123456) == 0, "BMP fixture pixels");
    require(SDL_SaveBMP(fixture, bmp.c_str()) == 0, "BMP fixture saved");
    SDL_FreeSurface(fixture);
    FILE* bad = fopen(invalid.c_str(), "wb");
    require(bad != nullptr, "invalid BMP fixture opened");
    require(fwrite("not a BMP", 1, 9, bad) == 9, "invalid BMP fixture written");
    require(fclose(bad) == 0, "invalid BMP fixture closed");
    virtualBMPBacking = bmp;
    SDL_Surface* bypass = SDL_LoadBMP(virtualBMPPath);
    require(bypass == nullptr && virtualBMPOpens == 0, "SDL bypass cannot resolve virtual path");
    for (const char* path : {bmp.c_str(), virtualBMPPath}) {
      GfxImage* image = gfxImageLoadBMP(path);
      require(image != nullptr, "title BMP loaded through application stream");
      require(gfxImageWidth(image) == 2 && gfxImageHeight(image) == 2, "BMP dimensions preserved");
      gfxImageFree(image);
      for (int layout = 0; layout <= 1; ++layout)
        require(gfxBackgroundLoadBMP(path, layout, (int)BackgroundTint::original, 100) == 1,
          "background BMP loaded for stretch and tile");
    }
    require(virtualBMPOpens == 3, "both BMP entry points use application fopen");
    virtualBMPBacking = invalid;
    require(gfxImageLoadBMP(virtualBMPPath) == nullptr, "invalid virtual title BMP rejected");
    require(gfxBackgroundLoadBMP(virtualBMPPath, 0, 0, 100) == 0, "invalid virtual background rejected");
    virtualBMPBacking = bmp + ".missing";
    require(gfxImageLoadBMP(virtualBMPPath) == nullptr, "missing virtual title BMP rejected");
    require(strstr(SDL_GetError(), "Could not open BMP") != nullptr, "open failure reports SDL error");
    require(gfxBackgroundLoadBMP(virtualBMPPath, 0, 0, 100) == 0, "missing virtual background rejected");
    require(gfxBackgroundLoadBMP("", 0, 0, 100) == 0, "empty BMP path rejected");
    require(gfxImageLoadBMP(nullptr) == nullptr, "null BMP path rejected");
    virtualBMPBacking.clear();
    require(std::filesystem::remove(bmp), "BMP fixture removed");
    require(std::filesystem::remove(invalid), "invalid BMP fixture removed");
    gfxCleanup(); SDL_Quit();
    printf("BMP stream paths passed: normal, virtual, tile/stretch, invalid, missing, empty\n");
    return 0;
  }
#endif
  if(argc==3 && !strcmp(argv[2],"--startup-smoke")) {
    require(!std::filesystem::exists(getAutosavePath()), "run startup smoke in an isolated package without autosave");
    appSetup(); appDraw();
    require(currentScreen == &screenTitle && !appHasProject() && chipnomadState->sampleRate == 0, "title has no active engine");
    key(1, keyEdit);
    require(currentScreen == &screenFileBrowser && !appHasProject(), "OPEN does not activate a project");
    key(1, keyOpt); key(0, 0);
    require(currentScreen == &screenTitle && !appHasProject(), "OPEN cancel returns to unloaded title");
    require(projectLoadFromPath("projects/does-not-exist.cct") != 0 && !appHasProject(), "invalid OPEN leaves project unloaded");
    MainLoopEventData event{}; event.type=MainLoopEvent::exit; appOnEvent(event);
    require(!std::filesystem::exists(getAutosavePath()), "exit before choice does not create autosave");
    key(1,keyDown); key(1,keyEdit);
    require(currentScreen == &screenSong && appHasProject() && chipnomadState->sampleRate > 0, "NEW activates blank project");
    appCleanup(); appSetup(); appDraw();
    require(projectLoadFromPath("projects/grieg-mountain-king-fm.cct") == 0 && appHasProject(), "OPEN activates chosen project");
    require(!projectSave(&chipnomadState->project,getAutosavePath()), "continue fixture saved");
    appCleanup(); appSetup(); appDraw(); key(1,keyEdit);
    require(currentScreen == &screenSong && appHasProject(), "CONTINUE loads autosave at confirmation");
    appCleanup(); gfxCleanup();
    printf("Startup paths passed: title, OPEN cancel/error/success, NEW, CONTINUE, exit protection\n");
    return 0;
  }
  chipnomadState=chipnomadCreate();require(chipnomadState,"state");require(!projectLoad(&chipnomadState->project,"projects/gm-midi-demo.cct"),"fixture");
  if(argc==3 && !strcmp(argv[2],"--live-queue-only")) {
    auto& p = chipnomadState->project;
    projectFree(&p); projectInitAY(&p);
    p.tracksCount = 3; p.tickRate = 100;
    for(auto& speed:p.grooves[0].speed) speed = 1;
    p.chains[0].rows[0].phrase = p.chains[0].rows[1].phrase = 0;
    for(int t=0;t<3;++t) p.song[0][t] = 0;
    chipnomadInitChips(chipnomadState,48000,nullptr);
    screensInitAll(); waveformDisplayInit(); monitorDisplayInit(); backgroundInit();
    screenSetup(&screenSong,-1);
    screenSong.onInput(1,keyOpt,1); screenSong.onInput(0,0,1);
    screenSong.onInput(1,keyOpt,2); screenSong.onInput(0,0,2);
    screenSong.onInput(1,keyPlay,1); screenSong.onInput(0,0,1);
    std::vector<float> audio(480*2);
    chipnomadRender(chipnomadState,audio.data(),480);
    require(chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].mode==PlaybackMode::live,"first live launch");
    *pSongTrack=1;
    screenSong.onInput(1,keyPlay,1); screenSong.onInput(0,0,1);
    chipnomadRender(chipnomadState,audio.data(),480);
    auto& playback=chipnomadState->playbackState;
    require(playback.tracks[1].mode==PlaybackMode::stopped &&
      playback.tracks[1].queue.liveAction==LiveQueueAction::normal,"real Song PLAY queues stopped track");
    playback.tracks[0].phraseRow=15; playback.tracks[0].chainRow=1;
    chipnomadRender(chipnomadState,audio.data(),480);
    require(playback.tracks[1].mode==PlaybackMode::live && playback.tracks[1].phraseRow==0,"live launch at chain boundary");
    *pSongTrack=2;
    screenSong.onInput(1,keyPlay,1); screenSong.onInput(0,0,1);
    chipnomadRender(chipnomadState,audio.data(),480);
    screenSong.onInput(1,keyPlay,2); screenSong.onInput(0,0,2);
    playback.tracks[0].phraseRow=15; playback.tracks[0].chainRow=0;
    chipnomadRender(chipnomadState,audio.data(),480);
    require(playback.tracks[2].mode==PlaybackMode::live && playback.tracks[2].phraseRow==0,"real Song urgent launch at phrase boundary");
    capture("live-queued-launch");
    chipnomadDestroy(chipnomadState); chipnomadState=nullptr; gfxCleanup();
    printf("Live Song input: stopped-track queue, chain launch and urgent phrase launch passed\n");
    return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--eq-presets-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();backgroundInit();
    projectCreateNewFromScreen(&screenSong);appDraw();
    auto& project=chipnomadState->project;
    require(instrumentEQIsDefault(project.masterEQ),"NEW master stays flat");
    for(int bank=0;bank<PROJECT_EQ_BANKS;++bank)
      require(instrumentEQAudible(project.eqBanks[bank])==(bank>=0x01&&bank<=0x14),
              "NEW presets only occupy 01-14");
    for(const auto& instrument:project.instruments)
      require(instrument.eq==EMPTY_VALUE_8,"NEW assigns no EQ");
    prepareHomepageInstrument(InstrumentType::Braids,"EQ test");
    project.instruments[0].eq=0x00;
    getInstrumentFunctions(InstrumentType::Braids).init(&project.instruments[1]);
    project.instruments[1].eq=0x00;
    screenSetup(&screenInstrumentEQ,0x00);appDraw();
    key(1,keyOpt|keyRight);key(0,0);
    require(project.instruments[0].eq==0x00,"OPT right outside EQ number does not browse");
    key(1,keyUp);key(0,0);
    for(int bank=0x01;bank<=0x14;++bank) {
      key(1,keyEdit);key(1,keyEdit|keyRight);key(0,0);
      require(currentScreen==&screenInstrumentEQ&&project.instruments[0].eq==bank,
              "EDIT right browses and assigns EQ without leaving screen");
      require(project.instruments[1].eq==0x00,"browsing leaves shared instrument alone");
      char name[64];snprintf(name,sizeof(name),"eq-preset-%02x",bank);capture(name);
    }
    key(1,keyEdit);key(1,keyEdit|keyLeft);key(0,0);
    require(project.instruments[0].eq==0x13,"EDIT left selects previous EQ");
    key(1,keyEdit);key(1,keyEdit|keyRight);key(0,0);
    require(project.instruments[0].eq==0x14,"EDIT right also changes EQ number");
    key(1,keyEdit);key(1,keyEdit|keyLeft);key(0,0);
    key(1,keyDown);key(0,0);
    key(1,keyEdit|keyUp);key(0,0);
    require(project.eqBanks[0x13].bands[0].q==44,"DOWN returns from EQ number to editable Band");
    key(1,keyUp);key(0,0);
    key(1,keyOpt);appDraw();
    require(currentScreen==&screenInstrument,"OPT immediately exits from EQ number");
    printf("NEW EQ presets 01-14: neutral master, no assigned instruments and twenty EQ screens passed\n");
    appCleanup();gfxCleanup();SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--modulation-transfer-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();backgroundInit();
    *pSongRow=*pChainRow=*pSongTrack=0;
    auto& project=chipnomadState->project;
    auto verify=[](const Instrument& before,const Instrument& after) {
      Instrument expected=after;
      instrumentTransferModulations(&before,&expected);
      require(!std::memcmp(expected.modulation,after.modulation,sizeof(after.modulation)),
              "UI applies full modulation transfer");
    };
    auto prepare=[&]() -> Instrument& {
      auto& instrument=prepareHomepageInstrument(InstrumentType::Braids,"Transfer");
      instrument.modulation[0]={ModulationType::ADSR,5,-70,11,22,180,44,55};
      instrument.modulation[1]={ModulationType::SLFO,3,80,9,1,100,4,187};
      instrument.modulation[2]={ModulationType::StickRate,4,-90,3,24,0,0,0};
      instrument.modulation[3]={ModulationType::Sequence,
        uint8_t(getInstrumentFunctions(instrument.type).modDestinationsCount+1+genericModFirstInsert+9),
        110,uint8_t(SequenceSource::tranceGate),0,17,249,64};
      screenSetup(&screenInstrument,0);appDraw();
      return instrument;
    };
    for(int header:{0,1}) {
      appSettings.persistentWaveform=header;
      Instrument before=prepare();
      require(instrumentCommonOnEdit(0,0,CellEditAction::increase),"direct type change");
      appDraw();
      require(project.instruments[0].type==InstrumentType::Plaits,"direct change selects Plaits");
      verify(before,project.instruments[0]);
      require(project.instruments[0].modulation[0].destination==7,"direct change retains Cutoff");
      require(project.instruments[0].modulation[1].destination==3,"direct change follows macro rank");
      require(std::strstr(screenGetActiveMessage(),"modulation target")!=nullptr,"adjusted targets reported");
      capture(header?"modulation-transfer-direct-header-on":"modulation-transfer-direct-header-off");
      before=prepare();
      tapEdit();
      require(currentScreen==&screenSelectionPopup,"type popup");
      key(1,keyRight);
      for(int step=0;step<3;++step)key(1,keyDown);
      tapEdit();
      require(currentScreen==&screenInstrument&&project.instruments[0].type==InstrumentType::MME,
              "popup selects MME");
      verify(before,project.instruments[0]);
      require(project.instruments[0].modulation[0].destination==9,"popup retains Cutoff");
      require(project.instruments[0].modulation[2].destination==4,"popup follows second macro");
      screenSetup(&screenModulation,0);appDraw();
      capture(header?"modulation-transfer-popup-header-on":"modulation-transfer-popup-header-off");
    }
    const Instrument saved=project.instruments[0];
    const char* path="modulation-transfer-test.cni";
    require(instrumentSave(&project,path,0)==0,"save transferred modulation");
    getInstrumentFunctions(InstrumentType::Sintered).init(&project.instruments[0]);
    require(instrumentLoad(&project,path,0)==0,"load transferred modulation");
    require(!std::memcmp(saved.modulation,project.instruments[0].modulation,sizeof(saved.modulation)),
            "CNI loads its own full modulation, not current engine modulation");
    require(std::remove(path)==0,"remove temporary instrument");
    prepare();
    chipnomadInitChips(chipnomadState,48000,nullptr);
    chipnomadReserveRenderBuffers(chipnomadState,512);
    project.tracksCount=1;project.song[0][0]=0;
    chainClear(&project.chains[0]);project.chains[0].rows[0].phrase=0;
    phraseClear(&project.phrases[0]);
    project.phrases[0].rows[0].note=36;project.phrases[0].rows[0].instrument=0;
    require(chipnomadQueueProjectRefresh(chipnomadState),"refresh playback fixture");
    require(chipnomadQueuePlaybackStartSong(chipnomadState,0,0,1),"start playback");
    std::vector<float> audio(1024);
    require(chipnomadRender(chipnomadState,audio.data(),512)==512,"render old engine");
    require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying,"playing before switch");
    const Instrument before=project.instruments[0];
    require(instrumentCommonOnEdit(0,0,CellEditAction::increase),"live type change");
    appDraw();verify(before,project.instruments[0]);
    require(chipnomadQueueProjectRefresh(chipnomadState),"publish switched instrument");
    for(int block=0;block<16;++block) {
      require(chipnomadRender(chipnomadState,audio.data(),512)==512,"render switched engine");
      for(float value:audio)require(std::isfinite(value),"finite live-switch audio");
    }
    require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying,"live switch preserves playback");
    require(!std::memcmp(project.instruments[0].modulation,chipnomadState->audioProject.instruments[0].modulation,
                         sizeof(before.modulation)),"audio snapshot receives transferred modulation");
    require(instrumentCommonOnEdit(0,0,CellEditAction::clear),"clear type");
    appDraw();
    require(project.instruments[0].type==InstrumentType::none,"None clears instrument");
    for(const auto& modulation:project.instruments[0].modulation)
      require(modulation.destination==0,"None does not retain routes");
    printf("Modulation transfer: direct and popup changes, both headers, CNI and live playback passed\n");
    chipnomadDestroy(chipnomadState);gfxCleanup();SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--modulation-gate-only")) {
    projectFree(&chipnomadState->project);
    projectInitAY(&chipnomadState->project);
    screensInitAll();waveformDisplayInit();monitorDisplayInit();backgroundInit();
    appSettings.backgroundIntensity=0;
    *pSongTrack=0;
    auto checkText=[](int x,int y,int width,const char* text,int color) {
      gfxUpdateScreen();
      SDL_Rect region{x*gfxGetCharWidth(),(y+gfxGetContentRowOffset())*gfxGetCharHeight(),
                      width*gfxGetCharWidth(),gfxGetCharHeight()};
      auto read=[&] {
        std::vector<uint32_t> pixels(region.w*region.h);
        require(SDL_RenderReadPixels(renderer,&region,SDL_PIXELFORMAT_ARGB8888,
                                     pixels.data(),region.w*4)==0,"read text pixels");
        return pixels;
      };
      const auto actual=read();
      gfxClearRect(x,y,width,1);gfxSetFgColor(color);gfxPrint(x,y,text);gfxUpdateScreen();
      const auto expected=read();
      if(actual!=expected)fprintf(stderr,"Text mismatch at (%d,%d), width %d: %s\n",x,y,width,text);
      require(actual==expected,text);
    };
    for(int header:{0,1}) {
      appSettings.persistentWaveform=header;
      for(auto type:{InstrumentType::Sintered,InstrumentType::MME}) {
        auto& instrument=prepareHomepageInstrument(type,"Mod names");
        for(int model=0;model<7;++model) {
          if(type==InstrumentType::Sintered)instrument.chip.sintered.model=(SinteredModel)model;
          else instrument.chip.mme.model=(MMEModel)model;
          for(int destination=3;destination<=8;++destination) {
            for(auto& modulation:instrument.modulation)modulation.destination=destination;
            screenSetup(&screenModulation,0);appDraw();
            const char* name=instrumentModDestinationNameForInstrument(&instrument,destination);
            checkText(7,screenModulationRowY(1),10,name,fieldNameColor(CellState::normal));
            checkText(24,screenModulationRowY(1),10,name,fieldNameColor(CellState::normal));
          }
        }
        char name[64];snprintf(name,sizeof(name),"modulation-names-%d-header-%d",int(type),header);capture(name);
      }
      auto& instrument=prepareHomepageInstrument(InstrumentType::Sintered,"Gate mode");
      screenSetup(&screenInstrument,0);appDraw();
      require(screenInstrumentSintered.rows==11&&screenInstrumentSintered.getColumnCount(10)==1,"Gate is seventh control");
      checkText(8,14,8,"OFF",fieldValueColor(CellState::normal));
      screenInstrumentSintered.onEdit(0,10,CellEditAction::tap);appDraw();
      require(instrument.chip.sintered.gate==1,"Gate tap turns on");
      checkText(8,14,8,"ON",fieldValueColor(CellState::normal));
      screenInstrumentSintered.drawCursor(0,10);
      int x,y,width;gfxGetCursor(&x,&y,&width);require(x==8&&y==14&&width==3,"Gate cursor width");
      capture(header?"sintered-gate-header-on":"sintered-gate-header-off");
      screenInstrumentSintered.onEdit(0,10,CellEditAction::tap);
      require(instrument.chip.sintered.gate==0,"Gate tap turns off");
      screenInstrumentSintered.onEdit(0,10,CellEditAction::increase);
      require(instrument.chip.sintered.gate==1,"Gate directional edit");
      screenInstrumentSintered.onEdit(0,10,CellEditAction::clear);
      require(instrument.chip.sintered.gate==0,"Gate clear restores one shot");
    }
    appSettings.persistentWaveform=0;
    auto& instrument=prepareHomepageInstrument(InstrumentType::Sintered,"Insert names");
    screenSetup(&screenModulation,0);appDraw();key(1,keyDown);key(0,0);
    for(int module=1;module<insertModuleCount;++module) {
      insertSelect(&chipnomadState->project.trackInserts[0][0],module);
      insertSelect(&chipnomadState->project.trackInserts[0][1],module);
      const auto& descriptor=insertDescriptor(module);
      int first=-1;
      for(int i=0;i<8;++i)
        if(insertParameterModulatable(module,i,chipnomadState->project.trackInserts[0][0].values[0])) {first=i;break;}
      require(first>=0,"insert has modulation targets");
      instrument.modulation[0].destination=getInstrumentFunctions(instrument.type).modDestinationsCount+1+genericModFirstInsert+first;
      currentScreen->fullRedraw();appDraw();tapEdit();
      require(currentScreen==&screenSelectionPopup,"insert destination popup");
      int row=2;
      for(int slot=0;slot<2;++slot)for(int parameter=0;parameter<8;++parameter) {
        if(!insertParameterModulatable(module,parameter,chipnomadState->project.trackInserts[0][slot].values[0]))continue;
        const auto& definition=descriptor.parameters[parameter];
        const char* name=strlen(definition.name)>14?definition.label:definition.name;
        char expected[64];snprintf(expected,sizeof(expected),"F%d%d %s",slot+1,parameter+1,name);
        require(strlen(expected)<=18,"insert description fits list");
        checkText(22,row++,18,expected,appSettings.colorScheme.textDefault);
      }
      char name[64];snprintf(name,sizeof(name),"modulation-insert-%d",module);capture(name);
      key(1,keyOpt);key(0,0);
    }
    printf("Modulation names, insert descriptions and Sintered Gate UI passed\n");
    chipnomadDestroy(chipnomadState);gfxCleanup();SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--midi-mapping-only")) {
    projectFree(&chipnomadState->project);
    projectInitAY(&chipnomadState->project);
    auto& project=chipnomadState->project;
    getInstrumentFunctions(InstrumentType::Braids).init(&project.instruments[3]);
    strcpy(project.instruments[3].name,"Deep Bass");
    getInstrumentFunctions(InstrumentType::Plaits).init(&project.instruments[5]);
    strcpy(project.instruments[5].name,"Lead");
    project.midiCCMappings[0]={1,0,41,0,midiCCDestinationSongPlayStop};
    project.midiCCMappings[1]={1,0,32,0,midiCCDestinationTrackMute};
    project.midiCCMappings[2]={1,0,33,1,midiCCDestinationTrackSolo};
    project.midiCCMappings[3]={1,0,0,1,midiCCDestinationTrackVolume};
    project.midiCCMappings[4]={1,0,16,3,1};
    project.midiCCMappings[5]={0,0,17,5,midiCCDestinationAttack};
    screensInitAll();waveformDisplayInit();monitorDisplayInit();backgroundInit();
    screenSetup(&screenMidiCC,0);
    auto tap=[](int keys){key(1,keys);key(0,0);};
    for(int col=0;col<3;++col)tap(keyRight);
    capture("midi-cc-mapping");
    require(screenMidiCC.getPlaybackLevel()==ScreenPlaybackLevel::song,"MIDI mapping permits song playback");
    tapEdit();
    require(currentScreen==&screenSelectionPopup,"destination uses existing popup");
    capture("midi-cc-song-popup");
    tap(keyDown); // Track 1
    tap(keyDown); // Track 2
    tap(keyRight);
    tap(keyDown); // Solo
    capture("midi-cc-track-popup");
    tapEdit();
    require(currentScreen==&screenMidiCC,"selection returns to mapping table");
    require(project.midiCCMappings[0].instrument==1&&project.midiCCMappings[0].destination==midiCCDestinationTrackSolo,
            "popup applies target and parameter together");
    int x,y,width;gfxGetCursor(&x,&y,&width);
    require(x==14&&y==2&&width==26,"popup preserves the destination cursor");
    tap(keyEdit|keyLeft);
    require(project.midiCCMappings[0].destination==midiCCDestinationTrackMute,"EDIT+Left cycles destination");
    require(currentScreen==&screenMidiCC,"cycling does not open popup");
    for(int row=0;row<4;++row)tap(keyDown);
    tapEdit();
    require(currentScreen==&screenSelectionPopup,"instrument destination opens popup");
    tap(keyRight);
    capture("midi-cc-instrument-popup");
    tap(keyOpt);
    require(currentScreen==&screenMidiCC,"cancel returns to same mapping");
    gfxGetCursor(&x,&y,&width);require(x==14&&y==6,"cancel preserves mapping row");
    require(screenTouchTap(39,17),"destination accepts touch across the full width");
    appDraw();require(currentScreen==&screenSelectionPopup,"touch opens destination popup");
    tap(keyOpt);gfxGetCursor(&x,&y,&width);
    require(x==14&&y==17,"touch returns to the selected mapping row");
    printf("MIDI CC mapping: merged destination, categorized popup, target/parameter selection, cursor return and direct cycling passed\n");
    chipnomadDestroy(chipnomadState);gfxCleanup();SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--background-only")) {
    screensInitAll(); waveformDisplayInit(); monitorDisplayInit(); backgroundInit();
    appSettings.backgroundIntensity=70; appSettings.backgroundReaction=0;
    screenSetup(&screenSong,0);appDraw();
    const BackgroundMode modes[]={BackgroundMode::plasma,BackgroundMode::stars,BackgroundMode::waves,
      BackgroundMode::tunnel,BackgroundMode::metaballs,BackgroundMode::spectrum,
      BackgroundMode::rain,BackgroundMode::moire};
    const char* names[]={"plasma","stars","waves","tunnel","metaballs","spectrum","rain","moire"};
    for(int i=0;i<8;++i){appSettings.backgroundMode=modes[i];backgroundApplySettings();currentScreen->fullRedraw();appDraw();capture((std::string("background-")+names[i]).c_str());}
    appSettings.colorScheme.background=0;appSettings.backgroundMode=BackgroundMode::plasma;
    backgroundApplySettings();currentScreen->fullRedraw();appDraw();capture("background-plasma-black");
    appSettings.backgroundMode=BackgroundMode::image; appSettings.backgroundLayout=BackgroundLayout::tile;
    appSettings.backgroundTint=BackgroundTint::original; appSettings.backgroundIntensity=60;
    strcpy(appSettings.backgroundImagePath,"title/night_sky.bmp");
    require(backgroundApplySettings(),"background BMP");
    currentScreen->fullRedraw();appDraw();capture("background-bmp-tile");
    backgroundCleanup(); chipnomadDestroy(chipnomadState); gfxCleanup(); SDL_Quit(); return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--gfx-benchmark")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();
    appSettings.persistentWaveform=0;
    appSettings.cursorBrackets=0;
    screenSetup(&screenSong,0);
    appDraw();gfxUpdateScreen();
    constexpr int frames=5000;
    auto started=std::chrono::steady_clock::now();
    for(int i=0;i<frames;++i){appDraw();gfxUpdateScreen();}
    SDL_RenderFlush(renderer);
    auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    std::ofstream benchmark(std::filesystem::path(output)/"gfx-benchmark.txt",std::ios::trunc);
    require(bool(benchmark),"graphics benchmark log");
    benchmark<<"static: "<<elapsed<<" ms, "<<elapsed*1000.0/frames<<" us/frame\n";
    started=std::chrono::steady_clock::now();
    for(int i=0;i<frames;++i){chipnomadState->trackClipping[0]=(i&1)?5:0;appDraw();gfxUpdateScreen();}
    SDL_RenderFlush(renderer);
    elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    benchmark<<"animated: "<<elapsed<<" ms, "<<elapsed*1000.0/frames<<" us/frame\n";
    benchmark.close();
    chipnomadDestroy(chipnomadState);gfxCleanup();SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--filter-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();cInstrument=0;
    auto& instrument=chipnomadState->project.instruments[0];
    getInstrumentFunctions(InstrumentType::Braids).init(&instrument);
    screenSetup(&screenInstrument,0);appDraw();
    require(instrument.chip.braids.filterCharacter==2,"default Classic");
    for(int character=3;character<=6;++character) {
      require(screenInstrumentBraids.onEdit(1,4,CellEditAction::increase),"filter type edit");
      appDraw();
      require(instrument.chip.braids.filterCharacter==character,"filter type selection");
    }
    require(screenInstrumentBraids.onEdit(1,4,CellEditAction::increase),"filter type edit at Snarl");
    require(instrument.chip.braids.filterCharacter==6,"Snarl is the last filter type");
    auto before=instrument.chip.braids.filterColor;
    require(screenInstrumentBraids.onEdit(1,6,CellEditAction::increase),"filter Color edit");
    appDraw();
    require(instrument.chip.braids.filterColor!=before,"filter Color changed");
    printf("Standard filter UI selection passed\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--audio-in-only")) {
    screensInitAll();waveformDisplayInit();monitorDisplayInit();cInstrument=0;
    auto& instrument=chipnomadState->project.instruments[0];
    getInstrumentFunctions(InstrumentType::AudioIn).init(&instrument);
    screenSetup(&screenInstrument,0);appDraw();
    require(screenTouchTap(12,8),"Audio in Device field selectable");
    tapEdit();
    require(currentScreen==&screenAudioInput,"Device opens Audio Input settings");
    key(1,keyOpt);
    require(currentScreen==&screenInstrument,"Audio Input returns to instrument");
    require(screenTouchTap(12,7),"Audio Input source selector is editable");
    auto source = instrument.audioInputSource;
    key(1,keyEdit|keyRight);key(0,0);
    require(instrument.audioInputSource != source,"Audio Input source changes");
    require(!screenTouchTap(12,11),"input meter is not an editable field");
    require(screenTouchTap(26,11),"filter cutoff selectable at Plaits coordinates");
    auto cutoff=instrument.chip.audioIn.filterCutoffHz;
    key(1,keyEdit|keyLeft);key(0,0);
    require(instrument.chip.audioIn.filterCutoffHz!=cutoff,"touch selected cutoff edits cutoff");
    require(screenTouchTap(7,14),"ADSR attack selectable at Plaits coordinates");
    key(1,keyEdit|keyRight);key(0,0);
    require(instrument.chip.audioIn.attack>0,"touch selected attack edits attack");
    AudioInputManagerTestPeer::captureOpenForTest(audioInputManager,true);
    AudioInputManagerTestPeer::armSlotForTest(audioInputManager,0,0);
    int16_t input[]={24000,-12000};audioInputManager.onCapture(input,1,2);
    appDraw();capture("audio-in-layout");
    require(!audioInputManager.monitor(),"meter does not enable dry monitoring");
    AudioInputManagerTestPeer::captureOpenForTest(audioInputManager,false);
    AudioInputManagerTestPeer::disarmSlotForTest(audioInputManager,0);
    printf("Audio in source navigation, filter/ADSR touch editing and input meter passed\n");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
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
    printf("Renderer: %s\n",gfxGetRendererType());
    screenSetup(&screenTitle,0);appDraw();
    gfxTitleEnd();screenSetup(&screenSong,0);appDraw();
    require(currentScreen==&screenSong,"title opens the tracker");
    const auto afterTitle=framePixels();
    require(trackerInkPixels(afterTitle)>1000,"tracker is fully drawn after the title");
    capture("post-title-before-redraw");
    MainLoopEventData redraw{};redraw.type=MainLoopEvent::fullRedraw;appOnEvent(redraw);appDraw();
    capture("post-title-after-redraw");
    require(trackerInkPixels(framePixels())>1000,"full redraw restores tracker text");
    const int nativeCharWidth=gfxGetCharWidth(),nativeCharHeight=gfxGetCharHeight();
    screenSetup(&screenPhrase,-1);appDraw();
    require(trackerInkPixels(framePixels())>1000,"tracker baseline contains text");
    gfxHandleResize(912,499);
    require(gfxGetCharWidth()==nativeCharWidth&&gfxGetCharHeight()==nativeCharHeight,
            "desktop resize keeps the fixed tracker canvas and font");
    redraw={};redraw.type=MainLoopEvent::fullRedraw;appOnEvent(redraw);appDraw();
    require(trackerInkPixels(framePixels())>1000,"desktop resize restores tracker text");
    capture("post-resize-tracker");
    gfxToggleFullscreen();currentScreen->fullRedraw();appDraw();
    require(trackerInkPixels(framePixels())>1000,"fullscreen entry restores tracker text");
    gfxToggleFullscreen();currentScreen->fullRedraw();appDraw();
    require(trackerInkPixels(framePixels())>1000,"fullscreen exit restores tracker text");
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
      {&screenGraphicsSettings,&screenMixer,&screenChain},{&screenBackgroundSettings,&screenMixer,&screenChain},{&screenTrackVisuals,&screenMixer,&screenChain},
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
    appSettings.mixVolume=chipnomadState->mixVolume=1.0f;projectModified=0;
    require(screenTouchTap(27,12),"master level is selectable beside EQ");
    eventKey(keyEdit|keyDown,true);eventKey(keyEdit|keyDown,false);
    require(std::abs(appSettings.mixVolume-0.9f)<0.001f&&
            std::abs(chipnomadState->mixVolume-0.9f)<0.001f,"master level changes output gain");
    require(!projectModified,"device volume does not modify the project");
    capture("mixer-master-level");
    require(screenTouchTap(12,12),"master EQ label is selectable");
    eventKey(keyEdit,true);eventKey(keyEdit,false);
    require(currentScreen==&screenInstrumentEQ,"master EQ opens bank FE");
    eventKey(keyOpt,true);eventKey(keyOpt,false);
    require(currentScreen==&screenMixer,"OPT returns from master EQ to mixer");
    require(screenTouchTap(4,12),"master FX label is selectable");
    eventKey(keyEdit,true);eventKey(keyEdit,false);
    require(currentScreen==&screenInsertFX,"master EDIT opens master effects");
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
  if(argc==3 && !strcmp(argv[2],"--canonical-song-audio-only")) {
    const char* fixture = std::getenv("CCT_COMPARE_FIXTURE");
    const char* mode = std::getenv("CCT_COMPARE_MODE");
    require(fixture && mode, "full-song comparison specifies fixture and mode");
    require(!strcmp(mode, "classic") || !strcmp(mode, "canonical") || !strcmp(mode, "reip"),
      "full-song comparison mode is valid");
    std::srand(1); stmlib::Random::Seed(1);
    auto* state = chipnomadCreate();
    require(!projectLoad(&state->project,
      (std::string("projects/arrangement-tests/") + fixture + ".cct").c_str()), "full-song fixture loads");
    auto original = std::make_unique<Project>(state->project);
    if (!strcmp(mode, "reip")) require(!convertChainsToPatterns(&state->project), "full-song Reip conversion succeeds");
    canonicalSimplifiedActive = !strcmp(mode, "canonical");
    chipnomadInitChips(state, 48000, nullptr);
    chipnomadReserveRenderBuffers(state, 1024);
    state->mixVolume = 1.0f;
    const auto stem = std::filesystem::path(output) / (std::string(fixture) + "-" + mode + "-full");
    FILE* pcm = fopen((stem.string() + ".f32").c_str(), "wb");
    FILE* trace = fopen((stem.string() + ".trace").c_str(), "wb");
    require(pcm && trace, "full-song PCM and trace open");
    require(chipnomadQueuePlaybackStartSong(state, 0, 0, 0), "full-song playback starts without Song looping");
    std::srand(1); stmlib::Random::Seed(1);
    float audio[256 * 2];
    int previous[PROJECT_MAX_TRACKS][4];
    for (auto& position : previous) std::fill_n(position, 4, -1);
    int reached[PROJECT_MAX_TRACKS];
    std::fill_n(reached, PROJECT_MAX_TRACKS, -1);
    bool started = false, ended = false;
    int frames = 0, tailFrames = 0;
    for (; frames < 48000 * 600; frames += 256) {
      chipnomadRender(state, audio, 256);
      require(fwrite(audio, sizeof(float), 512, pcm) == 512, "full-song PCM writes");
      const auto* status = chipnomadGetPlaybackStatus(state);
      for (int t = 0; t < state->project.tracksCount; ++t) {
        const auto& track = status->tracks[t];
        int position[] = {(int)track.mode, track.songRow, track.chainRow, track.phraseRow};
        if (memcmp(previous[t], position, sizeof(position))) {
          require(fprintf(trace, "%d %d %d %d %d %d\n", frames + 256, t,
            position[0], position[1], position[2], position[3]) > 0, "full-song trace writes");
          std::copy_n(position, 4, previous[t]);
        }
        if (track.mode == PlaybackMode::song)
          reached[t] = std::max(reached[t], track.songRow);
      }
      started |= status->isPlaying != 0;
      if (started && !status->isPlaying) ended = true;
      if (ended && (tailFrames += 256) >= 48000 * 5) { frames += 256; break; }
    }
    require(fclose(pcm) == 0 && fclose(trace) == 0, "full-song outputs close");
    require(started && ended && tailFrames >= 48000 * 5,
      "full-song arrangement reaches natural stop within 10 minutes, plus five-second tail");
    for (int t = 0; t < original->tracksCount; ++t) {
      int last = -1;
      for (int r = 0; r < PROJECT_MAX_LENGTH; ++r)
        if (original->song[r][t] < PROJECT_MAX_CHAINS &&
            original->chains[original->song[r][t]].rows[0].phrase < PROJECT_MAX_PHRASES) last = r;
      printf("track %d: last arranged row=%d, highest played row=%d\n", t, last, reached[t]);
      require(last < 0 || reached[t] >= last, "full-song playback reaches each track's final arranged row");
    }
    printf("%s %s: full arrangement and tail rendered, %.3f seconds (%d frames)\n",
      fixture, mode, frames / 48000.0, frames);
    chipnomadDestroy(state);
    chipnomadDestroy(chipnomadState); gfxCleanup(); SDL_Quit(); return 0;
  }
  if(argc==3 && (!strcmp(argv[2],"--canonical-simple-only") || !strcmp(argv[2],"--canonical-audio-only"))) {
    auto renderFixture = [&](const std::string& path, bool canonical, bool converted) {
      std::srand(1);
      stmlib::Random::Seed(1);
      auto* state = chipnomadCreate();
      require(!projectLoad(&state->project, path.c_str()), "comparison render loads fixture");
      if (converted) require(!convertChainsToPatterns(&state->project), "Reip comparison converts fixture");
      canonicalSimplifiedActive = canonical;
      chipnomadInitChips(state, 48000, nullptr);
      chipnomadReserveRenderBuffers(state, 1024);
      state->mixVolume = 1.0f;
      require(chipnomadQueuePlaybackStartSong(state, 0, 0, 0), "comparison queues canonical Song playback");
      std::vector<float> samples(48000 * 3 * 2);
      std::srand(1);
      stmlib::Random::Seed(1);
      for (int frame = 0; frame < 48000 * 3; frame += 1024)
        chipnomadRender(state, samples.data() + frame * 2, std::min(1024, 48000 * 3 - frame));
      chipnomadDestroy(state);
      return samples;
    };
    auto phraseCount = [](Project& p) {
      bool used[PROJECT_MAX_PHRASES] = {};
      for (const auto& chain : p.chains)
        for (const auto& row : chain.rows)
          if (row.phrase < PROJECT_MAX_PHRASES) used[row.phrase] = true;
      for (const auto& pattern : p.patterns)
        for (const auto& bar : pattern.bars)
          if (bar.phrase < PROJECT_MAX_PHRASES) used[bar.phrase] = true;
      return std::count(std::begin(used), std::end(used), true);
    };
    if (!strcmp(argv[2], "--canonical-audio-only")) {
      const char* fixture = std::getenv("CCT_COMPARE_FIXTURE");
      const char* mode = std::getenv("CCT_COMPARE_MODE");
      require(fixture && mode, "isolated audio comparison specifies fixture and mode");
      require(!strcmp(mode, "classic") || !strcmp(mode, "canonical") || !strcmp(mode, "reip"),
        "isolated audio comparison mode is valid");
      auto samples = renderFixture(std::string("projects/arrangement-tests/") + fixture + ".cct",
        !strcmp(mode, "canonical"), !strcmp(mode, "reip"));
      const auto path = std::filesystem::path(output) / (std::string(fixture) + "-" + mode + ".f32");
      FILE* file = fopen(path.string().c_str(), "wb");
      require(file != nullptr, "comparison PCM opens");
      require(fwrite(samples.data(), sizeof(float), samples.size(), file) == samples.size(), "comparison PCM writes");
      require(fclose(file) == 0, "comparison PCM closes");
      printf("%s %s: rendered %zu float samples at 48000 Hz\n", fixture, mode, samples.size());
      chipnomadDestroy(chipnomadState); gfxCleanup(); SDL_Quit(); return 0;
    }
    for (const char* name : {"Ambient_01", "ChipTek", "ChooChooseAcid"}) {
      const std::string path = std::string("projects/arrangement-tests/") + name + ".cct";
      canonicalSimplifiedActive = grooveboxEditorActive = false;
      require(!projectLoad(&chipnomadState->project, path.c_str()), "canonical fixture loads");
      auto& p = chipnomadState->project;
      auto original = std::make_unique<Project>(p);
      require(p.viewMode == ViewMode::classic, "comparison fixtures are canonical Classic projects");
      const int originalPhrases = phraseCount(p);
      projectModified = false;
      screenSetup(&screenViewSettings, 0); appDraw();
      appChord(keyEdit, keyLeft);
      require(canonicalSimplifiedActive && p.viewMode == ViewMode::classic && !projectModified,
        "View enters Simple C directly without conversion or project modification");
      int cells = 0, bars = 0, transposed = 0, shared = 0, retained = 0;
      for (int row = 0; row < PROJECT_MAX_LENGTH; ++row) {
        for (int t = 0; t < p.tracksCount; ++t) {
          int chain = p.song[row][t];
          if (chain >= PROJECT_MAX_CHAINS) continue;
          ++cells;
          int length = grooveboxTrackLength(&p, row, t);
          bars += length;
          for (int b = length; b < 16; ++b)
            retained += p.chains[chain].rows[b].phrase < PROJECT_MAX_PHRASES;
          for (int b = 0; b < std::max(1, length); ++b) {
            *pSongRow = row; *pSongTrack = t; *pChainRow = b;
            screenSetup(&screenPhrase, -1); appDraw();
            require(currentScreen == &screenPhrase, "canonical Simple C uses real Phrase editor");
            require(*pChainRow == b, "canonical browsing retains selected bar");
            transposed += p.chains[chain].rows[b].transpose != 0;
            shared += grooveboxCellShared(&p, row, t, b);
          }
        }
      }
      require(!projectModified && !memcmp(original.get(), &p, sizeof(Project)),
        "all canonical bars can be browsed without allocation, transpose baking or reference changes");
      *pSongRow = 0; *pSongTrack = 0; *pChainRow = 0;
      screenSetup(&screenSong, -1); appDraw(); appChord(keyShift, keyRight);
      require(currentScreen == &screenPhrase, "Simple C Song skips Chain");
      capture((std::string(name) + "-canonical-simple").c_str());
      appChord(keyShift, keyLeft);
      require(currentScreen == &screenSong, "Simple C Phrase returns directly to Song");
      screenSetup(&screenViewSettings, 0); appDraw(); appChord(keyEdit, keyRight);
      require(!canonicalSimplifiedActive && !grooveboxEditorActive && p.viewMode == ViewMode::classic,
        "Simple C returns to Classic directly");
      appChord(keyEdit, keyLeft); appChord(keyEdit, keyLeft);
      require(grooveboxEditorActive && !canonicalSimplifiedActive, "Simple C switches directly to Groovebox");
      appChord(keyEdit, keyLeft);
      require(currentScreen != &screenViewSettings && grooveboxEditorActive,
        "pending Reip conversion preserves the active editor");
      appChord(0, keyDown); appChord(0, keyDown); appChord(0, keyEdit);
      require(currentScreen == &screenViewSettings && grooveboxEditorActive && p.viewMode == ViewMode::classic,
        "cancelling Reip conversion preserves Groovebox and Classic backing");
      appChord(keyEdit, keyRight); appChord(keyEdit, keyRight);
      require(!grooveboxEditorActive && !canonicalSimplifiedActive, "direct editor cycle returns to Classic");
      require(!memcmp(original.get(), &p, sizeof(Project)), "canonical mode roundtrip preserves entire project");
      if (grooveboxCellShared(&p, 0, 0, 0)) {
        canonicalSimplifiedActive = true;
        screenPhrase.init(); screenSetup(&screenPhrase, -1); appDraw();
        appChord(keyEdit, keyRight);
        require(!memcmp(original.get(), &p, sizeof(Project)), "shared canonical note edit does not silently alter other occurrences");
        appChord(keyShift, keyLeft);
        appChord(keyShift, keyOpt);
        appResetInputState();
        appButton(keyShift, true);
        appButton(keyEdit, true); appButton(keyEdit, false);
        appButton(keyEdit, true); appButton(keyEdit, false);
        appButton(keyShift, false);
        require(!grooveboxCellShared(&p, 0, 0, 0),
          "Song deep-clone gesture isolates the selected chain and all its phrases");
        appChord(keyShift, keyRight);
        require(p.song[0][0] != original->song[0][0], "explicit copy creates a different chain");
        int sourceChain = original->song[0][0];
        require(!memcmp(&p.chains[sourceChain], &original->chains[sourceChain], sizeof(Chain)),
          "explicit copy preserves the complete original chain including transpose and gaps");
        for (int b = 0; b < 16; ++b) {
          int src = original->chains[sourceChain].rows[b].phrase;
          if (src < PROJECT_MAX_PHRASES)
            require(!memcmp(&p.phrases[src], &original->phrases[src], sizeof(Phrase)),
              "explicit copy preserves every original shared phrase");
        }
      }

      require(!projectLoad(&p, path.c_str()), "reload source for original Reip comparison");
      require(!convertChainsToPatterns(&p), "original Reip conversion succeeds");
      const int convertedPhrases = phraseCount(p);
      require(!convertPatternsToChains(&p), "original Reip reverse conversion succeeds");
      const bool sameChains = !memcmp(p.chains, original->chains, sizeof(p.chains));
      const bool sameSong = !memcmp(p.song, original->song, sizeof(p.song));
      printf("%s: cells=%d bars=%d transposed=%d shared-bars=%d retained=%d; SimpleC added phrases=0; Reip added phrases=%d roundtrip Song=%s Chains=%s\n",
        name, cells, bars, transposed, shared, retained, convertedPhrases - originalPhrases,
        sameSong ? "same" : "changed", sameChains ? "same" : "changed");

      require(!projectLoad(&p, path.c_str()), "reload source for edit workflow");
      canonicalSimplifiedActive = true;
      grooveboxEditorActive = false;
      chipnomadInitChips(chipnomadState, 48000, nullptr);
      chipnomadState->mixVolume = 1.0f;
      int emptyScene = 0;
      for (; emptyScene < PROJECT_MAX_LENGTH; ++emptyScene)
        if (std::all_of(std::begin(p.song[emptyScene]), std::end(p.song[emptyScene]),
            [](uint16_t c) { return c == EMPTY_VALUE_16; })) break;
      require(emptyScene < PROJECT_MAX_LENGTH, "fixture has room for isolated editing");
      screenPhrase.init(); screenSong.init();
      *pSongRow = emptyScene; *pSongTrack = 0; *pChainRow = 0;
      screenSetup(&screenSong, -1); appDraw(); appChord(0, keyEdit);
      appChord(keyShift, keyRight);
      require(currentScreen == &screenPhrase, "new canonical chain opens without conversion");
      appChord(0, keyEdit);
      int chain = p.song[emptyScene][0];
      require(chain < PROJECT_MAX_CHAINS, "new Song occurrence has a valid canonical chain");
      int phrase = p.chains[chain].rows[0].phrase;
      require(phrase < PROJECT_MAX_PHRASES && p.phrases[phrase].rows[0].note != EMPTY_VALUE_8,
        "new canonical note persists in a real phrase");
      appChord(0, keyRight); appChord(keyEdit, keyRight);
      require(p.phrases[phrase].rows[0].instrument == 1, "canonical instrument edits use Phrase controls");
      appChord(0, keyRight); appChord(keyEdit, keyLeft);
      require(p.phrases[phrase].rows[0].volume == PHRASE_VOLUME_MAX - 1, "canonical volume edits use Phrase controls");
      appChord(0, keyRight); appChord(keyEdit, keyUp);
      require(p.phrases[phrase].rows[0].fx[0].command != FX_ID_EMPTY, "canonical FX picker works");
      appChord(keyShift, keyUp);
      appChord(keyEdit, keyRight);
      require(grooveboxTrackLength(&p, emptyScene, 0) == appSettings.defaultPatternLength + 1,
        "canonical Length adds independent no-event bar");
      appChord(keyEdit, keyLeft);
      appChord(keyEdit, keyUp);
      require(grooveboxTrackLength(&p, emptyScene, 0) == appSettings.defaultPatternLength * 2,
        "canonical Length doubles using independent copies");
      appChord(keyEdit, keyDown);
      require(grooveboxTrackLength(&p, emptyScene, 0) == appSettings.defaultPatternLength,
        "canonical Length halves using the existing structural helper");
      appChord(0, keyRight); appChord(0, keyRight);
      appChord(0, keyEdit);
      require(p.playheadMode == PlayheadMode::loopActiveBar &&
        currentScreen->getPlaybackLevel() == ScreenPlaybackLevel::phrase, "canonical BAR uses existing phrase loop");
      float transportAudio[256 * 2];
      auto settleTransport = [&] {
        // Commands become visible on tracker ticks, not on every render block.
        for (int frames = 0; frames < std::ceil(48000.0 / p.tickRate) + 256; frames += 256)
          chipnomadRender(chipnomadState, transportAudio, 256);
        appDraw();
      };
      settleTransport();
      chipnomadQueuePlaybackStop(chipnomadState);
      settleTransport();
      appChord(0, keyPlay);
      settleTransport();
      require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying &&
        chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].mode == PlaybackMode::phrase &&
        chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].loop,
        "actual Start launches the BAR phrase loop");
      appChord(0, keyPlay);
      settleTransport();
      require(!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "actual Start stops the BAR loop");
      appChord(0, keyEdit);
      require(currentScreen->getPlaybackLevel() == ScreenPlaybackLevel::chain, "canonical PAT uses existing chain loop");
      appChord(0, keyPlay);
      settleTransport();
      require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying &&
        chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].mode == PlaybackMode::chain &&
        chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].loop,
        "actual Start launches the PAT chain loop");
      appChord(0, keyPlay);
      settleTransport();
      require(!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "actual Start stops the PAT loop");
      appChord(0, keyLeft);
      const auto notesBeforeTranspose = p.phrases[phrase];
      appChord(keyEdit, keyRight);
      require(p.chains[chain].rows[0].transpose == 1 &&
        !memcmp(&notesBeforeTranspose, &p.phrases[phrase], sizeof(Phrase)), "bar transpose is stored in chain, never baked into notes");
      capture((std::string(name) + "-canonical-edited").c_str());
      appChord(0, keyDown);
      appChord(keyShift, keyRight);
      require(currentScreen == &screenInstrument, "canonical SHIFT+RIGHT reaches Instrument");
      appChord(keyShift, keyLeft);
      require(currentScreen == &screenPhrase && canonicalSimplifiedActive, "Instrument returns to canonical pattern");
      auto saved = std::make_unique<Project>(p);
      const auto savePath = std::filesystem::path(output) / (std::string(name) + "-canonical-roundtrip.cct");
      require(!projectSave(&p, savePath.string().c_str()), "canonical edited project saves");
      require(!projectLoad(&p, savePath.string().c_str()), "canonical edited project reloads");
      require(p.viewMode == ViewMode::classic &&
        !memcmp(saved->song, p.song, sizeof(p.song)) &&
        !memcmp(saved->chains, p.chains, sizeof(p.chains)) &&
        !memcmp(saved->phrases, p.phrases, sizeof(p.phrases)), "save/reload retains all canonical events, transpose and references");
      std::filesystem::remove(savePath);
      canonicalSimplifiedActive = false;
    }
    auto& p = chipnomadState->project;
    projectFree(&p); projectInitAY(&p);
    canonicalSimplifiedActive = true;
    *pSongRow = 0; *pSongTrack = 0; *pChainRow = 0;
    p.song[0][0] = 0;
    p.chains[0].rows[0] = {0, 2};
    p.chains[0].rows[2] = {1, 0xff};
    p.phrases[0].rows[0].note = 48; p.phrases[1].rows[0].note = 60;
    screenPhrase.init(); screenSetup(&screenPhrase, -1); appDraw();
    auto gapOriginal = std::make_unique<Project>(p);
    appChord(keyShift, keyUp);
    appChord(keyEdit, keyRight);
    require(!memcmp(gapOriginal.get(), &p, sizeof(Project)),
      "Length refuses to bridge a gap and implicitly activate retained bars");
    appChord(0, keyDown);
    appChord(keyOpt, keyDown);
    require(*pChainRow == 0, "canonical bar navigation stops at the playback gap");
    require(p.chains[0].rows[2].phrase == 1 && p.chains[0].rows[2].transpose == 0xff,
      "retained reference and signed transpose survive browsing");
    p.song[0][1] = 1;
    p.chains[1].rows[2].phrase = 1;
    appChord(keyOpt, keyRight);
    require(*pSongTrack == 1 && *pChainRow == 0, "track navigation handles a leading gap without underflow");
    appChord(0, keyEdit);
    require(p.chains[1].rows[0].phrase == EMPTY_VALUE_16 && p.chains[1].rows[2].phrase == 1,
      "editing a leading gap does not activate retained data");
    p.song[0][1] = EMPTY_VALUE_16;
    p.chains[0].rows[2].phrase = EMPTY_VALUE_16;
    p.song[1][0] = p.song[2][0] = 0;
    const Chain sourceChain = p.chains[0];
    screenSong.init(); screenSetup(&screenSong, -1); appDraw();
    auto selectTwo = [&] {
      appChord(keyShift, keyOpt);
      appChord(0, keyDown);
    };
    selectTwo(); appChord(keyOpt, keyUp);
    int mergedChain = p.song[0][0];
    require(mergedChain != 0 && mergedChain < PROJECT_MAX_CHAINS &&
      p.song[1][0] == EMPTY_VALUE_16 && p.song[2][0] == 0,
      "canonical merge changes only selected Song occurrences");
    require(!memcmp(&sourceChain, &p.chains[0], sizeof(Chain)) &&
      p.chains[mergedChain].rows[0].phrase == 0 && p.chains[mergedChain].rows[1].phrase == 0 &&
      p.chains[mergedChain].rows[0].transpose == 2 && p.chains[mergedChain].rows[1].transpose == 2,
      "repeated-chain merge preserves source, phrase IDs and row transposes");
    p.song[0][0] = p.song[1][0] = 0;
    for (int b = 0; b < 9; ++b) p.chains[0].rows[b] = {0, 2};
    screenSong.init(); screenSetup(&screenSong, -1); appDraw(); selectTwo();
    auto refused = std::make_unique<Project>(p);
    appChord(keyOpt, keyUp);
    require(!memcmp(refused.get(), &p, sizeof(Project)), "overlength canonical merge changes nothing");
    chainClear(&p.chains[0]); p.chains[0].rows[0] = {0, 2};
    p.song[0][1] = p.song[1][1] = 1;
    chainClear(&p.chains[1]);
    for (int b = 0; b < 9; ++b) p.chains[1].rows[b] = {0, 2};
    screenSong.init(); screenSetup(&screenSong, -1); appDraw(); selectTwo(); appChord(0, keyRight);
    *refused = p;
    appChord(keyOpt, keyUp);
    require(!memcmp(refused.get(), &p, sizeof(Project)), "one refused track prevents any partial multi-track merge");
    p.song[0][1] = p.song[1][1] = EMPTY_VALUE_16;
    p.chains[0].rows[2] = {1, 0xff};
    screenSong.init(); screenSetup(&screenSong, -1); appDraw(); selectTwo();
    *refused = p;
    appChord(keyOpt, keyUp);
    require(!memcmp(refused.get(), &p, sizeof(Project)), "canonical merge preserves and refuses retained bars after a gap");
    p.chains[0].rows[2].phrase = EMPTY_VALUE_16;
    for (int c = 0; c < PROJECT_MAX_CHAINS; ++c) p.song[c][7] = c;
    screenSong.init(); screenSetup(&screenSong, -1); appDraw(); selectTwo();
    *refused = p;
    appChord(keyOpt, keyUp);
    require(!memcmp(refused.get(), &p, sizeof(Project)), "exhausted-chain canonical merge changes nothing");
    projectFree(&p); projectInitAY(&p);
    p.song[0][0] = 0;
    for (int f = 0; f < PROJECT_MAX_PHRASES; ++f) p.patterns[f / 16].bars[f % 16].phrase = f;
    screenSong.init(); screenPhrase.init();
    screenSetup(&screenPhrase, -1); appDraw();
    *refused = p;
    appChord(0, keyEdit);
    require(!memcmp(refused.get(), &p, sizeof(Project)), "exhausted-phrase canonical first edit changes nothing");
    projectFree(&p); projectInitAY(&p);
    p.song[0][0] = 0;
    screenSong.init(); screenPhrase.init();
    screenSetup(&screenPhrase, -1); appDraw();
    resetCopyBuffers();
    *refused = p;
    appChord(keyShift, keyEdit);
    require(!memcmp(refused.get(), &p, sizeof(Project)), "empty Phrase clipboard allocates no canonical bars");
    copyPhrase(1, 3, 0, 3, 0, 0);
    require(!canPastePhrase(0), "FX-only clipboard is incompatible with the note column");
    appChord(keyShift, keyEdit);
    require(!memcmp(refused.get(), &p, sizeof(Project)), "incompatible Phrase clipboard allocates no canonical bars");
    p.phrases[1].rows[0].note = 60;
    copyPhrase(1, 0, 0, 0, 0, 0);
    appChord(keyShift, keyEdit);
    int pasted = grooveboxPhraseAt(&p, 0, 0, 0);
    require(pasted >= 0 && pasted != 1 && p.phrases[pasted].rows[0].note == 60 &&
      p.phrases[1].rows[0].note == 60 && grooveboxTrackLength(&p, 0, 0) == appSettings.defaultPatternLength,
      "valid Phrase paste materializes the default bars and preserves its source");
    canonicalSimplifiedActive = false;
    printf("Canonical Simplified comparison, actual input editing and persistence passed for all three fixtures.\n");
    chipnomadDestroy(chipnomadState); gfxCleanup(); SDL_Quit(); return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--groovebox-feedback-only")) {
    auto& p = chipnomadState->project;
    projectFree(&p); projectInitAY(&p);
    for (int t = 0; t < 3; ++t) {
      p.song[0][t] = (t + 1) * 16;
      int length = t == 0 ? 1 : t == 1 ? 3 : 16;
      for (int b = 0; b < length; ++b) {
        int phrase = t * 128 + b;
        p.chains[p.song[0][t]].rows[b].phrase = phrase;
        p.phrases[phrase].rows[0].note = 48 + b;
        p.phrases[phrase].rows[0].instrument = 0;
      }
    }
    *pSongRow = *pSongTrack = *pChainRow = 0;
    grooveboxEditorActive = true;
    screenGroovebox.init();
    screenSetup(&screenGroovebox, 0); appDraw();
    auto original = std::make_unique<Project>(p);
    const ColorScheme theme = appSettings.colorScheme;
    auto cellPixels = [&](int x, int y, int width = 1) {
      SDL_Rect region{x * gfxGetCharWidth(), y * gfxGetCharHeight(),
        width * gfxGetCharWidth(), gfxGetCharHeight()};
      std::vector<uint32_t> pixels(region.w * region.h);
      require(SDL_RenderReadPixels(renderer, &region, SDL_PIXELFORMAT_ARGB8888,
        pixels.data(), region.w * 4) == 0, "Groovebox cell pixels");
      return pixels;
    };
    auto expectText = [&](int x, int y, const char* text, int color) {
      auto actual = cellPixels(x, y, (int)strlen(text));
      gfxSetFgColor(color); gfxPrint(x, y, text);
      require(actual == cellPixels(x, y, (int)strlen(text)), "Groovebox displayed text and colour");
    };
    expectText(0, 5, "10", theme.textLabel);
    expectText(2, 2, "0", theme.textTitles);
    expectText(4, 2, "1", theme.textInfo);
    expectText(2, 4, "o", theme.textTitles);
    expectText(0, 1, "                                        ", theme.textInfo);
    expectText(0, 17, "BAR 01                                  ", theme.textInfo);
    expectText(0, 0, "PATTERN", theme.textTitles);
    expectText(0, 11, "T1 STEP 00                              ", theme.textInfo);
    auto& playheads = chipnomadState->uiPlaybackStatus;
    playheads.isPlaying = 1;
    for (int t = 0; t < 3; ++t) {
      playheads.tracks[t].mode = PlaybackMode::song;
      playheads.tracks[t].songRow = 0;
      playheads.tracks[t].chainRow = 0;
      playheads.tracks[t].phraseRow = 1 + t;
    }
    currentScreen->fullRedraw(); appDraw();
    for (int t = 0; t < 3; ++t) {
      expectText(2 + (1 + t) * 2, 3 + t, ".", theme.playMarkers);
      const char label[] = {char('1' + t), 0};
      expectText(2 + (1 + t) * 2, 2, label, theme.playMarkers);
    }
    playheads.tracks[1].songRow = 1;
    currentScreen->fullRedraw(); appDraw();
    expectText(6, 2, "2", theme.textInfo);
    playheads.isPlaying = 0;
    currentScreen->fullRedraw(); appDraw();
    expectText(4, 2, "1", theme.textInfo);
    capture("feedback-grid-sidebar");
    appChord(keyShift, keyUp); appChord(0, keyLeft);
    appChord(keyEdit, keyRight);
    require(*pChainRow == 1 && *pSongTrack == 0, "Bar header scrolls beyond selected one-bar track");
    expectText(2, 3, "o", theme.textEmpty);
    expectText(8, 13, noteName(&p, 48), theme.textEmpty);
    playheads.isPlaying = 1;
    playheads.tracks[0].mode = PlaybackMode::song;
    playheads.tracks[0].songRow = 0;
    playheads.tracks[0].chainRow = 0;
    playheads.tracks[0].phraseRow = 3;
    currentScreen->fullRedraw(); appDraw();
    expectText(8, 3, ".", theme.playMarkers);
    expectText(8, 2, "3", theme.playMarkers);
    playheads.isPlaying = 0;
    currentScreen->fullRedraw(); appDraw();
    capture("feedback-repeated-bar");
    appChord(keyShift, keyDown);
    appChord(0, keyEdit);
    require(!memcmp(original.get(), &p, sizeof(Project)), "Repeated preview edit does not append or mutate source");
    appChord(keyShift, keyUp); appChord(0, keyLeft);
    for (int i = 0; i < 3; ++i) appChord(keyEdit, keyRight);
    appChord(keyShift, keyDown); appChord(keyOpt, keyDown);
    require(*pChainRow == 4 && *pSongTrack == 1, "Short track change retains global viewed bar");
    appChord(keyShift, keyUp);
    expectText(8, 13, noteName(&p, 49), theme.textEmpty);
    require(!memcmp(original.get(), &p, sizeof(Project)), "Non-power-of-two repeated preview preserves canonical data");
    appChord(keyOpt, keyUp);
    appChord(0, keyLeft);
    appChord(keyEdit, keyOpt); appChord(0, keyEdit);
    appChord(keyShift, keyDown);
    for (int i = 0; i < 16; ++i) appChord(keyOpt, keyRight);
    require(*pChainRow == 2, "Grid edge advances bar beyond short selected track");
    for (int i = 0; i < 16 * 14; ++i) appChord(keyOpt, keyRight);
    require(*pChainRow == 0, "Grid wraps at longest track scene length");
    appChord(keyOpt, keyLeft);
    require(*pChainRow == 15, "Left grid edge wraps to final scene bar");
    for (int i = 0; i < 3; ++i) appChord(keyOpt, keyDown);
    require(*pSongTrack == 3, "Empty track can be selected");
    appChord(keyOpt, keyRight);
    require(*pChainRow == 0, "Empty track can scroll and wrap scene bars");
    appChord(0, keyOpt);
    require(currentScreen == &screenGroovebox, "OPT no longer opens a redundant Scenes overview");
    appChord(keyShift, keyLeft);
    require(currentScreen == &screenSong, "SHIFT LEFT opens canonical Scenes");
    capture("feedback-scenes");
    *pSongTrack = 0;
    screenSetup(&screenSong, -1); appDraw();
    appChord(keyShift, keyOpt);
    appChord(0, keyOpt);
    require(*pSongRow == 1, "Scenes copy moves below standard Song selection");
    appChord(keyShift, keyEdit);
    require(p.song[1][0] == p.song[0][0], "Scenes pastes canonical pattern reference");
    appChord(keyShift, keyRight);
    require(currentScreen == &screenGroovebox, "Scenes SHIFT RIGHT opens step editor");
    chipnomadQueueProjectRefresh(chipnomadState);
    appChord(keyShift, keyLeft);
    appChord(keyEdit, keyPlay);
    float audio[4096 * 2];
    chipnomadRender(chipnomadState, audio, 4096);
    require(chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].mode == PlaybackMode::live,
      "Scenes EDIT PLAY launches scene using existing per-track live transport");
    *pSongRow = 0; *pSongTrack = 3;
    screenSetup(&screenSong, -1); appDraw();
    auto beforeLaunch = std::make_unique<Project>(p);
    appChord(keyEdit, keyPlay);
    require(!memcmp(beforeLaunch.get(), &p, sizeof(Project)),
      "Scenes EDIT PLAY on an empty track queues without first creating a pattern");
    chipnomadQueuePlaybackStop(chipnomadState);
    chipnomadRender(chipnomadState, audio, 4096);
    *pSongRow = *pSongTrack = 0;
    screenSetup(&screenSong, -1); appDraw();
    currentScreen->onInput(1, keyOpt, 2);
    currentScreen->onInput(0, 0, 2);
    for (int i = 0; i < 30; ++i) appDraw();
    expectText(0, 0, "SCENES LIVE", theme.textTitles);
    appChord(0, keyPlay);
    chipnomadRender(chipnomadState, audio, 4096);
    require(chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].mode == PlaybackMode::live,
      "SCENES LIVE PLAY launches selected track via Song live mode");
    require(chipnomadGetPlaybackStatus(chipnomadState)->tracks[1].mode == PlaybackMode::stopped,
      "SCENES LIVE does not launch unselected tracks");
    currentScreen->onInput(1, keyOpt, 2);
    currentScreen->onInput(0, 0, 2);
    for (int i = 0; i < 30; ++i) appDraw();
    expectText(0, 0, "SCENES     ", theme.textTitles);
    appChord(0, keyPlay);
    chipnomadRender(chipnomadState, audio, 4096);
    require(!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying,
      "Leaving SCENES LIVE restores standard PLAY stop");
    screenSetup(&screenGroovebox, 0); appDraw();
    getInstrumentFunctions(InstrumentType::SID).init(&p.instruments[0]);
    auto& status = chipnomadState->uiPlaybackStatus;
    status.tracks[0].note.instrument = 0;
    status.tracks[0].note.pitchFinal = 48;
    auto& monitor = chipnomadState->voiceMonitors[0];
    monitor.active = 0;
    waveformDisplayInvalidate(); appDraw();
    const auto idleWaveform = cellPixels(36, 3);
    for (int i = 0; i < VOICE_MONITOR_SAMPLES; ++i) monitor.samples[i] = std::sin(i * 0.083f) * 0.7f;
    monitor.active = 1;
    waveformDisplayInvalidate(); appDraw();
    require(cellPixels(36, 3) != idleWaveform, "Groovebox renders live shared waveform aligned with track row");
    capture("feedback-live-sidebar");
    grooveboxEditorActive = false;
    printf("Groovebox feedback navigation, read-only repeats, colours, Scenes clipboard and launch passed.\n");
    chipnomadDestroy(chipnomadState); gfxCleanup(); SDL_Quit(); return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--groovebox-only")) {
    for (const char* name : {"Ambient_01", "ChipTek", "ChooChooseAcid"}) {
      const std::string path = std::string("projects/arrangement-tests/") + name + ".cct";
      require(!projectLoad(&chipnomadState->project, path.c_str()), "Groovebox project loaded");
      auto original = std::make_unique<Project>(chipnomadState->project);
      projectModified = 0;
      *pSongRow = 0; *pSongTrack = 0; *pChainRow = 0;
      screenGroovebox.init();
      screenSetup(&screenSong, -1); appDraw();
      screenSetup(&screenViewSettings, 0); appDraw();
      key(1, keyEdit | keyLeft); key(0, 0);
      require(canonicalSimplifiedActive, "View selects canonical Simplified without conversion");
      key(1, keyEdit | keyLeft); key(0, 0);
      require(grooveboxEditorActive, "View selects Groovebox without conversion");
      screenSetup(&screenSong, -1); appDraw();
      require(currentScreen == &screenSong && grooveboxEditorActive, "Canonical Scenes retains Song editing");
      capture((std::string(name) + "-clips").c_str());
      key(1, keyShift | keyRight); key(0, 0);
      require(currentScreen == &screenGroovebox, "Scenes opens Groovebox with SHIFT+RIGHT");
      capture(name);
      require(trackerInkPixels(framePixels()) > 1000, "text grid is visible");
      key(1, keyRight); key(0, 0);
      key(1, keyDown); key(0, 0);
      key(1, keyOpt | keyRight); key(0, keyOpt);
      key(1, keyOpt | keyDown); key(0, 0);
      require(*pSongRow == 0 && *pSongTrack == 1, "B moves grid track without changing scene");
      capture((std::string(name) + "-step-editor").c_str());
      key(1, keyOpt); key(0, 0);
      require(currentScreen == &screenGroovebox, "OPT leaves editor open without redundant Scenes view");
      key(1, keyShift | keyLeft); key(0, 0);
      capture((std::string(name) + "-scenes").c_str());
      key(1, keyShift | keyRight); key(0, 0);
      key(1, keyShift | keyDown); key(0, 0);
      key(1, keyShift | keyUp); key(0, 0);
      key(1, keyRight); key(0, 0); key(1, keyEdit); key(0, 0);
      require(currentScreen == &screenSelectionPopup, "Groovebox tools opened");
      capture((std::string(name) + "-tools").c_str());
      key(1, keyOpt); key(0, 0);
      require(currentScreen == &screenGroovebox, "tools cancellation returns without conversion");
      key(1, keyOpt); key(0, 0);
      key(1, keyShift | keyDown); key(0, 0);
      key(1, keyOpt | keyDown); key(0, 0);
      key(1, keyShift | keyLeft); key(0, 0);
      require(currentScreen == &screenSong && grooveboxEditorActive,
        "SHIFT+LEFT returns to CLIPS without changing mode");
      key(1, keyShift | keyLeft); key(0, 0);
      require(currentScreen == &screenMixer, "CLIPS SHIFT+LEFT reaches Mixer");
      key(1, keyShift | keyRight); key(0, 0);
      require(currentScreen == &screenSong && grooveboxEditorActive, "Mixer SHIFT+RIGHT returns to CLIPS");
      for (const AppScreen* target : {&screenChain, &screenPhrase}) {
        screenSetup(target, -1); appDraw();
        require(currentScreen == &screenGroovebox, "ancillary returns cannot reopen Classic editors");
      }
      require(!projectModified && !memcmp(original.get(), &chipnomadState->project, sizeof(Project)),
        "view switching and navigation preserve the complete project");
      grooveboxEditorActive = false;
    }
    if (const char* repro = std::getenv("CCT_GROOVEBOX_REPRO_PROJECT")) {
      require(!projectLoad(&chipnomadState->project, repro), "console reproduction project loads");
      auto& p = chipnomadState->project;
      if (p.viewMode == ViewMode::simplified)
        require(!convertPatternsToChains(&p), "console reproduction converts via existing Reip code");
      chipnomadInitChips(chipnomadState, 48000, nullptr);
      *pSongRow = 0; *pSongTrack = 0; *pChainRow = 0;
      grooveboxEditorActive = true;
      screenGroovebox.init(); screenSetup(&screenGroovebox, 0); appDraw();
      const int sourceChain = p.song[0][0];
      const int sourcePhrase = p.chains[sourceChain].rows[0].phrase;
      const Phrase source = p.phrases[sourcePhrase];
      require(grooveboxCellShared(&p, 0, 0, 0), "console project reproduces shared edit lock");
      appChord(keyEdit, keyRight);
      require(currentScreen == &screenSelectionPopup, "shared edit offers explicit independence instead of dead shortcut");
      capture("shared-edit-choice");
      appChord(0, keyOpt);
      require(currentScreen == &screenGroovebox && p.song[0][0] == sourceChain &&
        !memcmp(&source, &p.phrases[sourcePhrase], sizeof(Phrase)), "cancel preserves shared references and phrase");
      appChord(keyEdit, keyRight);
      require(currentScreen == &screenSelectionPopup, "a later edit offers independence again");
      appChord(0, keyEdit);
      require(currentScreen == &screenGroovebox &&
        !grooveboxCellShared(&p, 0, 0, 0), "confirmed independence unlocks selected track");
      const int newPhrase = grooveboxPhraseAt(&p, 0, 0, 0);
      appChord(keyOpt, keyRight);
      appChord(keyOpt, keyRight);
      appChord(0, keyEdit);
      appChord(keyEdit, keyRight);
      require(p.phrases[newPhrase].rows[2].note != EMPTY_VALUE_8, "console project independent note can be edited");
      appChord(0, keyRight);
      appChord(keyEdit, keyUp);
      require(p.phrases[newPhrase].rows[2].fx[0].command != FX_ID_EMPTY, "console project independent FX can be edited");
      require(!memcmp(&source, &p.phrases[sourcePhrase], sizeof(Phrase)), "independence and edits preserve original shared phrase");
      grooveboxEditorActive = false;
    }
    auto& project = chipnomadState->project;
    projectFree(&project); projectInitAY(&project);
    getInstrumentFunctions(InstrumentType::AY1).init(&project.instruments[0]);
    chipnomadInitChips(chipnomadState, 48000, nullptr);
    chipnomadState->mixVolume = 1.0f;
    *pSongRow = 0; *pSongTrack = 0; *pChainRow = 0;
    screenGroovebox.init();
    screenSetup(&screenSong, -1); appDraw();
    grooveboxEditorActive = true;
    screenSetup(&screenSong, -1); appDraw();
    key(1, keyShift | keyRight); key(0, 0);
    require(currentScreen == &screenGroovebox, "empty Song cell enters Groovebox");
    appChord(0, keyEdit);
    int initialPhrase = project.chains[project.song[0][0]].rows[0].phrase;
    require(project.phrases[initialPhrase].rows[0].note != EMPTY_VALUE_8, "actual EDIT press inserts note");
    std::vector<float> previewAudio(1024 * 2);
    chipnomadRender(chipnomadState, previewAudio.data(), 1024);
    require(chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].mode == PlaybackMode::phraseRow,
      "note edit uses standard row preview instead of starting the song");
    require(std::any_of(previewAudio.begin(), previewAudio.end(), [](float v) { return std::abs(v) > 0.000001f; }),
      "default AY note editing produces audible preview samples");
    appChord(keyEdit, keyRight);
    appChord(0, keyRight);
    appChord(keyEdit, keyUp);
    require(project.phrases[initialPhrase].rows[0].fx[0].command != FX_ID_EMPTY,
      "actual EDIT+UP chooses FX and commits on release");
    appChord(0, keyRight);
    const int fxBefore = project.phrases[initialPhrase].rows[0].fx[0].value;
    appChord(keyEdit, keyRight);
    require(project.phrases[initialPhrase].rows[0].fx[0].value != fxBefore,
      "actual EDIT+RIGHT changes FX value");
    appChord(0, keyLeft); appChord(0, keyLeft);
    appChord(0, keyDown);
    appChord(0, keyEdit);
    appChord(keyEdit, keyRight);
    require(project.phrases[initialPhrase].rows[0].instrument == 1, "actual instrument field EDIT+RIGHT increments");
    appChord(keyEdit, keyOpt);
    require(project.phrases[initialPhrase].rows[0].instrument == EMPTY_VALUE_8, "actual instrument field clear");
    appChord(0, keyEdit);
    require(project.phrases[initialPhrase].rows[0].instrument == 1, "actual instrument tap restores remembered value");
    appChord(0, keyDown);
    appChord(keyEdit, keyLeft);
    require(project.phrases[initialPhrase].rows[0].volume == PHRASE_VOLUME_MAX - 1, "actual volume small decrease");
    appChord(keyEdit, keyDown);
    require(project.phrases[initialPhrase].rows[0].volume == PHRASE_VOLUME_MAX - 17, "actual volume big decrease");
    appChord(keyEdit, keyOpt);
    require(project.phrases[initialPhrase].rows[0].volume == EMPTY_VALUE_16, "actual volume clear");
    appChord(0, keyEdit);
    require(project.phrases[initialPhrase].rows[0].volume == PHRASE_VOLUME_MAX - 17, "actual volume restores remembered value");
    appChord(0, keyRight);
    appChord(keyEdit, keyUp);
    require(project.phrases[initialPhrase].rows[0].fx[2].command != FX_ID_EMPTY, "actual FX3 picker works");
    appChord(0, keyRight);
    appChord(keyEdit, keyRight);
    appChord(0, keyUp);
    appChord(0, keyLeft);
    appChord(keyEdit, keyUp);
    require(project.phrases[initialPhrase].rows[0].fx[1].command != FX_ID_EMPTY, "actual FX2 picker works");
    appChord(0, keyLeft); appChord(0, keyUp);
    capture("all-step-fields-edited");
    appChord(keyOpt, keyRight);
    appChord(0, keyEdit);
    require(project.phrases[initialPhrase].rows[1].note != EMPTY_VALUE_8, "actual B navigation then EDIT adds next note");
    appChord(keyOpt, keyDown);
    appChord(0, keyEdit);
    const int secondPhrase = grooveboxPhraseAt(&project, 0, 1, 0);
    require(project.phrases[secondPhrase].rows[1].note != EMPTY_VALUE_8, "actual B track navigation then EDIT adds note");
    appChord(keyOpt, keyUp); appChord(keyOpt, keyLeft);
    auto toolsChoice = [&](int index) {
      appChord(keyShift, keyUp);
      appChord(0, keyRight);
      appChord(0, keyEdit);
      require(currentScreen == &screenSelectionPopup, "Length header RIGHT then EDIT opens Tools");
      for (int i = 0; i < index; ++i) appChord(0, keyDown);
      appChord(0, keyEdit);
      require(currentScreen == &screenGroovebox, "Tools returns to lower step editor");
    };
    const PhraseRow copiedStep = project.phrases[initialPhrase].rows[0];
    toolsChoice(14);
    appChord(keyOpt, keyRight);
    appChord(keyShift, keyEdit);
    require(!memcmp(&copiedStep, &project.phrases[initialPhrase].rows[1], sizeof(PhraseRow)),
      "step clipboard preserves note/instrument/volume and all FX");
    toolsChoice(8);
    require(memcmp(&copiedStep, &project.phrases[initialPhrase].rows[1], sizeof(PhraseRow)),
      "step paste supports bounded undo");
    appChord(keyOpt, keyLeft);
    const auto savedPath = std::filesystem::path(output) / "groovebox-edited-roundtrip.cct";
    require(!projectSave(&project, savedPath.string().c_str()), "edited Groovebox saves");
    auto loadedProject = std::make_unique<Project>();
    projectInit(loadedProject.get());
    require(!projectLoad(loadedProject.get(), savedPath.string().c_str()), "edited Groovebox reloads");
    require(!memcmp(&loadedProject->phrases[initialPhrase], &project.phrases[initialPhrase], sizeof(Phrase)),
      "roundtrip preserves note/instrument/volume and all FX");
    for (int t = 0; t < project.tracksCount; ++t)
      require(grooveboxTrackLength(loadedProject.get(), 0, t) == grooveboxTrackLength(&project, 0, t),
        "roundtrip preserves referenced empty bars and track lengths");
    projectFree(loadedProject.get());
    std::filesystem::remove(savedPath);
    for (int t = 0; t < project.tracksCount; ++t) {
      require(project.song[0][t] == (t + 1) * 0x10, "empty scene prepared with one-based track banks");
      require(grooveboxTrackLength(&project, 0, t) == appSettings.defaultPatternLength,
        "empty scene uses configured default bar length");
    }
    capture("new-empty-scene");
    key(1, keyShift | keyUp); key(0, 0);
    capture("length-header");
    key(1, keyEdit | keyRight); key(0, 0);
    require(grooveboxTrackLength(&project, 0, 0) == appSettings.defaultPatternLength + 1,
      "Length header adds a bar to selected track");
    require(grooveboxTrackLength(&project, 0, 1) == appSettings.defaultPatternLength,
      "Length header preserves other track lengths");
    key(1, keyEdit | keyLeft); key(0, 0);
    key(1, keyShift | keyDown); key(0, 0);
    key(1, keyEdit); key(0, 0);
    require(projectModified && project.song[0][0] < PROJECT_MAX_CHAINS, "editing creates canonical chain");
    toolsChoice(2);
    require(currentScreen == &screenGroovebox && *pChainRow == 1,
      "OFF tool focuses the appended bar");
    const int chain = project.song[0][0];
    require(grooveboxTrackLength(&project,0,0) == 2 &&
      project.phrases[project.chains[chain].rows[1].phrase].rows[0].note == NOTE_OFF,
      "OFF tool creates the declared canonical event");
    toolsChoice(8);
    require(grooveboxTrackLength(&project,0,0) == 1, "Tools undo restores the original length");
    key(1, keyShift | keyRight); key(0, 0);
    require(currentScreen == &screenInstrument && grooveboxEditorActive, "SHIFT+RIGHT opens instrument without changing mode");
    key(1, keyShift | keyLeft); key(0, 0);
    require(currentScreen == &screenGroovebox && grooveboxEditorActive, "instrument returns to Groovebox not Phrase");
    std::vector<float> transportAudio(1024 * 2);
    auto startButton = [&] {
      MainLoopEventData event = {};
      event.data.input = {InputDeviceType::logical, keyPlay};
      event.type = MainLoopEvent::keyDown;
      appOnEvent(event);
      chipnomadRender(chipnomadState, transportAudio.data(), 1024);
      appDraw();
      event.type = MainLoopEvent::keyUp;
      appOnEvent(event);
    };
    require(chipnomadQueueProjectRefresh(chipnomadState), "publish transport fixture");
    chipnomadRender(chipnomadState, transportAudio.data(), 1024);
    chipnomadQueuePlaybackStop(chipnomadState);
    chipnomadRender(chipnomadState, transportAudio.data(), 1024);
    require(!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "transport test begins stopped after previews");
    appResetInputState();
    startButton();
    require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "Groovebox Start starts standard playback");
    startButton();
    require(!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "Groovebox second Start stops rather than queueing");
    key(1, keyShift | keyLeft); key(0, 0);
    startButton();
    require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "Scenes Start starts playback");
    startButton();
    require(!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "Scenes Start stops playback");
    require(currentScreen == &screenSong, "return to CLIPS before transport check");
    startButton();
    require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "CLIPS Start starts playback");
    startButton();
    require(!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "CLIPS Start stops playback");
    key(1, keyShift | keyLeft); key(0, 0);
    require(currentScreen == &screenMixer, "navigate to Mixer during transport check");
    startButton();
    require(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "Mixer Start starts playback");
    startButton();
    require(!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying, "Mixer Start stops playback");
    key(1, keyShift | keyRight); key(0, 0);
    require(currentScreen == &screenSong && grooveboxEditorActive, "Mixer returns to CLIPS without changing mode");
    *pSongRow = 3; *pSongTrack = 0;
    screenSetup(&screenSong, -1); appDraw();
    tapEdit();
    require(project.song[3][0] == 0x11, "CLIPS creates next track-one chain in the 10 bank");
    key(1, keyRight); key(0, 0); tapEdit();
    require(project.song[3][1] == 0x21, "CLIPS creates next track-two chain in the 20 bank");
    key(1, keyShift | keyRight); key(0, 0);
    tapEdit();
    require(project.chains[0x21].rows[0].phrase == 0x81, "grid retains phrase bank for track two");
    appChord(keyShift, keyUp); appChord(0, keyRight); appChord(0, keyEdit);
    for (int i = 0; i < 11; ++i) appChord(0, keyDown);
    appChord(0, keyEdit);
    require(currentScreen == &screenViewSettings && grooveboxEditorActive, "Tools opens explicit View choice");
    key(1, keyEdit | keyRight); key(0, 0);
    require(canonicalSimplifiedActive && !grooveboxEditorActive && project.viewMode == ViewMode::classic,
      "View switches from Groovebox to canonical Simplified without conversion");
    key(1, keyEdit | keyRight); key(0, 0);
    require(!canonicalSimplifiedActive && project.viewMode == ViewMode::classic, "View switches back to Classic");
    screenSetup(&screenSong, -1); appDraw();
    require(currentScreen == &screenSong, "Classic navigation restored after explicit View change");
    printf("Three Groovebox project captures and navigation/allocation integration passed\n");
    chipnomadDestroy(chipnomadState); gfxCleanup(); SDL_Quit(); return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--capture-homepage")) {
    captureHomepage();
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
  if(argc==3 && !strcmp(argv[2],"--layer-only")) {
    auto& project=chipnomadState->project;
    getInstrumentFunctions(InstrumentType::Layer).init(&project.instruments[0]);
    getInstrumentFunctions(InstrumentType::Sintered).init(&project.instruments[1]);
    getInstrumentFunctions(InstrumentType::AChChid).init(&project.instruments[2]);
    project.instruments[0].chip.layer.entries[0]={1,96,112};
    project.instruments[0].chip.layer.entries[1]={2,255,128};
    chipnomadInitChips(chipnomadState,48000,nullptr);
    screenSetup(&screenInstrument,0);appDraw();capture("instrument-31");
    std::vector<float> audio(2048);
    chipnomadQueuePlaybackPreviewNote(chipnomadState,0,45,0);
    chipnomadRender(chipnomadState,audio.data(),1024);chipnomadGetPlaybackStatus(chipnomadState);appDraw();
    project.instruments[0].chip.layer.entries[0].transpose=0xff;
    currentScreen->fullRedraw();appDraw();
    std::vector<uint32_t> before(640*480),after(before.size());
    require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,before.data(),640*4)==0,"read Layer pixels");
    require(chipnomadQueueProjectRefresh(chipnomadState),"refresh Layer project");
    chipnomadRender(chipnomadState,audio.data(),1024);chipnomadGetPlaybackStatus(chipnomadState);appDraw();
    require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,after.data(),640*4)==0,"read refreshed Layer pixels");
    require(before!=after,"Layer live bar refreshes without moving the cursor");
    capture("instrument-31-refreshed");
    chipnomadDestroy(chipnomadState);SDL_Quit();return 0;
  }
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
  if(argc==3 && !strcmp(argv[2],"--animated-fx-only")) {
    appSettings.backgroundMode=BackgroundMode::plasma;
    appSettings.backgroundIntensity=0; // Keep pixels stable while exercising animation ticks.
    backgroundApplySettings();
    *pSongTrack=0;*pSongRow=0;*pChainRow=0;
    chipnomadState->project.song[0][0]=0;
    chipnomadState->project.chains[0].rows[0].phrase=0;
    chipnomadState->project.phrases[0].rows[0].fx[0].command=fxARP;
    chipnomadState->project.tables[0].rows[0].fx[0].command=fxARP;
    for (const AppScreen* page : {&screenPhrase,&screenTable}) {
      for(int header : {0,1}) {
        appSettings.persistentWaveform=header;
        page->init();screenSetup(page,0);appDraw();
        for(int column=0;column<3;++column)key(1,keyRight);
        key(1,keyEdit|keyUp);
        key(1,keyEdit|keyRight); // Preserve the browsed choice, not the stored command.
        const auto before=framePixels();
        SDL_Delay(100);appDraw();
        const auto after=framePixels();
        for(int y=(1+gfxGetContentRowOffset())*gfxGetCharHeight();
            y<(18+gfxGetContentRowOffset())*gfxGetCharHeight();++y)
          for(int x=0;x<34*gfxGetCharWidth();++x)
            require(before[y*640+x]==after[y*640+x],"animated background preserves FX picker and selection");
        appSettings.backgroundMode=BackgroundMode::solid;
        const uint32_t refreshedBackground=0xff123456;
        gfxBackgroundSetPixels(&refreshedBackground,1,1);
        currentScreen->fullRedraw();appDraw();
        const auto refreshed=framePixels();
        require(refreshed.front()==refreshedBackground && refreshed.back()==refreshedBackground,
          "FX picker refreshes the background outside its list, including the header");
        appSettings.backgroundMode=BackgroundMode::plasma;backgroundApplySettings();
        key(0,0);
      }
    }
    screenSetup(&screenMixer,0);appDraw();
    const auto cpuBefore=framePixels();
    for(int frame=0;frame<3;++frame) {
      SDL_Delay(100);appDraw();
      const auto cpuAfter=framePixels();
      for(int y=gfxGetContentRowOffset()*gfxGetCharHeight();
          y<(1+gfxGetContentRowOffset())*gfxGetCharHeight();++y)
        for(int x=31*gfxGetCharWidth();x<40*gfxGetCharWidth();++x)
          require(cpuBefore[y*640+x]==cpuAfter[y*640+x],"animated background preserves Mixer CPU text");
    }
    printf("Animated FX picker and Mixer CPU redraw passed: Phrase/Table, waveform off/on\n");
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
  for(auto type:{InstrumentType::SID,InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7,InstrumentType::Layer}){
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
