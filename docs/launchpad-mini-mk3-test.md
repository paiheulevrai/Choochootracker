# Launchpad Mini MK3 test on TrimUI Brick / NextUI

The NextUI test is a separate ARM64 executable and launcher. It does not
replace the Pak Store v1.0.24-alpha executable, `launch.sh`, or `pak.json`.
The Launchpad setting is saved as `launchpadMiniMk3Enabled` and defaults to
off. MIDI Learn and the musical MIDI ports remain independent of the
Launchpad's dedicated MIDI 2 pair.

## Build

Push the feature branch to GitHub and wait for the **Launchpad Mini MK3
hardware test** workflow. Download its `launchpad-test-arm64` artifact. The
workflow uses the repository's existing NextUI/PortMaster ARM64 build target
and checks the AArch64 binary and GLIBC compatibility before uploading it.

## Preserve and deploy

Connect the Brick with USB debugging enabled. Use the Pak Store directory
below and take a backup copy of the existing launcher and metadata before
deployment. The copy is for rollback evidence; the procedure does not write
to those originals.

```sh
adb shell 'cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak && sha256sum launch.sh pak.json'
adb pull /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak/launch.sh ./launch.sh.v1.0.24-alpha.backup
adb pull /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak/pak.json ./pak.json.v1.0.24-alpha.backup
adb push choochootracker-launchpad-test.aarch64 /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak/
adb push launchpad-test.sh /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak/
adb shell 'chmod 755 /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak/choochootracker-launchpad-test.aarch64 /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak/launchpad-test.sh'
```

For launching from an ADB shell, use the same NextUI runtime environment as
the installed Pak:

```sh
adb shell
export SYSTEM_PATH=/mnt/SDCARD/.system/tg5040
export SDCARD_PATH=/mnt/SDCARD
export HOME=/mnt/SDCARD/.userdata/tg5040
export PATH=/mnt/SDCARD/.system/tg5040/bin:/usr/trimui/bin:/usr/sbin:/usr/bin:/sbin:/bin
export LD_LIBRARY_PATH=/mnt/SDCARD/.system/tg5040/lib:/usr/trimui/lib
cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak
./launchpad-test.sh
```

In the test executable, open **Settings → MIDI → Launchpad Mini MK3 Control
Surface** and set it to **ON**. It should connect to the `Launchpad Mini MK3
MIDI 2` identity, enter Programmer Mode, and use the 8×8 grid while the Song
screen is active. Set the option to **OFF** or exit the test app to release the
controller and request Live Mode.

## Expected hardware behaviour

- Empty cells are off; populated cells are blue.
- A queued live chain turns amber; a currently playing chain turns green.
- Pad columns address tracks 1–8. Top-to-bottom rows address `topRow` through
  `topRow + 7`; moving the TrimUI cursor so the Song screen scrolls shifts the
  Launchpad's rows to the same offset.
- Pressing a populated pad queues the existing live chain for that track and
  song row. Empty pads do nothing.
- Unplugging the Launchpad should leave ChooChooTracker running normally;
  reconnecting it while the option remains on should reconnect automatically.

## Rollback

Exit the test executable first. Remove only the two sidecar files and its log;
leave the installed Pak files and settings untouched:

```sh
adb shell 'cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak && rm -f choochootracker-launchpad-test.aarch64 launchpad-test.sh ChooChooTracker-launchpad-test.txt'
adb shell 'cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak && sha256sum launch.sh pak.json'
```

Compare the final hashes with those recorded before deployment. If rollback is
needed while the feature is enabled, set it off and exit the test app before
removing the sidecars so it can send the Live Mode command while connected.
