#ifndef __CHIPNOMAD_LIB__MIDI_BACKEND_ANDROID_H__
#define __CHIPNOMAD_LIB__MIDI_BACKEND_ANDROID_H__

#include "midi_router.h"

// Android MidiManager backend. The implementation lives in the Android
// platform sources because it uses SDL's JNI accessors and android.media.midi.
const MidiBackend* midiBackendAndroidGet(void);

#endif
