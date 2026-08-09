#include "sample_voice.h"

#include "../project_instruments.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float envelopeTime(uint8_t value) {
  float normalized = value / 255.0f;
  return normalized * normalized * 5.0f;
}

void SampleVoice::init(float outputSampleRate) {
  sample_ = nullptr;
  outputSampleRate_ = outputSampleRate;
  position_ = 0.0;
  step_ = 1.0;
  startFrame_ = 0;
  endFrame_ = 0;
  active_ = false;
  gain_ = 1.0f;
  cutoffHz_ = 20000;
  resonance_ = 0;
  envelopeStage_ = EnvelopeStage::idle;
  envelopeLevel_ = 0.0f;
  envelopeIncrement_ = 0.0f;
  envelopeSamplesLeft_ = 0;
  memset(filterIc1eq_, 0, sizeof(filterIc1eq_));
  memset(filterIc2eq_, 0, sizeof(filterIc2eq_));
  updateFilter();
}

void SampleVoice::configure(const InstrumentSample* sample, float pitchCents,
                            float gain, uint8_t start, uint8_t end,
                            uint16_t cutoffHz, uint8_t resonance) {
  sample_ = sample;
  gain_ = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
  if (!sample_ || !sample_->data || sample_->frameCount == 0) return;

  cutoffHz_ = cutoffHz;
  resonance_ = resonance;
  startFrame_ = (uint32_t)((uint64_t)start * (sample_->frameCount - 1) / 255);
  endFrame_ = end == 255
    ? sample_->frameCount
    : (uint32_t)((uint64_t)(end + 1) * sample_->frameCount / 256);
  if (endFrame_ <= startFrame_) endFrame_ = startFrame_ + 1;
  if (endFrame_ > sample_->frameCount) endFrame_ = sample_->frameCount;
  step_ = (sample_->sampleRate / outputSampleRate_) * pow(2.0, pitchCents / 1200.0);
  updateFilter();
}

void SampleVoice::updateFilter() {
  float cutoff = cutoffHz_;
  float resonance = resonance_ / 255.0f;
  if (cutoff < 20.0f) cutoff = 20.0f;
  if (cutoff > outputSampleRate_ * 0.45f) cutoff = outputSampleRate_ * 0.45f;
  float q = 0.5f + resonance * 19.5f;
  filterG_ = tanf(3.14159265358979323846f * cutoff / outputSampleRate_);
  filterK_ = 1.0f / q;
  filterA1_ = 1.0f / (1.0f + filterG_ * (filterG_ + filterK_));
  filterA2_ = filterG_ * filterA1_;
  filterA3_ = filterG_ * filterA2_;
}

float SampleVoice::processFilter(float input, int channel, int stage) {
  float v3 = input - filterIc2eq_[channel][stage];
  float band = filterA1_ * filterIc1eq_[channel][stage] + filterA2_ * v3;
  float low = filterIc2eq_[channel][stage] + filterA2_ * filterIc1eq_[channel][stage] + filterA3_ * v3;
  float high = input - filterK_ * band - low;
  filterIc1eq_[channel][stage] = 2.0f * band - filterIc1eq_[channel][stage];
  filterIc2eq_[channel][stage] = 2.0f * low - filterIc2eq_[channel][stage];
  if (sample_->filterMode == 1) return band;
  if (sample_->filterMode == 2) return high;
  return low;
}

void SampleVoice::enterEnvelopeStage(EnvelopeStage stage) {
  envelopeStage_ = stage;
  float target = envelopeLevel_;
  float seconds = 0.0f;
  if (stage == EnvelopeStage::attack) {
    target = 1.0f;
    seconds = envelopeTime(sample_->attack);
  } else if (stage == EnvelopeStage::decay) {
    target = sample_->sustain / 255.0f;
    seconds = envelopeTime(sample_->decay);
  } else if (stage == EnvelopeStage::release) {
    target = 0.0f;
    seconds = envelopeTime(sample_->release);
  } else if (stage == EnvelopeStage::sustain) {
    envelopeLevel_ = sample_->sustain / 255.0f;
    envelopeSamplesLeft_ = 0;
    return;
  } else {
    envelopeLevel_ = 0.0f;
    envelopeSamplesLeft_ = 0;
    active_ = false;
    return;
  }

  envelopeSamplesLeft_ = (uint32_t)(seconds * outputSampleRate_);
  if (envelopeSamplesLeft_ == 0) {
    envelopeLevel_ = target;
    if (stage == EnvelopeStage::attack) enterEnvelopeStage(EnvelopeStage::decay);
    else if (stage == EnvelopeStage::decay) enterEnvelopeStage(EnvelopeStage::sustain);
    else enterEnvelopeStage(EnvelopeStage::idle);
    return;
  }
  envelopeIncrement_ = (target - envelopeLevel_) / envelopeSamplesLeft_;
}

float SampleVoice::processEnvelope() {
  if (envelopeSamplesLeft_ > 0) {
    envelopeLevel_ += envelopeIncrement_;
    if (--envelopeSamplesLeft_ == 0) {
      if (envelopeStage_ == EnvelopeStage::attack) enterEnvelopeStage(EnvelopeStage::decay);
      else if (envelopeStage_ == EnvelopeStage::decay) enterEnvelopeStage(EnvelopeStage::sustain);
      else if (envelopeStage_ == EnvelopeStage::release) enterEnvelopeStage(EnvelopeStage::idle);
    }
  }
  return envelopeLevel_;
}

void SampleVoice::noteOn() {
  if (!sample_ || !sample_->data || endFrame_ <= startFrame_) return;
  position_ = startFrame_;
  active_ = true;
  envelopeLevel_ = 0.0f;
  memset(filterIc1eq_, 0, sizeof(filterIc1eq_));
  memset(filterIc2eq_, 0, sizeof(filterIc2eq_));
  enterEnvelopeStage(EnvelopeStage::attack);
}

void SampleVoice::noteOff() {
  if (active_) enterEnvelopeStage(EnvelopeStage::release);
}

void SampleVoice::kill() {
  active_ = false;
  envelopeStage_ = EnvelopeStage::idle;
  envelopeLevel_ = 0.0f;
}

void SampleVoice::render(float* output, size_t frames) {
  memset(output, 0, frames * 2 * sizeof(float));
  if (!active_ || !sample_ || !sample_->data) return;

  for (size_t i = 0; i < frames; i++) {
    uint32_t frame = (uint32_t)position_;
    if (frame >= endFrame_) {
      kill();
      break;
    }
    uint32_t next = frame + 1 < endFrame_ ? frame + 1 : frame;
    float fraction = (float)(position_ - frame);
    float envelope = processEnvelope() * gain_;
    for (int channel = 0; channel < 2; channel++) {
      int sourceChannel = sample_->channels == 1 ? 0 : channel;
      float a = sample_->data[frame * sample_->channels + sourceChannel] / 32768.0f;
      float b = sample_->data[next * sample_->channels + sourceChannel] / 32768.0f;
      float value = (a + (b - a) * fraction) * envelope;
      if (sample_->filterEnabled) {
        value = processFilter(value, channel, 0);
        if (sample_->filterSlope24dB) value = processFilter(value, channel, 1);
      }
      output[i * 2 + channel] = value;
    }
    position_ += step_;
  }
}

static uint16_t readU16(FILE* file, bool* ok) {
  uint8_t bytes[2];
  if (fread(bytes, 1, 2, file) != 2) { *ok = false; return 0; }
  return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

static uint32_t readU32(FILE* file, bool* ok) {
  uint8_t bytes[4];
  if (fread(bytes, 1, 4, file) != 4) { *ok = false; return 0; }
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
    ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

int sampleLoadWav16(const char* path, InstrumentSample* sample,
                    char* error, size_t errorSize) {
  FILE* file = fopen(path, "rb");
  if (!file) { snprintf(error, errorSize, "Cannot open WAV"); return 1; }
  char id[4];
  bool ok = fread(id, 1, 4, file) == 4 && !memcmp(id, "RIFF", 4);
  (void)readU32(file, &ok);
  ok = ok && fread(id, 1, 4, file) == 4 && !memcmp(id, "WAVE", 4);
  uint16_t format = 0, channels = 0, bits = 0;
  uint32_t sampleRate = 0, dataSize = 0;
  long dataOffset = 0;

  while (ok && fread(id, 1, 4, file) == 4) {
    uint32_t size = readU32(file, &ok);
    if (!ok) break;
    long nextChunk = ftell(file) + size + (size & 1);
    if (!memcmp(id, "fmt ", 4) && size >= 16) {
      format = readU16(file, &ok);
      channels = readU16(file, &ok);
      sampleRate = readU32(file, &ok);
      (void)readU32(file, &ok);
      (void)readU16(file, &ok);
      bits = readU16(file, &ok);
    } else if (!memcmp(id, "data", 4)) {
      dataOffset = ftell(file);
      dataSize = size;
    }
    if (fseek(file, nextChunk, SEEK_SET) != 0) ok = false;
  }

  if (!ok || format != 1 || (channels != 1 && channels != 2) ||
      (bits != 8 && bits != 16) || sampleRate < 1000 ||
      sampleRate > 192000 || !dataOffset) {
    fclose(file);
    snprintf(error, errorSize, "Need PCM8/16 mono/stereo WAV");
    return 1;
  }
  uint32_t bytesPerSample = bits / 8;
  uint32_t frames = dataSize / (channels * bytesPerSample);
  if (frames == 0 || dataSize > 64U * 1024U * 1024U) {
    fclose(file);
    snprintf(error, errorSize, "WAV empty or over 64 MB");
    return 1;
  }
  uint32_t sampleCount = frames * channels;
  int16_t* data = (int16_t*)malloc(sampleCount * sizeof(int16_t));
  if (!data || fseek(file, dataOffset, SEEK_SET) != 0) {
    free(data);
    fclose(file);
    snprintf(error, errorSize, "Cannot read WAV data");
    return 1;
  }
  if (bits == 16) {
    ok = fread(data, sizeof(int16_t), sampleCount, file) == sampleCount;
  } else {
    for (uint32_t i = 0; i < sampleCount; i++) {
      int value = fgetc(file);
      if (value == EOF) { ok = false; break; }
      data[i] = (int16_t)((value - 128) << 8);
    }
  }
  if (!ok) {
    free(data);
    fclose(file);
    snprintf(error, errorSize, "Cannot read WAV data");
    return 1;
  }
  fclose(file);
  free(sample->data);
  sample->data = data;
  sample->frameCount = frames;
  sample->sampleRate = sampleRate;
  sample->channels = (uint8_t)channels;
  strncpy(sample->path, path, sizeof(sample->path) - 1);
  sample->path[sizeof(sample->path) - 1] = 0;
  error[0] = 0;
  return 0;
}
