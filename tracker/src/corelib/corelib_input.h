#ifndef __CORELIB_INPUT_H__
#define __CORELIB_INPUT_H__

#include <stdint.h>

enum class InputDeviceType {
  none = 0,
  logical = 1,
  keyboard = 2,
  gamepad = 3,
};

struct InputCode {
  InputDeviceType deviceType;
  int32_t code;
};

enum Key {
  keyLeft = 0x1,
  keyRight = 0x2,
  keyUp = 0x4,
  keyDown = 0x8,
  keyEdit = 0x10,
  keyOpt = 0x20,
  keyPlay = 0x40,
  keyShift = 0x80,
  keyMotionLive = 0x100,
  keyMotionRecord = 0x200,
  keyMotionErase = 0x400,
  keyUnmapped = 0x800,
};

enum GamepadInputCode {
  gamepadTriggerLeft = 0x1000,
  gamepadTriggerRight,
  // Generic numbered buttons (Android KEYCODE_BUTTON_1..16): code is
  // gamepadExtraButton + n - 1, so any pad can be mapped by the user.
  gamepadExtraButton = 0x2000,
  gamepadExtraButtonCount = 16,
};

// Initialize default key mappings based on platform/keyboard layout
void inputInitDefaultKeyMapping(void);

// Convert input code to human-readable name
const char* inputGetKeyName(InputCode input);

// Key jazz (desktop): semitone offset (0-28) for a keyboard input's physical
// key position, or -1 if it isn't one of the note keys. Uses the key's
// scancode (physical position), not the character it produces, so the
// mapping is the same on AZERTY/QWERTY/QWERTZ keyboards.
int inputKeyJazzNoteOffset(InputCode input);

// Key jazz (desktop): true if this input is the on/off toggle key (Esc).
int inputIsKeyJazzToggle(InputCode input);

// Key jazz (desktop): octave shift direction for this input, -1/0/1
// (not an octave key when 0).
int inputKeyJazzOctaveDelta(InputCode input);

// Key jazz (desktop): live modifier state, for the Ctrl+C/X/V/S and
// Shift+arrow shortcuts (these need to know if the modifier is held
// alongside another key, not just whether the modifier itself was pressed).
int inputIsCtrlHeld(void);
int inputIsShiftHeld(void);

// Key jazz (desktop): true if this input IS a Shift key itself. Shift is
// mapped to the app's own logical "Select" button by default, so key jazz
// must swallow the raw key too (not just read the live modifier state via
// inputIsShiftHeld), or it leaks into the normal pressed-buttons bitmask
// and breaks the strict key-combo comparisons used for cursor movement.
int inputIsShiftKey(InputCode input);

// Key jazz (desktop): physical-key checks for the clipboard/save shortcuts
// (combine with inputIsCtrlHeld) and for Delete/Backspace/Insert. Delete and
// Backspace are deliberately distinct (Delete removes the whole row and
// shifts the rest up; Backspace steps back and clears just one column).
int inputIsCopyKey(InputCode input);
int inputIsCutKey(InputCode input);
int inputIsPasteKey(InputCode input);
int inputIsSaveKey(InputCode input);
int inputIsDeleteKey(InputCode input);
int inputIsBackspaceKey(InputCode input);
int inputIsInsertKey(InputCode input);

// Key jazz (desktop): arrow key direction as a Key bitmask value
// (keyUp/keyDown/keyLeft/keyRight), or 0 if this isn't an arrow key.
int inputArrowKeyDirection(InputCode input);

// Key jazz (desktop, Song/Chain): hex digit (0-15) for this input, or -1.
// Uses the typed character (keysym), not physical position - typing "3"
// should type 3 regardless of keyboard layout, unlike the note table.
int inputHexDigitValue(InputCode input);

// Key jazz (desktop, Project): typed character for this input (letters,
// digits, space, - _ .), or 0 if not typable. Also keysym-based; shiftHeld
// selects the uppercase letter.
char inputTypedCharacter(InputCode input, int shiftHeld);

#endif
