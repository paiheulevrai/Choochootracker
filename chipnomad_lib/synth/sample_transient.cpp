#include "sample_transient.h"

#include "../project_instruments.h"
#include "sample_voice.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <signalsmith-linear/fft.h>

// Spectral-flux onset detection (plan §5.1):
//
// 1. STFT with 1024-sample windows, 50% overlap (512-frame hop), Hann window.
// 2. Per-bin magnitude |X[k]| per frame.
// 3. flux[t] = sum over bins of max(0, mag[t][k] - mag[t-1][k]).
// 4. Adaptive threshold: median(flux) * (1 + (100 - sensitivity) / 50).
// 5. Peak-pick: flux above threshold and a local maximum within +-3 frames.
// 6. Sort peaks by flux descending, keep the strongest `maxCount`.
// 7. Sort the survivors by frame ascending - those are the slice bounds.
//
// A minimum inter-onset gap (50 ms) suppresses double triggers on one hit.
// Everything runs on scratch buffers allocated here; the sample is read
// only. The caller is responsible for pausing audio (the routine allocates
// and takes a few ms per second of audio).

namespace {

constexpr uint32_t kFftSize = 1024;
constexpr uint32_t kHop = kFftSize / 2;
constexpr int kPeakHalfWindow = 3; // local-maximum window is +-3 flux frames
constexpr float kMinGapSeconds = 0.05f; // 50 ms between onsets

// Hann window, periodic (denominator N, not N-1) so the overlap-add of the
// 50% hop stays flat.
void buildHannWindow(float* window, uint32_t size) {
  for (uint32_t i = 0; i < size; ++i) {
    window[i] = 0.5f - 0.5f * std::cos(2.0f * (float)M_PI * (float)i / (float)size);
  }
}

// Median of a copy of the values (small helper; flux arrays are short).
float medianOf(std::vector<float> values) {
  const size_t n = values.size();
  if (n == 0) return 0.0f;
  std::nth_element(values.begin(), values.begin() + n / 2, values.end());
  float median = values[n / 2];
  if ((n & 1) == 0) {
    // Even count: average with the lower middle element.
    float lower = *std::max_element(values.begin(), values.begin() + n / 2);
    median = 0.5f * (median + lower);
  }
  return median;
}

struct FluxPeak {
  uint32_t frame; // absolute sample frame of the onset
  float flux;
};

} // namespace

int sampleDetectTransients(const InstrumentSample* sample,
                           uint32_t loopStart, uint32_t loopEnd,
                           uint8_t maxCount, uint8_t sensitivity,
                           uint32_t* outFrames, uint8_t* outCount) {
  if (outCount) *outCount = 0;
  if (!sample || !sample->data || !outFrames || !outCount) return 1;
  if (sample->frameCount == 0 || sample->channels == 0) return 1;
  if (maxCount == 0) return 0;

  // Swap an inverted region and fall back to the whole sample when empty -
  // the same rules the playback path applies to the markers.
  if (loopStart > loopEnd) {
    const uint32_t swap = loopStart;
    loopStart = loopEnd;
    loopEnd = swap + 1;
  }
  if (loopEnd > sample->frameCount) loopEnd = sample->frameCount;
  if (loopEnd <= loopStart) return 1;

  // Cap the requested count so every slice keeps at least 16 frames.
  uint32_t regionLength = loopEnd - loopStart;
  uint32_t cap = regionLength / 16;
  if (cap < 1) cap = 1;
  if (cap > PROJECT_SAMPLE_MAX_SLICES) cap = PROJECT_SAMPLE_MAX_SLICES;
  if ((uint32_t)maxCount > cap) maxCount = (uint8_t)cap;

  // Too-short regions cannot carry a meaningful 1024-point analysis.
  if (regionLength < 64) return 0;

  if (sensitivity < 1) sensitivity = 1;
  if (sensitivity > 99) sensitivity = 99;

  const uint32_t sampleRate = sample->sampleRate > 0 ? sample->sampleRate : 44100;
  uint32_t minGap = (uint32_t)(kMinGapSeconds * sampleRate);
  if (minGap < 1) minGap = 1;

  // Mono-sum the region into a scratch buffer (stereo interleaved input).
  std::vector<float> mono(regionLength);
  for (uint32_t i = 0; i < regionLength; ++i) {
    const int16_t* frame = sample->data + (size_t)(loopStart + i) * sample->channels;
    if (sample->channels == 1) {
      mono[i] = frame[0] / 32768.0f;
    } else {
      float sum = 0.0f;
      for (uint8_t c = 0; c < sample->channels; ++c) sum += frame[c] / 32768.0f;
      mono[i] = sum / sample->channels;
    }
  }

  // STFT: hop 512, window 1024, Hann. The first window is centred on
  // frame 0 (zero-padded on the left), the last one on the final frame.
  signalsmith::linear::SimpleFFT<float> fft(kFftSize);
  std::vector<float> window(kFftSize);
  buildHannWindow(window.data(), kFftSize);

  const uint32_t frameCount = regionLength >= kFftSize
                                  ? (regionLength - kFftSize) / kHop + 1
                                  : 1;
  std::vector<float> flux(frameCount, 0.0f);
  std::vector<float> windowReal(kFftSize, 0.0f);
  std::vector<float> windowImag(kFftSize, 0.0f);
  std::vector<float> spectrumReal(kFftSize, 0.0f);
  std::vector<float> spectrumImag(kFftSize, 0.0f);
  std::vector<float> previousMag(kFftSize / 2, 0.0f);

  for (uint32_t t = 0; t < frameCount; ++t) {
    const int64_t centre = (int64_t)t * kHop;
    const int64_t begin = centre - (int64_t)(kFftSize / 2);
    for (uint32_t n = 0; n < kFftSize; ++n) {
      const int64_t pos = begin + (int64_t)n;
      float value = 0.0f;
      if (pos >= 0 && pos < (int64_t)regionLength) value = mono[(size_t)pos];
      windowReal[n] = value * window[n];
    }
    std::memset(windowImag.data(), 0, kFftSize * sizeof(float));
    fft.fft(windowReal.data(), windowImag.data(), spectrumReal.data(), spectrumImag.data());

    float fluxValue = 0.0f;
    for (uint32_t k = 0; k < kFftSize / 2; ++k) {
      const float magnitude = std::sqrt(spectrumReal[k] * spectrumReal[k] +
                                        spectrumImag[k] * spectrumImag[k]);
      const float rise = magnitude - previousMag[k];
      if (rise > 0.0f) fluxValue += rise;
      previousMag[k] = magnitude;
    }
    flux[t] = fluxValue;
  }

  // Adaptive threshold: the median flux scaled by the sensitivity, with a
  // floor relative to the strongest flux. The floor keeps steady-tone
  // spectral drift (median ~ 0) from spawning noise peaks; higher
  // sensitivity lowers both terms so weaker onsets survive.
  float maxFlux = 0.0f;
  for (uint32_t t = 0; t < frameCount; ++t) {
    if (flux[t] > maxFlux) maxFlux = flux[t];
  }
  const float median = medianOf(flux);
  const float factor = 1.0f + (100.0f - (float)sensitivity) / 50.0f;
  const float floorScale = 0.05f + (100.0f - (float)sensitivity) / 100.0f * 0.45f;
  const float medianTerm = median * factor;
  const float floorTerm = maxFlux * floorScale;
  const float threshold = medianTerm > floorTerm ? medianTerm : floorTerm;

  // Peak-pick: above threshold, a rising edge into the frame (collapses
  // plateaus) and the local maximum within +-3 frames.
  std::vector<FluxPeak> peaks;
  for (uint32_t t = 1; t + 1 < frameCount; ++t) {
    const float value = flux[t];
    if (value <= threshold) continue;
    if (flux[t - 1] >= value) continue; // must be a rising edge
    bool isMax = true;
    for (int d = -kPeakHalfWindow; d <= kPeakHalfWindow && isMax; ++d) {
      if (d == 0) continue;
      const int64_t neighbour = (int64_t)t + d;
      if (neighbour < 0 || neighbour >= (int64_t)frameCount) continue;
      if (flux[(size_t)neighbour] > value) isMax = false;
    }
    if (!isMax) continue;
    FluxPeak peak;
    // Centre the window that produced the peak: flux frame t analysed the
    // window centred on sample frame t * hop.
    peak.frame = loopStart + (uint32_t)std::min<int64_t>(
                     std::max<int64_t>((int64_t)t * kHop, 0), (int64_t)regionLength - 1);
    peak.flux = value;
    peaks.push_back(peak);
  }

  // Enforce the minimum inter-onset gap: walk the peaks strongest-first and
  // drop any that land too close to an already-accepted (stronger) peak.
  std::sort(peaks.begin(), peaks.end(),
            [](const FluxPeak& a, const FluxPeak& b) { return a.flux > b.flux; });
  std::vector<FluxPeak> accepted;
  for (const FluxPeak& peak : peaks) {
    if (accepted.size() >= maxCount) break;
    bool tooClose = false;
    for (const FluxPeak& kept : accepted) {
      const uint32_t distance = kept.frame > peak.frame ? kept.frame - peak.frame
                                                        : peak.frame - kept.frame;
      if (distance < minGap) {
        tooClose = true;
        break;
      }
    }
    if (!tooClose) accepted.push_back(peak);
  }

  // Deliver ascending by frame.
  std::sort(accepted.begin(), accepted.end(),
            [](const FluxPeak& a, const FluxPeak& b) { return a.frame < b.frame; });
  uint8_t count = 0;
  for (const FluxPeak& peak : accepted) {
    outFrames[count++] = peak.frame;
  }
  *outCount = count;
  return 0;
}

uint8_t sampleSliceInitAuto(InstrumentSample* sample, uint8_t count) {
  if (!sample) return 0;
  if (count < 1) count = 1;
  if (count > PROJECT_SAMPLE_MAX_SLICES) count = PROJECT_SAMPLE_MAX_SLICES;
  // Loop region with the same swap/fallback rules as the playback path.
  uint32_t loopStart = sampleMarkerToStartFrame(sample->frameCount, sample->start);
  uint32_t loopEnd = sampleMarkerToEndFrame(sample->frameCount, sample->end);
  if (loopStart > loopEnd) {
    const uint32_t swap = loopStart;
    loopStart = loopEnd;
    loopEnd = swap + 1;
  }
  if (loopEnd <= loopStart) loopEnd = sample->frameCount;
  uint32_t frames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t detected = 0;
  const int result = sampleDetectTransients(sample, loopStart, loopEnd, count,
                                            sample->autoSensitivity, frames, &detected);
  if (result != 0 || detected == 0) {
    // No usable onsets (silence, tiny region): fall back to the even
    // division so AUTO still yields a playable slice layout. The count is
    // capped the same way as detection (16-frame minimum per slice).
    uint32_t cap = (loopEnd - loopStart) / 16;
    if (cap < 1) cap = 1;
    if (cap > PROJECT_SAMPLE_MAX_SLICES) cap = PROJECT_SAMPLE_MAX_SLICES;
    uint8_t effective = count;
    if ((uint32_t)effective > cap) effective = (uint8_t)cap;
    return sampleSliceInitEven(sample, sliceModeAuto, effective);
  }
  memset(sample->sliceBounds, 0, sizeof(sample->sliceBounds));
  for (uint8_t i = 0; i < detected; ++i) {
    sample->sliceBounds[i] = frames[i];
  }
  sample->slice = sampleEncodeSlice(sliceModeAuto, detected);
  return detected;
}
