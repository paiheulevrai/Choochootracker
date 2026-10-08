#ifndef __SCREEN_INSTRUMENT_H__
#define __SCREEN_INSTRUMENT_H__

#include "screens.h"

extern int cInstrument;

int instrumentCommonColumnCount(int row);
void instrumentCommonDrawStatic(void);
void instrumentCommonDrawCursor(int col, int row);
void instrumentCommonDrawField(int col, int row, CellState state);
void instrumentCommonDrawEnvelopePreview(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release, uint8_t shape);
void instrumentCommonDrawLivePreview(void);
int instrumentCommonOnEdit(int col, int row, CellEditAction action);
void instrumentCommonDrawVoicePostStatic(int drawEnvelope);
int instrumentCommonDrawVoicePostCursor(int col, int row);
int instrumentCommonDrawVoicePostField(int col, int row, CellState state, const InstrumentVoicePostSettings* post);
int instrumentCommonOnEditVoicePost(int col, int row, CellEditAction action, InstrumentVoicePostSettings* post);
void instrumentFMAmpDrawStatic();
void instrumentFMRefreshStaticWaveform();
void instrumentFMAmpDrawCursor(int col, int row);
void instrumentFMAmpDrawField(int col, int row, CellState state);
int instrumentFMAmpEdit(int col, int row, CellEditAction action);
void instrumentFMToneDrawCursor(int col);
void instrumentFMSetContext(int instrument, InstrumentType type);
void instrumentFMToneDrawField(int col, CellState state);
int instrumentFMToneEdit(int col, CellEditAction action);

extern ScreenData screenInstrumentAY;
extern ScreenData screenInstrumentAY2;
extern ScreenData screenInstrumentAYSample;
extern ScreenData screenInstrumentBraids;
extern ScreenData screenInstrumentSample;
extern ScreenData screenInstrumentSCWF;
extern ScreenData screenInstrumentBYOWTBL;
extern ScreenData screenInstrumentPlaits;
extern ScreenData screenInstrumentAChChid;
extern ScreenData screenInstrumentDrumSynth;
extern ScreenData screenInstrumentMME;
extern ScreenData screenInstrumentSintered;
extern ScreenData screenInstrumentMidi;
extern ScreenData screenInstrumentOPLL;
extern ScreenData screenInstrumentOPL;
bool fmEditSupported(InstrumentType type);
extern ScreenData screenInstrumentSimpleChip;

#endif

void instrumentFMImportSysEx(const char* path);

const char* instrumentPresetCollectionName();
void instrumentPresetOpenCollections();
void instrumentPresetCycleCollection(int direction);
void instrumentPresetOpenSounds();
void instrumentPresetCycle(int direction);
