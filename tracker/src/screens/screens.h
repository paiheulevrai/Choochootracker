#ifndef __SCREENS_H__
#define __SCREENS_H__
#include "screen_layout.h"

#include "common.h"
#include "../chipnomad_lib/playback.h"

#define MESSAGE_TIME (60)
// Errors are unexpected and need to be read, not just glanced at like a
// routine confirmation ("Copied selection") - give them much longer on screen.
#define MESSAGE_TIME_ERROR (300)

enum class CellState : int {
  normal = 0,
  focus = 1,
  selected = 2,
};

enum class CellEditAction : int {
  clear,
  tap,
  doubleTap,
  increase,
  decrease,
  increaseBig,
  decreaseBig,
  shallowClone,
  deepClone,
  copy,
  cut,
  paste,
  switchSelection,
  multiIncrease,
  multiDecrease,
  multiIncreaseBig,
  multiDecreaseBig
};

enum class ScreenPlaybackLevel : int {
  none,
  song,
  chain,
  phrase,
};

enum class PopupEditInput : int {
  none,
  hold,
  cycle,
  open,
};

struct AppScreen {
  void (*init)(void);
  void (*setup)(int input);
  void (*fullRedraw)(void);
  void (*draw)(void);
  int (*onInput)(int isKeyDown, int keys, int tapCount); // Return 1 if handled, 0 if not
  ScreenPlaybackLevel (*getPlaybackLevel)(void); // Return playback level for this screen
};

struct ScreenData {
  //int cols;
  int rows;
  int cursorRow;
  int cursorCol;
  int topRow; // For scrollable screens
  int selectMode; // 0 - edit, 1 - select, -1 - select is disabled for this screen (e.g. Instrument screen)
  int selectStartRow;
  int selectStartCol;
  int selectAnchorRow; // Original cell where selection mode was entered
  int selectAnchorCol; // Original cell where selection mode was entered
  ScreenPlaybackLevel playbackLevel; // Playback level for this screen (None, Song, Chain, Phrase)
  int (*getColumnCount)(int row);
  void (*drawStatic)(void);
  void (*drawCursor)(int col, int row);
  void (*drawSelection)(int col1, int row1, int col2, int row2);
  void (*drawRowHeader)(int row, CellState state);
  void (*drawColHeader)(int col, CellState state);
  void (*drawField)(int col, int row, CellState state);
  int (*onEdit)(int col, int row, CellEditAction action);
  int (*onInput)(int isKeyDown, int keys, int tapCount);  // Optional: handle input before standard processing (return 1 if handled completely, 0 to continue)
  int (*onRawInput)(int keyCode, int isKeyboard, int isDown);  // Optional: capture raw SDL input
  int (*isCellValid)(int col, int row);  // Optional: return 0 for dead cells that cursor should skip
  LoopRange (*getLoopRange)(void);  // Optional: return loop range for ranged playback
};

extern const AppScreen screenProject;
extern const AppScreen screenProjectLoad;
extern const AppScreen screenProjectSave;
extern const AppScreen screenConfirm;
extern const AppScreen screenSaveChoice;
extern const AppScreen screenPitchTable;
extern const AppScreen screenScale;
extern const AppScreen screenFileBrowser;
extern const AppScreen screenCreateFolder;
extern const AppScreen screenEnterName;
extern const AppScreen screenSong;
extern const AppScreen screenChain;
extern const AppScreen screenPhrase;
extern const AppScreen screenGroove;
extern const AppScreen screenInstrument;
extern const AppScreen screenSampleSettings;
extern const AppScreen screenInstrumentPool;
extern const AppScreen screenModulation;
extern const AppScreen screenInsertFX;
extern const AppScreen screenTable;
extern const AppScreen screenAYWavetable;
extern const AppScreen screenExport;
extern const AppScreen screenBounce;
extern const AppScreen screenManage;
extern const AppScreen screenSettings;
extern const AppScreen screenTrackVisuals;
extern const AppScreen screenSynthSettings;
extern const AppScreen screenMixerSettings;
extern const AppScreen screenGraphicsSettings;
extern const AppScreen screenMixer;
extern const AppScreen screenSelectionPopup;
int screenMixerGetPage(void);
extern const AppScreen screenColorTheme;
extern const AppScreen screenKeyMapping;
extern const AppScreen screenMidi;
extern const AppScreen screenMidiChannelMap;
extern const AppScreen screenMidiCC;
extern const AppScreen screenQuickHelp;
extern const AppScreen screenTitle;

extern const AppScreen* currentScreen;

void screenSetup(const AppScreen* screen, int input);
void screenDraw(void);
void screenMessage(int time, const char* format, ...);
// Current message text set by screenMessage(), or "" if none is active.
// screenTitle draws through its own gfxTitle* calls (see screenDraw()), so it
// needs this to show a message instead of the normal gfxPrint-based banner.
const char* screenGetActiveMessage(void);
// Clear the message bar immediately (timed or not) and stop its timer.
void screenClearMessage(void);
void screensInitAll(void);
void drawScreenMap(void);
enum ScreenPlaybackLevel screenGetPlaybackLevel(const AppScreen* screen);

// Spreadsheet functions
void screenFullRedraw(ScreenData* screen);
void screenDrawOverlays(ScreenData* screen);
int screenInput(ScreenData* screen, int isKeyDown, int keys, int tapCount);
void screenClearOptPressed(void);
int screenTouchTap(int col, int row);
enum TouchAdjustResult { touchAdjustNone, touchAdjustCoarse, touchAdjustFine };
TouchAdjustResult screenTouchAdjust(int col, int row);

// Utility functions
void setCellColor(CellState state, int isEmpty, int hasContent);
void getSelectionBounds(ScreenData* screen, int* startCol, int* startRow, int* endCol, int* endRow);
int isSingleColumnSelection(ScreenData* screen);
LoopRange screenGetLoopRange(const AppScreen* screen);

// Confirmation dialog
void confirmSetup(const char* message, void (*confirmCallback)(void), void (*cancelCallback)(void));

// Save-destination dialog (sample WAV cues vs project bounds)
void saveChoiceSetup(const char* sampleName, void (*onSample)(void),
                     void (*onProject)(void), void (*onCancel)(void));

// Common edit functions
int edit16withLimit(CellEditAction action, uint16_t* value, uint16_t* lastValue, uint16_t bigStep, uint16_t max);
int edit8withLimit(CellEditAction action, uint8_t* value, uint8_t* lastValue, uint8_t bigStep, uint8_t max);
int edit8noLimit(CellEditAction action, uint8_t* value, uint8_t* lastValue, uint8_t bigStep);
int edit8noLast(CellEditAction action, uint8_t* value, uint8_t bigStep, uint8_t min, uint8_t max);
int editNormalized8(CellEditAction action, uint8_t* value, uint8_t nativeMax);
int editNormalized16(CellEditAction action, uint16_t* value, uint16_t nativeMax);
void cycle8(uint8_t* value, int direction, uint8_t min, uint8_t max, int wrap);
int editSigned16(CellEditAction action, int16_t* value, int16_t bigStep, int16_t min, int16_t max);
int editSigned8(CellEditAction action, int8_t* value, int8_t bigStep, int8_t min, int8_t max);
int edit16withMinMax(CellEditAction action, uint16_t* value, uint16_t bigStep, uint16_t min, uint16_t max);
int edit16withOverflow(CellEditAction action, uint16_t* value, uint16_t bigStep, uint16_t min, uint16_t max);
int editOscillatorParameter(CellEditAction action, uint16_t* value);
int editFilterCutoff(CellEditAction action, uint16_t* cutoffHz);
int applyMultiEdit(int startCol, int startRow, int endCol, int endRow, CellEditAction action, int (*editFunc)(int col, int row, CellEditAction action));
int applyPhraseRotation(int phraseIdx, int startRow, int endRow, int direction);
int applyTableRotation(int tableIdx, int startRow, int endRow, int direction);
int applySongMoveDown(int startCol, int startRow, int endCol, int endRow);
int applySongMoveUp(int startCol, int startRow, int endCol, int endRow);
CellEditAction convertMultiAction(CellEditAction action);
PopupEditInput popupEditInput(int isKeyDown, int keys, int* buttonDown);

// Character edit
int editCharacter(CellEditAction action, char* str, int idx, int maxLen);
char charEditInput(int keys, int tapCount, char* str, int idx, int maxLen);

// FX edit
int editFX(CellEditAction action, uint8_t* fx, uint8_t* lastFX, int isTable, uint8_t instrumentIdx);
void selectInstrumentFX(uint8_t* fx, uint8_t selected, uint8_t instrumentIdx);
int editFXValue(CellEditAction action, uint8_t* fx, uint8_t* lastFX, int isTable, uint8_t instrumentIdx);
int fxEditInput(int keys, int tapCount, uint8_t* fx, uint8_t* lastFX);
void fxEditFullDraw(uint8_t currentFX, uint8_t instrumentIdx, int isTable);

// Key jazz (desktop): lets screen_phrase.cpp own its toggle/note-entry state
// while app.cpp only needs to route raw keyboard events to it. Returns 1 if
// the key was consumed by key jazz, 0 to let normal input processing continue.
#ifdef DESKTOP_BUILD
int phraseKeyJazzHandleRawKey(InputCode input, int isDown);
int songKeyJazzHandleRawKey(InputCode input, int isDown);
int chainKeyJazzHandleRawKey(InputCode input, int isDown);
#endif

// Key jazz text entry (desktop): type directly into any text field (project
// filename/title/author, instrument name, theme name, pitch table name,
// bounce name, enter-name and create-folder dialogs) instead of using the
// on-screen character popup. Esc toggles it; the state is shared by all
// these screens. Each screen with a text field exposes a getter describing
// the field under its cursor.
#ifdef DESKTOP_BUILD
typedef struct {
  ScreenData* screen;       // Screen owning the field (cursorCol is the caret)
  int row;                  // Row of the text field
  char* str;                // NULL when the cursor is not on a text field
  int maxLen;
  int popupOpen;            // The character popup is open: leave input alone
  int marksProjectModified; // Editing the field dirties the project
} KeyJazzTextField;

int keyJazzTextHandleRawKey(InputCode input, int isDown, const AppScreen* current);
int projectKeyJazzTextField(KeyJazzTextField* field);
int instrumentKeyJazzTextField(KeyJazzTextField* field);
int colorThemeKeyJazzTextField(KeyJazzTextField* field);
int enterNameKeyJazzTextField(KeyJazzTextField* field);
int createFolderKeyJazzTextField(KeyJazzTextField* field);
int pitchTableKeyJazzTextField(KeyJazzTextField* field);
int bounceKeyJazzTextField(KeyJazzTextField* field);
#endif

// Manage screen functions
// TODO: Remove this
int manageColumnCount(int row);
void manageDrawStatic(void);
void manageDrawCursor(int col, int row);
void manageDrawField(int col, int row, CellState state);
int manageOnEdit(int col, int row, CellEditAction action);

#endif
