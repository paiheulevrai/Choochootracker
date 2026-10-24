#ifndef CHOOCHOO_SELECTION_POPUP_H
#define CHOOCHOO_SELECTION_POPUP_H

#include "screens.h"

struct SelectionItem {
  const char* label;
  int value;
  const SelectionItem* children;
  int childCount;
  const char* helper;
  bool folder = false; // Right/EDIT enters this physical folder directly.
};

void selectionPopupSetup(const char* title, const SelectionItem* items,
                         int count, int currentValue,
                         void (*selected)(int), void (*cancelled)(void),
                         bool fullWidth = false, void (*preview)(int, bool) = nullptr, bool collections = false, bool focusPreset = false,
                         const char* (*loadingStatus)() = nullptr, bool folderNavigation = false);

bool selectionPopupIsFullWidth(void);

extern const AppScreen screenSelectionPopup;

#endif
