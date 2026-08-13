#ifndef CHOOCHOO_PLAITS_VOICE_H
#define CHOOCHOO_PLAITS_VOICE_H

#include "plaits/dsp/voice.h"
#include "mutable_voice_base.h"

class PlaitsVoice : public MutableVoiceBase<plaits::Voice, plaits::Patch, plaits::Modulations> {};

#endif
