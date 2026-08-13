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
  envelope_.init(outputSampleRate_);
  filter_.init(outputSampleRate_);
}

void SampleVoice::configure(const InstrumentSample* sample, float pitchCents,
                            float gain, uint8_t start, uint8_t end,
                            uint16_t cutoffHz, uint8_t resonance) {
  sample_ = sample;
  gain_ = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
  if (!sample_ || !sample_->data || sample_->frameCount == 0) return;

  startFrame_ = (uint32_t)((uint64_t)start * (sample_->frameCount - 1) / 255);
  endFrame_ = end == 255
    ? sample_->frameCount
    : (uint32_t)((uint64_t)(end + 1) * sample_->frameCount / 256);
  if (endFrame_ <= startFrame_) endFrame_ = startFrame_ + 1;
  if (endFrame_ > sample_->frameCount) endFrame_ = sample_->frameCount;
  step_ = (sample_->sampleRate / outputSampleRate_) * pow(2.0, pitchCents / 1200.0);
  filter_.configure(sample_->filterEnabled != 0, sample_->filterMode,
                    sample_->filterSlope24dB != 0, cutoffHz,
                    resonance / 255.0f);
  envelope_.configure(envelopeTime(sample_->attack), envelopeTime(sample_->decay),
                      sample_->sustain / 255.0f, envelopeTime(sample_->release),
                      sample_->envelopeShape);
}

void SampleVoice::noteOn() {
  if (!sample_ || !sample_->data || endFrame_ <= startFrame_) return;
  position_ = startFrame_;
  active_ = true;
  filter_.reset();
  envelope_.noteOn();
}

void SampleVoice::noteOff() {
  if (active_) envelope_.noteOff();
}

void SampleVoice::kill() {
  active_ = false;
  envelope_.kill();
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
    float envelope = envelope_.next() * gain_;
    if (!envelope_.active()) { kill(); break; }
    for (int channel = 0; channel < 2; channel++) {
      int sourceChannel = sample_->channels == 1 ? 0 : channel;
      float a = sample_->data[frame * sample_->channels + sourceChannel] / 32768.0f;
      float b = sample_->data[next * sample_->channels + sourceChannel] / 32768.0f;
      float value = (a + (b - a) * fraction) * envelope;
      value = filter_.process(value, channel);
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
