// Standalone check: g++ -std=c++17 tests/native_nextui_power.cpp -o build/nextui-power-test
#include "../platforms/sdl2/nextui_power.h"
#include <cassert>
#include <stdint.h>

int main() {
  NextUIPowerButton power;
  assert(!power.update(false, 0)); // no press to release
  assert(!power.update(true, 100));
  assert(power.update(false, 200));
  assert(!power.update(false, 201)); // duplicate release

  assert(!power.update(true, 300));
  assert(!power.update(true, 900, true)); // repeat does not restart the timer
  assert(!power.update(false, 1300)); // long press does not sleep

  power.onResume(1500);
  assert(!power.update(true, 1501));
  assert(!power.update(false, 1550)); // wake press must not put us back to sleep
  assert(!power.update(true, 2599, true)); // a held wake key is still not a new press
  assert(!power.update(false, 2600)); // late wake release is harmless too
  assert(!power.update(true, 2700));
  assert(power.update(false, 2800));

  power.onResume(UINT32_MAX - 500);
  assert(!power.update(true, UINT32_MAX - 300));
  assert(!power.update(false, 100)); // wake suppression survives tick wraparound
  assert(!power.update(true, 600));
  assert(power.update(false, 700));
}
