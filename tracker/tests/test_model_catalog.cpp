#include "doctest.h"
#include "screens/model_catalog.h"

TEST_CASE("every Braids and Plaits model belongs to exactly one category") {
  CHECK(modelCatalogsValid());
}
