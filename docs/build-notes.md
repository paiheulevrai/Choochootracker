# Build notes

Run all commands from the repository root unless stated otherwise.

## Windows

Use the Windows wrapper; it keeps MSYS2's home and temporary files inside
the checkout, avoiding permissions failures under restricted environments:

```bat
tracker\build-windows.bat
```

The executable and bundled files are written to `tracker/build/windows/`.
The Windows makefile enables `_USE_MATH_DEFINES` before compiling, so the
vendored SID core can use the CRT math constants under C++17. The Docker
cross-build enables the same flag.

The ChooChooPlayer visualizer uses the same Windows toolchain:

```sh
cd tracker
make -j4 choochooplayer
```

Its self-contained package is written to `choochooplayer/build/windows/`.
Run `launch-alf-dance.bat` there to preview the bundled `alf dance.cct` project.

## macOS (experimental)

The release is a universal **Apple Silicon + Intel** application targeting
macOS 11.0 or later. CI validates startup on newer macOS; this is not a claim
that every older version or physical audio/MIDI device has been tested.

On a Mac with Xcode Command Line Tools installed, from `tracker`:

```sh
bash platforms/macos/check-sdl2.sh --download
make -j4 macOS-deploy
```

The installer checks the pinned SHA-256 of the official SDL2 2.32.10 DMG and
requires both architectures. It refuses to overwrite an existing framework;
run without `--download` to validate one already installed.

The result is `releases/ChooChooTracker-v<version>-macOS-universal.zip`.
It contains `ChooChooTracker.app` and installation instructions. The bundle
includes SDL2, all `packaging/common` assets, the manual and licenses. No
Homebrew or external SDL installation is required on the user's Mac.

The application uses `~/Library/Application Support/ChooChooTracker` for
settings, autosave and writable assets. Before loading settings or fonts, a
bundled build copies missing resources there and switches to that workspace.
Existing files are never overwritten, including when restoring an interrupted
initialization. The application bundle itself is not modified at runtime.
Unbundled development executables retain their caller's asset directory.

The bundle and framework have **ad-hoc signatures**, not Apple Developer
signatures or notarization. Gatekeeper can still block downloaded packages:
after trying to open a trusted package, authorize it in System Settings >
Privacy & Security > Open Anyway. Do not disable Gatekeeper globally.

`.github/workflows/macos.yml` builds on `macos-15` and validates the extracted
ZIP natively on `macos-15` (arm64) and `macos-15-intel` (x86_64). Checks cover
Mach-O architectures/minimum OS/dependencies, plist, signatures, resource
completeness, non-destructive workspace initialization and bounded startup to
the first rendered title frame with dummy SDL video/audio. Logs are retained as
validation artifacts. Local macOS checks against a built/extracted bundle:

```sh
SDL2_FRAMEWORK_PARENT="$PWD/build/macos/ChooChooTracker.app/Contents/Frameworks" \
  python3 -m unittest discover -s tests -p 'test_macos_*.py' -v
python3 ../scripts/validate_macos.py build/macos/ChooChooTracker.app
```

PRs, `main`, `v*` tags and manual runs produce the `choochootracker-macos`
Actions artifact. Only a published release or a manual run with `release_tag`
attaches files to an existing GitHub release, after both native validations
pass. Tags alone do not create or publish a release.

To add macOS to the current release after the workflow has been pushed:

```sh
gh workflow run macos.yml --repo paiheulevrai/Choochootracker \
  --ref main -f release_tag=v1.0.23-alpha
```

Choose a source ref whose `appVersion` matches the requested release tag.
Manual backfills build that selected ref, not necessarily the old tag commit.
The uploaded `*-macOS-build-info.json` records the exact source SHA, ref and
Actions URL. A dedicated `*-macOS-SHA256SUMS.txt` covers the ZIP without racing
other platform workflows updating the shared checksum manifest.

Before removing experimental status, community testers must check Finder
launch/Gatekeeper, Retina/resizing, keyboard/gamepad, real audio playback and
recording (including microphone permission), MIDI and Ableton Link on Intel
and Apple Silicon. CI does not replace these hardware checks.

## Developer UI screenshots

The developer-only offscreen SDL harness lives in
`tracker/tests/native_chip_ui.cpp`; it is not part of the application package.
From `tracker`, build its Windows executable with
`make -f Makefile.windows native-chip-screenshots`. Run the executable from
`tracker/build/windows` so the packaged fonts, themes, projects, and samples
resolve. Set `SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy`, then pass an
existing output directory followed by `--capture-homepage`. It writes BMPs at
640 x 480; convert them to PNG before publishing. The homepage mode uses
`projects/ChooTest_01.cct` and `projects/ChooBreaks01.cct` for real song,
phrase, and chain views. The sample editor uses a bundled 909 one-shot; break
slicing uses `AMEN.WAV`, and the sampler, wavetable, and SCWF views load
bundled sample assets.

## Linux / Steam Deck (x86_64)

For a native local test, use the existing Ubuntu-24.04 WSL distribution with
`g++`, `make`, `libsdl2-dev`, and `libasound2-dev` (for MIDI I/O, see below)
installed. From `tracker` in that distribution:

```sh
make -j4 -f Makefile.linux linux-package \
  COMMON_CFLAGS='-std=c++17 -Wall -g -Os -DTEST' VERSION=1.0.9-deck-test
ldd build/linux/choochootracker
tar -tzf build/linux/ChooChooTracker-$(date +%Y-%m-%d)-1.0.9-deck-test-Linux-x86_64.tar.gz
```

The archive includes the executable, runtime assets, manual, and licenses.
SDL2 and its system dependencies must be available on the target. A build
against Ubuntu 24.04 is a local compatibility test, not a Steam Linux Runtime
compatibility guarantee; check dependencies and playback on the target Deck.

Use SteamOS Devkit Client to pair the Deck and upload the extracted package.
Set the launch command to `./chipnomad.sh` and disable Steam Play for this
native build. The existing complete Windows package can also be tested with
Steam Play enabled and `./choochootracker.exe` as its launch command. Local
devkit testing does not require uploading a Steam store build.

For the Deck test, set `audioBufferSize: 512` in the extracted application's
`settings.txt` while it is closed, then relaunch. On the tested Deck with
SteamOS 3.8.16 and Proton 11.0, PSY played cleanly and its scope, waveforms,
and Table cursor became fluid at 512 frames. With the default 4906 frames
at 48 kHz, those displays advanced in visible jumps despite a 60 FPS overlay:
the engine processes about 102 ms of audio per callback. At 512 frames this
falls to about 10.7 ms. This observation validates that test configuration;
it does not establish a safe buffer size for every supported device.

### MIDI I/O

Desktop builds (Linux/Windows/macOS) link RtMidi (vendored at
`chipnomad_lib/external/rtmidi`) for realtime MIDI in/out - see
`docs/USER_MANUAL.md`'s MIDI section for what it's used for. Backend
selection is per platform Makefile: `-D__LINUX_ALSA__ -D__LINUX_ALSASEQ__`
plus `-lasound` on Linux, `-D__WINDOWS_MM__` plus `-lwinmm` on Windows,
`-D__MACOSX_CORE__` plus the CoreMIDI/CoreAudio/CoreFoundation frameworks on
macOS. `Dockerfile.linux` installs `libasound2-dev` for the Docker-based
Linux build.

### AppImage

```sh
make -j4 -f Makefile.linux appimage
```

Produces `releases/ChooChooTracker-<date>-<version>-x86_64.AppImage`. Bundles
only `libSDL2` and its `libsamplerate` dependency into `usr/lib` - the two
libraries the tar.gz release's README tells users to `apt install` - and
lets everything else (X11, ALSA, glibc...) resolve from the host, to avoid
the ABI-mismatch risk of bundling core OS libraries into an AppImage.

Requires `appimagetool` on `PATH` - it is not downloaded automatically, since
the only available URL for it is a `continuous` release tag that isn't
pinned to a fixed version, which would make the build not reproducible.
Install it first, e.g.:

```sh
curl -L -o /usr/local/bin/appimagetool https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x /usr/local/bin/appimagetool
```

To use the modern statically linked Type 2 runtime (no host libfuse2 needed),
pass it to `make`: `make -f Makefile.linux appimage APPIMAGE_RUNTIME=/path/to/runtime-x86_64`
(needs appimagetool 1.9 or newer, which has `--runtime-file`). CI pins both:
appimagetool 1.9.1 and type2-runtime `20251108`, each checked by SHA-256
(see the workflow). On systems without FUSE the AppImage still starts with
`APPIMAGE_EXTRACT_AND_RUN=1 ./ChooChooTracker-*.AppImage`.

The GitHub Actions workflow `.github/workflows/appimage.yml` builds the
AppImage inside a `debian:10` container (glibc 2.28, packages from
`archive.debian.org`) so it runs on Debian 10+ and Ubuntu 20.04+ including
22.04. Trigger it by hand (`workflow_dispatch`), by pushing `linux-appimage`
or a `v*` tag; the AppImage is uploaded as the `choochootracker-appimage`
artifact.

`fileGetDefaultDirectory()` (`src/corelib/corelib_file.cpp`) detects the
`APPIMAGE` environment variable the AppImage runtime sets and resolves
settings/autosave to `$XDG_DATA_HOME/ChooChooTracker`, falling back to
`~/.local/share/ChooChooTracker` when `$XDG_DATA_HOME` is unset, instead of
next to the executable, since an AppImage mounts itself read-only -
confirmed by running the produced AppImage under Xvfb with an isolated
`$HOME` and checking that `settings.txt`/`autosave.cct` land there rather
than inside the `/tmp/.mount_*` squashfs mount.

`packaging/common/*` (bonus themes, instruments, sample projects, title art)
is bundled read-only under `usr/share/choochootracker/common`. Since the
AppImage itself can't be written to, `AppRun` seeds a writable copy into the
same `$XDG_DATA_HOME`-aware directory every time it runs, copying each
top-level bonus directory independently and only if it's missing on the
destination - so an interrupted first run, or a user who already created
their own e.g. `projects` folder before the rest was seeded, still gets the
remaining bonus content on a later launch instead of never seeing it.
`AppRun` then starts the executable from that workspace, so relative assets
including the animated title artwork resolve from the writable copy rather
than the read-only mount (or the caller's current directory).
`initDefaultAppSettings()` (`src/common.cpp`) points
`projectPath`/`samplePath`/`themePath`/etc at that same directory when
`fileIsRunningFromAppImage()` is true - normal desktop builds keep their
existing behavior (relative paths, resolved via the launcher script's `cd`)
untouched. Confirmed under Xvfb with a fresh `$HOME`: first launch populates
every subfolder, the files are writable, and the Project screen's Load
browser lists the bundled example songs from the seeded (not the read-only)
copy.

## Raspberry Pi (ARM)

The release GitHub Actions workflow `.github/workflows/raspberry-pi.yml`
runs for `v*` tags or manually. It uses GitHub's native ARM runner to produce the
`*-RaspberryPi-aarch64.tar.gz` artifact for Raspberry Pi OS 64-bit on Pi 3B+,
Pi 4, Pi 5, Zero 2 W, and compatible Compute Modules.

The archive contains the executable, runtime assets, manual, and licenses. It
requires SDL2 on the target (`sudo apt install libsdl2-2.0-0`). Launch the
extracted application with `./chipnomad.sh`.

## Web

Emscripten is installed locally at `.tmp/emsdk`; do not search for or install
another copy. In PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
. .\.tmp\emsdk\emsdk_env.ps1
$env:PATH += ';C:\msys64\usr\bin;C:\msys64\ucrt64\bin'
$empy = $env:EMSDK_PYTHON -replace '\\','/'
$empp = "$env:EMSDK/upstream/emscripten/em++.py"
Set-Location tracker
& 'C:\msys64\usr\bin\make.exe' -j8 -f Makefile.web web-deploy `
  'COMMON_CFLAGS=-std=c++17 -Wall -Os -DTEST' `
  "EMXX=$empy $empp"
```

The deploy target updates the checked-in browser bundle in `web/dist/`. GitHub
Actions rebuilds and commits that directory when Web, tracker or engine sources
change on `main`; Cloudflare Pages and the legacy Vercel site then publish the
updated bundle. The bundled SDK requires its own Python, hence the explicit
`EMXX`.
If Windows reports `clang++.exe: permission denied`, unblock the bundled
compiler with `Unblock-File -LiteralPath .tmp\emsdk\upstream\bin\clang++.exe`,
verify `clang++.exe --version`, then retry.

## PortMaster (ARM64)

Use the existing Ubuntu WSL2 toolchain:

```sh
cd tracker
make -j4 -f Makefile.portmaster PortMaster-deploy \
  COMMON_CFLAGS='-std=c++17 -Wall -g -Os -DTEST'
```

The package is copied to `releases/choochootracker.zip`. The local build copy
is `tracker/build/portmaster/choochootracker.zip`. The explicit flags avoid a
current GCC 9 LTO internal compiler error. Validate the release archive before
uploading it:

```powershell
& 'C:\Program Files\7-Zip\7z.exe' t releases\choochootracker.zip
```

### PortMaster Library submission and weekly updates

The `PortMaster` GitHub Actions workflow produces the normal installable ZIP
and a `choochootracker-portmaster-library` artifact. The latter contains
`ports/choochootracker/`, `PR_BODY.md` and `provenance.json`. Metadata is moved
to the Library source root; component/font licenses are flattened without
changing their contents. All runtime assets are retained.

Publishing a GitHub release triggers preparation automatically. Its source
is the release tag. The workflow replaces that release's versioned PortMaster
ZIP and only its existing entry in `SHA256SUMS.txt`; other checksum lines are
preserved. The release must already contain a valid checksum entry. If assets
are still being uploaded when the workflow runs, it fails explicitly: rerun
after publication is complete. PortMaster and Miyoo release-manifest writers
share a concurrency group.

To rebuild an existing release from a corrected source revision, dispatch the
workflow on that branch with `release_tag` set explicitly, for example:

```powershell
gh workflow run portmaster.yml --ref main -f release_tag=v1.0.23-alpha
```

This deliberately replaces the public ZIP and its checksum without moving the
tag. Back up both existing assets first, record the run/source SHA, and check
the rebuilt package on hardware. Omitting `release_tag` only produces build
artifacts on a manual branch run; it does not replace public release assets.

Create a GitHub fork of `PortsMaster/PortMaster-New`, then use a partial,
shallow sparse clone in a separate directory, not this source worktree:

```powershell
git clone --filter=blob:none --depth=1 --single-branch --no-checkout https://github.com/YOUR_USERNAME/PortMaster-New.git C:\path\PortMaster-New
git -C C:\path\PortMaster-New sparse-checkout set --no-cone /AGENTS.md /SOURCE_SETUP.txt /ports/choochootracker/
git -C C:\path\PortMaster-New switch -c add-choochootracker
```

Download a successful run's Library artifact and import it into that branch:

```powershell
gh run download RUN_ID --name choochootracker-portmaster-library --dir C:\path\library-artifact
python scripts\prepare_portmaster_library.py import --prepared C:\path\library-artifact --repository C:\path\PortMaster-New
git -C C:\path\PortMaster-New diff --stat
git -C C:\path\PortMaster-New status --short
```

The importer replaces only a clean `ports/choochootracker/` tree. It refuses
local or ignored changes there; it does not commit, push or open a PR. On
updates, reuse the existing submission branch/PR and import the next release's
artifact after reviewing its source and hardware results.

For an already published ZIP, run `prepare` with `--archive`, `--checksums`,
`--output` (an empty directory), `--tag`, `--source-sha` and `--run-url` to
produce the same artifact locally. The ZIP must match its exact checksum entry.
Python 3.8+ and Git are required; no third-party Python packages are needed.

Review `PR_BODY.md` before submission. Its historical test reports are not
automatic certification of new builds; check only the tests actually performed
and personally confirm the authorship/AI-assistance declaration. The testing
thread is private: do not invent a public link. No cross-repository token,
automatic fork push or PR creation is configured.

Focused checks, from the repository root with Python and Bash:

```sh
python3 -m unittest discover -s tracker/tests -p 'test_portmaster_*.py'
```

## PlayStation Vita (experimental, untested on hardware)

Built with [VitaSDK](https://vitasdk.org) on top of the shared SDL2 platform
layer (`platforms/sdl2`) plus `platforms/vita` (CPU clocks, heap size and the
first-run copy of the bundled assets). The target define is `VITA_BUILD`.

```sh
export VITASDK=/usr/local/vitasdk      # or the vitasdk/vitasdk Docker image
vdpm install sdl2                      # the SDL2 port for the Vita
cd tracker
make -j4 -f Makefile.vita Vita-deploy  # writes releases/ChooChooTracker-<version>-Vita.vpk
```

`.github/workflows/vita.yml` runs the same build in the `vitasdk/vitasdk`
container and uploads the VPK as a workflow artifact.

Notes:

- Install the VPK with VitaShell. On first launch the assets bundled in the VPK
  (`app0:common`) are copied to `ux0:data/ChooChooTracker`, which is also where
  settings, autosave and projects live. Existing files are never overwritten.
- No MIDI and no Ableton Link (the stub backends are used). The touch screen is
  not used.
- Controller defaults: Cross = edit, Circle = option, Start = play,
  Select = shift, L = live motion, Square = record motion, Triangle = erase
  motion. Everything is remappable in Settings > Controls.
- The default synth quality is `low` and the CPU runs at 444 MHz; the heavier
  Plaits/Braids/FM engines may still not keep up on the Vita's Cortex-A9.
- Audio uses SDL's Vita driver. Keep the sample rate at 48000 Hz: lower rates
  are routed to the system "BGM" port.

## NextUI Pak Store (TrimUI tg5040)

The NextUI package reuses the same ARM64 executable as PortMaster:

```sh
cd tracker
make -j4 -f Makefile.portmaster NextUI-deploy \
  COMMON_CFLAGS='-std=c++17 -Wall -g -Os -DTEST'
```

This writes `releases/ChooChooTracker.pak.zip`. Its files must be at the ZIP
root; Pak Store extracts them into `Tools/tg5040/ChooChooTracker.pak`.
Keep the root `pak.json` version equal to the GitHub release tag and validate
the archive before upload:

```powershell
& 'C:\Program Files\7-Zip\7z.exe' t releases\ChooChooTracker.pak.zip
```

For `v*` tags, the PortMaster workflow builds this archive and attaches it to
the matching GitHub release automatically. Updating `src/version.cpp` without
the same version in the root `pak.json` makes the packaging job fail.

## Android / Google Play

The Android app is `com.paiheulevrai.choochootracker`, targets API 36, and
uses no storage permissions. It builds both 64-bit and 32-bit ARM libraries.
Install the Android SDK/NDK selected by `ANDROID_HOME` (or set
`ANDROID_NDK_ROOT`). Build each native ABI with MSYS2 UCRT64. This only needs
to happen after native C/C++ changes:

```powershell
$env:ANDROID_NDK_ROOT = "$env:LOCALAPPDATA\Android\Sdk\ndk\30.0.16248370"
& 'C:\msys64\usr\bin\bash.exe' -c `
  'export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /c/Users/<you>/Desktop/choochootracker/tracker; make -f Makefile.android android ARCH=arm64-v8a'
& 'C:\msys64\usr\bin\bash.exe' -c `
  'export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /c/Users/<you>/Desktop/choochootracker/tracker; make -f Makefile.android android ARCH=armeabi-v7a'
```

The native build also needs SDL 2.32.10 headers in
`.tmp/SDL2-2.32.10/SDL2`; retrieve the matching SDL source archive once and
copy its `include/*.h` files into that directory if it is absent.
If an NDK wrapper reports `clang++.exe: Permission denied`, unblock the shared
NDK compiler (not the wrapper) before retrying:

```powershell
Unblock-File -LiteralPath "$env:ANDROID_NDK_ROOT\toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe"
```

The first Gradle package build can download its wrapper distribution, so it
needs network access.

For a signed release, create one upload key once. Keep its `.jks` file in a
safe backup and its password in a password manager: losing either prevents
future updates with the same upload identity. Do not commit the key or its
password. `tracker/platforms/android/*.jks` and `keystore.properties` are
ignored by Git.

The build accepts either an ignored `keystore.properties` file or environment
variables. On Windows PowerShell, the latter avoids putting a secret in a
project file:

```powershell
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
$env:CCT_KEYSTORE_FILE = 'choochootracker-upload.jks' # relative to platforms/android
$env:CCT_KEY_ALIAS = 'choochootracker-upload'
$env:CCT_KEYSTORE_PASSWORD = '<upload-key-password>'
$env:CCT_KEY_PASSWORD = $env:CCT_KEYSTORE_PASSWORD
Push-Location tracker\platforms\android
& "$env:JAVA_HOME\bin\java.exe" -classpath gradle\wrapper\gradle-wrapper.jar `
  org.gradle.wrapper.GradleWrapperMain --no-daemon --console=plain `
  assembleStoreRelease assembleDirectRelease bundleStoreRelease -x buildNativeArm64 -x buildNativeArm32
Pop-Location
```

The `-x` options avoid concurrent/redundant native builds: Gradle only packages
the two libraries produced above. Every bundle uploaded to Play Console needs a
strictly greater `versionCode`; update `versionCode` in
`tracker/platforms/android/app/build.gradle` before each upload. The displayed
`versionName` may stay unchanged for a replacement internal build.

The signed Play artifact is
`tracker/platforms/android/app/build/outputs/bundle/storeRelease/app-store-release.aab`.
It contains `arm64-v8a` and `armeabi-v7a` native libraries plus all bundled
content from `tracker/packaging/common`: projects, samples, instruments,
themes, fonts, AY/SR wavetables, waveforms and title assets. Confirm it before
uploading:

```powershell
& 'C:\Program Files\Android\Android Studio\jbr\bin\jar.exe' tf `
  tracker\platforms\android\app\build\outputs\bundle\storeRelease\app-store-release.aab
```

Android has two Gradle flavors, selected by the task name:

- `store` (`assembleStoreRelease`, `bundleStoreRelease`): private app storage
  by default, with a picked SAF folder available in Settings. No storage
  permission is needed. This is the Play Store build.
- `direct` (`assembleDirectRelease`): for F-Droid and direct APK download.
  Android 11+ defaults to `/sdcard/ChooChooTracker` with all-files access; the
  Working folder setting can switch to a picked SAF folder or private storage.
  Android 10 uses the system folder picker and a persisted SAF grant; on first
  launch it asks for a working folder and restarts after one is chosen. Until
  then it uses private app storage.

The picked folder has no filesystem path, so the native engine reaches it
through `platforms/android/saf_storage.cpp`: `Makefile.android` links with
`-Wl,--wrap` for `fopen`, `open`, `stat`, `opendir`, `rename`... and the wrappers
serve paths under the virtual root `/ChooChooTracker` through the `saf*` methods
of `ChooChooTrackerActivity` and `SafWorkspace.java` (DocumentsContract, with a
short-lived folder listing cache). The bundled banks, presets and demo projects
stay in the private workspace and show through as a read-only overlay: the
picked folder wins when both have a path, writes always go to it. Add a libc
call used by the engine to `SAF_WRAP` in `Makefile.android` and to
`saf_storage.cpp` together.

The flavor only changes `src/<flavor>/` (the `direct_storage` bool read by
`ChooChooTrackerActivity`). Install the signed
direct APK (`app/build/outputs/apk/direct/release/app-direct-release.apk`)
with `adb install -r <apk>`. For handoff, copy and name the files outside Gradle's
output directory, for example `releases/ChooChooTracker-1.0-code2-release.aab`.
Upload that AAB to Play Console's Internal testing track. Complete the values
and artwork in `docs/play-store-listing.md` before submission.

For audio traces and the separately installed debug APK, see

## Validation

```sh
cd tracker
make -f Makefile.test -j4
```

On Windows (`OS=Windows_NT`), the test makefile enables `_USE_MATH_DEFINES`,
matching the application build's CRT math constants for the vendored SID core.

If MSYS2 reports exit code 127 after `Built: build/tests/run_tests.exe`, run
`build/tests/run_tests.exe` directly; the executable is the authoritative test
result in that environment.

## Native chip instruments

The normal builds include the native instruments and shipped preset catalog.
Synth core sources live under `chipnomad_lib/external/`; each new dependency
includes its license and provenance. Preset source licenses are packaged under
`tracker/packaging/common/licenses/`. See `docs/native-chip-instruments.md`.

PortMaster appends `-O3` only for native synth cores and adapters through
`Makefile.native-chip-flags`; other platform and audio settings are unchanged.
The native UI/audio harnesses are developer-only targets in
`Makefile.native-chip-device` and are not included in release packages.

To regenerate factory data, build `chip-factory` with `Makefile.test`, then use
`tools/chip_banks/convert.py` and the documented expansion/SID conversion tools.
The ordinary build uses the checked-in presets and needs no network access.
