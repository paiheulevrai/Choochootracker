// How settings pages draw a field: its label, its value and, for values with
// a range, a bar. The selected field's label lights up with its value, and
// its bar shows a faint track across its full width.
#ifndef __FIELD_STYLE_H__
#define __FIELD_STYLE_H__

#include "screens.h"

// Sound pages lay their controls out in two columns of 17 characters: a
// name of up to seven letters and a space, a two-digit value, and a bar of
// six. The left column's values line up under Type, Name and Transp.
constexpr int kColumnNameX[2] = {0, 17}, kColumnValueX[2] = {8, 25}, kColumnBarX[2] = {10, 27};
constexpr int kFieldBarWidth = 6, kFieldNumberWidth = 2 + kFieldBarWidth;

// The label in the label colour, or lit like the value when selected.
void fieldDrawLabel(int x, int y, const char* label, CellState state);
// A text field of width characters, or dashes showing the empty field.
void fieldDrawText(int x, int y, const char* text, int width, CellState state);
// Colours for a number and for a named value (Off, Classic, LFO...).
int fieldValueColor(CellState state);
int fieldNameColor(CellState state);
// A two-digit value and its bar at x, y. position defaults to value / 255.
void fieldDrawNumber(int x, int y, uint8_t value, CellState state, float position = -1.0f, bool bipolar = false);
// A bar of width characters at x, y. position runs from 0 to 1; a bipolar
// bar fills from its middle, for values such as amounts and pan.
void fieldDrawBar(int x, int y, int width, float position, CellState state, bool bipolar = false);

#endif
