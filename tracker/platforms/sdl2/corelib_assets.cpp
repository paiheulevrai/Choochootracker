#include "../../src/corelib/corelib_assets.h"

#ifdef MACOS_BUILD
#include "../../src/corelib/corelib_file.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <stdio.h>

int assetsInit(void) {
    namespace fs = std::filesystem;
    char* base = SDL_GetBasePath();
    if (!base) {
        fprintf(stderr, "Cannot locate macOS resources: %s\n", SDL_GetError());
        return 1;
    }
    fs::path resources(base);
    SDL_free(base);
    // SDL returns Contents/Resources for bundles, the binary's directory otherwise.
    resources = resources.lexically_normal();
    if (resources.filename().empty()) resources = resources.parent_path();
    if (resources.filename() != "Resources" || resources.parent_path().filename() != "Contents") {
        return 0; // Unbundled development builds keep their caller's asset directory.
    }

    char workspace[4096];
    if (fileGetDefaultDirectory(workspace, sizeof(workspace)) != 0) return 1;
    std::error_code error;
    fs::copy(resources / "common", workspace,
             fs::copy_options::recursive | fs::copy_options::skip_existing, error);
    if (error) {
        fprintf(stderr, "Cannot seed macOS resources into '%s': %s\n", workspace, error.message().c_str());
        return 1;
    }
    fs::current_path(workspace, error);
    if (error) {
        fprintf(stderr, "Cannot enter macOS workspace '%s': %s\n", workspace, error.message().c_str());
        return 1;
    }
    return 0;
}
#elif !defined(ANDROID_BUILD)
int assetsInit(void) {
    return 0;
}
#endif
