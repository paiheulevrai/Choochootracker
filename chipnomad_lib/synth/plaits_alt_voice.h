#ifndef CHOOCHOO_PLAITS_ALT_VOICE_H
#define CHOOCHOO_PLAITS_ALT_VOICE_H

#include "plaits_alt/dsp/voice.h"
#include "mutable_voice_base.h"

class PlaitsAltVoice : public MutableVoiceBase<plaits_alt::Voice, plaits_alt::Patch, plaits_alt::Modulations> {};

#endif
