#ifndef __SCREEN_INSTRUMENT_H__
#define __SCREEN_INSTRUMENT_H__

#include "screens.h"

extern int cInstrument;

int instrumentCommonColumnCount(int row);
void instrumentCommonDrawStatic(void);
void instrumentCommonDrawCursor(int col, int row);
void instrumentCommonDrawField(int col, int row, CellState state);
void instrumentCommonDrawEnvelopePreview(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release, uint8_t shape);
int instrumentCommonOnEdit(int col, int row, CellEditAction action);
void instrumentCommonDrawVoicePostStatic(int drawEnvelope);
int instrumentCommonDrawVoicePostCursor(int col, int row);
int instrumentCommonDrawVoicePostField(int col, int row, CellState state, const InstrumentVoicePostSettings* post);
int instrumentCommonOnEditVoicePost(int col, int row, CellEditAction action, InstrumentVoicePostSettings* post);

extern ScreenData screenInstrumentAY;
extern ScreenData screenInstrumentAY2;
extern ScreenData screenInstrumentAYSample;
extern ScreenData screenInstrumentBraids;
extern ScreenData screenInstrumentSample;
extern ScreenData screenInstrumentPlaits;

#endif
