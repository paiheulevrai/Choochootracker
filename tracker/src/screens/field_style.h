// How settings pages draw a field: its label, its value and, for values with
// a range, a bar. The selected field's label lights up with its value, and
// its bar shows a faint track across its full width.
#ifndef __FIELD_STYLE_H__
#define __FIELD_STYLE_H__

#include "screens.h"

// Characters a value bar takes after its value.
constexpr int kFieldBarWidth = 6;

// The label in the label colour, or lit like the value when selected.
void fieldDrawLabel(int x, int y, const char* label, CellState state);
// A text field of width characters, or dashes showing the empty field.
void fieldDrawText(int x, int y, const char* text, int width, CellState state);
// Colours for a number and for a named value (Off, Classic, LFO...).
int fieldValueColor(CellState state);
int fieldNameColor(CellState state);
// A bar of width characters at x, y. position runs from 0 to 1; a bipolar
// bar fills from its middle, for values such as amounts and pan.
void fieldDrawBar(int x, int y, int width, float position, CellState state, bool bipolar = false);

#endif
