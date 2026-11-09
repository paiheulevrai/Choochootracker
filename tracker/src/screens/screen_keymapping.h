#ifndef __SCREEN_KEYMAPPING_H__
#define __SCREEN_KEYMAPPING_H__

#include "screens.h"

extern const AppScreen screenKeyMapping;
#if defined(PORTMASTER_BUILD) || defined(TEST_PORTMASTER_INPUT)
#define CCT_CONSOLE_CONTROLS
void screenKeyMappingTick(void);
void screenKeyMappingCancel(void);
int screenKeyMappingRecoveryRequested(void);
void screenKeyMappingUnmappedInput(InputCode input, int isDown);
#endif

#endif
