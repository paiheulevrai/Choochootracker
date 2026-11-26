#include "../../src/corelib/corelib_assets.h"

#if defined(MACOS_BUILD) || defined(SNDSTATION_BUILD)
#include "../../src/corelib/corelib_file.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <stdio.h>

int assetsInit(void) {
    namespace fs = std::filesystem;
#ifdef MACOS_BUILD
    const char* platform = "macOS";
#else
    const char* platform = "SNDStation";
#endif
    char* base = SDL_GetBasePath();
    if (!base) {
        fprintf(stderr, "Cannot locate %s resources: %s\n", platform, SDL_GetError());
        return 1;
    }
    fs::path resources(base);
    SDL_free(base);
    resources = resources.lexically_normal();
    if (resources.filename().empty()) resources = resources.parent_path();
#ifdef MACOS_BUILD
    // SDL returns Contents/Resources for bundles, the binary's directory otherwise.
    if (resources.filename() != "Resources" || resources.parent_path().filename() != "Contents") {
        return 0; // Unbundled development builds keep their caller's asset directory.
    }
    const fs::path source = resources / "common";
#else
    const fs::path source = resources / "../share/choochootracker/common";
#endif

    char workspace[4096];
    if (fileGetDefaultDirectory(workspace, sizeof(workspace)) != 0) return 1;
    std::error_code error;
#ifdef SNDSTATION_BUILD
    if (fs::equivalent(resources, workspace, error)) {
        fs::current_path(workspace, error);
        if (!error) return 0;
    }
    if (error) {
        fprintf(stderr, "Cannot access SNDStation workspace '%s': %s\n", workspace, error.message().c_str());
        return 1;
    }
#endif
    fs::recursive_directory_iterator entry(source, error), end;
    while (!error && entry != end) {
        const fs::path target = fs::path(workspace) / entry->path().lexically_relative(source);
        // New workspace content must not inherit a read-only bundle's permissions.
        if (entry->is_directory(error)) {
            if (fs::create_directory(target, error)) {
                fs::permissions(target, fs::perms::owner_all, fs::perm_options::add, error);
            }
        } else if (!error && fs::copy_file(entry->path(), target, fs::copy_options::skip_existing, error)) {
            fs::permissions(target, fs::perms::owner_read | fs::perms::owner_write,
                            fs::perm_options::add, error);
        }
        if (!error) entry.increment(error);
    }
    if (error) {
        fprintf(stderr, "Cannot seed %s resources into '%s': %s\n", platform, workspace, error.message().c_str());
        return 1;
    }
    fs::current_path(workspace, error);
    if (error) {
        fprintf(stderr, "Cannot enter %s workspace '%s': %s\n", platform, workspace, error.message().c_str());
        return 1;
    }
    return 0;
}
#elif !defined(ANDROID_BUILD)
int assetsInit(void) {
    return 0;
}
#endif
