// Streaming WAV writer for audio input recording (Phase 3 of the audio input
// recording feature). Mirrors ExporterWAV's open/write/close pattern
// (chipnomad_lib/export/export_wav.cpp) but writes int16 PCM directly from
// the recorder's writer thread.
//
// Threading contract:
// - open()/close() run on the UI thread only.
// - write() runs on the recorder writer thread only (never the audio
//   callback). Realtime-safe in the sense that matters for the writer: no
//   allocation after open(), only fwrite + atomic counter bumps. A failed
//   fwrite drops the block and bumps a counter instead of blocking.
#pragma once

#include <atomic>
#include <stdint.h>
#include <stdio.h>

class RecordingWavWriter {
 public:
  RecordingWavWriter() = default;
  ~RecordingWavWriter() { if (file_) close(); }

  // Opens the WAV file with a placeholder data-size of 0. Returns 0 on
  // success, non-zero on failure (fopen failed or already open).
  int open(const char* path, int sampleRate, int channels, int bitDepth);

  // Appends PCM16 interleaved samples. Returns 0 on success, non-zero when
  // the block was dropped (not open, or fwrite failed).
  int write(const int16_t* samples, int stereoSampleCount);

  // Patches the WAV header with the actual data size and closes the file.
  // Returns 0 on success.
  int close(void);

  uint64_t totalSamples() const { return totalSamples_.load(std::memory_order_acquire); }
  uint64_t droppedSamples() const { return droppedSamples_.load(std::memory_order_acquire); }

 private:
  FILE* file_ = nullptr;
  int sampleRate_ = 0, channels_ = 0, bitDepth_ = 0;
  std::atomic<uint64_t> totalSamples_{0};
  std::atomic<uint64_t> droppedSamples_{0};
};
