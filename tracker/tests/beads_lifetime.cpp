// Standalone: global allocation hooks must not enter the application/test runner.
#include "chipnomad_lib.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>

namespace {
struct alignas(std::max_align_t) Allocation { size_t bytes; };
size_t beadsBytes = 0;
bool measureBeads = false;
thread_local bool audioThread = false;
std::atomic<int> audioAllocations{0}, audioFrees{0}, liveBeads{0}, failBeads{0};
std::atomic<int> beadAllocations{0};
void require(bool condition, const char* message) {
  if (!condition) { std::fprintf(stderr, "Beads lifetime: %s\n", message); std::abort(); }
}
void render(ChipNomadState* state) {
  float buffer[2048]{};
  audioThread = true;
  chipnomadRender(state, buffer, 1024);
  audioThread = false;
  for (float x : buffer) require(std::isfinite(x), "non-finite output");
}
void settle(ChipNomadState* state) {
  for (int i = 0; i < 4; ++i) render(state);
  chipnomadGetPlaybackStatus(state); // Producer reclaims retired instances.
}
void selectAll(Project& p, int module) {
  for (auto& track : p.trackInserts) for (auto& slot : track) insertSelect(&slot, module);
  for (auto& slot : p.masterInserts) insertSelect(&slot, module);
}
}
void* operator new(size_t bytes) {
  if (audioThread) ++audioAllocations;
  if (measureBeads) beadsBytes = bytes;
  if (bytes == beadsBytes && failBeads.load() > 0) { --failBeads; throw std::bad_alloc(); }
  auto* p = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + bytes));
  if (!p) throw std::bad_alloc();
  p->bytes = bytes;
  if (bytes == beadsBytes) { ++liveBeads; ++beadAllocations; }
  return p + 1;
}
void operator delete(void* memory) noexcept {
  if (!memory) return;
  if (audioThread) ++audioFrees;
  auto* p = static_cast<Allocation*>(memory) - 1;
  if (p->bytes == beadsBytes) --liveBeads;
  std::free(p);
}
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, size_t) noexcept { ::operator delete(p); }
void* operator new(size_t bytes, const std::nothrow_t&) noexcept {
  try { return ::operator new(bytes); } catch (...) { return nullptr; }
}
void* operator new[](size_t bytes, const std::nothrow_t& tag) noexcept { return ::operator new(bytes, tag); }
void operator delete(void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }

int main() {
  measureBeads = true;
  { BeadsInsert probe(48000); require(probe.ready(), "probe allocation"); }
  measureBeads = false;
  require(beadsBytes > 786432 && liveBeads == 0, "DSP allocation measurement");
  auto* state = chipnomadCreate();
  require(state != nullptr, "engine creation");
  state->project.tracksCount = PROJECT_MAX_TRACKS;
  state->project.tickRate = 50;
  chipnomadInitChips(state, 48000, nullptr);
  require(chipnomadReserveRenderBuffers(state, 1024) == 0, "reserve callback buffers");
  require(liveBeads == 0, "unused engine allocated Beads");
  settle(state);

  insertSelect(&state->project.trackInserts[0][0], insertBeads);
  require(chipnomadQueueProjectRefresh(state), "queue single instance");
  require(liveBeads == 1, "single instance not prepared on producer");
  const int initialAllocations = beadAllocations;
  for (int i = 0; i < 100; ++i) {
    insertEdit(&state->project.trackInserts[0][0], 0, i);
    require(chipnomadQueueProjectRefresh(state), "coalesced refresh");
  }
  require(beadAllocations == initialAllocations, "coalesced parameter edits reallocated DSP");
  chipnomadDiscardQueuedProject(state);
  require(liveBeads == 0, "discard leaked prepared instance");

  selectAll(state->project, insertBeads);
  require(chipnomadQueueProjectRefresh(state), "queue all instances");
  require(liveBeads == BeadsPool::capacity, "all track/master slots must work simultaneously");
  settle(state);
  state->project.masterInserts[0].values[6] |= 128;
  require(chipnomadQueueProjectRefresh(state), "freeze refresh");
  settle(state);
  require(liveBeads == BeadsPool::capacity, "freeze lost state");
  chipnomadQueuePlaybackStop(state);
  settle(state);
  require(liveBeads == BeadsPool::capacity, "stop lost selected instances");

  // Reinitialization must retire queued as well as attached instances.
  require(chipnomadQueueProjectRefresh(state), "refresh before rate change");
  chipnomadInitChips(state, 44100, nullptr);
  require(liveBeads == BeadsPool::capacity, "rate change leaked instances");
  settle(state);
  selectAll(state->project, insertOff);
  require(chipnomadQueueProjectRefresh(state), "remove all instances");
  settle(state);
  require(liveBeads == 0, "removal failed to reclaim instances");

  insertSelect(&state->project.trackInserts[0][0], insertBeads);
  insertSelect(&state->project.trackInserts[7][1], insertBeads);
  require(chipnomadQueueProjectRefresh(state), "prepare muted/hidden regression");
  settle(state);
  uint8_t enabled[PROJECT_MAX_TRACKS];
  std::fill_n(enabled, PROJECT_MAX_TRACKS, 1);
  enabled[0] = 0;
  require(chipnomadQueueTrackEnabled(state, enabled), "mute track");
  settle(state);
  state->project.tracksCount = 1;
  selectAll(state->project, insertOff);
  require(chipnomadQueueProjectRefresh(state), "remove on muted/hidden tracks");
  settle(state);
  require(liveBeads == 0, "muted/hidden removal retained instances");
  enabled[0] = 1;
  chipnomadQueueTrackEnabled(state, enabled);
  state->project.tracksCount = PROJECT_MAX_TRACKS;

  // A failed Beads allocation must leave other effects and snapshots usable.
  failBeads = 1;
  insertSelect(&state->project.trackInserts[0][0], insertBeads);
  insertSelect(&state->project.trackInserts[0][1], insertDistortion);
  require(chipnomadQueueProjectRefresh(state), "allocation failure rejected project");
  require(failBeads == 0 && liveBeads == 0, "failure injection missed DSP");
  settle(state);
  require(state->insertChains[0]->ready(), "Beads failure disabled chain");
  require(state->audioProject.trackInserts[0][1].module == insertDistortion, "other edit was lost");
  require(chipnomadQueueProjectRefresh(state), "allocation retry");
  settle(state);
  require(liveBeads == 1, "later refresh did not recover Beads");

  // Exercise the actual snapshot handoff concurrently with collection/reuse.
  std::atomic<bool> done{false};
  std::thread consumer([&] { while (!done.load()) render(state); });
  for (int i = 0; i < 300; ++i) {
    selectAll(state->project, (i % 3) ? insertBeads : insertOff);
    require(chipnomadQueueProjectRefresh(state), "concurrent refresh");
    if (i % 7 == 0) chipnomadDiscardQueuedProject(state);
    chipnomadGetPlaybackStatus(state);
    require(liveBeads <= BeadsPool::capacity, "pool exceeded bound");
  }
  done = true;
  consumer.join();
  selectAll(state->project, insertOff);
  require(chipnomadQueueProjectRefresh(state), "final removal");
  settle(state);
  require(liveBeads == 0, "concurrent edits leaked instances");
  // Destroy with both audio-owned and unconsumed snapshot leases.
  insertSelect(&state->project.masterInserts[0], insertBeads);
  chipnomadInitChips(state, 48000, nullptr);
  insertSelect(&state->project.masterInserts[1], insertBeads);
  require(chipnomadQueueProjectRefresh(state), "pending shutdown refresh");
  chipnomadDestroy(state);
  require(liveBeads == 0, "shutdown leaked instances");
  require(audioAllocations == 0 && audioFrees == 0, "audio thread allocated or freed heap storage");
  std::printf("Beads lifetime passed: %zu bytes/instance; 0 unused, %d simultaneous; "
              "300 concurrent edits; zero audio new/delete calls; zero retained instances\n",
              beadsBytes, BeadsPool::capacity);
}
