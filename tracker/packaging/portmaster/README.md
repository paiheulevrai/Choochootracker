# ChooChooTracker

ChooChooTracker is a handheld tracker based on ChipNomad. It combines AY/YM synthesis, the 47 accessible Mutable Instruments Braids models, 24 Plaits engines and clean PCM8/16 samples.

## Controls

- D-Pad: move
- A: edit
- B: option
- Start: play
- Select: shift
- Select + D-Pad: change screen
- B + A: clear value

The first PortMaster build targets aarch64 devices such as the Anbernic RG353V. Audio runs at 96 kHz.

## Ableton Link

Ableton Link tempo sync requires Wi-Fi: it only works when the device is on a network with other Link participants (e.g. a phone running LinkHut or a computer running Ableton Live). Enable it on the Project screen (Clock source → Ableton Link, then Connect). Beat alignment is best-effort within roughly 10-15 ms on PortMaster; tempo sync itself is exact.

## Instrument FX

- Braids: `BMD` model, `BTM` timbre, `BCL` color, `BCF` cutoff, `BRS` resonance.
- Plaits: `PMD` engine, `PHA` harmonic, `PTM` timbre, `PMO` morph, `PAX` Main/Aux, `PCF` cutoff, `PRS` resonance.
- Sample: `SPT` pitch, `SST` start, `SEN` end, `SVL` volume, `SCF` cutoff, `SRS` resonance.

Mixer provides per-track level, mute/solo, Clouds Reverb send and tick-synchronized ping-pong Delay send. `PRO`, `MOD` and `SPD` provide conditional triggers and per-track speed. See `USER_MANUAL.md` in the port folder for the complete manual.

Instrument FX use the existing FX columns and remain active until the next note trigger.

ChipNomad is by Megus. Braids, Plaits, Clouds and stmlib are by Emilie Gillet / Mutable Instruments.
