#include "screen_instrument.h"
#include "field_style.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "misc.h"

static int getColumnCount(int row) {
  // The first 3 rows come from the common instrument fields
  if (row < 3) return instrumentCommonColumnCount(row);

  // Row 3: Tone only
  if (row == 3) {
    return 1;
  }

  // Rows 4-5: Noise/Env in col 0, Auto Envelope in col 1-2
  if (row == 4) {
    return 2;
  }
  if (row == 5) {
    int isAutoEnvEnabled = chipnomadState->project.instruments[cInstrument].chip.ay.autoEnvN != 0;
    return isAutoEnvEnabled ? 3 : 1;
  }

  // Volume envelope parameters (rows 6-9) are in column 0 only
  if (row >= 6 && row <= 9) {
    return 1;
  }

  return 1;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();

  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 6, "Tone");
  gfxPrint(0, 7, "Noise");
  gfxPrint(0, 8, "Env");

  gfxPrint(kColumnNameX[1], 7, "Enabled");
  gfxPrint(kColumnNameX[1], 8, "Rate");
  gfxPrint(0, 11, "Atk");
  gfxPrint(0, 12, "Dec");
  gfxPrint(0, 13, "Sus");
  gfxPrint(0, 14, "Rel");

  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(kColumnNameX[1], 6, "AUTO ENV");
  gfxPrint(0, 10, "VOLUME");
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);

  if (col == 0) {
    if (row == 3) {
      gfxCursor(8, 6, 3);
    } else if (row == 4) {
      gfxCursor(8, 7, 3);
    } else if (row == 5) {
      gfxCursor(8, 8, 1);
    } else if (row >= 6 && row <= 9) {
      gfxCursor(8, row + 5, kFieldNumberWidth);  // the volume envelope and its bars
    }
  } else if (col == 1) {
    if (row == 4) {
      gfxCursor(kColumnValueX[1], 7, 3);
    } else if (row == 5) {
      gfxCursor(kColumnValueX[1], 8, 1);
    }
  } else if (col == 2) {
    if (row == 4 || row == 5) {
      gfxCursor(kColumnValueX[1] + 2, 8, 1);
    }
  }
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);

  const InstrumentAY1& ay = chipnomadState->project.instruments[cInstrument].chip.ay;
  uint8_t defaultMixer = ay.defaultMixer;
  int hasTone = (defaultMixer & 0x01) != 0;
  int hasNoise = (defaultMixer & 0x02) != 0;
  int envShape = (defaultMixer >> 4) & 0x0F;
  int isAutoEnvEnabled = ay.autoEnvN != 0;

  // The selected field's label lights up with its value; switches are names.
  static const char* const left[] = {"Tone", "Noise", "Env", "Atk", "Dec", "Sus", "Rel"};
  if (col == 0 && row <= 9) fieldDrawLabel(0, row <= 5 ? row + 3 : row + 5, left[row - 3], state);
  else if (row == 4 && col == 1) fieldDrawLabel(kColumnNameX[1], 7, "Enabled", state);
  else if (row == 5 || (row == 4 && col == 2)) fieldDrawLabel(kColumnNameX[1], 8, "Rate", state);
  gfxSetFgColor((row <= 4 && col <= 1) ? fieldNameColor(state) : fieldValueColor(state));

  if (row == 3 && col == 0) {
    gfxPrintf(8, 6, hasTone ? "On " : "Off");
  } else if (row == 4 && col == 0) {
    gfxPrintf(8, 7, hasNoise ? "On " : "Off");
  } else if (row == 5 && col == 0) {
    gfxPrintf(8, 8, "%X %s", envShape, getEnvelopeShapeASCII(envShape));
  } else if (row >= 6 && row <= 9 && col == 0) {
    // Attack, decay and release run to FF, sustain to F.
    const uint8_t values[] = {ay.volumeEnvelope.p1, ay.volumeEnvelope.p2, ay.volumeEnvelope.p3, ay.volumeEnvelope.p4};
    const uint8_t value = values[row - 6];
    fieldDrawNumber(8, row + 5, value, state, value / (row == 8 ? 15.0f : 255.0f));
  } else if (row == 4 && col == 1) {
    gfxPrintf(kColumnValueX[1], 7, isAutoEnvEnabled ? "On " : "Off");
    if (!isAutoEnvEnabled) gfxPrint(kColumnValueX[1], 8, "   ");
  } else if (row == 5 && col == 1) {
    if (isAutoEnvEnabled) {
      gfxPrintf(kColumnValueX[1], 8, "%hhX:", ay.autoEnvN);
    }
  } else if ((row == 4 || row == 5) && col == 2) {
    if (isAutoEnvEnabled) {
      gfxPrintf(kColumnValueX[1] + 2, 8, "%hhX", ay.autoEnvD);
    }
  }
}

static int onEdit(int col, int row, enum CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);

  int handled = 0;
  uint8_t* defaultMixer = &chipnomadState->project.instruments[cInstrument].chip.ay.defaultMixer;

  if (col == 0) {
    if (row == 3) {
      uint8_t value = (*defaultMixer & 0x01) != 0;
      handled = edit8noLast(action, &value, 1, 0, 1);
      if (handled) {
        *defaultMixer = (*defaultMixer & 0xFE) | (value & 0x01);
      }
    } else if (row == 4) {
      uint8_t value = (*defaultMixer & 0x02) != 0;
      handled = edit8noLast(action, &value, 1, 0, 1);
      if (handled) {
        *defaultMixer = (*defaultMixer & 0xFD) | ((value & 0x01) << 1);
      }
    } else if (row == 5) {
      uint8_t envShape = (*defaultMixer >> 4) & 0x0F;
      handled = edit8noLast(action, &envShape, 8, 0, 15);
      if (handled) {
        *defaultMixer = (*defaultMixer & 0x0F) | (envShape << 4);
      }
    } else if (row == 6) {
      handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].chip.ay.volumeEnvelope.p1, 16, 0, 255);  // A
    } else if (row == 7) {
      handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].chip.ay.volumeEnvelope.p2, 16, 0, 255);  // D
    } else if (row == 8) {
      handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].chip.ay.volumeEnvelope.p3, 1, 0, 15);   // S
    } else if (row == 9) {
      handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].chip.ay.volumeEnvelope.p4, 16, 0, 255);  // R
    }
  } else if (col == 1) {
    if (row == 4) {
      uint8_t value = chipnomadState->project.instruments[cInstrument].chip.ay.autoEnvN != 0;
      uint8_t oldValue = value;
      handled = edit8noLast(action, &value, 1, 0, 1);
      if (oldValue != value && value == 1) {
        chipnomadState->project.instruments[cInstrument].chip.ay.autoEnvN = 1;
        chipnomadState->project.instruments[cInstrument].chip.ay.autoEnvD = 1;
        drawField(1, 5, CellState::normal);
        drawField(2, 5, CellState::normal);
      } else if (oldValue != value && value == 0) {
        chipnomadState->project.instruments[cInstrument].chip.ay.autoEnvN = 0;
        chipnomadState->project.instruments[cInstrument].chip.ay.autoEnvD = 0;
      }
    } else if (row == 5) {
      handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].chip.ay.autoEnvN, 16, 1, 15);
    }
  } else if (col == 2 && (row == 4 || row == 5)) {
    handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].chip.ay.autoEnvD, 16, 1, 15);
  }

  if (handled) projectModified = 1;
  return handled;
}

ScreenData screenInstrumentAY = {
  .rows = 10,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = getColumnCount,
  .drawStatic = drawStatic,
  .drawCursor = drawCursor,
  .drawSelection = NULL,
  .drawRowHeader = NULL,
  .drawColHeader = NULL,
  .drawField = drawField,
  .onEdit = onEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};
