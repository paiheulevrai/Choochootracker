#include "audio_recorder.h"
#include "audio_input_diagnostics.h"
#include <string.h>

// Same layout as ExporterWAV's WAVHeader (export_wav.cpp).
struct WAVHeader {
  char riff[4];
  uint32_t fileSize;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t audioFormat;
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];
  uint32_t dataSize;
};

static void writeWAVHeader(FILE* file, int sampleRate, int channels, int bitDepth, int dataSize) {
  WAVHeader header;

  memcpy(header.riff, "RIFF", 4);
  header.fileSize = 36 + dataSize;
  memcpy(header.wave, "WAVE", 4);
  memcpy(header.fmt, "fmt ", 4);
  header.fmtSize = 16;
  header.audioFormat = (bitDepth == 32) ? 3 : 1;
  header.channels = channels;
  header.sampleRate = sampleRate;
  header.bitsPerSample = bitDepth;
  header.byteRate = sampleRate * channels * (bitDepth / 8);
  header.blockAlign = channels * (bitDepth / 8);
  memcpy(header.data, "data", 4);
  header.dataSize = dataSize;

  fwrite(&header, sizeof(WAVHeader), 1, file);
}

int RecordingWavWriter::open(const char* path, int sampleRate, int channels, int bitDepth) {
  if (file_) return 1;
  if (!path || sampleRate <= 0 || channels <= 0 || (bitDepth != 16 && bitDepth != 24 && bitDepth != 32)) return 1;
  file_ = fopen(path, "wb");
  if (!file_) return 1;
  sampleRate_ = sampleRate;
  channels_ = channels;
  bitDepth_ = bitDepth;
  totalSamples_.store(0, std::memory_order_relaxed);
  droppedSamples_.store(0, std::memory_order_relaxed);
  writeWAVHeader(file_, sampleRate_, channels_, bitDepth_, 0);
  return 0;
}

int RecordingWavWriter::write(const int16_t* samples, int stereoSampleCount) {
  if (!file_ || !samples || stereoSampleCount <= 0) {
    droppedSamples_.fetch_add(stereoSampleCount > 0 ? (uint64_t)stereoSampleCount : 1, std::memory_order_relaxed);
    audioInputDiagnostics::writerFailures.fetch_add(1, std::memory_order_relaxed);
    return 1;
  }
  size_t frames = (size_t)stereoSampleCount;
  if (fwrite(samples, sizeof(int16_t) * (size_t)channels_, frames, file_) != frames) {
    droppedSamples_.fetch_add((uint64_t)stereoSampleCount, std::memory_order_relaxed);
    audioInputDiagnostics::writerFailures.fetch_add(1, std::memory_order_relaxed);
    return 1;
  }
  totalSamples_.fetch_add((uint64_t)stereoSampleCount, std::memory_order_relaxed);
  return 0;
}

int RecordingWavWriter::close(void) {
  if (!file_) return 1;
  uint64_t totalBytes = totalSamples_.load(std::memory_order_acquire) * (uint64_t)channels_ * (uint64_t)(bitDepth_ / 8);
  fseek(file_, 0, SEEK_SET);
  writeWAVHeader(file_, sampleRate_, channels_, bitDepth_, (int)totalBytes);
  int result = fclose(file_);
  file_ = nullptr;
  return result;
}
