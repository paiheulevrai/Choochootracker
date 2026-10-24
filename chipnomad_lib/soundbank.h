#pragma once
#include <array>
#include <map>
#include <memory>

#include "soundbank_source.h"

namespace soundbank {
constexpr size_t maxZones = 8192;
enum class Loop { off, continuous, sustain, oneShot };
struct EnvelopeSettings {
  float attack = 0, hold = 0, decay = 0, sustain = 1, release = .01f, delay = 0;
};
struct Asset {
  Location source;
  uint64_t offset = 0, length = 0;  // SF PCM/Vorbis range; 0 length means audio file
  uint32_t sampleRate = 0;
  bool vorbis = false;
  int root = 60;
  float tune = 0;
  uint32_t loopStart = 0, loopEnd = 0;
  SourcePtr reader;  // parser-owned random-access SF data; never enters a voice
  std::string identity() const;
};
struct Zone {
  size_t asset = 0;
  int loKey = 0, hiKey = 127, loVel = 0, hiVel = 127, root = 60;
  float tune = 0, gain = 1, pan = 0, keytrack = 100, velocityTrack = 100;
  uint32_t start = 0, end = 0, loopStart = 0, loopEnd = 0;
  Loop loop = Loop::off;
  EnvelopeSettings envelope;
  float cutoff = 20000, resonance = 0;
  int group = 0, offBy = 0, sequenceLength = 1, sequencePosition = 1;
  int fadeInLo = 0, fadeInHi = 0, fadeOutLo = 127, fadeOutHi = 127;
  float randomLow = 0, randomHigh = 1;
  float pitchRandom = 0, ampRandom = 0, offTime = .006f;
  std::array<float, 128> velocityCurve{};
  bool customVelocityCurve = false;
  bool releaseTrigger = false;
  bool reverse = false, rootExplicit = false, loopExplicit = false;
};
struct Preset {
  std::string id, name;
  std::vector<Zone> zones;
  std::vector<std::string> warnings;
};
struct Bank {
  Location source;
  std::string name, format;
  std::vector<Asset> assets;
  std::vector<Preset> presets;
  std::vector<std::string> warnings;
};
struct Audio {
  std::vector<int16_t> pcm;
  std::vector<std::string> warnings;
  uint32_t sampleRate = 0;
  uint8_t channels = 1;
  int root = 60;
  float tune = 0;
  uint32_t loopStart = 0, loopEnd = 0;
  uint32_t frames() const { return uint32_t(pcm.size() / channels); }
};
struct PreparedZone {
  Zone zone;
  std::shared_ptr<const Audio> audio;
};
struct PreparedPreset {
  std::string id, name;
  std::vector<PreparedZone> zones;
  std::array<std::vector<uint16_t>, 128> keys;
  std::vector<std::string> warnings;
  size_t bytes = 0;
};
class SampleCache {
 public:
  explicit SampleCache(size_t limit = 64 * 1024 * 1024) : limit_(limit) {}
  std::shared_ptr<const PreparedPreset> prepare(const Bank&, const std::string& presetId);
  size_t bytes() const;
  size_t limit() const { return limit_; }
  void trim();  // only unpinned audio; caller runs outside callback
 private:
  size_t limit_;
  std::map<std::string, std::shared_ptr<const Audio>> audio_;
};
Bank parseBank(const Location& source);
std::shared_ptr<Audio> decodeAudio(const Asset&, size_t maxBytes);
void parseSoundFont(Bank&, const SourcePtr&);
void parseSFZ(Bank&, const SourcePtr&);
void parseEXS(Bank&, const SourcePtr&);
}  // namespace soundbank
