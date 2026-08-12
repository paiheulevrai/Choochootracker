#include "selection_popup.h"

#include "corelib_gfx.h"
#include <string.h>

static char title[32];
static const SelectionItem* rootItems;
static int rootCount, categoryIndex, itemIndex, activePanel, currentValue;
static void (*onSelected)(int);
static void (*onCancelled)(void);

static const SelectionItem* currentCategory() {
  return &rootItems[categoryIndex];
}

static void selectCurrentValue() {
  categoryIndex = itemIndex = 0;
  for (int i = 0; i < rootCount; ++i) {
    const SelectionItem* category = &rootItems[i];
    if (category->value == currentValue) {
      categoryIndex = i;
      return;
    }
    for (int j = 0; j < category->childCount; ++j) {
      if (category->children[j].value == currentValue) {
        categoryIndex = i;
        itemIndex = j;
        return;
      }
    }
  }
}

void selectionPopupSetup(const char* popupTitle, const SelectionItem* items,
                         int count, int selectedValue,
                         void (*selected)(int), void (*cancelled)(void)) {
  strncpy(title, popupTitle, sizeof(title) - 1);
  title[sizeof(title) - 1] = 0;
  rootItems = items;
  rootCount = count;
  currentValue = selectedValue;
  activePanel = 0;
  onSelected = selected;
  onCancelled = cancelled;
  selectCurrentValue();
}

static void setup(int input) {}

static void fullRedraw() {
  gfxClear();
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, title);
  gfxPrint(18, 1, "|");
  const SelectionItem* category = currentCategory();
  int categoryStart = categoryIndex > 7 ? categoryIndex - 7 : 0;
  int itemStart = itemIndex > 7 ? itemIndex - 7 : 0;
  for (int row = 0; row < 16; ++row) {
    int categoryItem = categoryStart + row;
    int childItem = itemStart + row;
    int y = 2 + row;
    if (categoryItem < rootCount) {
      gfxSetFgColor(categoryItem == categoryIndex && activePanel == 0 ?
        appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
      gfxPrintf(0, y, "%c %-16.16s", categoryItem == categoryIndex ? '>' : ' ',
        rootItems[categoryItem].label);
    }
    if (childItem < category->childCount) {
      const SelectionItem* item = &category->children[childItem];
      gfxSetFgColor(childItem == itemIndex && activePanel == 1 ?
        appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
      gfxPrintf(19, y, "%c%c %-18.18s", childItem == itemIndex ? '>' : ' ',
        item->value == currentValue ? '*' : ' ', item->label);
    }
  }
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 19, "L/R PANEL U/D MOVE EDIT SELECT OPT EXIT");
}

static void draw() {}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (!isKeyDown) return 1;
  if (keys == keyUp || keys == keyDown) {
    int direction = keys == keyUp ? -1 : 1;
    if (activePanel == 0) {
      categoryIndex += direction;
      if (categoryIndex < 0) categoryIndex = rootCount - 1;
      if (categoryIndex >= rootCount) categoryIndex = 0;
      itemIndex = 0;
    } else {
      int count = currentCategory()->childCount;
      if (!count) return 1;
      itemIndex += direction;
      if (itemIndex < 0) itemIndex = count - 1;
      if (itemIndex >= count) itemIndex = 0;
    }
    fullRedraw();
    return 1;
  }
  if (keys == keyLeft || keys == keyRight) {
    activePanel = keys == keyLeft ? 0 : 1;
    if (!currentCategory()->childCount) activePanel = 0;
    fullRedraw();
    return 1;
  }
  if (keys == keyEdit) {
    if (activePanel == 0) {
      if (currentCategory()->childCount) {
        activePanel = 1;
        fullRedraw();
      } else if (onSelected) {
        onSelected(currentCategory()->value);
      }
    } else if (onSelected && currentCategory()->childCount) {
      onSelected(currentCategory()->children[itemIndex].value);
    }
    return 1;
  }
  if (keys == keyOpt) {
    if (onCancelled) {
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
