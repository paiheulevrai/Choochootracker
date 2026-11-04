```*        .         *               .            *
   ______ __ __  ____   ____   ______ __ __  ____   ____
  / ____// // / / __ \ / __ \ / ____// // / / __ \ / __ \
 / /___ /  __  // /_/ // /_/ // /___ /  __  // /_/ // /_/ /
 \____//_/ /_/ \____/ \____/ \____//_/ /_/ \____/ \____/

             ______  ___    ___    ______  __  __  ______  ____
            /_  __/ / _ \  / _ |  / ____/ / /_/ / / ____/ / _  \
             / /   / , _/ / __ | / /___  /  _  / / _/   / , _/
            /_/   /_/|_| /_/ |_| \____/ /_/ |_| /____/ /_/|_|

       -=[ MOBILE GROOVEBOX, MULTI-ENGINE TRACKER ]=-
             _________________________________
       _____/  ___                ___         \_____
   ___/_____|_|___|______________|___|_____________\___
  /  _   _   _   _   _   _   _   _   _   _   _         \
 |  |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |RG353| |
 |______________________________________________________|
    O==O       O==O             O==O       O==O
 _.-'--`-.___.-'--`-.___==___.-'--`-.___.-'--`-._
==========================================================>
     CAHORS                                       MONTAUBAN
          >>>  8 TRACKS  / CHIPNOMAD-BASED  >>>

.oO[ SYNTHS - DRUMS - SAMPLING - CHIPS - EFFECTS - SF2 - FM ]Oo.
````
# ChooChooTracker 🚂

<p align="center">   
  <b>A multiengine music tracker for handheld consoles, Android, Linux and desktop computers.</b><br>
  <i>Don't miss your stop.</i>
</p>

<p align="center">
  <a href="https://choochootracker.pages.dev/">🌐 Website</a> &nbsp;·&nbsp;
  <a href="https://choochootracker.pages.dev/play/">🚀 Try online</a> &nbsp;·&nbsp;
  <a href="https://github.com/paiheulevrai/Choochootracker/releases">📦 Downloads</a> &nbsp;·&nbsp;
  <a href="https://discord.gg/Ut9vM6zgKU">💬 Discord</a> &nbsp;·&nbsp;
  <a href="https://www.instagram.com/choochootracker/">📸 Instagram</a>
</p>

---

## 🎧 What is ChooChooTracker?

A music tracker for Portmaster consoles, Android, Linux and computers, with advanced sampling, soundfonts, wavetables, chips, synths, effects, joysticks and more.

Originally forked from [ChipNomad](https://github.com/Megus/chipnomad-tracker), it has evolved into a multiengine tracker combining classic tracking with modern synthesis, sampling, modulation, effects and MIDI.

The project started with a simple idea: build a portable groovebox to make techno in the train, then contributions started and now its like a small music studio in your pocket.

Make music on the move. Sequence, experiment, perform, and don't miss your stop.

---

## 🚧 Project status

**Alpha — actively developed**

ChooChooTracker is functional and capable of producing complete musical projects, but development is ongoing.

- Occasional bugs and crashes may occur.
- Features and workflows are still evolving.
- Sound engines and effects may receive further improvements.
- Look may change.

Save regularly and keep backups of your projects.

Bug reports, feedback, testing and contributions are welcome!

---

## 🚀 Quick start

Get the latest packages from the **[GitHub Releases page](https://github.com/paiheulevrai/Choochootracker/releases)**.

### 🌐 Web app

Try ChooChooTracker directly in your browser:

**[Launch the web app](https://choochootracker.pages.dev/play/)**

Use a keyboard or gamepad on desktop, or the on-screen controls on mobile devices.

### 🕹️ Handheld consoles — PortMaster

ChooChooTracker supports PortMaster-compatible Linux handhelds.

1. Download the latest PortMaster package.
2. Extract the package into your device's `ports` directory.
3. Launch ChooChooTracker from your console's Ports menu.

Tested on ArkOS, DarkOs, Knulli, amberElec, Anbernic and TrimUI devices, among others.

### 🚂 TrimUI Brick / Smart Pro — NextUI

ChooChooTracker is also distributed as a NextUI tool Pak for the `tg5040`
platform. Install `ChooChooTracker.pak.zip` from Pak Store, or extract its
contents into `Tools/tg5040/ChooChooTracker.pak` on the SD card.

### 🕹️ Miyoo Mini / Mini Plus

1. Download the MiyooPorts package 
2. Extract the archive at the root of your SD card, merging the `Roms` and `PORTS` folders with your existing Onion OS installation.
3. Refresh your game collection and launch ChooChooTracker from the Ports menu.

### 💻 Desktop

Native Windows builds and appImage are available for desktop use, development and testing.

**macOS (experimental):** universal Apple Silicon + Intel packages target
macOS 11+. SDL2 and runtime assets are included. Packages are ad-hoc signed,
not notarized; see the [installation notes](docs/USER_MANUAL.md#macos-experimental)
for Gatekeeper and user-file locations. Availability can vary between releases.

### 📱 Android

Android builds are available through Google Play beta testing.

Check the [website](https://choochootracker.pages.dev/downloads/) or join [Discord](https://discord.gg/Ut9vM6zgKU) for current availability and access information.

---

## 🎛️ Key features

Its the tracker we wanted. 

For detailed descriptions, screenshots and examples, visit the **[ChooChooTracker website](https://choochootracker.pages.dev/)**.

---

## 📚 Documentation

- [Visual field guide](https://choochootracker.pages.dev/user-guide/) — Learn the interface through screenshots and examples.
- [User manual](https://choochootracker.pages.dev/manual/) — Instruments, effects, commands and technical reference.
- [Downloads](https://choochootracker.pages.dev/downloads/) — Packages and installation information.

---

## 🛠️ Building 

ChooChooTracker is an SDL2-based application. The project is developed with portability in mind, targeting handheld Linux devices, desktop environments and Android.

Your agent should know what to do.

---

## 🤝 Contributing

ChooChooTracker is a collaborative FOSS project. It is shaped by musicians, developers and tracker enthusiasts who share an interest in portable music-making.

There are many ways to help the project:
- use the app, report bugs or things that hurt the workflow
- make social media content, share your tracks 
- contribute to the code: we accept PRs, read contrib.md

Wanna chat? we're on [Discord](https://discord.gg/Ut9vM6zgKU)

---
## 🚄Origin story

I wanted a mobile groovebox to make techno... but none of the available option ticked all the boxes, so I decided to make one myself by stitching a few open source projects together. 

The first proof of concept was written during an Intercité train ride between Cahors and Montauban, so the name stuck. Choo choo, don't miss your stop.

---
## 🙏 Credits & licenses

ChooChooTracker is a fork of [ChipNomad](https://github.com/Megus/chipnomad-tracker), created by Megus.

It incorporates code and ideas from several generous open-source projects and contributors.

Special thanks to:

- **Megus** — Creator of ChipNomad and the original tracker foundation.
- **Pichenettes / Mutable Instruments** — Braids, Plaits, Clouds and stmlib.
- **Lylepmills** — Additional Plaits engines.
- **Robin Schmidt** — Open303 engine.
- **Signalsmith Audio** — Signalsmith Stretch time-stretching processor.
- **Airwindows** — Audio effects processing.
- **JonET** — Monk Synth.
- **CureAudio** — Roar filter.
- **Filliformes** — Aggro filter.

Thanks also to our contributors:

- **luginf** — Project import exports in many formats, jazz mode, desktop mode improvements, manual translations, etc..
- **aiaaaa** — Chips and FM engines, Monk synth, SlowLFO, Beads, field guide, UI improvement, etc.
- **am0k161** — Sampler engine and sample editor, MIDI, bugfixes, etc.
- **WillyIcon** — UI improvement, bugfixes, etc.
- **Myldy20** — bugfixes

Thanks to the people who shared their misery on the Discord: your reports helped us fix and make future users less miserable!

### Licensing

ChooChooTracker is released under the **[MIT License](LICENSE)**.

Individual components may be subject to their own licenses. See the included license files and attribution notices for details.

**Ableton Link notice:** Official ChooChooTracker releases may include [Ableton Link](https://github.com/Ableton/link) under a separate authorisation. The project's MIT license does not grant rights to Ableton Link; forks, derivative works and third-party distributors must obtain their own authorisation before they build or distribute a product that includes it. See
[Link's license](https://github.com/Ableton/link/blob/master/LICENSE.md).

---

<p align="center">
  <b>Choo Choo! 🚂</b><br>
</p>

<p align="center">
  <a href="https://choochootracker.pages.dev/">Website</a> ·
  <a href="https://choochootracker.pages.dev/play/">Play online</a> ·
  <a href="https://github.com/paiheulevrai/Choochootracker/releases">Downloads</a> ·
  <a href="https://discord.gg/Ut9vM6zgKU">Discord</a>
</p>
