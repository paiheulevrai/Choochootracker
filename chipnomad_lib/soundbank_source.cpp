#include "soundbank_source.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <limits>
#include <set>
#include <stdexcept>

#include "external/miniz/miniz_tinfl.h"

namespace soundbank {
namespace {
thread_local LoadProgressCallback progressCallback = nullptr;
thread_local LoadStage progressStage = LoadStage::reading;
void reportStage(LoadStage stage) {
  progressStage = stage;
  if (progressCallback) progressCallback(stage);
}
}
LoadProgressScope::LoadProgressScope(LoadProgressCallback callback) : previous_(progressCallback) {
  progressCallback = callback;
}
LoadProgressScope::~LoadProgressScope() { progressCallback = previous_; }
LoadStageScope::LoadStageScope(LoadStage stage) : previous_(progressStage) { reportStage(stage); }
LoadStageScope::~LoadStageScope() { reportStage(previous_); }

bool isPlatformMetadata(const std::string& path) {
  for (const auto& part : std::filesystem::path(path)) {
    const auto name = part.string();
    if (name == "__MACOSX" || name.rfind("._", 0) == 0) return true;
  }
  return false;
}
void offlineYield() {
#ifdef __EMSCRIPTEN__
  static double previous = 0;
  double now = emscripten_get_now();
  if (now - previous >= 4) {
    // Asyncify re-enters this branch while rewinding. Updating the timestamp
    // before sleeping can skip the matching sleep and strand the main loop.
    emscripten_sleep(0);
    previous = emscripten_get_now();
  }
#endif
}
void require(bool c, const std::string& m) {
  if (!c) throw std::runtime_error(m);
}
uint16_t read16(const uint8_t* p, bool b) {
  return b ? uint16_t(p[0]) * 256 + p[1] : uint16_t(p[1]) * 256 + p[0];
}
uint32_t read32(const uint8_t* p, bool b) {
  return b ? uint32_t(read16(p, true)) * 65536 + read16(p + 2, true)
           : uint32_t(read16(p + 2)) * 65536 + read16(p);
}
static uint64_t u64(const uint8_t* p) { return uint64_t(read32(p + 4)) << 32 | read32(p); }
std::vector<uint8_t> Source::bytes(uint64_t o, size_t n, size_t limit) const {
  require(n <= limit, "Sound bank metadata/member exceeds memory limit");
  require(o <= size() && n <= size() - o, "Truncated sound bank");
  std::vector<uint8_t> result(n);
  read(o, result.data(), n);
  return result;
}
class FileSource final : public Source {
 public:
  explicit FileSource(FILE* file) : file_(file) {
    require(file_ != nullptr, "Cannot open sound bank source");
    if (fseeko(file_, 0, SEEK_END)) {
      fclose(file_);
      throw std::runtime_error("Cannot seek sound bank");
    }
    auto n = ftello(file_);
    if (n < 0) {
      fclose(file_);
      throw std::runtime_error("Cannot size sound bank");
    }
    size_ = uint64_t(n);
  }
  ~FileSource() override { fclose(file_); }
  uint64_t size() const override { return size_; }
  void read(uint64_t o, void* p, size_t n) const override {
    offlineYield();
    require(o <= size_ && n <= size_ - o && o <= uint64_t(std::numeric_limits<off_t>::max()),
            "Truncated sound bank source");
    require(!fseeko(file_, off_t(o), SEEK_SET) && fread(p, 1, n, file_) == n,
            "Cannot read sound bank source");
  }

 private:
  FILE* file_;
  uint64_t size_;
};
SourcePtr openFile(const std::string& p) {
  return std::make_shared<FileSource>(fopen(p.c_str(), "rb"));
}
class SliceSource final : public Source {
 public:
  SliceSource(SourcePtr s, uint64_t o, uint64_t n) : source_(s), offset_(o), size_(n) {}
  uint64_t size() const override { return size_; }
  void read(uint64_t o, void* p, size_t n) const override {
    offlineYield();
    require(o <= size_ && n <= size_ - o, "Truncated ZIP member");
    source_->read(offset_ + o, p, n);
  }

 private:
  SourcePtr source_;
  uint64_t offset_, size_;
};
std::string normalizedPath(std::string s) {
  std::replace(s.begin(), s.end(), '\\', '/');
  require(!s.empty() && s.size() <= 4096 && s[0] != '/' && s.find(':') == std::string::npos,
          "Unsafe sound bank path");
  std::vector<std::string> parts;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i < s.size()) require(uint8_t(s[i]) >= 32, "Unsafe sound bank path");
    if (i == s.size() || s[i] == '/') {
      auto p = s.substr(start, i - start);
      start = i + 1;
      if (p == "..") {
        require(!parts.empty(), "Sound bank path escapes root");
        parts.pop_back();
      } else if (!p.empty() && p != ".")
        parts.push_back(p);
    }
  }
  std::string r;
  for (auto& p : parts) {
    if (!r.empty()) r += '/';
    r += p;
  }
  require(!r.empty(), "Empty sound bank path");
  return r;
}
std::string extension(const std::string& p) {
  auto r = std::filesystem::path(p).extension().string();
  for (auto& c : r) c = char(std::tolower(uint8_t(c)));
  return r;
}
Archive::Archive(const std::string& path) : source_(openFile(path)) {
  require(source_->size() >= 22, "Truncated ZIP");
  auto n = std::min<uint64_t>(65557, source_->size());
  auto tail = source_->bytes(source_->size() - n, size_t(n));
  size_t e = tail.size();
  for (size_t i = tail.size() - 22;; --i) {
    if (read32(&tail[i]) == 0x06054b50 && i + 22 + read16(&tail[i + 20]) == tail.size()) {
      e = i;
      break;
    }
    if (!i) break;
  }
  require(e < tail.size(), "Missing ZIP directory");
  auto p = &tail[e];
  require(!read16(p + 4) && !read16(p + 6) && read16(p + 8) == read16(p + 10),
          "Split ZIP is unsupported");
  uint64_t count = read16(p + 10), length = read32(p + 12), offset = read32(p + 16),
           end = source_->size() - n + e;
  if (count == 65535 || length == 0xffffffffu || offset == 0xffffffffu) {
    require(end >= 20, "Truncated ZIP64 locator");
    auto loc = source_->bytes(end - 20, 20);
    require(
        read32(loc.data()) == 0x07064b50 && !read32(loc.data() + 4) && read32(loc.data() + 16) == 1,
        "Invalid ZIP64 locator");
    auto z = source_->bytes(u64(loc.data() + 8), 56);
    p = z.data();
    require(
        read32(p) == 0x06064b50 && !read32(p + 16) && !read32(p + 20) && u64(p + 24) == u64(p + 32),
        "Invalid ZIP64 directory");
    count = u64(p + 32);
    length = u64(p + 40);
    offset = u64(p + 48);
  }
  require(count <= 200000 && length <= 64 * 1024 * 1024 && offset <= end && length <= end - offset,
          "ZIP directory exceeds limits");
  auto cd = source_->bytes(offset, size_t(length), 64 * 1024 * 1024);
  size_t pos = 0;
  std::set<std::string> names;
  for (uint64_t i = 0; i < count; ++i) {
    require(pos <= cd.size() && cd.size() - pos >= 46, "Truncated ZIP directory entry");
    p = cd.data() + pos;
    require(read32(p) == 0x02014b50, "Invalid ZIP directory entry");
    auto ns = read16(p + 28), xs = read16(p + 30), cs = read16(p + 32);
    require(size_t(46) + ns + xs + cs <= cd.size() - pos, "Truncated ZIP filename");
    ZipEntry entry;
    entry.flags = read16(p + 8);
    entry.method = read16(p + 10);
    entry.crc = read32(p + 16);
    entry.compressed = read32(p + 20);
    entry.size = read32(p + 24);
    entry.offset = read32(p + 42);
    require(!read16(p + 34), "Split ZIP member unsupported");
    for (size_t x = 46 + ns; x + 4 <= size_t(46 + ns + xs);) {
      size_t sz = read16(p + x + 2);
      require(sz <= 46 + ns + xs - x - 4, "Invalid ZIP extra field");
      if (read16(p + x) == 1) {
        size_t q = x + 4;
        auto take = [&]() {
          require(q + 8 <= x + 4 + sz, "Truncated ZIP64 field");
          auto v = u64(p + q);
          q += 8;
          return v;
        };
        if (entry.size == 0xffffffffu) entry.size = take();
        if (entry.compressed == 0xffffffffu) entry.compressed = take();
        if (entry.offset == 0xffffffffu) entry.offset = take();
      }
      x += 4 + sz;
    }
    std::string raw(reinterpret_cast<const char*>(p + 46), ns);
    // Dropbox folder downloads include an empty root directory marker. It is
    // metadata only: never expose it as a member or accept absolute file paths.
    if (raw == "/" || raw == "./") {
      require(entry.size == 0 && entry.compressed == 0 && entry.method == 0,
              "Invalid ZIP root directory marker");
      pos += 46 + ns + xs + cs;
      continue;
    }
    entry.name = normalizedPath(raw);
    require(names.insert(entry.name).second, "Duplicate ZIP member path");
    require(entry.offset < offset && entry.compressed <= offset - entry.offset,
            "Invalid ZIP member extent");
    if (!raw.empty() && raw.back() != '/') entries_.push_back(entry);
    pos += 46 + ns + xs + cs;
  }
  require(pos == cd.size(), "Unexpected ZIP directory data");
}
static uint32_t crcUpdate(uint32_t crc, const uint8_t* p, size_t n) {
  static const auto table = []() {
    std::array<uint32_t, 256> t{};
    for (unsigned i = 0; i < 256; ++i) {
      auto c = i;
      for (int b = 0; b < 8; ++b) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
      t[i] = c;
    }
    return t;
  }();
  while (n--) crc = table[(crc ^ *p++) & 255] ^ (crc >> 8);
  return crc;
}
SourcePtr Archive::open(const std::string& name, uint64_t maxBytes) const {
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&](const ZipEntry& e) { return e.name == name; });
  require(it != entries_.end(), "Missing ZIP sample: " + name);
  const auto& e = *it;
  LoadStageScope progress(e.method == 8 ? LoadStage::unzipping : LoadStage::checking);
  require(!(e.flags & 1) && (e.method == 0 || e.method == 8), "Encrypted/unsupported ZIP member");
  require(e.size <= maxBytes, "ZIP member exceeds preparation limit");
  auto local = source_->bytes(e.offset, 30);
  require(read32(local.data()) == 0x04034b50 && read16(local.data() + 8) == e.method &&
              read16(local.data() + 6) == e.flags,
          "Invalid ZIP local header");
  auto nl = read16(local.data() + 26), xl = read16(local.data() + 28);
  auto nm = source_->bytes(e.offset + 30, nl);
  require(normalizedPath(std::string(nm.begin(), nm.end())) == name, "ZIP local filename mismatch");
  uint64_t offset = e.offset + 30 + nl + xl;
  require(offset <= source_->size() && e.compressed <= source_->size() - offset,
          "Truncated ZIP payload");
  auto input = std::make_shared<SliceSource>(source_, offset, e.compressed);
  std::unique_ptr<FILE, decltype(&fclose)> temporary(nullptr, fclose);
  if (e.method == 8) {
    temporary.reset(tmpfile());
    require(bool(temporary), "Cannot create temporary ZIP cache");
  }
  std::array<uint8_t, 32768> in{}, out{};
  uint64_t consumed = 0, produced = 0;
  uint32_t crc = ~0u;
  if (e.method == 0) {
    require(e.size == e.compressed, "Invalid stored ZIP size");
    while (consumed < e.size) {
      auto n = size_t(std::min<uint64_t>(in.size(), e.size - consumed));
      input->read(consumed, in.data(), n);
      crc = crcUpdate(crc, in.data(), n);
      consumed += n;
    }
    produced = consumed;
  } else {
    tinfl_decompressor decoder;
    tinfl_init(&decoder);
    size_t have = 0, pos = 0, op = 0;
    tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;
    while (status != TINFL_STATUS_DONE) {
      offlineYield();
      if (pos == have && consumed < e.compressed) {
        have = size_t(std::min<uint64_t>(in.size(), e.compressed - consumed));
        input->read(consumed, in.data(), have);
        consumed += have;
        pos = 0;
      }
      size_t ni = have - pos, no = out.size() - op;
      status = tinfl_decompress(&decoder, in.data() + pos, &ni, out.data(), out.data() + op, &no,
                                consumed < e.compressed ? TINFL_FLAG_HAS_MORE_INPUT : 0);
      pos += ni;
      require(status >= 0 && (ni || no || status == TINFL_STATUS_DONE),
              "Corrupt ZIP deflate stream");
      require(no <= e.size - produced, "ZIP decompression exceeds declared size");
      crc = crcUpdate(crc, out.data() + op, no);
      require(fwrite(out.data() + op, 1, no, temporary.get()) == no,
              "ZIP temporary cache write failed");
      produced += no;
      op = (op + no) % out.size();
    }
    require(consumed - (have - pos) == e.compressed, "Trailing ZIP deflate data");
  }
  require(produced == e.size && ~crc == e.crc, "ZIP checksum/size mismatch");
  if (temporary) return std::make_shared<FileSource>(temporary.release());
  return input;
}
// A worker reuses one central directory while resolving a collection. No sample
// payload is retained, and a changed file invalidates the directory immediately.
static const Archive& cachedArchive(const std::string& path) {
  static thread_local std::string priorPath;
  static thread_local uintmax_t priorSize = 0;
  static thread_local std::filesystem::file_time_type priorTime{};
  static thread_local std::unique_ptr<Archive> prior;
  auto size = std::filesystem::file_size(path);
  auto time = std::filesystem::last_write_time(path);
  if (!prior || path != priorPath || size != priorSize || time != priorTime) {
    prior = std::make_unique<Archive>(path);
    priorPath = path;
    priorSize = size;
    priorTime = time;
  }
  return *prior;
}
std::string Location::id() const { return path + (member.empty() ? "" : "::" + member); }
SourcePtr Location::open(uint64_t maxBytes) const {
  if (!member.empty()) return cachedArchive(path).open(member, maxBytes);
  auto s = openFile(path);
  require(s->size() <= maxBytes, "Sound bank source exceeds preparation limit");
  return s;
}
Location Location::relative(const std::string& ref) const {
  std::string s = ref;
  std::replace(s.begin(), s.end(), '\\', '/');
  require(!s.empty() && s[0] != '/' && s.find(':') == std::string::npos,
          "Absolute sample reference requires relocation");
  if (!member.empty())
    return {path,
            normalizedPath((std::filesystem::path(member).parent_path() / s).generic_string()),
            root};
  auto result = Location{
      (std::filesystem::path(path).parent_path() / s).lexically_normal().string(), "", root};
  if (!root.empty()) {
    auto rel = std::filesystem::weakly_canonical(result.path)
                   .lexically_relative(std::filesystem::weakly_canonical(root))
                   .generic_string();
    require(!rel.empty() && rel != ".." && rel.rfind("../", 0) != 0,
            "Sample reference escapes SoundBanks root");
  }
  return result;
}

Location Location::relocated() const {
  LoadStageScope progress(LoadStage::locating);
  namespace fs = std::filesystem;
  auto lower = [](std::string s) {
    for (auto& c : s) c = char(std::tolower(uint8_t(c)));
    return s;
  };
  auto suffixScore = [&](const std::string& a, const std::string& b) {
    std::vector<std::string> left, right;
    for (const auto& part : fs::path(a)) left.push_back(lower(part.string()));
    for (const auto& part : fs::path(b)) right.push_back(lower(part.string()));
    size_t score = 0;
    while (score < left.size() && score < right.size() &&
           left[left.size() - score - 1] == right[right.size() - score - 1]) ++score;
    return score;
  };
  if (!member.empty()) {
    const auto& archive = cachedArchive(path);
    for (const auto& e : archive.entries())
      if (e.name == member) return *this;
    std::vector<std::string> exact, basename;
    const auto wanted = lower(member), leaf = lower(fs::path(member).filename().string());
    for (const auto& e : archive.entries()) {
      if (lower(e.name) == wanted) exact.push_back(e.name);
      if (lower(fs::path(e.name).filename().string()) == leaf) basename.push_back(e.name);
    }
    if (exact.empty() && basename.size() > 1) {
      size_t best = 0;
      std::vector<std::string> ranked;
      for (const auto& name : basename) {
        auto score = suffixScore(name, member);
        if (score > best) { ranked.clear(); best = score; }
        if (score == best) ranked.push_back(name);
      }
      basename = std::move(ranked);
    }
    const auto& matches = exact.empty() ? basename : exact;
    require(matches.size() == 1, matches.empty() ? "Missing ZIP sample: " + member
                                                 : "Ambiguous ZIP sample reference: " + member);
    return {path, matches.front(), root};
  }
  std::error_code ec;
  if (fs::is_regular_file(path, ec)) return *this;
  // Repair case component by component before considering filename relocation.
  fs::path candidate;
  for (const auto& part : fs::path(path)) {
    auto next = candidate / part;
    if (fs::exists(next, ec)) {
      candidate = next;
      continue;
    }
    std::vector<fs::path> matches;
    for (fs::directory_iterator i(candidate.empty() ? fs::path(".") : candidate,
                                  fs::directory_options::skip_permission_denied, ec),
         end;
         !ec && i != end; i.increment(ec))
      if (lower(i->path().filename().string()) == lower(part.string()))
        matches.push_back(i->path());
    if (matches.size() != 1) {
      candidate.clear();
      break;
    }
    candidate = matches.front();
  }
  if (!candidate.empty() && fs::is_regular_file(candidate, ec))
    return {candidate.string(), "", root};
  // Prefer the nearest enclosing collection before searching the whole root.
  // Legacy EXS files often keep mappings and Samples in sibling directories;
  // unrelated collections may legitimately reuse the same sample basename.
  auto search = fs::path(path).parent_path();
  const auto boundary = fs::weakly_canonical(root.empty() ? search : fs::path(root));
  const auto leaf = lower(fs::path(path).filename().string());
  size_t count = 0;
  for (;;) {
    std::vector<fs::path> matches;
    size_t best = 0;
    ec.clear();
    for (fs::recursive_directory_iterator
             i(search, fs::directory_options::skip_permission_denied, ec),
         end;
         !ec && i != end; i.increment(ec)) {
      require(++count <= 200000, "Sample relocation search limit exceeded");
      if (i.depth() > 32 || i->is_symlink(ec) || isPlatformMetadata(i->path().string())) {
        i.disable_recursion_pending();
        continue;
      }
      if (i->is_regular_file(ec) && lower(i->path().filename().string()) == leaf) {
        auto score = suffixScore(i->path().string(), path);
        if (score > best) { matches.clear(); best = score; }
        if (score == best) matches.push_back(i->path());
      }
    }
    require(matches.size() < 2, "Ambiguous sample reference: " + path);
    if (matches.size() == 1) return {matches.front().string(), "", root};
    if (root.empty() || fs::weakly_canonical(search) == boundary) break;
    auto parent = search.parent_path();
    const auto relative = fs::weakly_canonical(parent).lexically_relative(boundary).generic_string();
    if (parent == search || relative.empty() || relative == ".." || relative.rfind("../", 0) == 0)
      break;
    search = parent;
  }
  throw std::runtime_error("Missing sample: " + path);
}
}  // namespace soundbank
