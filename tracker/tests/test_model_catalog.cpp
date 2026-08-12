#include "doctest.h"
#include "screens/model_catalog.h"
#include <string.h>

TEST_CASE("every Braids and Plaits model belongs to exactly one category") {
  CHECK(modelCatalogsValid());
}

TEST_CASE("Plaits-Alt contains exactly its 24 supplemental engines") {
  int count = 0;
  bool seen[24] = {};
  for (int category = 0; category < plaitsAltCategoryCount; ++category) {
    for (int item = 0; item < plaitsAltCategories[category].childCount; ++item) {
      int value = plaitsAltCategories[category].children[item].value;
      CHECK(value >= 0);
      CHECK(value < 24);
      CHECK_FALSE(seen[value]);
      seen[value] = true;
      ++count;
    }
  }
  CHECK(count == 24);
}
