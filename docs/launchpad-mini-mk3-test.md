# Launchpad Mini MK3 test on TrimUI Brick / NextUI

The Brick's existing ChooChooTracker Pak Store entry now launches the test
build. The original v1.0.24-alpha executable is kept as
`choochootracker.aarch64.v1.0.24-alpha.backup`; `launch.sh` and `pak.json`
are unchanged. The Launchpad setting is saved as `launchpadMiniMk3Enabled` and
defaults to off. MIDI Learn and the musical MIDI ports remain independent of
the Launchpad's dedicated MIDI 2 pair.

## Build

Push the feature branch to GitHub and wait for the **Launchpad Mini MK3
hardware test** workflow. Download its `launchpad-test-arm64` artifact. The
workflow uses the repository's existing NextUI/PortMaster ARM64 build target
and checks the AArch64 binary and GLIBC compatibility before uploading it.

## Backup and deployment

The test is launched through the existing Pak Store entry so NextUI supplies
its normal audio and controller environment. Do not launch it from an ADB
shell; that path reported an ALSA audio-open failure on this Brick.

Before replacing an installation, save the original executable and record the
launcher and manifest hashes:

```sh
adb shell 'cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak && sha256sum launch.sh pak.json && cp choochootracker.aarch64 choochootracker.aarch64.v1.0.24-alpha.backup'
adb push choochootracker-launchpad-test.aarch64 /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak/.choochootracker-new.aarch64
adb shell 'cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak && chmod 755 .choochootracker-new.aarch64 && mv .choochootracker-new.aarch64 choochootracker.aarch64'
```

Launch **ChooChooTracker** from the existing Pak Store menu entry. Open
**Settings → MIDI → Launchpad Mini MK3 Support** and set it to **ON**. The
test connects to the `Launchpad Mini MK3 MIDI 2` identity, enters Programmer
Mode, and uses the 8×8 grid while the Song screen is active. Set the option to
**OFF** or exit the app to release the controller and request Live Mode.

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

Exit the app first. Restore the saved v1.0.24-alpha executable and check the
original launcher and manifest hashes:

```sh
adb shell 'cd /mnt/SDCARD/Tools/tg5040/ChooChooTracker.pak && cp choochootracker.aarch64.v1.0.24-alpha.backup choochootracker.aarch64 && sha256sum choochootracker.aarch64 launch.sh pak.json'
```

The backup executable hash is `ae50cb05c4b37bd8de4f6ac5e7ebaadf5510bb345c9d69517db4bebf3c544b4a`.
The installed `launch.sh` and `pak.json` hashes remain
`88ae26c6fa0b0c21951190fde9c4bf07614357d127b4ea388ecd264467085396` and
`a551f6f6e7c632b64c4a509059e745e9cc722333202f24c2399a6e0280dfc9b8`.
