#include "waveform_display.h"
#include <algorithm>
#include <cmath>
#include "corelib_gfx.h"
#include "chipnomad_lib.h"
#include "four_op_patch.h"
#include "opl_patch.h"
#include "opll_presets.h"
#include "sid_patch.h"
#include "playback_chips.h"
#include "synth/braids_voice.h"
#include "synth/achchid_voice.h"
#include "synth/plaits_voice.h"
#include "synth/plaits_alt_voice.h"
#include "synth/mme_voice.h"
#include "synth/sintered_voice.h"
#include "synth/four_op_voice.h"
#include "synth/opl_voice.h"
#include "synth/opll_voice.h"
#include "synth/dx7_voice.h"
#include "synth/simple_chip_voice.h"
#include "common.h"
#include "monitor_display.h"
#include "audio_monitor.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <functional>
#include <chrono>

#define ENVELOPE_DIM_BRIGHTNESS 160

static Bitmap* emptyBitmap = NULL;
static Bitmap* waveformBitmaps[PROJECT_MAX_TRACKS];
static int charW = 0;
static int charH = 0;
static uint8_t noisePattern[512];
static int noiseAnimIdx = 0;
// The audio callback may be much slower than the display (notably with a
// large Android audio buffer).  Keep a UI-side copy so waveform changes are
// blended over display frames instead of visibly jumping once per callback.
static float displayedVoiceSamples[PROJECT_MAX_TRACKS][VOICE_MONITOR_SAMPLES];
static float displayedVoiceEnvelopes[PROJECT_MAX_TRACKS];
static uint8_t displayedVoiceActive[PROJECT_MAX_TRACKS];
static std::chrono::steady_clock::time_point lastWaveformRefresh;
static float voiceBlend = 0.3f;

// ============================================================================
// Playback wavevorm display
// ============================================================================

// Create bitmaps for track waveform display and initialize noise pattern
void waveformDisplayInit(void) {
  charW = gfxGetCharWidth();
  charH = gfxGetCharHeight();

  if (emptyBitmap) gfxBitmapFree(emptyBitmap);
  emptyBitmap = gfxBitmapCreate(1, 1);

  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    if (waveformBitmaps[i]) gfxBitmapFree(waveformBitmaps[i]);
    waveformBitmaps[i] = gfxBitmapCreate(1, 1);
  }
  memset(displayedVoiceSamples, 0, sizeof(displayedVoiceSamples));
  memset(displayedVoiceEnvelopes, 0, sizeof(displayedVoiceEnvelopes));
  memset(displayedVoiceActive, 0, sizeof(displayedVoiceActive));
  lastWaveformRefresh = std::chrono::steady_clock::time_point();

  for (int i = 0; i < 512; i++) {
    noisePattern[i] = rand() & 1;
  }
}

static void drawVerticalLine(Bitmap* bitmap, int x, int y1, int y2, uint8_t shade) {
  int startY = y1 < y2 ? y1 : y2;
  int endY = y1 > y2 ? y1 : y2;
  for (int dy = startY; dy <= endY; dy++) {
    bitmap->data[dy * charW + x] = shade;
  }
}

static void drawAYWaveformSlice(Bitmap* bitmap, int x, int amplitude, int toneHigh, int hasEnvelope, int hasNoise, int noiseShadeBase) {
  static int prevAmplitude = -1;
  static int prevToneHigh = 1;
  static int prevEnvAmplitude = -1;

  if (x == 0) {
    prevAmplitude = -1;
    prevToneHigh = 1;
    prevEnvAmplitude = -1;
  }

  if (amplitude > charH - 1) amplitude = charH - 1;

  int y = charH - 1 - amplitude;
  int lowY = charH - 1;

  if (toneHigh) {
    bitmap->data[y * charW + x] = 255;

    if (prevAmplitude >= 0) {
      int prevY = charH - 1 - prevAmplitude;
      if (prevToneHigh) {
        drawVerticalLine(bitmap, x, y, prevY, 255);
      } else {
        drawVerticalLine(bitmap, x, y, prevY, 255);
      }
    }
  } else {
    bitmap->data[lowY * charW + x] = 255;

    if (hasEnvelope) {
      bitmap->data[y * charW + x] = ENVELOPE_DIM_BRIGHTNESS;
      if (prevEnvAmplitude >= 0) {
        int prevY = charH - 1 - prevEnvAmplitude;
        drawVerticalLine(bitmap, x, y, prevY, ENVELOPE_DIM_BRIGHTNESS);
      }
    }

    if (prevToneHigh && prevAmplitude >= 0) {
      int prevY = charH - 1 - prevAmplitude;
      drawVerticalLine(bitmap, x, lowY, prevY, 255);
    }
  }

  if (hasNoise && toneHigh && amplitude > 0) {
    for (int dy = y + 1; dy < charH; dy++) {
      int noiseShade = noisePattern[noiseAnimIdx] ? noiseShadeBase : 64;
      noiseAnimIdx = (noiseAnimIdx + 1) & 511;
      bitmap->data[dy * charW + x] = noiseShade;
    }
  }

  prevAmplitude = toneHigh ? amplitude : 0;
  prevToneHigh = toneHigh;
  if (hasEnvelope) {
    prevEnvAmplitude = amplitude;
  }
}

static int getAYEnvelopeHeight(int x, int envShape) {
  int shape = envShape & 0x0F;
  int halfW = charW / 2;
  int maxH = charH - 1;
  int isFirstHalf = x < halfW;
  int decay = maxH - (x * maxH) / halfW;
  int attack = (x * maxH) / halfW;
  int decay2 = maxH - ((x - halfW) * maxH) / halfW;
  int attack2 = ((x - halfW) * maxH) / halfW;

  if (shape <= 3 || shape == 9) return isFirstHalf ? decay : 0;
  if ((shape >= 4 && shape <= 7) || shape == 15) return isFirstHalf ? attack : 0;
  if (shape == 8) return isFirstHalf ? decay : decay2;
  if (shape == 10) return isFirstHalf ? decay : attack2;
  if (shape == 11) return isFirstHalf ? decay : maxH;
  if (shape == 12) return isFirstHalf ? attack : attack2;
  if (shape == 13) return isFirstHalf ? attack : maxH;
  if (shape == 14) return isFirstHalf ? attack : decay2;
  return 0;
}

static Bitmap* drawVoiceWaveform(int trackIdx) {
  VoiceMonitor* monitor = &chipnomadState->voiceMonitors[trackIdx];
  if (!monitor->active) {
    displayedVoiceActive[trackIdx] = 0;
    Bitmap* bitmap = waveformBitmaps[trackIdx];
    memset(bitmap->data, 0, bitmap->widthPixels * bitmap->heightPixels);
    return bitmap;
  }

  // Convert the original 60 Hz smoothing to the selected refresh cadence.
  const float blend = voiceBlend;
  if (!displayedVoiceActive[trackIdx]) {
    memcpy(displayedVoiceSamples[trackIdx], monitor->samples,
           sizeof(displayedVoiceSamples[trackIdx]));
    displayedVoiceEnvelopes[trackIdx] = monitor->envelope;
    displayedVoiceActive[trackIdx] = 1;
  } else {
    for (int i = 0; i < VOICE_MONITOR_SAMPLES; ++i) {
      displayedVoiceSamples[trackIdx][i] +=
        (monitor->samples[i] - displayedVoiceSamples[trackIdx][i]) * blend;
    }
    displayedVoiceEnvelopes[trackIdx] +=
      (monitor->envelope - displayedVoiceEnvelopes[trackIdx]) * blend;
  }

  Bitmap* bitmap = waveformBitmaps[trackIdx];
  memset(bitmap->data, 0, bitmap->widthPixels * bitmap->heightPixels);
  int previousY = charH / 2;
  for (int x = 0; x < charW; ++x) {
    int sampleIdx = charW > 1 ? (x * (VOICE_MONITOR_SAMPLES - 1)) / (charW - 1) : 0;
    float sample = displayedVoiceSamples[trackIdx][sampleIdx];
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    int y = (charH - 1) / 2 - (int)(sample * (charH - 1) / 2.0f);
    drawVerticalLine(bitmap, x, previousY, y, 255);
    previousY = y;
  }

  int envelopeY = charH - 1 - (int)(displayedVoiceEnvelopes[trackIdx] * (charH - 1));
  envelopeY = std::max(0, std::min(charH - 1, envelopeY));
  for (int x = 0; x < charW; ++x) bitmap->data[envelopeY * charW + x] = ENVELOPE_DIM_BRIGHTNESS;
  return bitmap;
}

static Bitmap* renderWaveform(int trackIdx) {
  const PlaybackTrackState* track = &chipnomadGetPlaybackStatus(chipnomadState)->tracks[trackIdx];
  if (track->note.instrument == EMPTY_VALUE_8) {
    gfxBitmapClear(waveformBitmaps[trackIdx]);
    return waveformBitmaps[trackIdx];
  }
  InstrumentType type = chipnomadState->project.instruments[track->note.instrument].type;
  // Native chips use voice monitors, never AY register inspection.
  if (type != InstrumentType::AY1 && type != InstrumentType::AY2 && type != InstrumentType::AYSample)
    return drawVoiceWaveform(trackIdx);

  // Check if track is playing
  if (track->note.pitchFinal == EMPTY_VALUE_8) {
    Bitmap* bitmap = waveformBitmaps[trackIdx];
    memset(bitmap->data, 0, bitmap->widthPixels * bitmap->heightPixels);
    return bitmap;
  }

  // The engine owns one AY chip per track and uses channel A.
  int chipIdx = trackIdx;
  int ayChannel = 0;

  SoundChipAY* chip = static_cast<SoundChipAY*>(chipnomadState->chips[chipIdx]);
  if (!chip) return emptyBitmap;

  // Read mixer register (reg 7)
  uint8_t mixerReg = chip->getRegister(7);
  int hasTone = !((mixerReg >> ayChannel) & 1);
  int hasNoise = !((mixerReg >> (ayChannel + 3)) & 1);

  // Read volume register (reg 8/9/10)
  uint8_t volumeReg = chip->getRegister(8 + ayChannel);
  int envEnabled = (volumeReg & 0x10) != 0;

  // Read noise period (reg 6, lower 5 bits)
  uint8_t noisePeriod = chip->getRegister(6) & 0x1F;
  int noiseShadeBase = 128 + noisePeriod * 2;

  Bitmap* bitmap = waveformBitmaps[trackIdx];
  memset(bitmap->data, 0, bitmap->widthPixels * bitmap->heightPixels);

  if (!envEnabled) {
    // Simple volume-based waveform
    int volume = volumeReg & 0x0F;
    int amplitude = (volume * (charH - 1)) / 15;

    if (!hasTone && !hasNoise) {
      // Both disabled - horizontal line (tone always HIGH)
      for (int x = 0; x < charW; x++) {
        drawAYWaveformSlice(bitmap, x, amplitude, 1, 0, 0, 0);
      }
    } else if (hasTone && !hasNoise) {
      // Tone only - square wave
      for (int x = 0; x < charW / 2; x++) {
        drawAYWaveformSlice(bitmap, x, amplitude, 1, 0, 0, 0);
      }
      for (int x = charW / 2; x < charW; x++) {
        drawAYWaveformSlice(bitmap, x, amplitude, 0, 0, 0, 0);
      }
    } else if (!hasTone && hasNoise) {
      // Noise only (tone always HIGH)
      for (int x = 0; x < charW; x++) {
        drawAYWaveformSlice(bitmap, x, amplitude, 1, 0, 1, noiseShadeBase);
      }
    } else {
      // Tone + noise - square wave with noise
      for (int x = 0; x < charW / 2; x++) {
        drawAYWaveformSlice(bitmap, x, amplitude, 1, 0, 1, noiseShadeBase);
      }
      for (int x = charW / 2; x < charW; x++) {
        drawAYWaveformSlice(bitmap, x, amplitude, 0, 0, 1, noiseShadeBase);
      }
    }
  } else {
    // Envelope enabled
    uint8_t envShape = chip->getRegister(13);

    for (int x = 0; x < charW; x++) {
      int amplitude = getAYEnvelopeHeight(x, envShape);

      // Determine tone state (2 periods across width)
      int toneHigh = 1; // Default HIGH when tone disabled
      if (hasTone) {
        float phase = (x * 2.0f) / charW;
        float periodPhase = phase - (int)phase;
        toneHigh = periodPhase < 0.5f;
      }

      drawAYWaveformSlice(bitmap, x, amplitude, toneHigh, 1, hasNoise, noiseShadeBase);
    }
  }

  return bitmap;
}

void waveformDisplayInvalidate(void) {
  lastWaveformRefresh = std::chrono::steady_clock::time_point();
}

void waveformDisplayRefresh(void) {
  if (!chipnomadState) return;

  const auto now = std::chrono::steady_clock::now();
  const int refreshHz = appSettings.waveformRefreshHz < 1 ? 1 :
    (appSettings.waveformRefreshHz > 60 ? 60 : appSettings.waveformRefreshHz);
  float elapsedSeconds = lastWaveformRefresh == std::chrono::steady_clock::time_point() ?
    1.0f / refreshHz : std::chrono::duration<float>(now - lastWaveformRefresh).count();
  // A note can change on every tracker tick.  It must not bypass the selected
  // waveform cadence, otherwise low settings still redraw at the UI rate.
  if (lastWaveformRefresh != std::chrono::steady_clock::time_point() &&
      elapsedSeconds < 1.0f / refreshHz) return;

  voiceBlend = 1.0f - powf(0.7f, elapsedSeconds * 60.0f);
  if (voiceBlend > 1.0f) voiceBlend = 1.0f;
  for (int i = 0; i < chipnomadState->project.tracksCount; ++i) {
    if (appSettings.trackVisuals[i].mode == TrackVisualMode::audio)
      renderTrackAudioWaveform(waveformBitmaps[i], monitorDisplayTrackSamples(i), AUDIO_MONITOR_SAMPLES);
    else
      renderWaveform(i);
  }
  lastWaveformRefresh = now;
}

Bitmap* waveformDisplayGetBitmap(int trackIdx) {
  if (trackIdx < 0 || trackIdx >= PROJECT_MAX_TRACKS) return emptyBitmap;
  return waveformBitmaps[trackIdx];
}

// ============================================================================
// Waveform Preview Rendering
// ============================================================================

// Generic waveform preview renderer
static void renderWaveformPreview(
  Bitmap* bitmap,
  uint8_t* data,
  uint16_t dataLength,
  std::function<uint8_t(uint8_t)> transformFunc
) {
  if (!bitmap || !data || dataLength == 0) {
    return;
  }

  int width = bitmap->widthPixels;
  int height = bitmap->heightPixels;

  // Convert to Y coordinates (inverted: higher value = lower Y)
  auto sampleToY = [height](uint8_t sample) -> int {
    return ((255 - sample) * (height - 1)) / 255;
  };

  // For each pixel, find the range of samples it represents and draw a vertical line
  for (int x = 0; x < width; x++) {
    // Calculate sample range for this pixel
    uint16_t sampleStart = (x * dataLength) / width;
    uint16_t sampleEnd = ((x + 1) * dataLength) / width;

    // Boundary safety checks
    if (sampleStart >= dataLength) sampleStart = dataLength - 1;
    if (sampleEnd >= dataLength) sampleEnd = dataLength - 1;

    // Find min and max sample values in this range
    uint8_t minVal = 255;
    uint8_t maxVal = 0;

    if (sampleStart == sampleEnd) {
      uint8_t val = transformFunc ? transformFunc(data[sampleStart]) : data[sampleStart];
      minVal = maxVal = val;
    } else {
      for (uint16_t i = sampleStart; i <= sampleEnd; i++) {
        uint8_t val = transformFunc ? transformFunc(data[i]) : data[i];
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
      }
    }

    int minY = sampleToY(maxVal);
    int maxY = sampleToY(minVal);

    // Draw vertical line from minY to maxY
    for (int y = minY; y <= maxY; y++) {
      bitmap->data[y * width + x] = 255;
    }
  }
}

void renderSamplePreview(Bitmap* bitmap, uint8_t* sampleData, uint16_t startSample, uint16_t endSample) {
  if (bitmap) gfxBitmapClear(bitmap);
  if (!bitmap || !sampleData || startSample >= endSample) return;

  renderWaveformPreview(bitmap, sampleData + startSample, endSample - startSample, nullptr);
}

void renderPCM16Preview(Bitmap* bitmap, const int16_t* sampleData, uint32_t startFrame,
                        uint32_t endFrame, uint8_t channels) {
  if (bitmap) gfxBitmapClear(bitmap);
  if (!bitmap || !sampleData || startFrame >= endFrame || !channels) return;
  const uint32_t frames = endFrame - startFrame;
  const int width = bitmap->widthPixels, height = bitmap->heightPixels;
  for (int x = 0; x < width; ++x) {
    uint32_t first = startFrame + (uint64_t)x * frames / width;
    uint32_t last = startFrame + (uint64_t)(x + 1) * frames / width;
    if (last <= first) last = first + 1;
    if (last > endFrame) last = endFrame;
    int16_t low = 32767, high = -32768;
    for (uint32_t frame = first; frame < last; ++frame) {
      int16_t value = sampleData[frame * channels];
      if (value < low) low = value;
      if (value > high) high = value;
    }
    int top = (int)(((int64_t)32767 - high) * (height - 1) / 65535);
    int bottom = (int)(((int64_t)32767 - low) * (height - 1) / 65535);
    for (int y = top; y <= bottom; ++y) bitmap->data[y * width + x] = 255;
  }
}

void renderSCWFPreview(Bitmap* bitmap, const InstrumentSCWF* instrument,
                       const uint16_t* frameSize, const uint8_t* frameIndex) {
  if (bitmap) gfxBitmapClear(bitmap);
  if (!bitmap || !instrument) return;
  const int width = bitmap->widthPixels, height = bitmap->heightPixels;
  int detuneCents = 0;
  if (instrument->detune) {
    detuneCents = instrument->detune <= 127
      ? (int)(powf(200.0f, (instrument->detune - 1) / 126.0f) + 0.5f)
      : (instrument->detune - 125) * 100;
  }
  const float detuneRatio = powf(2.0f, detuneCents / 1200.0f);
  int previousY = height / 2;
  for (int x = 0; x < width; ++x) {
    const float phase = width > 1 ? (float)x * 3.0f / (width - 1) : 0.0f;
    float mix = 0.0f;
    for (int osc = 0; osc < 2; ++osc) {
      const InstrumentSample& source = instrument->oscillator[osc];
      uint32_t cycle = frameSize && frameSize[osc] ? frameSize[osc] : source.frameCount;
      if (!source.data || !cycle || cycle > source.frameCount) continue;
      uint32_t tables = source.frameCount / cycle;
      float position = frameIndex && tables > 1 ? frameIndex[osc] * (tables - 1) / 255.0f : 0.0f;
      uint32_t table = (uint32_t)position;
      float blend = position - table;
      float oscillatorPhase = osc ? phase * detuneRatio : phase;
      uint32_t sample = (uint32_t)(oscillatorPhase * cycle) % cycle;
      float value = source.data[(table * cycle + sample) * source.channels] / 32768.0f;
      if (blend && table + 1 < tables) {
        float next = source.data[((table + 1) * cycle + sample) * source.channels] / 32768.0f;
        value += (next - value) * blend;
      }
      mix += value * (osc ? instrument->mix / 255.0f : 1.0f - instrument->mix / 255.0f);
    }
    int y = (height - 1) / 2 - (int)(mix * (height - 1) / 2.0f);
    if (y < 0) y = 0;
    if (y >= height) y = height - 1;
    int from = previousY < y ? previousY : y, to = previousY > y ? previousY : y;
    for (int row = from; row <= to; ++row) bitmap->data[row * width + x] = 255;
    previousY = y;
  }
}

void renderFloatPreview(Bitmap* bitmap, const float* samples, uint32_t count) {
  if (bitmap) gfxBitmapClear(bitmap);
  if (!bitmap || !samples || !count) return;
  const int width = bitmap->widthPixels, height = bitmap->heightPixels;
  float peak = 0.001f;
  for (size_t i = 0; i < count; ++i) if (fabsf(samples[i]) > peak) peak = fabsf(samples[i]);
  size_t start = 0;
  while (start + 1 < count && fabsf(samples[start]) < peak * 0.02f) ++start;
  const size_t visible = count - start;
  int previousY = height / 2;
  for (int x = 0; x < width; ++x) {
    size_t sample = start + (width > 1 ? (size_t)x * (visible - 1) / (width - 1) : 0);
    float value = samples[sample] / peak;
    int y = (height - 1) / 2 - (int)(value * (height - 1) / 2.0f);
    if (y < 0) y = 0;
    if (y >= height) y = height - 1;
    int from = previousY < y ? previousY : y, to = previousY > y ? previousY : y;
    for (int row = from; row <= to; ++row) bitmap->data[row * width + x] = 255;
    previousY = y;
  }
}

void renderBraidsPreview(Bitmap* bitmap, const InstrumentBraids* instrument) {
  if (!instrument) { if (bitmap) gfxBitmapClear(bitmap); return; }
  float samples[768];
  BraidsVoice voice;
  voice.init();
  voice.setModel(instrument->model);
  voice.setPitch(72 << 7);
  voice.setParameters(instrument->timbre, instrument->color);
  voice.setGain(1.0f);
  voice.strike();
  voice.render(samples, sizeof(samples) / sizeof(samples[0]));
  renderFloatPreview(bitmap, samples, sizeof(samples) / sizeof(samples[0]));
}

void renderAChChidPreview(Bitmap* bitmap, const InstrumentAChChid* instrument) {
  if (!instrument) { if (bitmap) gfxBitmapClear(bitmap); return; }
  float samples[768];
  AChChidVoice voice;
  voice.init(48000.0f);
  voice.configure((uint8_t)instrument->wave, instrument->fineTune, instrument->model,
                  instrument->timbre, instrument->color, instrument->saturation,
                  instrument->cutoff, instrument->resonance, instrument->envMod,
                  instrument->decay, instrument->accent, 1.0f);
  voice.noteOn(48, false, false, 0);
  voice.render(samples, sizeof(samples) / sizeof(samples[0]));
  renderFloatPreview(bitmap, samples, sizeof(samples) / sizeof(samples[0]));
}

template <typename Voice>
static void renderPlaitsPreviewVoice(Bitmap* bitmap, const InstrumentPlaits* instrument) {
  float samples[768];
  Voice voice;
  voice.init();
  voice.configure(instrument->engine, instrument->harmonics, instrument->timbre,
                  instrument->morph, instrument->auxMix, instrument->envelopeMode,
                  instrument->decay, instrument->sustain, 72.0f, 1.0f);
  voice.setEnvelope(0.0f, 0.0f, 1.0f, 0.0f);
  voice.noteOn();
  voice.render(samples, sizeof(samples) / sizeof(samples[0]));
  renderFloatPreview(bitmap, samples, sizeof(samples) / sizeof(samples[0]));
}

void renderPlaitsPreview(Bitmap* bitmap, const InstrumentPlaits* instrument, int alt) {
  if (!instrument) { if (bitmap) gfxBitmapClear(bitmap); return; }
  if (alt) renderPlaitsPreviewVoice<PlaitsAltVoice>(bitmap, instrument);
  else renderPlaitsPreviewVoice<PlaitsVoice>(bitmap, instrument);
}

void renderMMEPreview(Bitmap* bitmap, const InstrumentMME* instrument) {
  if (!instrument) { if (bitmap) gfxBitmapClear(bitmap); return; }
  // The preview shows MME's oscillator interaction, not a potentially silent
  // user envelope or filter setting.
  InstrumentMME preview = *instrument;
  preview.attack = preview.decay = preview.release = 0;
  preview.sustain = 255;
  preview.filterEnabled = 0;
  float samples[768];
  MMEVoice voice;
  voice.init(48000.0f);
  voice.configure(&preview, 6900.0f, 1.0f, 20000, 0);
  voice.noteOn();
  voice.render(samples, sizeof(samples) / sizeof(samples[0]));
  renderFloatPreview(bitmap, samples, sizeof(samples) / sizeof(samples[0]));
}

void renderFMPreview(Bitmap* bitmap, const Instrument* instrument) {
  if (!bitmap || !instrument) { if (bitmap) gfxBitmapClear(bitmap); return; }
  constexpr size_t frames = 768;
  float samples[frames];
  float stereo[frames * 2];
  if (isFourOp(instrument->type)) {
    FourOpVoice voice;
    voice.init(48000.0f);
    voice.configure(instrument->type, &instrument->chip.fourOp, 6900.0f, 1.0f);
    voice.noteOn();
    voice.render(stereo, frames);
    for (size_t i = 0; i < frames; ++i) samples[i] = stereo[i * 2];
  } else if (isOPLL(instrument->type)) {
    OPLLVoice voice;
    voice.init(48000.0f);
    voice.configure(&instrument->chip.opll, 6900.0f, 1.0f);
    voice.noteOn();
    voice.render(samples, frames);
    renderFloatPreview(bitmap, samples, frames);
    return;
  } else if (isOPL(instrument->type)) {
    OPLVoice voice;
    voice.init(48000.0f);
    voice.configure(instrument->type, &instrument->chip.opl, 6900.0f, 1.0f);
    voice.noteOn();
    voice.render(stereo, frames);
    for (size_t i = 0; i < frames; ++i) samples[i] = stereo[i * 2];
  } else if (instrument->type == InstrumentType::DX7) {
    DX7Part voice;
    voice.init(48000.0f);
    voice.voices[0].configure(&instrument->chip.dx7, 6900.0f, 1.0f);
    voice.voices[0].noteOn();
    voice.render(samples, frames);
    renderFloatPreview(bitmap, samples, frames);
    return;
  } else if (instrument->type == InstrumentType::SID) {
    const auto& sid = instrument->chip.sid;
    const float pulse = sid.value[sidPulse] / 4095.0f;
    uint32_t noise = 0x1aceu;
    for (size_t i = 0; i < frames; ++i) {
      const float phase = float(i % 96) / 96.0f;
      const float triangle = 1.0f - 4.0f * std::abs(phase - 0.5f);
      const float saw = phase * 2.0f - 1.0f;
      const float square = phase < pulse ? 1.0f : -1.0f;
      noise = noise * 1103515245u + 12345u;
      const float random = float((noise >> 16) & 0x7fff) / 16384.0f - 1.0f;
      switch (sid.value[sidWave]) {
        case 1: samples[i] = triangle; break;
        case 2: samples[i] = saw; break;
        case 3: samples[i] = triangle * saw; break;
        case 4: samples[i] = square; break;
        case 5: samples[i] = triangle * square; break;
        case 6: samples[i] = saw * square; break;
        case 7: samples[i] = triangle * saw * square; break;
        case 8: samples[i] = random; break;
        default: samples[i] = 0.0f; break;
      }
    }
    renderFloatPreview(bitmap, samples, frames);
    return;
  } else {
    gfxBitmapClear(bitmap);
    return;
  }
  renderFloatPreview(bitmap, samples, frames);
}

void renderSimpleChipPreview(Bitmap* bitmap, InstrumentType type,
                             const InstrumentSimpleChip* instrument) {
  if (!bitmap || !instrument) { if (bitmap) gfxBitmapClear(bitmap); return; }
  float samples[768];
  SimpleChipVoice voice;
  voice.init(48000.0f);
  voice.configure(type, instrument, 6900.0f, 1.0f);
  voice.noteOn();
  voice.render(samples, sizeof(samples) / sizeof(samples[0]));
  renderFloatPreview(bitmap, samples, sizeof(samples) / sizeof(samples[0]));
}

void renderAYWavetablePreview(Bitmap* bitmap, uint8_t* wavetable, int isYM) {
  if (bitmap) gfxBitmapClear(bitmap);
  if (!bitmap || !wavetable) return;

  uint8_t* dacTable = isYM ? cnDACTableYM : cnDACTableAY;

  auto transformFunc = [dacTable](uint8_t value) -> uint8_t {
    return dacTable[value & 0x0F];
  };

  renderWaveformPreview(bitmap, wavetable, 32, transformFunc);
}

uint8_t ayWavetableLfoPreviewLevel(uint8_t value) {
  return (uint8_t)((value & 0x0F) * 255 / 15);
}

void renderAYWavetableLfoPreview(Bitmap* bitmap, uint8_t* wavetable) {
  if (bitmap) gfxBitmapClear(bitmap);
  if (!bitmap || !wavetable) return;

  const int zeroY = bitmap->heightPixels / 2;
  for (int x = 0; x < bitmap->widthPixels; ++x) bitmap->data[zeroY * bitmap->widthPixels + x] = 64;
  renderWaveformPreview(bitmap, wavetable, 32, ayWavetableLfoPreviewLevel);
}
void renderTrackAudioWaveform(Bitmap* bitmap, const float* samples, int count) {
  if (!bitmap) return;
  gfxBitmapClear(bitmap);
  if (!samples || count <= 0) return;
  const int w = bitmap->widthPixels, h = bitmap->heightPixels;
  if (w < 3 || h < 5) return;
  const int centre = (h - 1) / 2, radius = (h - 4) / 2;
  int previous = centre;
  for (int x = 1; x < w - 1; ++x) {
    int start = (x - 1) * count / (w - 2), end = std::max(start + 1, x * count / (w - 2));
    float low = 1, high = -1;
    for (int i = start; i < end && i < count; ++i) {
      float value = std::isfinite(samples[i]) ? samples[i] : 0;
      low = std::min(low, std::max(-1.0f, value)); high = std::max(high, std::min(1.0f, value));
    }
    int upper = centre - (int)lroundf(high * radius), lower = centre - (int)lroundf(low * radius);
    if (x > 1) { upper = std::min(upper, previous); lower = std::max(lower, previous); }
    for (int y = upper; y <= lower; ++y) bitmap->data[y * w + x] = low == 0 && high == 0 ? 64 : 255;
    previous = (upper + lower) / 2;
  }
}
