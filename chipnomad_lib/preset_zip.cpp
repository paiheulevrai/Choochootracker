#include "preset_zip.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include "external/miniz/miniz_tinfl.c"

namespace {
uint16_t u16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
uint32_t u32(const uint8_t* p) { return u16(p) | (uint32_t(u16(p + 2)) << 16); }
uint32_t crc32(const std::vector<uint8_t>& bytes) {
  uint32_t crc = ~0u;
  for (auto b : bytes) {
    crc ^= b;
    for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
}

bool readPresetFile(const std::string& path, std::vector<uint8_t>& bytes, std::string& error, size_t limit) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) { error = "Could not open preset file"; return false; }
  bool ok = !fseek(f, 0, SEEK_END);
  long size = ok ? ftell(f) : -1;
  ok = size >= 0 && size_t(size) <= limit && !fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> staged;
  if (ok) {
    staged.resize(size);
    ok = fread(staged.data(), 1, staged.size(), f) == staged.size() && !ferror(f);
  }
  fclose(f);
  if (!ok) { error = "Preset file unreadable or too large"; return false; }
  bytes.swap(staged); error.clear(); return true;
}

bool PresetZip::safePath(const std::string& path) {
  if (path.empty() || path.size() > 512 || path.front() == '/') return false;
  size_t start = 0;
  for (size_t i = 0; i <= path.size(); ++i) {
    if (i < path.size() && (uint8_t(path[i]) < 32 || path[i] == '\\' || path[i] == ':')) return false;
    if (i == path.size() || path[i] == '/') {
      const auto component = path.substr(start, i - start);
      if (component == "." || component == ".." || (component.empty() && i != path.size())) return false;
      start = i + 1;
    }
  }
  return true;
}

bool PresetZip::open(const std::string& path, std::string& error) {
  bytes_.clear(); entries_.clear();
  std::vector<uint8_t> data;
  if (!readPresetFile(path, data, error, 32 * 1024 * 1024)) return false;
  auto fail = [&]() { error = "Invalid or unsupported preset ZIP"; return false; };
  if (data.size() < 22) return fail();
  size_t eocd = data.size();
  const size_t start = data.size() > 65557 ? data.size() - 65557 : 0;
  for (size_t i = data.size() - 22;; --i) {
    if (u32(&data[i]) == 0x06054b50 && i + 22 + u16(&data[i + 20]) == data.size()) { eocd = i; break; }
    if (i == start) break;
  }
  if (eocd == data.size()) return fail();
  const auto* end = &data[eocd];
  const uint16_t count = u16(end + 10);
  const uint32_t central = u32(end + 16), centralSize = u32(end + 12);
  if (u16(end + 4) || u16(end + 6) || count != u16(end + 8) || count > 8192 ||
      uint64_t(central) + centralSize != eocd) return fail();
  std::set<std::string> names;
  std::vector<Entry> entries;
  size_t pos = central;
  uint64_t total = 0;
  for (int i = 0; i < count; ++i) {
    if (pos + 46 > eocd || u32(&data[pos]) != 0x02014b50) return fail();
    const auto* h = &data[pos];
    const size_t n = u16(h + 28), extra = u16(h + 30), comment = u16(h + 32);
    if (pos + 46 + n + extra + comment > eocd || u16(h + 34)) return fail();
    Entry e{std::string(reinterpret_cast<const char*>(h + 46), n), u32(h + 42), u32(h + 20),
            u32(h + 24), u32(h + 16), u16(h + 10), u16(h + 8), false};
    if (!safePath(e.name) || !names.insert(e.name).second || (e.flags & ~(8 | 0x800 | 6)) ||
        (e.method != 0 && e.method != 8) || ((u32(h + 38) >> 16) & 0170000) == 0120000 ||
        e.size > 1024 * 1024 || (total += e.size) > 64 * 1024 * 1024 || uint64_t(e.offset) + 30 > central)
      return fail();
    e.directory = e.name.back() == '/';
    const auto* local = &data[e.offset];
    const size_t localName = u16(local + 26), localExtra = u16(local + 28);
    const uint64_t payload = uint64_t(e.offset) + 30 + localName + localExtra;
    if (u32(local) != 0x04034b50 || u16(local + 6) != e.flags || u16(local + 8) != e.method ||
        localName != n || payload + e.compressed > central ||
        memcmp(local + 30, e.name.data(), n) ||
        (!(e.flags & 8) && (u32(local + 14) != e.crc || u32(local + 18) != e.compressed || u32(local + 22) != e.size)) ||
        (e.method == 0 && e.compressed != e.size)) return fail();
    e.offset = uint32_t(payload);
    entries.push_back(std::move(e));
    pos += 46 + n + extra + comment;
  }
  if (pos != eocd) return fail();
  bytes_.swap(data); entries_.swap(entries); error.clear(); return true;
}

bool PresetZip::read(const std::string& name, std::vector<uint8_t>& output, std::string& error) const {
  const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.name == name; });
  if (it == entries_.end() || it->directory) { error = "Preset not found in ZIP"; return false; }
  const auto& e = *it;
  std::vector<uint8_t> data(std::max(1u, e.size));
  if (e.method == 0) std::copy_n(bytes_.data() + e.offset, e.size, data.data());
  else {
    tinfl_decompressor state; tinfl_init(&state);
    size_t in = e.compressed, out = e.size;
    const auto status = tinfl_decompress(&state, bytes_.data() + e.offset, &in,
      data.data(), data.data(), &out, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (status != TINFL_STATUS_DONE || in != e.compressed || out != e.size) {
      error = "Invalid compressed preset"; return false;
    }
  }
  data.resize(e.size);
  if (crc32(data) != e.crc) { error = "Preset ZIP checksum mismatch"; return false; }
  output.swap(data); error.clear(); return true;
}
