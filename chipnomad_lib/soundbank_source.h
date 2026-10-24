#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace soundbank {
// Cooperative browser preparation; native builds do their work on a worker.
// Never call this, or any Source API, from a realtime callback.
void offlineYield();
enum class LoadStage { reading, locating, unzipping, checking, loading };
using LoadProgressCallback = void (*)(LoadStage);
// Worker-local observers; never used by the audio callback.
class LoadProgressScope {
 public:
  explicit LoadProgressScope(LoadProgressCallback callback);
  ~LoadProgressScope();
  LoadProgressScope(const LoadProgressScope&) = delete;
 private:
  LoadProgressCallback previous_;
};
class LoadStageScope {
 public:
  explicit LoadStageScope(LoadStage stage);
  ~LoadStageScope();
  LoadStageScope(const LoadStageScope&) = delete;
 private:
  LoadStage previous_;
};
// All APIs in this file are offline only. Source is random access; large audio
// payloads never need to be read merely to enumerate a bank.
class Source {
 public:
  virtual ~Source() = default;
  virtual uint64_t size() const = 0;
  virtual void read(uint64_t offset, void* output, size_t count) const = 0;
  std::vector<uint8_t> bytes(uint64_t offset, size_t count, size_t limit = 16 * 1024 * 1024) const;
};
using SourcePtr = std::shared_ptr<Source>;
SourcePtr openFile(const std::string& path);
struct ZipEntry {
  std::string name;
  uint64_t offset = 0, compressed = 0, size = 0;
  uint32_t crc = 0;
  uint16_t method = 0, flags = 0;
};
class Archive {
 public:
  explicit Archive(const std::string& path);
  const std::vector<ZipEntry>& entries() const { return entries_; }
  // Deflated members use bounded-memory temporary disk storage, deleted on close.
  SourcePtr open(const std::string& member, uint64_t maxBytes = 2ull * 1024 * 1024 * 1024) const;

 private:
  SourcePtr source_;
  std::vector<ZipEntry> entries_;
};
struct Location {
  std::string path, member;
  std::string root;  // optional permitted library root for relocated sample lookup
  std::string id() const;
  SourcePtr open(uint64_t maxBytes = 2ull * 1024 * 1024 * 1024) const;
  Location relative(const std::string& reference) const;
  Location relocated() const;  // metadata-only, refuses ambiguous filename matches
};
std::string normalizedPath(std::string path);  // rejects absolute/escaping paths
std::string extension(const std::string& path);
bool isPlatformMetadata(const std::string& path);
uint16_t read16(const uint8_t* p, bool big = false);
uint32_t read32(const uint8_t* p, bool big = false);
void require(bool condition, const std::string& message);
}  // namespace soundbank
