#include "doctest.h"

#include "project.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_CASE("projectSave preserves an existing file when the temporary write fails") {
  namespace fs = std::filesystem;

  const fs::path path = "build/tests/project_save_preserve_existing.cct";
  const fs::path tempPath = path.string() + ".tmp";
  fs::create_directories(path.parent_path());
  fs::remove(path);
  fs::remove_all(tempPath);

  {
    std::ofstream existing(path, std::ios::binary | std::ios::trunc);
    REQUIRE(existing.good());
    existing << "known-good-project";
  }

  // projectSave() writes the sibling temporary path first. Making that path a
  // directory forces the temporary open to fail before the destination is
  // touched.
  REQUIRE(fs::create_directory(tempPath));

  Project project;
  projectInit(&project);

  CHECK(projectSave(&project, path.string().c_str()) != 0);

  std::ifstream preserved(path, std::ios::binary);
  REQUIRE(preserved.good());
  std::string contents((std::istreambuf_iterator<char>(preserved)),
                       std::istreambuf_iterator<char>());
  CHECK(contents == "known-good-project");

  projectFree(&project);
  fs::remove_all(tempPath);
  fs::remove(path);
}
