/* Standalone production-backend integration check, from tracker:
g++ -std=c++17 -DPORTMASTER_BUILD -ffunction-sections -fdata-sections \
  -I../chipnomad_lib -Isrc -Isrc/corelib -Iplatforms/shared \
  tests/native_nextui_suspend.cpp $(pkg-config --cflags --libs sdl2) \
  -Wl,--gc-sections -o build/nextui-suspend-test
Asset loading and renderer calls are stubbed; SDL events and the helper run normally.
*/
#include "../platforms/sdl2/corelib_mainloop.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
#include <sys/stat.h>

static std::vector<int> events;

void gfxHandleRenderReset(void) { events.push_back(-1); }
void gfxUpdateScreen(void) {}
int assetsInit(void) { return 0; }
static void onEvent(MainLoopEventData event) { events.push_back((int)event.type); }
static void draw(void) {}

static void pushPower(bool down) {
  SDL_Event event = {};
  event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  event.key.keysym.scancode = SDL_SCANCODE_POWER;
  event.key.keysym.sym = SDLK_POWER;
  assert(SDL_PushEvent(&event) == 1);
}

int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() /
      ("cct-nextui-suspend-" + std::to_string(getpid()));
  const fs::path helper = root / "bin/suspend";
  const fs::path marker = root / "invoked";
  fs::create_directories(helper.parent_path());
  setenv("SYSTEM_PATH", root.c_str(), 1);
  setenv("CCT_SUSPEND_TEST_MARKER", marker.c_str(), 1);

  // A missing or non-executable helper must not leave audio paused.
  nextuiSuspend(onEvent);
  assert(events.empty());
  {
    std::ofstream file(helper);
    file << "#!/bin/sh\nprintf suspended > \"$CCT_SUSPEND_TEST_MARKER\"\n";
  }
  nextuiSuspend(onEvent);
  assert(events.empty());

  assert(chmod(helper.c_str(), 0700) == 0);
  nextuiSuspend(onEvent);
  assert(fs::exists(marker));
  const std::vector<int> lifecycle = {
    (int)MainLoopEvent::sleep, (int)MainLoopEvent::wake,
    -1, (int)MainLoopEvent::fullRedraw,
  };
  assert(events == lifecycle);

  // A failed platform suspend still restores audio and redraws the UI.
  events.clear();
  {
    std::ofstream file(helper);
    file << "#!/bin/sh\nexit 7\n";
  }
  nextuiSuspend(onEvent);
  assert(events == lifecycle);

  // Exercise the actual SDL loop, including consuming the wake key pair.
  events.clear();
  assert(SDL_Init(SDL_INIT_EVENTS) == 0);
  setenv("CCT_NEXTUI", "1", 1);
  pushPower(true);
  pushPower(false);
  pushPower(true);
  pushPower(false);
  mainLoopTriggerQuit();
  mainLoopRun(draw, onEvent);
  std::vector<int> loopEvents = lifecycle;
  loopEvents.push_back((int)MainLoopEvent::exit);
  assert(events == loopEvents);

  // PortMaster must keep forwarding the same keyboard events normally.
  events.clear();
  unsetenv("CCT_NEXTUI");
  pushPower(true);
  pushPower(false);
  mainLoopTriggerQuit();
  mainLoopRun(draw, onEvent);
  const std::vector<int> portmasterEvents = {
    (int)MainLoopEvent::keyDown, (int)MainLoopEvent::keyUp,
    (int)MainLoopEvent::exit,
  };
  assert(events == portmasterEvents);
  SDL_Quit();

  fs::remove_all(root);
}
