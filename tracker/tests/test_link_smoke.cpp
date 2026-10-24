// T21: Link smoke test.
//
// The test build compiles with -DTEST only (no DESKTOP_BUILD), so this file
// is empty there: the real Link headers pull in asio networking and threads
// that the mock-based test environment must not depend on. The smoke test
// runs in the desktop/PortMaster/Android builds where Link is compiled in.
//
// What it verifies (when compiled in): a Link client can be created, enabled,
// reports the initial tempo and zero peers, and is disabled again.
//
// API notes (upstream Link.hpp): enable()/disable() are a single
// enable(bool) overload; the peer count accessor is numPeers().
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD) || defined(ANDROID_BUILD)

#include "../external/doctest.h"

#include "../../chipnomad_lib/external/ableton_link/include/ableton/Link.hpp"

#include <chrono>
#include <thread>

TEST_CASE("ableton link smoke: enable, capture state, disable") {
  ableton::Link link(125.0);
  link.enable(true);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  const auto sessionState = link.captureAppSessionState();
  const auto tempo = sessionState.tempo();
  const auto numPeers = link.numPeers();

  link.enable(false);

  CHECK(tempo == doctest::Approx(125.0).epsilon(0.01));
  CHECK(numPeers == 0);
}

#endif  // DESKTOP_BUILD || PORTMASTER_BUILD || ANDROID_BUILD
