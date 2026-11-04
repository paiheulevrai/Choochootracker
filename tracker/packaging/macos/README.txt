ChooChooTracker - experimental macOS release
==========================================

Universal application for Apple Silicon and Intel. Targets macOS 11 or later;
automated startup checks run on newer macOS, not every supported OS version.
SDL2, presets, fonts, samples, example projects, manual and licenses are included.
No Homebrew installation is required.

Extract the ZIP, move ChooChooTracker.app to Applications, then open it.
This application is ad-hoc signed, but NOT Apple Developer signed or notarized.
If macOS blocks it, try opening it once, then use System Settings > Privacy &
Security > Open Anyway and confirm. Only allow a package you trust, downloaded
from https://github.com/paiheulevrai/Choochootracker/releases.
Do not disable Gatekeeper globally.

User files, settings and autosave live in:
  ~/Library/Application Support/ChooChooTracker

The first launch copies bundled resources there. Later launches fill in missing
files without overwriting existing ones. Replacing the .app does not remove your
workspace. Back it up regularly. Open .cct projects from the app's Load browser;
Finder document opening is not implemented.

The sampler's audio recording needs microphone access, requested when enabled.
Dummy-audio CI checks do not validate real audio, microphone permissions, MIDI,
Ableton Link, Retina rendering or Finder/Gatekeeper behavior. Community testing
on real Macs is needed before this platform can leave experimental status.

The user manual and third-party licenses are in the application's
Contents/Resources directory (Finder > Show Package Contents).
