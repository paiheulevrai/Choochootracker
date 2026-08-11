#include "selection_popup.h"

#include "corelib_gfx.h"
#include <string.h>

static char title[32];
static const SelectionItem* rootItems;
static const SelectionItem* currentItems;
static int rootCount, currentCount, selectedIndex, inCategory, currentValue;
static void (*onSelected)(int);
static void (*onCancelled)(void);

static void selectCurrentValue() {
  selectedIndex = 0;
  for (int i = 0; i < currentCount; ++i) {
    if (currentItems[i].value == currentValue) selectedIndex = i;
    for (int j = 0; j < currentItems[i].childCount; ++j) {
      if (currentItems[i].children[j].value == currentValue) selectedIndex = i;
    }
  }
}

void selectionPopupSetup(const char* popupTitle, const SelectionItem* items,
                         int count, int selectedValue,
                         void (*selected)(int), void (*cancelled)(void)) {
  strncpy(title, popupTitle, sizeof(title) - 1);
  title[sizeof(title) - 1] = 0;
  rootItems = currentItems = items;
  rootCount = currentCount = count;
  currentValue = selectedValue;
  inCategory = 0;
  onSelected = selected;
  onCancelled = cancelled;
  selectCurrentValue();
}

static void setup(int input) {}

static void fullRedraw() {
  gfxClear();
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrintf(0, 0, "%s%s", inCategory ? "< " : "", title);
  for (int i = 0; i < currentCount; ++i) {
    gfxSetFgColor(i == selectedIndex ? appSettings.colorScheme.textValue :
                                      appSettings.colorScheme.textDefault);
    gfxPrintf(0, 2 + i, "%c %s%s", i == selectedIndex ? '>' : ' ',
      currentItems[i].label, currentItems[i].childCount ? " >" : "");
  }
}

static void draw() {}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (!isKeyDown) return 1;
  if (keys == keyUp || keys == keyDown) {
    selectedIndex += keys == keyUp ? -1 : 1;
    if (selectedIndex < 0) selectedIndex = currentCount - 1;
    if (selectedIndex >= currentCount) selectedIndex = 0;
    fullRedraw();
    return 1;
  }
  if (keys == keyEdit || keys == keyRight) {
    const SelectionItem* item = &currentItems[selectedIndex];
    if (item->childCount) {
      currentItems = item->children;
      currentCount = item->childCount;
      inCategory = 1;
      selectCurrentValue();
      fullRedraw();
    } else if (onSelected) {
      onSelected(item->value);
    }
    return 1;
  }
  if (keys == keyOpt || keys == keyLeft) {
    if (inCategory) {
      currentItems = rootItems;
      currentCount = rootCount;
      inCategory = 0;
      selectCurrentValue();
      fullRedraw();
    } else if (onCancelled) {
      onCancelled();
    }
    return 1;
  }
  return 1;
}

static ScreenPlaybackLevel playbackLevel() { return ScreenPlaybackLevel::none; }

const AppScreen screenSelectionPopup = {
  .init = NULL, .setup = setup, .fullRedraw = fullRedraw, .draw = draw,
  .onInput = onInput, .getPlaybackLevel = playbackLevel,
};
