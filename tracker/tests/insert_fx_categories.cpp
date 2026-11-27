#include "../src/screens/screen_insert_fx.cpp"
#include <cassert>
#include <cstring>

int main() {
  const SelectionItem* reverbCategory = nullptr;
  const SelectionItem* delayCategory = nullptr;
  for (const auto& category : insertCategories) {
    if (std::strcmp(category.label, "Reverb") == 0) reverbCategory = &category;
    if (std::strcmp(category.label, "Delay") == 0) delayCategory = &category;
  }
  assert(reverbCategory && reverbCategory->childCount == 2);
  assert(reverbCategory->children[0].value == insertCloudsReverb);
  assert(reverbCategory->children[1].value == insertBeadsReverb);
  assert(delayCategory && delayCategory->childCount == 3);
  assert(delayCategory->children[0].value == insertPingPongDelay);
  assert(delayCategory->children[1].value == insertTapeEcho);
  assert(delayCategory->children[2].value == insertDiffusingEchoverb);
  for (int module = 0; module < insertModuleCount; ++module) {
    const int next = cycleInsertModule(module, 1);
    assert(cycleInsertModule(next, -1) == module);
  }
}
