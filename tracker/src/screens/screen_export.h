#ifndef __SCREEN_EXPORT_H__
#define __SCREEN_EXPORT_H__

#include "screens.h"
#include "export/export.h"

// Common rows on the export screen:
// 0 start row, 1 WAV (song/stems), 2 sample rate, 3 bit depth,
// 4 folder, 5 MIDI, 6 M8S, 7 LSDJ and 8 Furnace (desktop and web)
#if defined(DESKTOP_BUILD) || defined(WEB_BUILD)
#define SCR_EXPORT_ROWS (9) // includes the M8S, LSDJ and Furnace rows
#define EXPORT_HAS_M8S
#define EXPORT_ROW_LSDJ (7)
#define EXPORT_ROW_FUR (8)
#elif defined(ANDROID_BUILD)
#define SCR_EXPORT_ROWS (8) // includes the LSDJ and Furnace rows, no M8S
#define EXPORT_ROW_LSDJ (6)
#define EXPORT_ROW_FUR (7)
#else
#define SCR_EXPORT_ROWS (6)
#endif
#ifdef EXPORT_ROW_LSDJ
#define EXPORT_HAS_LSDJ
#endif
#ifdef EXPORT_ROW_FUR
#define EXPORT_HAS_FUR
#endif

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
