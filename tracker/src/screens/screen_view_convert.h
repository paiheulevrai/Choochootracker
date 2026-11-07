#ifndef __SCREEN_VIEW_CONVERT_H__
#define __SCREEN_VIEW_CONVERT_H__

#include "screens.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opens the view-mode conversion screen with the warning text and the three
// options (Save and proceed / Proceed without saving / Cancel). `target` is
// the mode the project will be converted TO. `fromScreen` is returned to on
// cancel and after a successful (or failed) conversion — normally the View
// settings window.
void screenViewConvertSetup(const AppScreen* fromScreen, ViewMode target);

#ifdef __cplusplus
}
#endif

#endif // __SCREEN_VIEW_CONVERT_H__
