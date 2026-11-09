#ifndef SCREEN_GROOVEBOX_H
#define SCREEN_GROOVEBOX_H

#include "screens.h"

extern bool grooveboxEditorActive;
extern bool canonicalSimplifiedActive;
extern const AppScreen screenGroovebox;
extern const AppScreen screenGrooveboxAfterConversion;

int grooveboxPhraseAt(const Project* project, int songRow, int track, int bar);
bool grooveboxCellShared(const Project* project, int songRow, int track, int bar);
int grooveboxTrackLength(const Project* project, int songRow, int track);
int grooveboxFindFreeChain(Project* project, int track);
bool grooveboxMergePatternsUp(int firstRow, int lastRow, int firstCol, int lastCol);
bool grooveboxPrepareScene(int songRow);
enum class GrooveboxAction { makeIndependent, emptyBar, offBar, duplicateBar, doubleLength, shorten, copyScene, pasteScene, undo, halveLength, resetLength };
bool grooveboxAction(GrooveboxAction action, int songRow, int track);

#endif
