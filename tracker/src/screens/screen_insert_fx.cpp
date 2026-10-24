#include "chipnomad_lib.h"
#include "corelib_gfx.h"
#include "screens.h"
#include "selection_popup.h"
#include "utils.h"
#include "audio_manager.h"
#include <stdio.h>

static void fullRedraw();
static int onInput(int, int, int);
static int moduleButtonDown;
static int popupSlot;
static int displayedCpuLoad = -1;
static bool master;
static int columns(int) { return 2; }
static int y(int row) { return 3 + row + (row >= 5 ? 2 : 0); }
static int parameter(int col, int row) { return col * 4 + row % 5 - 1; }
static InsertConfig& config(int row) {
  return master ? chipnomadState->project.masterInserts[row / 5] :
                  chipnomadState->project.trackInserts[*pSongTrack][row / 5];
}
static int valid(int col, int row) {
  return row >= 0 && row < 10 && col >= 0 && col < 2 &&
         (row % 5 == 0 || insertParameterActive(config(row).module, parameter(col, row), config(row).values[0]));
}
static void drawCpuLoad(int cpuLoad) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxClearRect(31, 0, 9, 1);
  gfxPrintf(31, 0, "CPU %03d%%", cpuLoad);
}
static void drawStatic() {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  if (master) gfxPrint(0, 0, "MASTER FX");
  else gfxPrintf(0, 0, "TRACK %d - INSERT FX", *pSongTrack + 1);
  drawCpuLoad(audioManager.getCpuLoadPercent());
}
static void hint(int col, int row) {
  auto& c = config(row);
  int p = parameter(col, row);
  if (row % 5) {
    char decoded[32];
    insertDescribe(decoded, sizeof(decoded), c.module, p, c.values[p]);
    screenMessage(0, "F%d%d %s: %s", row / 5 + 1, p + 1,
                  insertDescriptor(c.module).parameters[p].name, decoded);
  } else
    screenMessage(0, "TF%d %s", row / 5 + 1,
                  col ? "Bypass retains settings" : "Select insert module");
}
static void cursor(int col, int row) {
  gfxCursor(row % 5 == 0 ? (col ? 27 : 4) : col * 17 + 12, y(row),
            row % 5 == 0 ? (col ? 3 : 11) : 2);
  hint(col, row);
}
static void field(int col, int row, CellState) {
  auto& c = config(row);
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  int x = col * 17;
  gfxClearRect(x, y(row), 17, 1);
  if (row % 5 == 0) {
    if (!col)
      gfxPrintf(0, y(row), "TF%d %-11s", row / 5 + 1, insertDescriptor(c.module).name);
    else
      gfxPrintf(17, y(row), "Bypass    %s", c.bypass ? "ON " : "OFF");
  } else if (valid(col, row)) {
    int p = parameter(col, row);
    char label[16];
    snprintf(label, sizeof(label), "%s", insertDescriptor(c.module).parameters[p].label);
    if (c.module == insertFormant && p == 0)
      snprintf(label, sizeof(label), "%s", c.values[0] ? "CUSTOM" : "MODE VOW");
    if (c.module == insertFormant && p == 1) {
      char decoded[32];
      insertDescribe(decoded, sizeof(decoded), c.module, p, c.values[p]);
      snprintf(label, sizeof(label), "VOW %.3s", decoded);
    }
    gfxPrintf(x, y(row), "%d %-8s", p + 1, label);
    gfxSetFgColor(appSettings.colorScheme.textValue);
    gfxPrintf(x + 12, y(row), "%02X", c.values[p]);
  }
}
static int edit(int col, int row, CellEditAction action) {
  if (!valid(col, row)) return 0;
  auto& c = config(row);
  int handled = 0;
  if (row % 5 == 0) {
    uint8_t v = col ? c.bypass : c.module;
    handled = edit8noLast(action, &v, 1, 0, col ? 1 : insertModuleCount - 1);
    if (handled) {
      if (col)
        c.bypass = v;
      else if (c.module != v)
        insertSelect(&c, v);
    }
  } else {
    int p = parameter(col, row);
    uint8_t v = c.values[p];
    const auto& a = insertDescriptor(c.module).parameters[p];
    // Shape/Freeze is a packed byte, discrete for DSP slew rather than an enum.
    const bool packedShapeFreeze = c.module == insertBeads && p == 6;
    handled = edit8noLast(action, &v,
                          a.mapping == InsertMapping::discrete && !packedShapeFreeze ? 1 : 16, 0,
                          insertParameterMaximum(c.module, p));
    if (handled) insertEdit(&c, p, v);
  }
  if (handled) {
    projectModified = 1;
    fullRedraw();
  }
  return handled;
}
static void noHeader(int, CellState) {}
static ScreenData data = {.rows = 10,
                          .cursorRow = 0,
                          .cursorCol = 0,
                          .topRow = 0,
                          .selectMode = -1,
                          .selectStartRow = 0,
                          .selectStartCol = 0,
                          .selectAnchorRow = 0,
                          .selectAnchorCol = 0,
                          .playbackLevel = ScreenPlaybackLevel::phrase,
                          .getColumnCount = columns,
                          .drawStatic = drawStatic,
                          .drawCursor = cursor,
                          .drawSelection = nullptr,
                          .drawRowHeader = noHeader,
                          .drawColHeader = noHeader,
                          .drawField = field,
                          .onEdit = edit,
                          .onInput = onInput,
                          .onRawInput = nullptr,
                          .isCellValid = valid,
                          .getLoopRange = nullptr};
static void fullRedraw() {
  if (!valid(data.cursorCol, data.cursorRow)) {
    data.cursorRow = (data.cursorRow / 5) * 5;
    data.cursorCol = 0;
  }
  screenFullRedraw(&data);
}
static void selected(int module) {
  insertSelect(&config(popupSlot * 5), module);
  projectModified = 1;
  screenSetup(&screenInsertFX, -1);
}
static void cancelled() { screenSetup(&screenInsertFX, -1); }

static const SelectionItem mix[] = {
  {"OTT", insertOTT, nullptr, 0, nullptr},
  {"Compressor", insertCompressor, nullptr, 0, nullptr},
  {"EQ", insertParametricEQ, nullptr, 0, nullptr},
};
static const SelectionItem character[] = {
  {"Distortion", insertDistortion, nullptr, 0, nullptr},
  {"Saturation", insertSaturation, nullptr, 0, nullptr},
  {"Bitcrusher", insertBitcrusher, nullptr, 0, nullptr},
  {"Destruction", insertDestruction, nullptr, 0, nullptr},
  {"TAPESCAM", insertTape, nullptr, 0, nullptr},
};
static const SelectionItem modulation[] = {
  {"Chorus", insertChorus, nullptr, 0, nullptr},
  {"Phaser", insertPhaser, nullptr, 0, nullptr},
  {"Flanger", insertFlanger, nullptr, 0, nullptr},
  {"Trance Gate", insertTranceGate, nullptr, 0, nullptr},
};
static const SelectionItem spatial[] = {
  {"Rotary", insertRotary, nullptr, 0, nullptr},
  {"Doubler", insertDoubler, nullptr, 0, nullptr},
};
static const SelectionItem texture[] = {
  {"Beads", insertBeads, nullptr, 0, nullptr},
  {"Vowel Filter", insertFormant, nullptr, 0, nullptr},
};
static const SelectionItem insertCategories[] = {
  {"OFF", insertOff, nullptr, 0, nullptr},
  {"Mix", -1, mix, (int)(sizeof(mix) / sizeof(mix[0])), nullptr},
  {"Character", -1, character, (int)(sizeof(character) / sizeof(character[0])), nullptr},
  {"Modulation", -1, modulation, (int)(sizeof(modulation) / sizeof(modulation[0])), nullptr},
  {"Spatial", -1, spatial, (int)(sizeof(spatial) / sizeof(spatial[0])), nullptr},
  {"Texture", -1, texture, (int)(sizeof(texture) / sizeof(texture[0])), nullptr},
};

static int cycleInsertModule(int module, int direction) {
  int modules[insertModuleCount];
  int count = 0;
  for (const auto& category : insertCategories) {
    if (category.value >= 0)
      modules[count++] = category.value;
    for (int child = 0; child < category.childCount; ++child)
      modules[count++] = category.children[child].value;
  }

  for (int i = 0; i < count; ++i) {
    if (modules[i] == module)
      return modules[(i + direction + count) % count];
  }
  return modules[0];
}
static int onInput(int down, int keys, int taps) {
  if (data.cursorRow % 5 == 0 && data.cursorCol == 0) {
    auto input = popupEditInput(down, keys, &moduleButtonDown);
    if (input == PopupEditInput::hold) return 1;
    if (input == PopupEditInput::cycle) {
      auto& c = config(data.cursorRow);
      insertSelect(&c, cycleInsertModule(c.module, (keys & keyLeft) ? -1 : 1));
      projectModified = 1;
      fullRedraw();
      return 1;
    }
    if (input == PopupEditInput::open) {
      popupSlot = data.cursorRow / 5;
      screenMessage(0, "");
      char popupTitle[32];
      snprintf(popupTitle, sizeof(popupTitle), "INSERT MODULE CPU %03d%%",
               audioManager.getCpuLoadPercent());
      selectionPopupSetup(popupTitle, insertCategories,
                          (int)(sizeof(insertCategories) / sizeof(insertCategories[0])),
                          config(data.cursorRow).module,
                          selected, cancelled, true);
      screenSetup(&screenSelectionPopup, 0);
      return 1;
    }
  }
  if (down) {
    if (master && keys == keyOpt) {
      screenSetup(&screenMixer, 0);
      return 1;
    }
    if (!master && keys == (keyShift | keyDown)) {
      screenMessage(0, "");
      screenSetup(&screenModulation, -1);
      return 1;
    }
    if (!master && (keys == (keyOpt | keyLeft) || keys == (keyOpt | keyRight))) {
      int t = *pSongTrack + (keys & keyRight ? 1 : -1);
      if (t >= 0 && t < chipnomadState->project.tracksCount) {
        *pSongTrack = t;
        fullRedraw();
      }
      return 1;
    }
  }
  return screenInput(&data, down, keys, taps);
}
static void init() {}
static void setup(int) {
  moduleButtonDown = 0;
  displayedCpuLoad = -1;
  if (!master && *pSongTrack >= chipnomadState->project.tracksCount) *pSongTrack = 0;
}
static void draw() {
  // Releasing all buttons clears untimed app messages. Restore the selected
  // field's context here, leaving nonempty notices alone until they expire.
  if (!screenGetActiveMessage()[0]) hint(data.cursorCol, data.cursorRow);
  int cpuLoad = audioManager.getCpuLoadPercent();
  if (cpuLoad != displayedCpuLoad) {
    displayedCpuLoad = cpuLoad;
    drawCpuLoad(cpuLoad);
  }
}
static ScreenPlaybackLevel level() { return ScreenPlaybackLevel::phrase; }
const AppScreen screenInsertFX = {init, setup, fullRedraw, draw, onInput, level};
void screenInsertFXOpenMaster(void) { master = true; screenSetup(&screenInsertFX, -1); }
void screenInsertFXOpenTrack(void) { master = false; screenSetup(&screenInsertFX, -1); }
int screenInsertFXIsMaster(void) { return master; }
