# Android audio diagnosis

Use the phone speaker, a fixed volume, and the same song for comparisons.
The mixer CPU display measures smoothed callback computation time; it does
not include scheduling delays before the callback or downstream audio writes.

Android now prefers SDL's AAudio backend, with OpenSL ES as a fallback when
AAudio initialization is unavailable. This policy is set before SDL initializes;
other platforms and saved buffer settings are unchanged. SDL's current AAudio
path is buffered: on the tested Pixel 7a, AudioFlinger estimated about 170 ms
of track latency versus about 53 ms on OpenSL ES. These are server estimates,
not measurements of input-to-speaker latency. Low-latency live performance
needs a separate tuning pass against the same glitch test.

## Capture the installed application

From the repository root, with one authorized ADB device connected:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
./scripts/capture-android-audio.ps1 -Label baseline
```

Keep playback running during the 30-second capture and note audible glitches.
The ignored `.tmp/android-audio-<timestamp>-<label>/` directory contains the
trace, device time, PID, AudioFlinger snapshots and recent filtered logcat.
Logcat includes history: restrict interpretation to the capture interval.
Perfetto requires `/data/misc/perfetto-traces/` on the device. The script
deletes its remote trace only after successfully copying it locally.

Analyze locally using the official Perfetto trace processor, without uploading
the trace. Download its launcher once into `.tmp/trace_processor` from
`https://get.perfetto.dev/trace_processor`, then run:

```powershell
python .tmp/trace_processor query -f scripts/android-audio-summary.sql TRACE
```

`R` is time waiting for a CPU; `Running` is execution; `S` includes ordinary
buffer waits and timer sleeps. Long gaps alone are not underruns. Compare
FastMixer events and counters with the application's own track, identified
by the captured PID; system-wide accumulated counts are not app error counts.

## Instrumented companion APK

Follow the two native ABI builds in [build-notes.md](build-notes.md), then package:

```powershell
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
Push-Location tracker/platforms/android
& "$env:JAVA_HOME/bin/java.exe" -classpath gradle/wrapper/gradle-wrapper.jar `
  org.gradle.wrapper.GradleWrapperMain --no-daemon --console=plain `
  assembleDebug -PaudioDiag -x buildNativeArm64 -x buildNativeArm32
Pop-Location
$adb = "$env:LOCALAPPDATA/Android/Sdk/platform-tools/adb.exe"
& $adb install -r tracker/platforms/android/app/build/outputs/apk/debug/app-debug.apk
```

This installs `com.paiheulevrai.choochootracker.audiodiag` alongside the Play
app, with separate settings and projects. Open the same bundled song (`psy.cct`
for the reported reproduction), or import the user's exported song. Do not
uninstall the Play app to bypass its signature or erase its settings.

Read the companion's actual saved buffer with:

```powershell
& $adb shell run-as com.paiheulevrai.choochootracker.audiodiag cat files/workspace/settings.txt
```

For each experiment, restart only the companion so SDL reinitializes:

```powershell
$pkg = 'com.paiheulevrai.choochootracker.audiodiag'
& $adb shell am force-stop $pkg
& $adb shell am start -n "$pkg/com.paiheulevrai.choochootracker.ChooChooTrackerActivity" `
  --ez cct_audio_diag true --es cct_audio_driver openslES --ez cct_audio_tone false
./scripts/capture-android-audio.ps1 -Package $pkg -Label opensles-song
```

Replace `openslES` with `aaudio` for the backend comparison. Set
`cct_audio_tone` to `true` for a continuous 440 Hz sine at about -24 dBFS,
with phase preserved across callbacks. Tone replaces engine rendering but
uses the same SDL callback and audio format. Keep the buffer identical between
experiments. All extras are ignored by non-debuggable release applications.

`CCTAudio` logs the selected backend, loaded requested buffer, callback bytes,
sample rate and format once at startup. Every second a separate thread logs
callback frames, maximum render duration and start-to-start gap in microseconds,
plus cumulative over-budget, invalid-buffer and failed-render counts.
Maxima cover reporting windows; the other counters are cumulative since open.
`render_failures` counts short engine returns whose output the audio manager
replaces with silence, including the normal stopped state. Interpret its
increase during uninterrupted playback, not the initial absolute count.
Audio-manager pauses reset gap timing; exclude background/foreground lifecycle
gaps separately using SDL's lifecycle logs. Counters are lock-free on both ARM ABIs;
the callback neither allocates memory nor writes logs. The reported callback
buffer is distinct from the hardware/server buffers shown by AudioFlinger.

Compare song/tone on OpenSL ES, then on AAudio. If a backend removes audible
glitches, verify the same song and a heavier project for ten minutes, with
pause/resume and background/foreground checks. A clean trace without an
auditory check does not establish that the crackling is fixed.

## Callback regression check

From `tracker` in MSYS2 UCRT64, this standalone check uses the real SDL
wrapper and a dummy device. It checks forwarding to the engine, output bounds,
stereo samples and sine phase continuity at 512, 1024, 2048 and 4906 frames:

```sh
g++ -std=c++17 -DANDROID_BUILD -Isrc/corelib -Itests/mocks \
  tests/audio_diagnostics_check.cpp -lSDL2 -o build/tests/audio_diagnostics_check.exe
./build/tests/audio_diagnostics_check.exe
```

## Pixel 7a measurements, 2026-09-16

The phone reported Android 17. The installed Play build was 1.0.4 (version
code 7); experiments used the separately installed companion built from the
same repository state plus diagnostics. Playback used the built-in speaker.

| 30-second capture | Callback frames | FastMixer underrun events | Listening feedback |
| --- | ---: | ---: | --- |
| Installed app, initial reproduction | Not directly readable | 28 | Crackling |
| OpenSL ES, PSY | 2048 | 27 | Crackling |
| OpenSL ES, sine | 2048 | 11 | Crackling |
| AAudio, sine | 2048 | 2 across two mixer threads | Clean |
| AAudio, PSY | 2048 | 0 | Clean |
| AAudio, PSY | 1024 | 0 | Not separately requested |
| AAudio, PSY | 512 | 0 | Clean |
| AAudio, alf dance | 4906 | 0 | Clean |

The OpenSL ES application track stayed supplied while the downstream fast
mixer underrun count increased. The sine callback took less than 0.3 ms for
42.7 ms of sound, so the musical synthesis is not needed to reproduce the
problem. This isolates a problem in the OpenSL ES output path on this phone;
it does not establish which Android scheduler/HAL component is responsible.
AAudio's sine capture retained one early event on the old mixer thread and
one on the new mixer, without an audible glitch reported. Do not describe
the backend as eliminating every system-wide underrun.

Reducing the engine buffer from 2048 to 1024 and 512 frames left the Android
track buffer at 4810 frames and its reported latency at roughly 170 ms.
During the longer 512-frame run, one callback took 11.904 ms, exceeding its
10.667 ms computation budget without a reported audible glitch. The fix
therefore changes the backend priority and preserves buffer settings/defaults.
The user accepted the buffered output's latency for a handheld groovebox.

Raw traces, logs and summaries are under the ignored `.tmp/android-audio-*`
directories. `psy-duration-summary.json` in `.tmp/android-audio-20260916/`
records 341.749 seconds of fully rendered PSY audio across the AAudio trials,
excluding callbacks replaced with silence. A further five-minute monitoring
window on alf dance at 4906 frames recorded 303.559 seconds of rendered audio
since stream open: 645.308 seconds total across the music trials (10 min 45 s,
cumulative rather than one uninterrupted session). Its peak callback was
30.414 ms against a 102.208 ms budget, with zero invalid buffers and zero
over-budget callbacks. The ten short returns around 10:55:36 corresponded to
a stop/restart confirmed by the user. The user also confirmed clean sound on
alf dance. `stability-summary.json` preserves these counters.

Pause/restart and background/foreground checks retained the same process and
reopened the audio path successfully. Existing app behavior intentionally
stops transport in the background; pressing Play after returning resumed
rendering. The callback regression check (including diagnostics disabled),
both Android ABI builds, APK integrity/assets check and all 217 existing test
cases passed (1,888,847 assertions in the existing suite).
