ChooChooTracker Linux Package
=======================

This is a Linux build of ChooChooTracker, a handheld music tracker.

REQUIREMENTS:
- SDL2 library (install with: sudo apt install libsdl2-2.0-0)
- x86_64 Linux system

INSTALLATION:
1. Extract this archive to any directory
2. Make sure SDL2 is installed (see requirements above)
3. Run ChooChooTracker using one of these methods:
   - Double-click chipnomad.sh (if your file manager supports it)
   - Run ./chipnomad.sh from terminal
   - Run ./choochootracker directly from terminal

CONTROLS:
- Keyboard mappings can be changed in Settings > Key mapping.
- Gamepad: D-pad, A button (Edit), B button (Opt), Start (Play), Menu (Shift)
- Steam Deck: Add chipnomad.sh as a non-Steam game, or upload this directory
  with SteamOS Devkit Client and use ./chipnomad.sh as the start command.
  Leave Steam Play disabled for this native Linux build. Select a Gamepad
  controller layout. Audio, controls and save/load need an on-device check.

For more information, visit: https://github.com/paiheulevrai/Choochootracker

Compatibility depends on the installed SDL2 and C/C++ runtime versions.
