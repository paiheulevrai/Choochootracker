#ifndef __WAVEFORM_DISPLAY_H__
#define __WAVEFORM_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "corelib_gfx.h"

struct InstrumentSCWF;
struct InstrumentBraids;
struct InstrumentPlaits;

/**
 * @brief Initialize waveform display system
 */
void waveformDisplayInit(void);

/**
 * @brief Get waveform bitmap for a track
 *
 * @param trackIdx Track index
 * @return Bitmap* Pointer to bitmap
 */
Bitmap* waveformDisplayGetBitmap(int trackIdx);

/**
 * @brief Render a sample waveform preview into a bitmap
 *
 * @param bitmap Target bitmap to render into (must be pre-allocated)
 * @param sampleData 8-bit unsigned sample data (128 = center)
 * @param startSample First sample index to render
 * @param endSample Last sample index to render (exclusive)
 */
void renderSamplePreview(Bitmap* bitmap, uint8_t* sampleData, uint16_t startSample, uint16_t endSample);

/** Render a PCM16 waveform preview; stereo samples are reduced to the left channel. */
void renderPCM16Preview(Bitmap* bitmap, const int16_t* sampleData, uint32_t startFrame,
                        uint32_t endFrame, uint8_t channels);

void renderSCWFPreview(Bitmap* bitmap, const struct InstrumentSCWF* instrument,
                       const uint16_t* frameSize, const uint8_t* frameIndex);

/** VCO output preview for synth engines; rendered only by the instrument UI. */
void renderBraidsPreview(Bitmap* bitmap, const struct InstrumentBraids* instrument);
void renderPlaitsPreview(Bitmap* bitmap, const struct InstrumentPlaits* instrument, int alt);
void renderFloatPreview(Bitmap* bitmap, const float* samples, uint32_t count);

/**
 * @brief Render a wavetable preview into a bitmap
 *
 * @param bitmap Target bitmap to render into (must be pre-allocated)
 * @param wavetable 32 4-bit wavetable values (0-15)
 * @param isYM 1 for YM chip, 0 for AY chip (affects amplitude scaling)
 */
void renderAYWavetablePreview(Bitmap* bitmap, uint8_t* wavetable, int isYM);


#ifdef __cplusplus
}
#endif

#endif
