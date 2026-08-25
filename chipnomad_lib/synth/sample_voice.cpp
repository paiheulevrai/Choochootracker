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
  direction_ = 1;
  reverse_ = false;
  loopMode_ = 0;
  timeStretch_ = 1.0f;
  granular_ = false;
  grainExhausted_ = false;
  startFrame_ = 0;
  endFrame_ = 0;
  active_ = false;
  post_.init(outputSampleRate_);
}

void SampleVoice::configure(const InstrumentSample* sample, float pitchCents,
                            float gain, float speedPercent, uint8_t start, uint8_t end, uint8_t loopMode,
                            uint16_t cutoffHz, uint8_t resonance, int attack, int decay,
                            int sustain, int release, int envelopeShape) {
  sample_ = sample;
  post_.setGain(gain);
  if (!sample_ || !sample_->data || sample_->frameCount == 0) return;

  uint32_t startFrame = (uint32_t)((uint64_t)start * (sample_->frameCount - 1) / 255);
  uint32_t endFrame = end == 255
    ? sample_->frameCount
    : (uint32_t)((uint64_t)(end + 1) * sample_->frameCount / 256);
  reverse_ = start > end;
  startFrame_ = reverse_ ? endFrame - 1 : startFrame;
  endFrame_ = reverse_ ? startFrame + 1 : endFrame;
  if (endFrame_ <= startFrame_) endFrame_ = startFrame_ + 1;
  if (endFrame_ > sample_->frameCount) endFrame_ = sample_->frameCount;
  step_ = (sample_->sampleRate / outputSampleRate_) * pow(2.0, pitchCents / 1200.0);
  timeStretch_ = speedPercent / 100.0f;
  granular_ = fabsf(timeStretch_ - 1.0f) > 0.001f;
  loopMode_ = loopMode > 2 ? 0 : loopMode;
  grainSize_ = (uint32_t)(outputSampleRate_ * 0.040f);
  grainHop_ = grainSize_ / 2;
  post_.setFilter(sample_->filterEnabled != 0, sample_->filterCharacter, sample_->filterMode,
                  sample_->filterSlope24dB != 0, cutoffHz, resonance / 255.0f);
  post_.setEnvelope(true, envelopeTime(attack < 0 ? sample_->attack : attack),
                    envelopeTime(decay < 0 ? sample_->decay : decay),
                    (sustain < 0 ? sample_->sustain : sustain) / 255.0f,
                    envelopeTime(release < 0 ? sample_->release : release),
                    envelopeShape < 0 ? sample_->envelopeShape : envelopeShape);
}

void SampleVoice::noteOn() {
  if (!sample_ || !sample_->data || endFrame_ <= startFrame_) return;
  position_ = startFrame_;
  direction_ = reverse_ ? -1 : 1;
  grainExhausted_ = false;
  grainPosition_[0] = reverse_ ? endFrame_ - 1 : startFrame_;
  grainPosition_[1] = grainPosition_[0] + direction_ * step_ * grainHop_ * timeStretch_;
  grainAge_[0] = grainHop_;
  grainAge_[1] = 0;
  nextGrainPosition_ = grainPosition_[1] + direction_ * step_ * grainHop_ * timeStretch_;
  active_ = true;
  post_.noteOn(true);
}

float SampleVoice::sampleAt(double position, int channel) const {
  uint32_t frame = (uint32_t)position;
  uint32_t next = direction_ > 0
    ? (frame + 1 < endFrame_ ? frame + 1 : frame)
    : (frame > startFrame_ ? frame - 1 : frame);
  int sourceChannel = sample_->channels == 1 ? 0 : channel;
  float a = sample_->data[frame * sample_->channels + sourceChannel] / 32768.0f;
  float b = sample_->data[next * sample_->channels + sourceChannel] / 32768.0f;
  return a + (b - a) * (float)(position - frame);
}

float SampleVoice::grainSampleAt(double position, int channel) const {
  const double first = startFrame_;
  const double length = endFrame_ - startFrame_;
  if (loopMode_ == 1) {
    position = fmod(position - first, length);
    if (position < 0.0) position += length;
    position += first;
  } else if (loopMode_ == 2) {
    double span = length > 1.0 ? length - 1.0 : 1.0;
    double period = span * 2.0;
    position = fmod(position - first, period);
    if (position < 0.0) position += period;
    position = first + (position <= span ? position : period - position);
  } else if (position < first || position >= endFrame_) {
    return 0.0f;
  }
  uint32_t frame = (uint32_t)position;
  uint32_t next = frame + 1 < endFrame_ ? frame + 1 : frame;
  int sourceChannel = sample_->channels == 1 ? 0 : channel;
  float a = sample_->data[frame * sample_->channels + sourceChannel] / 32768.0f;
  float b = sample_->data[next * sample_->channels + sourceChannel] / 32768.0f;
  return a + (b - a) * (float)(position - frame);
}

bool SampleVoice::advancePosition() {
  position_ += step_ * direction_;
  if (position_ >= startFrame_ && position_ < endFrame_) return true;
  if (loopMode_ == 0) return false;
  double first = startFrame_;
  double last = endFrame_ - 1.0;
  if (loopMode_ == 1) {
    double length = endFrame_ - startFrame_;
    while (position_ >= endFrame_) position_ -= length;
    while (position_ < startFrame_) position_ += length;
  } else if (position_ > last) {
    position_ = last - (position_ - last);
    direction_ = -1;
  } else {
    position_ = first + (first - position_);
    direction_ = 1;
  }
  return true;
}

void SampleVoice::noteOff() {
  if (active_) post_.noteOff();
}

void SampleVoice::kill() {
  active_ = false;
  post_.kill();
}

void SampleVoice::render(float* output, size_t frames) {
  memset(output, 0, frames * 2 * sizeof(float));
  if (!active_ || !sample_ || !sample_->data) return;

  for (size_t i = 0; i < frames; i++) {
    if (!post_.envelopeActive()) { kill(); break; }
    if (granular_) {
      for (int grain = 0; grain < 2; ++grain) {
        if (grainAge_[grain] >= grainSize_) {
          grainPosition_[grain] = nextGrainPosition_;
          nextGrainPosition_ += direction_ * step_ * grainHop_ * timeStretch_;
          grainAge_[grain] = 0;
          if (loopMode_ == 0 && nextGrainPosition_ >= endFrame_) grainExhausted_ = true;
        }
      }
      for (int channel = 0; channel < 2; ++channel) {
        float value = 0.0f;
        for (int grain = 0; grain < 2; ++grain) {
          float phase = (float)grainAge_[grain] / (float)(grainSize_ - 1);
          float window = sinf(3.14159265f * phase);
          value += grainSampleAt(grainPosition_[grain] + direction_ * step_ * grainAge_[grain], channel) * window * window;
        }
        output[i * 2 + channel] = post_.process(value, channel);
      }
      grainAge_[0]++;
      grainAge_[1]++;
      if (grainExhausted_ && loopMode_ == 0 &&
          grainPosition_[0] + step_ * grainAge_[0] >= endFrame_ &&
          grainPosition_[1] + step_ * grainAge_[1] >= endFrame_) { kill(); break; }
    } else {
      for (int channel = 0; channel < 2; channel++) {
        output[i * 2 + channel] = post_.process(sampleAt(position_, channel), channel);
      }
      if (!advancePosition()) { kill(); break; }
    }
    if (!post_.envelopeActive()) { kill(); break; }
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
