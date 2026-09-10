# Build notes

Run all commands from the repository root unless stated otherwise.

## Windows

Use MSYS2 UCRT64:

```sh
cd tracker
make -j4 windows
```

The executable and bundled files are written to `tracker/build/windows/`.

The ChooChooPlayer visualizer uses the same Windows toolchain:

```sh
cd tracker
make -j4 choochooplayer
```

Its self-contained package is written to `choochooplayer/build/windows/`.
Run `launch-alf-dance.bat` there to preview the bundled `alf dance.cct` project.

## Web

Emscripten is installed locally at `.tmp/emsdk`; do not search for or install
another copy. In PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
. .\.tmp\emsdk\emsdk_env.ps1
$env:PATH += ';C:\msys64\usr\bin;C:\msys64\ucrt64\bin'
$empy = '/c/Users/<you>/Desktop/mobilegroove/.tmp/emsdk/python/3.13.3_64bit/python.exe'
$empp = '/c/Users/<you>/Desktop/mobilegroove/.tmp/emsdk/upstream/emscripten/em++.py'
Set-Location tracker
& 'C:\msys64\usr\bin\make.exe' -j8 -f Makefile.web web-deploy `
  'COMMON_CFLAGS=-std=c++17 -Wall -g -Os -DTEST' `
  "EMXX=$empy $empp"
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
current GCC 9 LTO internal compiler error. Validate the release archive before
uploading it:

```powershell
& 'C:\Program Files\7-Zip\7z.exe' t releases\choochootracker.zip
```

## Android / Google Play

The Android app is `com.paiheulevrai.choochootracker`, targets API 36, and
uses no storage permissions. It builds both 64-bit and 32-bit ARM libraries.
Install the Android SDK/NDK selected by `ANDROID_HOME` (or set
`ANDROID_NDK_ROOT`). Build each native ABI with MSYS2 UCRT64. This only needs
to happen after native C/C++ changes:

```powershell
$env:ANDROID_NDK_ROOT = "$env:LOCALAPPDATA\Android\Sdk\ndk\30.0.16248370"
& 'C:\msys64\usr\bin\bash.exe' -c `
  'export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /c/Users/<you>/Desktop/mobilegroove/tracker; make -f Makefile.android android ARCH=arm64-v8a'
& 'C:\msys64\usr\bin\bash.exe' -c `
  'export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /c/Users/<you>/Desktop/mobilegroove/tracker; make -f Makefile.android android ARCH=armeabi-v7a'
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
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
$env:CCT_KEYSTORE_FILE = 'choochootracker-upload.jks' # relative to platforms/android
$env:CCT_KEY_ALIAS = 'choochootracker-upload'
$env:CCT_KEYSTORE_PASSWORD = '<upload-key-password>'
$env:CCT_KEY_PASSWORD = $env:CCT_KEYSTORE_PASSWORD
Push-Location tracker\platforms\android
& "$env:JAVA_HOME\bin\java.exe" -classpath gradle\wrapper\gradle-wrapper.jar `
  org.gradle.wrapper.GradleWrapperMain --no-daemon --console=plain `
  assembleRelease bundleRelease -x buildNativeArm64 -x buildNativeArm32
Pop-Location
```

The `-x` options avoid concurrent/redundant native builds: Gradle only packages
the two libraries produced above. Every bundle uploaded to Play Console needs a
strictly greater `versionCode`; update `versionCode` in
`tracker/platforms/android/app/build.gradle` before each upload. The displayed
`versionName` may stay unchanged for a replacement internal build.

The signed Play artifact is
`tracker/platforms/android/app/build/outputs/bundle/release/app-release.aab`.
It contains `arm64-v8a` and `armeabi-v7a` native libraries plus all bundled
content from `tracker/packaging/common`: projects, samples, instruments,
themes, fonts, AY/SR wavetables, waveforms and title assets. Confirm it before
uploading:

```powershell
& 'C:\Program Files\Android\Android Studio\jbr\bin\jar.exe' tf `
  tracker\platforms\android\app\build\outputs\bundle\release\app-release.aab
```

The same Gradle command produces the signed direct-install APK; install it with
`adb install -r <apk>`. For handoff, copy and name the files outside Gradle's
output directory, for example `releases/ChooChooTracker-1.0-code2-release.aab`.
Upload that AAB to Play Console's Internal testing track. Complete the values
and artwork in `docs/play-store-listing.md` before submission.

## Validation

```sh
cd tracker
make -f Makefile.test -j4
```

If MSYS2 reports exit code 127 after `Built: build/tests/run_tests.exe`, run
`build/tests/run_tests.exe` directly; the executable is the authoritative test
result in that environment.
