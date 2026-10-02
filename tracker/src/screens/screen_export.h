#ifndef __SCREEN_EXPORT_H__
#define __SCREEN_EXPORT_H__

#include "screens.h"
#include "export/export.h"

// Common rows on the export screen:
// 0 start row, 1 WAV (song/stems), 2 sample rate, 3 bit depth,
// 4 folder, 5 MIDI
#define SCR_EXPORT_ROWS (6)

// Rows on the bounce-to-sample screen:
// 0 file name, 1 sample rate, 2 bit depth, 3-5 name prefix checkboxes,
// 6 start/cancel
#define SCR_BOUNCE_ROWS (7)

// Export state
extern Exporter* currentExporter;
extern int startRow;

int exportCommonColumnCount(int row);
void exportCommonDrawStatic(void);
void exportCommonDrawCursor(int col, int row);
void exportCommonDrawField(int col, int row, CellState state);
int exportCommonOnEdit(int col, int row, CellEditAction action);
void generateExportPath(char* outputPath, int maxLen, const char* extension);

// Bounce selection to audio (triggered from song/chain/phrase selection mode)
void exportBounceBegin(const ExportSelection& selection);

// Export folder picker (row on the export screen)
void exportFolderPickerBegin(void);

#endif
