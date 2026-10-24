#ifndef __CHIPNOMAD_LIB__MIDI_BACKEND_WEB_H__
#define __CHIPNOMAD_LIB__MIDI_BACKEND_WEB_H__

#include "midi_router.h"

// Browser Web MIDI adapter. Its implementation stays in the web platform
// sources because it bridges to navigator.requestMIDIAccess().
const MidiBackend* midiBackendWebGet(void);

#endif
