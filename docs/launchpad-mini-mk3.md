# Launchpad Mini MK3 control surface

The Launchpad Mini MK3 is an optional control surface for the Song/Scenes
screen on NextUI devices. **Settings → MIDI → Launchpad Mini MK3 Support** is
Off by default and persists across launches.

## Connection

When enabled, ChooChooTracker finds the Launchpad by the advertised
`Launchpad Mini MK3 MIDI 2` port identity, then opens its dedicated MIDI input
and output. It enters Programmer Mode on connection and requests Live Mode
when the setting is disabled or the app exits. These connections are separate
from musical MIDI input/output and MIDI Learn. The connection stays open on
other screens, while Launchpad actions and pattern LEDs are inactive there.

## Song/Scenes layout

- Grid columns map to tracks 1–8. The eight grid rows and right-side scene
  buttons follow the Song/Scenes screen's shared `topRow` viewport.
- Top-row CC 91–94 are Up, Down, Left, and Right. They use the tracker's
  existing navigation input. CC 95 (Session) is ignored.
- A single grid press queues that cell's chain at the normal chain boundary.
  A second press of the same control within the configured double-tap interval
  upgrades the launch to the existing urgent phrase-boundary action. Empty
  cells queue normal or urgent stops through the same live-chain API.
- Right-side scene buttons queue all tracks in the corresponding visible song
  row, with normal or urgent per-track boundaries. Empty scene cells use the
  playback engine's existing stop behavior.
- LEDs reflect playback and queue status: off for empty cells, blue for
  populated, amber for normally queued, flashing amber for urgent queue, and
  green for currently playing. A queued stop is shown on the affected
  track's currently playing cell. Scene-button LEDs summarize the row.
- Leaving Song/Scenes clears grid feedback and disables controller actions.
  Returning redraws from current song and playback state.

Other screens do not have Launchpad mappings yet.

## Verification

Focused mapping and playback tests are in `tracker/tests/test_launchpad_grid.cpp`
and `tracker/tests/test_playback.cpp`. Run the full suite with
`make -f Makefile.test -j4` from `tracker`.

On TrimUI Brick / NextUI, MIDI 2 connection, Programmer Mode, pad input/output,
grid feedback, scrolling, playback, and the corrected top-row navigation were
verified on hardware. Automated tests do not substitute for that device
verification.
