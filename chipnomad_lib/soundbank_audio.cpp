#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>

#include "soundbank.h"
#define STB_VORBIS_NO_STDIO
#include "external/stb/stb_vorbis.c"

namespace soundbank {
std::shared_ptr<Audio> decodeAudio(const Asset& asset, size_t maxBytes) {
  auto s = asset.reader ? asset.reader : asset.source.open();
  auto a = std::make_shared<Audio>();
  a->sampleRate = asset.sampleRate;
  a->root = asset.root;
  a->tune = asset.tune;
  a->loopStart = asset.loopStart;
  a->loopEnd = asset.loopEnd;
  if (!asset.length &&
      extension(asset.source.member.empty() ? asset.source.path : asset.source.member) == ".ogg") {
    Asset compressed = asset;
    compressed.length = s->size();
    compressed.vorbis = true;
    compressed.reader = s;
    return decodeAudio(compressed, maxBytes);
  }
  if (asset.length) {
    require(asset.offset <= s->size() && asset.length <= s->size() - asset.offset,
            "Truncated SoundFont sample");
    if (asset.vorbis) {
      auto data = s->bytes(asset.offset, size_t(asset.length), maxBytes);
      require(data.size() <= size_t(INT_MAX), "Vorbis input too large");
      require(data.size() < 36 || memcmp(data.data() + 28, "OpusHead", 8),
              "Ogg Opus is unsupported; Vorbis is supported");
      int error = 0;
      std::unique_ptr<stb_vorbis, decltype(&stb_vorbis_close)> v(
          stb_vorbis_open_memory(data.data(), int(data.size()), &error, nullptr), stb_vorbis_close);
      require(bool(v), "Invalid Ogg Vorbis sample (decoder " + std::to_string(error) + ")");
      auto info = stb_vorbis_get_info(v.get());
      require(!asset.reader || !asset.sampleRate || info.channels == 1,
              "SF3 compressed samples must be mono");
      auto frames = stb_vorbis_stream_length_in_samples(v.get());
      require((info.channels == 1 || info.channels == 2) && frames &&
                  uint64_t(frames) * info.channels * 2 <= maxBytes,
              "Vorbis decoded sample exceeds cache limit");
      a->channels = info.channels;
      a->sampleRate = info.sample_rate;
      a->pcm.resize(size_t(frames) * a->channels);
      size_t done = 0;
      while (done < frames) {
        offlineYield();
        auto n = stb_vorbis_get_samples_short_interleaved(
            v.get(), a->channels, a->pcm.data() + done * a->channels,
            int(std::min<size_t>((frames - done) * a->channels, 32768)));
        require(n > 0, "Truncated SF3 audio");
        done += n;
      }
    } else {
      require(asset.length % 2 == 0 && asset.length <= maxBytes,
              "SoundFont sample exceeds cache limit");
      auto data = s->bytes(asset.offset, size_t(asset.length), maxBytes);
      a->pcm.resize(data.size() / 2);
      for (size_t i = 0; i < a->pcm.size(); ++i) a->pcm[i] = int16_t(read16(data.data() + i * 2));
    }
    require(a->sampleRate >= 1000 && a->sampleRate <= 384000, "Invalid sample rate");
    return a;
  }
  auto h = s->bytes(0, 12);
  bool wav = !memcmp(h.data(), "RIFF", 4) && !memcmp(h.data() + 8, "WAVE", 4);
  bool aiff = !memcmp(h.data(), "FORM", 4) &&
              (!memcmp(h.data() + 8, "AIFF", 4) || !memcmp(h.data() + 8, "AIFC", 4));
  require(wav || aiff, "Unsupported audio asset (expected WAV/AIFF)");
  bool big = aiff, pcmBig = aiff;
  uint16_t bits = 0, encoding = 1, channels = 0;
  uint64_t dataOffset = 0, dataSize = 0;
  uint32_t declaredFrames = 0;
  std::map<int, uint32_t> markers;
  int beginMarker = 0, endMarker = 0, loopMode = 0;
  uint64_t end = uint64_t(read32(h.data() + 4, big)) + 8;
  require(end <= s->size() && end >= 12, "Invalid audio container length");
  for (uint64_t pos = 12; pos + 8 <= end;) {
    auto ch = s->bytes(pos, 8);
    uint32_t len = read32(ch.data() + 4, big);
    uint64_t start = pos + 8;
    require(len <= end - start, "Truncated audio chunk");
    auto is = [&](const char* id) { return !memcmp(ch.data(), id, 4); };
    if (wav && is("fmt ")) {
      auto d = s->bytes(start, len, 1024);
      require(d.size() >= 16, "Invalid WAV format");
      encoding = read16(d.data());
      channels = read16(d.data() + 2);
      a->sampleRate = read32(d.data() + 4);
      bits = read16(d.data() + 14);
      if (encoding == 0xfffe) {
        require(d.size() >= 40 && read16(d.data() + 18) == bits,
                "Unsupported WAV extensible format");
        encoding = read16(d.data() + 24);
      }
    } else if (wav && is("data")) {
      dataOffset = start;
      dataSize = len;
    } else if (wav && is("smpl")) {
      auto d = s->bytes(start, len, 65536);
      require(d.size() >= 36, "Invalid WAV smpl chunk");
      a->root = int(read32(d.data() + 12));
      a->tune = float(read32(d.data() + 16)) * 100.0f / 4294967296.0f;
      if (read32(d.data() + 28)) {
        require(d.size() >= 60, "Truncated WAV loop");
        const auto type = read32(d.data() + 40);
        if (type >= 32) {
          // RIFF reserves these for sampler-specific semantics. The PCM is still
          // usable; do not invent a forward loop from an unknown vendor mode.
          a->loopStart = a->loopEnd = 0;
          a->warnings.push_back("WAV sampler loop type " + std::to_string(type) +
                                " ignored; sample plays without this loop");
        } else {
          require(type == 0, type <= 2 ? "Unsupported WAV alternating/backward loop"
                                      : "Unsupported reserved WAV loop type");
          a->loopStart = read32(d.data() + 44);
          auto last = read32(d.data() + 48);
          require(last < UINT32_MAX, "Invalid WAV loop end");
          a->loopEnd = last + 1;
        }
      }
    } else if (aiff && is("COMM")) {
      auto d = s->bytes(start, len, 1024);
      require(d.size() >= 18, "Invalid AIFF COMM");
      channels = read16(d.data(), true);
      declaredFrames = read32(d.data() + 2, true);
      bits = read16(d.data() + 6, true);
      uint16_t exp = read16(d.data() + 8, true);
      uint64_t mant = uint64_t(read32(d.data() + 10, true)) << 32 | read32(d.data() + 14, true);
      double rate = std::ldexp(double(mant), int(exp & 32767) - 16383 - 63);
      require(!(exp & 32768) && std::isfinite(rate) && rate >= 1000 && rate <= 384000,
              "Invalid AIFF rate");
      a->sampleRate = uint32_t(std::lround(rate));
      if (!memcmp(h.data() + 8, "AIFC", 4)) {
        require(d.size() >= 22, "Invalid AIFC compression");
        if (!memcmp(d.data() + 18, "sowt", 4))
          pcmBig = false;
        else
          require(!memcmp(d.data() + 18, "NONE", 4) || !memcmp(d.data() + 18, "twos", 4),
                  "Unsupported compressed AIFC");
      }
    } else if (aiff && is("SSND")) {
      require(len >= 8, "Invalid AIFF SSND");
      auto d = s->bytes(start, 8);
      auto skip = read32(d.data(), true);
      require(skip <= len - 8, "Invalid AIFF offset");
      dataOffset = start + 8 + skip;
      dataSize = len - 8 - skip;
    } else if (aiff && is("INST")) {
      auto d = s->bytes(start, len, 1024);
      require(d.size() >= 20, "Invalid AIFF INST");
      a->root = d[0];
      a->tune = int8_t(d[1]);
      loopMode = read16(d.data() + 8, true);
      beginMarker = read16(d.data() + 10, true);
      endMarker = read16(d.data() + 12, true);
      require(loopMode <= 1, "Unsupported AIFF ping-pong loop");
    } else if (aiff && is("MARK")) {
      auto d = s->bytes(start, len, 1024 * 1024);
      require(d.size() >= 2, "Invalid AIFF MARK");
      size_t q = 2;
      for (int i = 0; i < read16(d.data(), true); ++i) {
        require(q + 7 <= d.size(), "Truncated AIFF marker");
        int id = read16(d.data() + q, true);
        markers[id] = read32(d.data() + q + 2, true);
        size_t name = d[q + 6];
        q += 6 + ((name + 2) & ~size_t(1));
        require(q <= d.size(), "Truncated AIFF marker name");
      }
    }
    pos = start + len + (len & 1);
    if (wav && (len & 1) && start + len < end) {
      auto plausible = [&](uint64_t offset) {
        if (offset + 8 > end) return false;
        auto next = s->bytes(offset, 8);
        for (int i = 0; i < 4; ++i)
          if (next[i] < 32 || next[i] > 126) return false;
        return read32(next.data() + 4) <= end - offset - 8;
      };
      if (!plausible(pos) && plausible(start + len)) {
        pos = start + len;
        a->warnings.push_back("WAV omitted RIFF alignment padding");
      }
    }
  }
  require((channels == 1 || channels == 2) &&
              (bits == 8 || bits == 16 || bits == 24 || bits == 32) &&
              (encoding == 1 || (wav && encoding == 3 && bits == 32)) && a->sampleRate >= 1000 &&
              a->sampleRate <= 384000 && dataOffset,
          "Unsupported or missing audio format/data");
  size_t width = bits / 8;
  const size_t frameBytes = channels * width;
  // A few PCM WAV writers include the RIFF alignment byte in data's size.
  // Recognize only a single zero pad after an odd number of complete frames.
  if (wav && encoding == 1 && (frameBytes & 1) && !(dataSize & 1) &&
      dataSize % frameBytes == 1 && s->bytes(dataOffset + dataSize - 1, 1)[0] == 0) {
    --dataSize;
    a->warnings.push_back("WAV data size included alignment padding");
  }
  uint64_t frames = dataSize / (channels * width);
  require(dataSize % (channels * width) == 0, "Audio asset contains a partial PCM frame");
  require(frames > 0 && frames <= UINT32_MAX, "Invalid audio frame count");
  require(frames * channels * 2 <= maxBytes, "Decoded sample exceeds remaining PCM cache budget");
  if (aiff) require(frames == declaredFrames, "AIFF frame count mismatch");
  a->channels = uint8_t(channels);
  a->pcm.resize(size_t(frames) * channels);
  std::vector<uint8_t> block(16384 * width);
  for (size_t pos = 0; pos < a->pcm.size();) {
    size_t count = std::min<size_t>(16384, a->pcm.size() - pos);
    s->read(dataOffset + pos * width, block.data(), count * width);
    for (size_t i = 0; i < count; ++i) {
      auto p = block.data() + i * width;
      int32_t value = 0;
      if (encoding == 3) {
        uint32_t raw = read32(p);
        float f;
        memcpy(&f, &raw, 4);
        require(std::isfinite(f), "Nonfinite WAV sample");
        value = int32_t(std::lround(std::clamp(f, -1.f, 1.f) * 32767));
      } else if (bits == 8)
        value = (wav ? int(p[0]) - 128 : int(int8_t(p[0]))) * 256;
      else if (bits == 16)
        value = int16_t(read16(p, pcmBig));
      else if (bits == 24) {
        uint32_t raw = pcmBig ? uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2]
                              : uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0];
        value = int16_t(raw >> 8);
      } else
        value = int32_t(read32(p, pcmBig)) / 65536;
      a->pcm[pos + i] = int16_t(value);
    }
    pos += count;
  }
  if (loopMode) {
    require(markers.count(beginMarker) && markers.count(endMarker), "Missing AIFF loop marker");
    a->loopStart = markers[beginMarker];
    a->loopEnd = markers[endMarker];
  }
  // Some WAV writers store an exclusive loop end at the sample count instead
  // of RIFF's inclusive final sample. Accept that exact boundary only: never
  // extend the PCM or clamp an otherwise out-of-range loop into validity.
  if (wav && a->loopStart < frames && uint64_t(a->loopEnd) == frames + 1) {
    a->loopEnd = uint32_t(frames);
    a->warnings.push_back("WAV loop end corrected to sample boundary");
  }
  require(a->root >= 0 && a->root <= 127, "Invalid audio root note");
  require(a->loopStart <= a->loopEnd && a->loopEnd <= frames,
          "Audio loop outside sample range");
  return a;
}
}  // namespace soundbank
