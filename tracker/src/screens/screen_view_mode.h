#ifndef __SCREEN_VIEW_MODE_H__
#define __SCREEN_VIEW_MODE_H__

#include "screens.h"

// Opens the mode chooser for a new project. The chosen mode is applied to
// the freshly initialized project; on confirm the song screen opens, on
// cancel the caller screen is restored.
void viewModeChooserSetup(const AppScreen* fromScreen);

#endif
