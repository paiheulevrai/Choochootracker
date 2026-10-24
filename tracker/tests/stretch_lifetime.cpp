// Standalone allocation regression; do not link its allocation hooks into the app.
// c++ -std=c++17 -O1 -Ichipnomad_lib/external/signalsmith \
//   tracker/tests/stretch_lifetime.cpp chipnomad_lib/synth/stretch_processor.cpp -o /tmp/stretch-lifetime
#include "../../chipnomad_lib/synth/stretch_processor.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
struct alignas(std::max_align_t) Allocation { std::size_t bytes; };
std::size_t liveBytes = 0;
}
void* operator new(std::size_t bytes) {
  auto* p = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + bytes));
  if (!p) throw std::bad_alloc();
  p->bytes = bytes; liveBytes += bytes;
  return p + 1;
}
void operator delete(void* memory) noexcept {
  if (!memory) return;
  auto* p = static_cast<Allocation*>(memory) - 1;
  liveBytes -= p->bytes; std::free(p);
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

int main() {
  const auto baseline = liveBytes;
  for (int iteration = 0; iteration < 32; ++iteration) {
    {
      StretchProcessor processor;
      processor.init(48000, false);
      processor.init(96000, true);
      processor.reset();
      if (liveBytes <= baseline) return 2; // Ensure the allocating path was tested.
    }
    if (liveBytes != baseline) {
      std::fprintf(stderr, "Stretch lifetime leaked %zu bytes after cycle %d\n",
                   liveBytes - baseline, iteration + 1);
      return 1;
    }
  }
  std::puts("Stretch lifetime passed: 32 create/reinitialize/destroy cycles, zero retained allocation bytes");
}
