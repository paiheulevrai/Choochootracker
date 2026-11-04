# SDL2 framework for macOS

From `tracker` on a Mac:

```sh
bash platforms/macos/check-sdl2.sh --download
make -j4 macOS-deploy
```

The installer downloads the official **SDL2 2.32.10** DMG, verifies its pinned
SHA-256, and copies the universal framework here. It refuses to overwrite an
existing installation. Run without `--download` to check an installed framework.
The framework is a local build dependency, not a checked-in file.

The release embeds it inside `ChooChooTracker.app/Contents/Frameworks`; users
do not need to install SDL2 or Homebrew. See [build notes](../../../../docs/build-notes.md#macos-experimental)
for packaging and CI validation.
