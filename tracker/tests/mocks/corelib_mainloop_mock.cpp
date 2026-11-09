#include "corelib_mainloop.h"

int mockQuitTriggered;
uint32_t mockMainLoopTicks;
uint32_t mainLoopGetTicks(void) { return mockMainLoopTicks; }

void mainLoopRun(void (*draw)(void), void (*onEvent)(MainLoopEventData eventData)) {}
void mainLoopDelay(int ms) {}
void mainLoopQuit(void) {}
void mainLoopTriggerQuit(void) { mockQuitTriggered = 1; }
