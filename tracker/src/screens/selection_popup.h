#ifndef CHOOCHOO_SELECTION_POPUP_H
#define CHOOCHOO_SELECTION_POPUP_H

#include "screens.h"

struct SelectionItem {
  const char* label;
  int value;
  const SelectionItem* children;
  int childCount;
};

void selectionPopupSetup(const char* title, const SelectionItem* items,
                         int count, int currentValue,
                         void (*selected)(int), void (*cancelled)(void));

extern const AppScreen screenSelectionPopup;

#endif
