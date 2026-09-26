#include "corelib_mainloop.h"

int mockQuitTriggered;

void mainLoopRun(void (*draw)(void), void (*onEvent)(MainLoopEventData eventData)) {}
void mainLoopDelay(int ms) {}
void mainLoopQuit(void) {}
void mainLoopTriggerQuit(void) { mockQuitTriggered = 1; }
