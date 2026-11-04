---
title: It's free!
description: Download ChooChooTracker for your computer or handheld.
eyebrow: Downloads
---
## Find your platform

Get the app from the Releases page of our GitHub repository. Open the newest release and expand **Assets** to choose the package for your device. Package availability can vary between releases.

- **Windows** — download and extract the complete Windows package. Keep the executable, DLLs and asset folders together.
- **macOS (experimental)** — choose the universal Apple Silicon + Intel ZIP, extract it and move ChooChooTracker.app to Applications. Targets macOS 11+; SDL2 and assets are included, with no Homebrew required. The app is ad-hoc signed, not notarized: after trying to open a trusted package, use **System Settings > Privacy & Security > Open Anyway** if blocked. User files live in `~/Library/Application Support/ChooChooTracker`. Real audio/MIDI and Finder validation still need community testing; package availability varies between releases.
- **PortMaster handhelds** — choose the PortMaster package and follow the release's installation notes for your console. Usually just unzip everything into /ports so it sits with your other ports. Miyoo has a specific build; use the zip version.
- **Linux** — look for the x86_64 AppImage for desktop Linux, or the Raspberry Pi package for supported ARM64 systems.
- **Android** — Direct version has fewer restrictions on filesystem access. Store version uses SAF (Storage Access Framework). Android 8 or later.

## An instrument in progress

This is **alpha software**. Features and sound engines are still evolving, and bugs or crashes can happen. Save often and keep backups of songs you care about.

Found a bug? Please report it on GitHub or Discord! Every bug we fix spares hundreds of users a bit of misery!

## Coming from the Vercel web app?

ChooChooTracker has moved to the [new web app](https://choochootracker.pages.dev/play/). Please move there as soon as possible. The old Vercel app will remain available temporarily so you can migrate your browser projects.

Browser storage belongs to each site's address, so your old projects will not appear on the new site automatically. On the Vercel app, download each project you want to keep with **Download .cct**. Keep copies of imported samples and sound banks. Then open the new web app and import your files. Do not clear the old site's browser data until you have finished migrating.
