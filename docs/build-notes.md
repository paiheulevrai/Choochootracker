# Build notes

Run all commands from the repository root unless stated otherwise.

## Windows

Use MSYS2 UCRT64:

```sh
cd tracker
make -j4 windows
```

The executable and bundled files are written to `tracker/build/windows/`.

## Web

Emscripten is installed locally at `.tmp/emsdk`; do not search for or install
another copy. In PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
. .\.tmp\emsdk\emsdk_env.ps1
$env:PATH += ';C:\msys64\usr\bin;C:\msys64\ucrt64\bin'
Set-Location tracker
& 'C:\msys64\usr\bin\make.exe' -j8 -f Makefile.web web-deploy `
  'COMMON_CFLAGS=-std=c++17 -Wall -g -Os -DTEST' `
  "EMXX=$env:EMSDK_PYTHON $env:EMSDK\upstream\emscripten\em++.py"
```

The deploy target updates the checked-in browser bundle in `web/dist/`; Vercel
serves it directly. Commit that directory after every WebAssembly source
change. The bundled SDK requires its own Python, hence the explicit `EMXX`.

## PortMaster (ARM64)

Use the existing Ubuntu WSL2 toolchain:

```sh
cd tracker
make -j4 -f Makefile.portmaster PortMaster-deploy \
  COMMON_CFLAGS='-std=c++17 -Wall -g -Os -DTEST'
```

The package is copied to `releases/choochootracker.zip`. The local build copy
is `tracker/build/portmaster/choochootracker.zip`. The explicit flags avoid a
current GCC 9 LTO internal compiler error.

## Android / Google Play

The Android app is `com.paiheulevrai.choochootracker`, targets API 36, and
uses no storage permissions. It builds both 64-bit and 32-bit ARM libraries.
Install the Android SDK/NDK selected by `ANDROID_HOME` (or set
`ANDROID_NDK_ROOT`) and use MSYS2 UCRT64's `make` from PowerShell:

```powershell
Set-Location tracker
& 'C:\\msys64\\usr\\bin\\make.exe' -f Makefile.android android-apk
& 'C:\\msys64\\usr\\bin\\make.exe' -f Makefile.android android-bundle
```

The native build also needs SDL 2.32.10 headers in
`.tmp/SDL2-2.32.10/SDL2`; retrieve the matching SDL source archive once and
copy its `include/*.h` files into that directory if it is absent.

For a signed release, create one upload key once. Keep its `.jks` file in a
safe backup and its password in a password manager: losing either prevents
future updates with the same upload identity. Do not commit the key or its
password. `tracker/platforms/android/*.jks` and `keystore.properties` are
ignored by Git.

The build accepts either an ignored `keystore.properties` file or environment
variables. On Windows PowerShell, the latter avoids putting a secret in a
project file:

```powershell
$env:CCT_KEYSTORE_FILE = 'choochootracker-upload.jks'
$env:CCT_KEY_ALIAS = 'choochootracker-upload'
$env:CCT_KEYSTORE_PASSWORD = '<upload-key-password>'
$env:CCT_KEY_PASSWORD = $env:CCT_KEYSTORE_PASSWORD
Set-Location tracker
& 'C:\\msys64\\usr\\bin\\make.exe' -f Makefile.android android-bundle
```

The signed Play artifact is
`tracker/platforms/android/app/build/outputs/bundle/release/app-release.aab`.
It contains `arm64-v8a` and `armeabi-v7a` native libraries plus all bundled
content from `tracker/packaging/common`: projects, samples, instruments,
themes, fonts, AY/SR wavetables, waveforms and title assets. Confirm it before
uploading:

```powershell
& 'C:\\Program Files\\Android\\Android Studio\\jbr\\bin\\jar.exe' tf `
  tracker\\platforms\\android\\app\\build\\outputs\\bundle\\release\\app-release.aab
```

Build `android-apk` for the signed direct-install APK, then install it with
`adb install -r <apk>`. Upload the AAB to Play Console's Internal testing
track. Complete the values and artwork in `docs/play-store-listing.md` before
submission.

## Validation

```sh
cd tracker
make -f Makefile.test -j4
```
