# Launchpad Mini MK3 test on TrimUI Brick / NextUI

The opt-in setting is saved as `launchpadMiniMk3Enabled` and defaults to off.
MIDI Learn and musical MIDI use their existing ports. The controller uses the
dedicated `Launchpad Mini MK3 MIDI 2` input/output pair.

## Build

Push the feature branch and wait for the **Launchpad Mini MK3 hardware test**
workflow. Download the `launchpad-test-arm64` artifact. It builds with the
repository's `Makefile.portmaster NextUI` target and validates AArch64 and
GLIBC compatibility. The workflow packages a separate Pak launcher and
executable; it does not modify the Pak Store package.

## Preserve the installed Pak and deploy the test

Connect the Brick over ADB. The earlier test replaced only the installed
executable and retained the original as the backup named below. Restore that
executable first so the Pak Store installation is back to v1.0.24-alpha.
`launch.sh` and `pak.json` were not changed.

```sh
adb shell 'cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak && cp choochootracker.aarch64.v1.0.24-alpha.backup choochootracker.aarch64 && sha256sum choochootracker.aarch64 launch.sh pak.json'
```

Unpack the downloaded artifact locally, then install it beside the original Pak
under its own NextUI Tools entry:

```sh
adb shell 'mkdir -p /mnt/SDCARD/Tools/tg5040/ChooChooTrackerLaunchpadTest.pak'
adb push launch.sh pak.json choochootracker-launchpad-test.aarch64 /mnt/SDCARD/Tools/tg5040/ChooChooTrackerLaunchpadTest.pak/
adb shell 'chmod 755 /mnt/SDCARD/Tools/tg5040/ChooChooTrackerLaunchpadTest.pak/launch.sh /mnt/SDCARD/Tools/tg5040/ChooChooTrackerLaunchpadTest.pak/choochootracker-launchpad-test.aarch64'
```

Start **ChooChooTracker Launchpad Test** from NextUI's Pak menu so it receives
the normal NextUI audio and controller environment. Do not launch it directly
from an ADB shell; that path previously failed to open ALSA audio on this
Brick. In **Settings → MIDI → Launchpad Mini MK3 Support**, set the option to
**ON**.

## Expected hardware behaviour

- On Song/Scenes, empty cells are off, populated cells blue, playing cells
  green, normal queued cells amber, and urgent queued cells flashing amber.
- Grid columns launch tracks 1–8. Grid rows and right-side scene buttons address
  the eight Song/Scenes rows starting at the shared `topRow` viewport.
- A pad press queues the populated chain; a double press upgrades it to urgent.
  Empty cells queue a normal stop, and a double press upgrades that stop to
  urgent. These use the existing chain/phrase boundary semantics.
- Right-side scene buttons queue all tracks in the row; double press requests
  urgent per-track launches/stops.
- Top-row Up/Down/Left/Right controls use Song screen navigation, moving the
  same TrimUI cursor and scrolling the shared viewport.
- Leaving Song/Scenes clears the Launchpad grid and ignores launcher controls
  while keeping the controller connection open. Returning redraws from current
  song and playback state.
- Unplugging should leave the tracker running. Reconnecting while enabled
  should reopen the dedicated ports and enter Programmer Mode again.
- Musical MIDI and MIDI Learn should continue to work as before.

## Hardware test checklist

Verify on the Brick: setting and Programmer Mode; blue populated cells; normal
and urgent chain launch; normal and urgent empty-cell stop; complete scene
launch; arrow navigation and scrolling; actual queued/playing LED feedback;
clearing and disabling controls off Song/Scenes; and ordinary musical MIDI/MIDI
Learn. Automated tests and a successful ARM64 build do not verify these device
behaviours.

## Rollback

Exit the test app, then remove only its separate Pak directory. The original Pak
remains installed, with its original executable, `launch.sh`, and `pak.json`:

```sh
adb shell 'rm -rf /mnt/SDCARD/Tools/tg5040/ChooChooTrackerLaunchpadTest.pak'
adb shell 'cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak && sha256sum choochootracker.aarch64 launch.sh pak.json'
```

Expected original hashes:

- Executable: `ae50cb05c4b37bd8de4f6ac5e7ebaadf5510bb345c9d69517db4bebf3c544b4a`
- `launch.sh`: `88ae26c6fa0b0c21951190fde9c4bf07614357d127b4ea388ecd264467085396`
- `pak.json`: `a551f6f6e7c632b64c4a509059e745e9cc722333202f24c2399a6e0280dfc9b8`
