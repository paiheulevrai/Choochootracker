# ChooChooTracker Gebruikershandleiding
> Dit is een automatisch gegenereerde vertaling van `docs/USER_MANUAL.md` uit de huidige Engelse handleiding. Als er verschillen zijn, geldt de Engelse tekst.
>
> **Conventies.** De applicatie-interface is in het Engels. Houd scherm-, menu-, veld- en knopnamen, FX-codes, module- en motor-namen, en bestandsextensies in het Engels.


ChooChooTracker is een 8-sporen muziektracker voor handheld consoles.

Het volgt de klassieke LSDj workflow en navigatiesysteem, met diverse sound design-opties:

- Emulator van vintage chips en consoles
- Soundfonts
- Mutable Instruments Braids, Plaits en Plaits-Alt oscillatoren, met een multimode filter
- PCM-samples, single-cycle waveforms en wavetable-oscillatoren, met een multimode filter
- aChChid, een veelzijdige acid synth gebaseerd op Open303
- Bogie, een native 12-model drum synthesizer met een multimode filter
- MonkSynth, een vocale synthesizer met tienstemmige unison en stereo delay
- MME, een twee-oscillator multi-modulatie synthesizer met een multimode filter
- Sintered, een native zes-model experimentele digitale drum synthesizer

## 1. Installatie en bestanden

### ArkOS / PortMaster

Installeer `choochootracker.zip` via PortMaster, of kopieer het uitgepakte pakket naar de `ports` map van de console. Start **ChooChooTracker** vanuit het Ports-menu.

### Miyoo Mini / Mini Plus (Onion OS)

Pak het MiyooPorts-archief uit in de root van de SD-kaart zodat de `Roms/PORTS/` mappen samenvloeien met de bestaande Onion-mappen. Start **ChooChooTracker** vanuit het Ports-menu na het verversen van de collectie. Maak een back-up van de app-map `projects/` voordat je een bestaande installatie vervangt.

De laadtijd van een project kan variëren afhankelijk van de instrumenttypes die in de song worden gebruikt. Als opstarten of laden van een project ongewoon traag lijkt, geef dan aan hoe lang elk duurt, samen met het consolemodel en de Onion OS-versie.

### Raspberry Pi OS 64-bit

Pak het Raspberry Pi AArch64-archief uit, installeer de SDL2-runtime (`sudo apt install libsdl2-2.0-0`) en voer `./chipnomad.sh` uit vanuit de uitgepakte map. Houd de meegeleverde instrumenten en overige bestanden naast het uitvoerbare bestand.

### Windows

Start `choochootracker.exe` met `SDL2.dll` en `libwinpthread-1.dll` in dezelfde map.

Windows ondersteunt toetsenbord (zie sectie Jazz mode voor details) of Gamepad.

### Web

Je kunt Choochootracker online gebruiken op https://choochootracker.vercel.app/ .

Je bestuurt deze versie met het toetsenbord, het on-screen keyboard op mobiele apparaten, of je gamepad.

Opgeslagen instellingen, samples, thema’s, projecten etc. worden in de browseropslag bewaard.

Gebruik **Download .cct** om je projecten te exporteren.

Let op: vanwege iOS-beperkingen kan het gebruik van midi met de webversie op iPhone onmogelijk zijn.

### Mapconfiguratie

| Map | Inhoud |
|---|---|
| `AY_Wavetables` | `.aywave` AY chip wavetables |
| `fonts` | `.cnfont` ChipNomad lettertypebestanden |
| `instruments` | `.cni` instrument presets, maar ook FM-banken en Soundfonts |
| `pitch-tables` | `.csv` alternatieve stemmingen |
| `projects` | `.cct` song- en projectbestanden |
| `samples` | `.wav` bestanden voor de Sampler-engine |
| `SR_wavetables` | Serum-formaat wavetables voor de BYOWTBL-engine |
| `themes` | `.cth` ChipNomad-thema’s |
| `title` | Titelscherm assets |
| `waveforms` | `.wav` single-cycle waveforms voor de 2xSCWF-engine |

## 2. Besturing

ChooChooTracker werkt het best met een gamepad met een D-pad, 2 analoge sticks en 7 knoppen.

| Logische bediening | Handheld standaard | Windows standaard |
|---|---|---|
| **[LEFT/RIGHT/UP/DOWN]** | D-pad | Pijltjestoetsen |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Spatie |
| **SELECT** | Select | Shift |
| Modulatie-assen | Linker- en rechterstick | Game-controller sticks |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

Toewijzingen kunnen worden aangepast in **Settings > Controls > Key mapping**, ook op Android met een externe gamepad. Sla op PortMaster een aangepaste mapping op met **Done**, laat dan de knop los voordat je verdergaat in Settings.

### Navigatie

- Gebruik **[UP/DOWN/LEFT/RIGHT]** om de cursor te bewegen.
- Houd **SELECT + [RICHTING]** ingedrukt om tussen schermen te schakelen.
- Houd **OPT + [RICHTING]** ingedrukt voor scherm-specifieke navigatie.
- Op een waarde, gebruik **EDIT + [LEFT/RIGHT]** voor fijne aanpassingen of **EDIT + [UP/DOWN]** voor grove aanpassingen.
- Tik op **EDIT** om een waarde in te voeren of een commando te activeren.

### Selectie en klembord

- **SELECT + OPT** schakelt selectie modus in en doorloopt nuttige selectiebereiken.
- Gebruik **[RICHTING]** om het bereik uit te breiden.
- Tik en laat **OPT** los om de selectie te kopiëren en selectie modus te verlaten.
- **OPT + EDIT** knipt de selectie.
- Buiten selectie modus, houd **SELECT** ingedrukt en druk op **EDIT** om te plakken.
- **EDIT + [RICHTING]** bewerkt de geselecteerde cellen samen.

### Afspelen

- **PLAY** start bij de cursor. Druk nogmaals om te stoppen.
- Je kunt noten, instrumenten en chains aanpassen tijdens het afspelen; wijzigingen zijn te horen bij de volgende sequencer-tick.
- **SELECT + PLAY** start alle sporen als het buiten het Song-scherm wordt gebruikt.
- **EDIT + PLAY** speelt een instrument af vanuit het Instrument Pool.

### Global Bounce

Selecteer een bereik van Song-, Chain- of Phrase-rijen en kolommen, en dubbelklik op **EDIT** om het BOUNCE TO SAMPLE scherm te openen. De bounce bevat elke geselecteerde track: tracks waarvan de eerste chain later begint in de selectie wachten stil en starten bij hun eerste chain, zodat het bestand alle geselecteerde chains bevat.

Het scherm biedt:

- **Bestandsnaam**: bewerkbaar met het tekens toetsenbord. Bij openen wordt het eerstvolgende vrije volgnummer (`001`, `002`, ...) voorgesteld: het eerste nummer waarvan het bestand `.wav` nog niet bestaat in de exportmap. Pas vrij aan — de naam wordt precies zo gebruikt. Bestaat een bestand met die naam al, dan wordt er een `_001`, `_002`, ... achter gezet om overschrijven te voorkomen. De teller reset bij laden of nieuw project.
- **Sample rate** en **Bit depth**: dezelfde opties als in het exportscherm
- **Opnemen in sample naam**: drie selectievakjes die context-tags voor de bestandsnaam plaatsen bij het starten (standaard uit):
  - **[BPM]** — huidige tempo van het project, bv `[120]`
  - **[Key]** — root noot en toonladder, bv `[Cmaj]`, `[F#min Pent]`
  - **[Bars:Beats:16ths]** — lengte van de bounce: één phrase is één maat (16 zestienden), een beat 4 zestienden. Volledige phrases tonen alleen maten, bv `[2]`; gedeeltelijke `[0:2]` (twee beats) of `[0:1:3]` (één beat + drie zestienden). Bij multitrack bounces geldt de langste track. Een selectie die midden in een beat eindigt telt één zestiende minder: de laatste geselecteerde rij is de snijpunt, dus het noot-uit moment valt op het einde van de vorige zestiende.
  - Tags worden in deze volgorde toegepast: `[BPM][Key][Length]` vóór de bestandsnaam.
- **Start**: render de selectie naar een WAV-bestand in de exportmap (zie *Export location* in het Project scherm)
- **Cancel**: terug zonder te bouncen

Tijdens renderen annuleert **OPT** de bounce. Na afronden of annulering keert het scherm terug waarvandaan je boog.

## 3. Song-structuur en schermoverzicht

Houd **SELECT + [RICHTING]** ingedrukt om tussen schermen te schakelen.

De hoofd-navigatierij is:

<pre>
        I
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

Middenrij:

- **M**: Mixer (vertakt naar Reverb, Delay)
- **S**: Song (vertakt naar Project, Settings)
- **C**: Chain
- **P**: Phrase (vertakt naar Groove)
- **I**: Instrument (vertakt naar Modulation en Instrument Pool)
- **T**: Table (vertakt naar de AY Wavetable editor)

Bovenste rij:

- **R**: Reverb
- **P**: Project
- **G**: Groove
- **M**: Modulation
- **I**: Insert FX

Onderste rij:

- **D**: Delay
- **S**: Settings
- **P**: Instrument Pool
- **W**: AY Wavetable editor

Een ChooChooTracker song volgt deze hiërarchie:

```text
Song -> Chain -> Phrase rij -> Noot + Instrument + FX
```

Een song bevat 8 tracks. Elke track bevat een reeks chains, en elke chain een reeks phrases. Phrases zijn 16-step patronen met noten. Een noot bevat een pitch, een instrument, velocity en tot 3 track FX. Een instrument bepaalt het geluid.

## 4. Song, Chain en Phrase

### Song

Het Song-scherm toont 8 kolommen, 1 per audiotrack. Elke cel verwijst naar een chain. Een song kan tot 256 rijen bevatten (`00-FF`).

Tracks lopen van links naar rechts, hun chains verticaal. Elke track is onafhankelijk en chains kunnen 1 tot 16 phrases bevatten, waardoor tracks uit sync kunnen raken. Ontwerp chains om synchroon te blijven, of laat ze uitlopen als de track dat vereist.

Het is gebruikelijk in de tracker-community om 1 chain te reserveren voor lege phrases, meestal `00` of `FE`.

Je kunt meerdere sub-songs in een project houden door “eilanden” te maken: secties gescheiden door lege rijen.

#### Live Mode

Dubbelklik **OPT** om Live Mode in of uit te schakelen; `LIVE` vervangt de Song titel. In Live Mode start **PLAY** de geselecteerde chain op de eigen track als die track stopt. Bij een spelende track wordt de geselecteerde chain in de wachtrij gezet tot de volgende chain-grens; druk tweemaal op **PLAY** voor een noodwissel bij de volgende phrase-grens. Een geselecteerde lege Song-cel zet een gekwantiseerde stop in wachtrij. `+` markeert de wachtrij, `!` een noodwissel en `-` een wachtrij-stop.

Selecteer één Song-rij over meerdere kolommen om die tracks tegelijk te starten of stoppen. Meerdere geselecteerde rijen zijn niet toegestaan. Live wachtrijen worden geleegd als een Song-loop actief is. Live is een performance-modus en wordt niet opgeslagen.

#### Bediening

- **OPT + [UP/DOWN]**: spring 16 posities omhoog of omlaag
- Selecteer een bereik en gebruik **SHIFT + EDIT**: vlakke kopieer van chains zonder phrases te veranderen
- Selecteer een bereik en dubbelklik **SHIFT + EDIT**: diepe kopie van chains én phrases
- Selecteer een bereik en gebruik **EDIT + [UP/DOWN]**: verplaats selectie omhoog of omlaag
- Tik 3x op **OPT**: voeg een markering toe of verwijder voor visuele organisatie
- **OPT + SHIFT**: demp de huidige track of geselecteerde kolommen (laat eerst **OPT** los om demping te behouden)
- **OPT + PLAY**: solo de huidige track of geselecteerde kolommen (laat eerst **OPT** los om solo te behouden)
- **OPT + [LEFT/RIGHT]**: solo alle tracks links of rechts van de huidige track
- Selecteer een bereik en dubbelklik op **EDIT**: open het BOUNCE TO SAMPLE scherm (zie Bounce onder Song)

Playback stoppen en mute of solo wijzigen gebruiken een korte output-correctie van 3 ms om klikken te verminderen. Dit geldt ook voor het opheffen van mute en het verwijderen van solo's tijdens playback.

### Chain

Een chain is een geordende lijst van 16-step phrases met optionele transpositie. Een chain kan tot 16 phrases bevatten, en dezelfde phrase kan meerdere keren voorkomen. De 2e kolom bepaalt de transpositiewaarde in halve tonen.

Een sterretje (`*`) verschijnt naast een chain die opnieuw gebruikt wordt in het project. Je kunt chains klonen vanaf het Song-scherm.

#### Bediening

- **OPT + [LEFT/RIGHT]**: wissel tussen tracks
- **OPT + [UP/DOWN]**: wissel tussen chains in de huidige track
- Selecteer een bereik en gebruik **SHIFT + EDIT**: cloneer phrases
- Selecteer een bereik en dubbelklik **EDIT**: open het BOUNCE TO SAMPLE scherm voor de geselecteerde chain-rijen van de huidige track (zie Bounce in Song)

### Phrase

Een phrase is het patroon van de track in een traditionele step sequencer.

Een phrase is 16 stappen lang, met 1 rij per stap. Elke rij bevat een noot, een instrument, een volume van `00` tot `7F` en FX-kolommen. Noten gebruiken tracker-notatie zoals `C-4` (noot en octaaf).

Om een noot te stoppen, gebruik je `NOTE OFF`, de kill-note FX, of speel een andere noot zonder instrument.

Als een chain meerdere phrases bevat, werkt het Phrase-scherm als een doorlopende pattern editor. Een beweging voorbij rij `F` of onder rij `0` opent de volgende of vorige phrase in de chain.

Een sterretje (`*`) verschijnt naast een phrase die opnieuw gebruikt wordt in het project. Je kunt phrases klonen vanuit het Chain-scherm.

De FX-selector toont veelgebruikte commando’s plus die ondersteund door het instrument in die rij. Zo is er geen lange globale lijst met engine-specifieke commando’s. ChooChooTracker ondersteunt meer FX dan ChipNomad.

### Bediening- **OPT + EDIT** op een lege noot: voeg `NOTE OFF` in
- **EDIT + [OMHOOG/OMLAAG]** op een FX-naamkolom: open het FX-selectiescherm
- **OPT + [LINKS/RECHTS]**: wissel tussen tracks
- **OPT + [OMHOOG/OMLAAG]**: wissel tussen phrases in de huidige chain
- Selecteer een bereik in de instrumentenkolom, gebruik dan **SHIFT + EDIT**: kloneer instrumenten
- Selecteer een bereik, gebruik dan **EDIT + [OMHOOG/OMLAAG]**: roteer de phrase-rijen
- Selecteer een bereik, dubbelklik op **EDIT**: open het BOUNCE TO SAMPLE-scherm voor de geselecteerde phrase-rijen van de huidige track (zie Bounce onder Song)

Op dit scherm kan, met een actieve selectie, de notenkolom gevuld worden met ritmepatronen:

- **OPT + LINKS** vult de geselecteerde notencellen met een vast 16-stappenpatroon
  en gaat bij elke druk naar het volgende patroon. Na het laatste patroon worden
  de noten verwijderd en begint de cyclus opnieuw. Patronen worden uitgelijnd op
  absolute phrase-rijen, dus een gedeeltelijke selectie krijgt alleen de
  stappen die daarin vallen.
- **OPT + RECHTS** vult de geselecteerde notencellen met een willekeurig patroon
  van 3-8 noten (7 en 8 zijn minder waarschijnlijk), dat oneindig doorloopt zolang
  **OPT** ingedrukt wordt gehouden.
- **OPT + OMHOOG** verandert de reeds geplaatste noten in de selectie: elke druk
  kiest 1-3 ervan en verschuift elke noot met een willekeurige waarde binnen een
  octaaf (kleine verschuivingen zijn waarschijnlijker dan extreme). De 8e druk
  herstelt de oorspronkelijke noten bij het starten van de cyclus, waarna de
  cyclus opnieuw begint.
- **OPT + OMHOOG** op een selectie die alleen de volumekolom beslaat (geen noot-,
  instrument- of FX-cellen) randomiseert in plaats daarvan de velocities: de eerste
  druk past het Minimal-patroon toe (elk volume verschuift maximaal +-20), de
  tweede Medium (maximaal +-50), de derde Random (volledig willekeurige waarden),
  en de vierde herstelt de volumes zoals ze bij de start van de cyclus waren.
  Loslaten van **OPT** zet de cyclus terug naar Minimal. Rijen zonder volume blijven
  ongemoeid.
- **OPT + OMLAAG** verspreidt de geplaatste noten van geslicede Sampler-instrumenten
  in de selectie in een chromatische loop: elke eerste noot van een instrument
  (hoogste in de sequencer) behoudt zijn waarde, en elke volgende noot van dat
  instrument stijgt één halve toon vergeleken met de vorige, waardoor een stapel
  identieke noten verandert in opeenvolgende slice-triggers. Meerdere geslicede
  instrumenten verspreiden onafhankelijk van hun eigen eerste noten, noten van
  instrumenten zonder slicing blijven onveranderd, en het resultaat wordt nooit
  aangepast aan de scaleset van het project. De laatste noot die door de spreiding
  wordt geschreven, wordt de laatst gebruikte noot in de nootinvoer, zodat het
  handmatig voortzetten van de loop de volgende slice pakt. Een selectie die alleen
  geslicede instrumenten bevat, verspreidt op elke druk; aanwezig zijn ook niet-
  geslicede noten, dan spreidt het op de eerste druk en neemt de random arp die noten
  vanaf de tweede druk over.
- **OPT + OMLAAG** op een selectie zonder geslicede instrumentnoten is een willekeurige
  arpeggiator voor de reeds geplaatste noten. Er wordt een akkoord gekozen uit de CRD-
  palette - 3-noot-akkoorden voor selecties van 1-2 noten, 3+ noot-akkoorden voor 3,
  elk akkoord voor 4 of meer noten - met de grondtoon op een willekeurige noot binnen
  de schaal van het project binnen het register van de selectie (elke chromatische
  noot als de schaal uitstaat of de track ervan is uitgesloten), en zijn tonen worden
  gequantized in de schaal zoals de weergave dat doet voor CRD. Elke druk verdeelt het
  akkoord opnieuw over de geplaatste noten: willekeurig, laag-naar-hoog, hoog-naar-
  laag, vervolgens op-en-neer; de 5e druk herstelt de originele noten en de volgende
  kiest een nieuw akkoord. De akkoordnaam verschijnt in de statusbalk zolang de arp
  actief is, er worden geen noten toegevoegd of verwijderd, en loslaten van **OPT**
  zet het hele proces terug.
- Elke vul-actie verwijdert eerst de noten in de selectie voordat wordt geschreven,
  en geplaatste noten behouden het laatst gebruikte instrument; loslaten van **OPT**
  zet de cycli terug op hun eerste stap.

### Key jazz (alleen desktop)

Druk op **Esc** in het Phrase-scherm om key jazz in of uit te schakelen, een QWERTY-
piano-achtige noteninvoermodus (zoals [m8c](https://github.com/laamaa/m8c)):

- `Z S X D C V G B H N J M` en `Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P` typen noten
  chromatisch over meer dan twee octaven, gebruikmakend van elke toets zijn fysieke
  positie zodat de indeling hetzelfde is op AZERTY-, QWERTY- en QWERTZ-toetsenborden.
- Elke toetsaanslag schrijft de noot op de huidige rij en gaat naar de volgende rij.
- `[` / `]` verschuiven het octaaf.
- **Shift + pijltjestoetsen** selecteert een bereik van rijen en kolommen; een gewone
  pijl erna wist de selectie.
- **Ctrl+C** / **Ctrl+X** kopieert of knipt de geselecteerde kolommen, of de noot/
  instrument/volume van de huidige rij als niets is geselecteerd; **Ctrl+V** plakt
  op de cursorpositie.
- **Delete** verwijdert hele rij(en) (alle kolommen) en schuift de rest van de phrase
  omhoog om de ruimte te vullen; de cursor blijft op dezelfde rij.
- **Backspace** verwijdert het element onder de cursor in de huidige kolom alleen
  (of die van de selectie, als actief), schuift alles eronder omhoog en zet de cursor
  een rij terug, als in een teksteditor.
- **Insert** voegt een lege rij in en duwt de rest van de phrase naar beneden.
- **Ctrl+S** slaat het project op. Buiten key jazz doet Ctrl+S niets.
- Esc opnieuw zet key jazz uit.

Terwijl key jazz aanstaat, neemt het alle noottjestoetsen volledig over in het Phrase-scherm,
ook Edit/Opt/Motion-sneltoetsen die dezelfde toetsen gebruiken — zet het uit met Esc om
die sneltoetsen weer te gebruiken.

Key jazz werkt ook, onafhankelijk in/uit te schakelen met Esc, op:

- **Song** en **Chain**: typ de hex-index van een chain of phrase direct (`0-9`, `A-F`)
  in plaats van verhogen/verlagen met Omhoog/Omlaag. Het eerste cijfer op een cel
  vervangt de waarde; volgende zonder te verplaatsen schuiven in die cel (bijv. "3"
  dan "F" zet 3F). In Song selecteert **Shift+pijltjestoetsen** een bereik, **Delete**
  verwijdert hele rij(en) en schuift omhoog, **Backspace** verwijdert de huidige/
  geselecteerde kolom(men) en schuift omhoog, **Insert** voegt een lege rij in, en
  **Ctrl+C**/**Ctrl+X**/**Ctrl+V** kopieert/knipt/plakt de geselecteerde rijen (of de
  huidige rij als niets geselecteerd) – dezelfde structuur-bewerkingssneltoetsen als
  in Phrase. Dit neemt Shift over, dus Shift+Rechts/Omhoog springen niet meer naar
  Chain/Project zolang key jazz hier actief is – Esc brengt die sneltoetsen terug.
- **Elk scherm met een tekstveld**: typ direct op het toetsenbord in plaats van het
  virtuele schermtoetsenbord. Dit geldt voor bestandsnaam project, titel en auteur,
  instrumentnaam, kleurthemanaam, pitch table naam, bounce-bestandsnaam en naamdialogen
  (naam invoeren, map maken). Backspace werkt als in normale tekstvelden; Shift typt
  hoofdletters. Esc schakelt het voor al deze schermen tegelijk aan/uit, het virtuele
  toetsenbord opent normaal als key jazz uitstaat.
- **Instrument**: met key jazz aan spelen de noottjestoetsen het instrument van de
  huidige track om het te beluisteren, zonder een noot in te voeren (dezelfde piano-
  layout als het Phrase-scherm; de laatst ingedrukte toets klinkt, geluid stopt na
  loslaten). De octaaftoetsen schuiven het octaaf, startend bij het octaaf van de
  eerste noot van het instrument. In het **Naam**-veld typen de toetsen tekst. Er
  klinkt niets als het nummer die track afspeelt.

## 5. Instrumenten

Alle instrumenten hebben een paar gemeenschappelijke parameters:

- Instrumenttype
- Naam, tot 15 tekens
- Standaard tabelsnelheid, in ticks per tabelrij
- Volume
- Panning
- Transpose aan of uit

Elk instrument heeft een standaardtabel met hetzelfde nummer in het `00-7F` bereik.
Je kunt deze als hulptabellen gebruiken, maar hulptabellen in het `80-FE` bereik bewaren
voorkomt conflicten en verwarring. Tabellen zijn één van de belangrijkste geluidsontwerptools
in ChooChooTracker.

### Bediening

- **OPT + [LINKS/RECHTS]**: wissel tussen instrumenten
- **EDIT + PLAY**: preview het instrument
- **SHIFT + OPT**: kopieer het instrument
- **SHIFT + EDIT**: plak het instrument

### SOUNDFONT (experimenteel)

Kopieer originele SF2/SF3-banken, SFZ/EXS-instrumenten met hun samples, of ZIP-archieven
naar `instruments/USER/soundfont`. Kies **SOUNDFONT** onder Instrument. De scan-rij
toont **Not scanned**, **Scanning...**, of **Scanned**, met een **Rescan**-actie.
Scannen leest metadata, het laadt niet elk preset-sample in het geheugen.

De **Preset**-rij toont het actieve preset. Tik en los EDIT om de browser op dat preset te
openen: collecties verschijnen links, hun presets rechts. Elke collectie toont een vlakke
lijst, inclusief presets in bankbestanden en ZIP-archieven. Ondersteunde WAV/AIFF-bestanden
blijven gekoppeld aan hun gemapte instrumenten; collecties met alleen losse samples tonen
die als presets. Download-ID's en archief-hashes verdwijnen uit labels; muzikale noot, BPM
en take-nummers blijven zichtbaar. EDIT selecteert een preset; EDIT+PLAY speelt de
geselecteerde preset af.

Houd EDIT ingedrukt en druk Links/Rechts op Preset om sequentieel te bladeren in dezelfde
volgorde als de browser, door de volgende collectie en wrap-around aan beide kanten. Snel
wisselen bereidt de laatst gevraagde selectie voor. Het huidige preset blijft speelbaar
terwijl het nieuwe wordt klaargemaakt. Lukt het laden niet, blijft het aangevraagde preset
geselecteerd en wordt het instrument stil. **Error:** met de reden verschijnt onderin
Instrument; delen gaan door vanaf dat preset in beide richtingen. Rescan na importeren
of wijzigen van bibliotheekbestanden. Preset-status verschijnt direct onder Preset in een
vaste kleur. Laden toont geen meldingen op SONG of andere pagina’s.

**Native keys** toont het noot-on bereik van het geselecteerde preset, met C4 als MIDI noot 60.
**(gaps)** betekent dat binnen die grenzen sommige noten geen gemapte zone hebben. Dit is
de unie over velocity-lagen; niet elke velocity of articulatie hoeft op elke getoonde toets
te klinken. Samples alleen voor release verlengen de range niet. Tuning en pitch FX wijzigen
deze weergave of de aard van de mapping niet.

**Edit**, rechts van Preset, opent de experimentele SoundFont-editor: tune (−2400 tot +2400
cents), cutoff (20–20000 Hz), resonance (0–40 dB), envelope delay/attack/hold/decay/release
(0–20000 ms), sustain (0–100%), volume, pan, en **Bend step** (1–100 cents per PBN-eenheid).
Nieuwe instrumenten gebruiken 1-cent bend steps. Oudere bestanden tonen **Stock**, die hun
vorige PBN-schaal behouden (25 cents in lineaire pitch-modus, 1 in period-modus); zet Bend
step op 1 voor fijnere bends. Clear zet terug naar 1-cent bend step. Dit beïnvloedt alleen
SoundFont PBN; FPT blijft vaste tuning. Volume en pan zijn dezelfde instrumentwaarden als
op de homepage. Toon- en envelope-velden zijn standaard **Bank**, die elke laag haar
oorspronkelijke waarde behoudt. Een expliciete waarde overschrijft die parameter in de hele
preset. Clear zet terug op **Bank**, 0 cents voor Tune, volle volume of gecentreerde pan.
EDIT+Links/Rechts maakt kleine aanpassingen; EDIT+Omhoog/Omlaag grotere. OPT keert terug
naar Instrument; EDIT+PLAY speelt het instrument af met zijn songnoot of standaard.
Hiermee bewerk je de instrumentafspeelcontrole, niet de bank’s mapping per sample, key/
velocity zones of loop points.

Tuning, cutoff en resonance zijn direct hoorbaar. Attack, delay en hold worden ingesteld bij
noot-aan. Decay, sustain en release kunnen bij een spelende stem worden aangepast; release
start bij een note OFF, niet alleen bij een lege rij. Decay is niet hoorbaar bij 100% sustain.
Release kan een sample niet verlengen voorbij zijn beschikbare audio. Een expliciete release-
override zorgt dat een one-shot sample reageert op note OFF. Met Bank-instellingen blijft
het gedefinieerde one-shot gedrag bewaard. Key- en velocitybereiken van het preset blijven
leidend; noten buiten bereik zijn stil. FPT kan een al geselecteerde sample pitch-shiften buiten
dat bereik zonder de mapping te veranderen. Bewerking wordt opgeslagen in instrument- en
projectbestanden zonder het bronbankbestand te wijzigen; oudere nummers zonder overrides
behouden hun oorspronkelijke bankinstellingen.Een WAV met een onbekend sampler-specifiek looptype kan de audio afspelen zonder die loop. De Instrument-waarschuwing noemt het genegeerde looptype; het bronbestand blijft ongewijzigd. Een forward WAV-loop die precies één sample voorbij het audio-einde ligt, wordt geaccepteerd als een exclusief einde op de samplegrens, met een waarschuwing; de PCM- en expliciete SFZ/EXS-looptoewijzingen blijven ongewijzigd. Andere ongeldige loopgrenzen en root-noten melden specifieke fouten. Een enkele nul RIFF-alignment-byte die per ongeluk is meegeteld in een oneven grote PCM-frame-indeling wordt genegeerd met een waarschuwing; andere gedeeltelijke PCM-frames blijven fouten. Standaard alternatieve/achterwaartse WAV-loops worden nog niet ondersteund.

**SoundFont FX** biedt acht bedieningselementen in de contextuele Lane FX-zoeker. Alle waarden hieronder gebruiken tracker-hexadecimale notatie. De FX beïnvloeden alleen hun eigen track en laten de opgeslagen instrumentinstellingen ongewijzigd. Elke nieuwe noot start vanuit de instrumentinstellingen en past vervolgens commando’s toe op de huidige rij en volgende rijen.

| FX   | Bediening | Waarden                                          |
|------|-----------|-------------------------------------------------|
| FPT  | Pitch     | `00` = −24 halve tonen, `18` = nul, `30` = +24; exacte halve toon stappen |
| FCF  | Cutoff    | `00`–`FE`: 20 Hz–20 kHz, logaritmisch           |
| FRS  | Resonantie| `00`–`FE`: 0–40 dB                              |
| FAT  | Attack    | `00`–`FE`: 0–20 seconden, fijnere spreiding bij korte tijden |
| FHD  | Hold      | `00`–`FE`: 0–20 seconden voor decay, ingesteld op de nootrij |
| FSU  | Sustain   | `00`–`FE`: 0–100% aangehouden niveau            |
| FDC  | Decay     | `00`–`FE`: 0–20 seconden, fijnere spreiding bij korte tijden |
| FRL  | Release   | `00`–`FE`: 0–20 seconden, fijnere spreiding bij korte tijden |

Voor FCF/FRS/FAT/FHD/FDC/FSU/FRL zorgt `FF` dat het terugkeert naar de Edit-instelling (inclusief **Bank**). Zet Attack/Hold op de nootrij. Decay/Sustain/Release kunnen de actieve noot veranderen; gebruik OFF om de release te horen. Volume en pan gebruiken de gebruikelijke bedieningselementen; legacy FVL/FPN-commando’s laden en spelen nog wel maar worden niet meer aangeboden. Pitch FX veranderen de stemming zonder de geselecteerde key zone van de bank te wijzigen. Bestaande sequencer pitch/fine-pitch-commando’s, MODs en track-effecten blijven beschikbaar. Projecten slaan stabiele bank/preset-referenties en FX-commando’s op; samplebibliotheken blijven extern.

Alle banken zijn USER-banken. Grote instrumenten kunnen het standaard 64 MiB PCM-cache overschrijden, en niet-ondersteunde bronparameters geven waarschuwingen. Zie de [SoundBanks gids](soundbanks/README.md) voor platformpaden, browserimport, cacheconfiguratie, formaten-ondersteuning en actuele validatielimieten.

#### SoundFont-compatibiliteit: banken, presets en modulatie

Een SF2- of SF3-**bankbestand** kan meerdere **presets** bevatten, samen met hun samples. Een bibliotheek**collectie** is de map die wordt gebruikt om die bestanden in de browser te groeperen. Dit zijn verschillende niveaus: een preset kan ongewenst gedrag vertonen, zelfs als andere presets in hetzelfde bestand goed werken.

**Preset modulatie** betekent geprogrammeerde koppelingen van invoer zoals velocity, toets, modwheel of druk naar een geluidsparameter. Die koppelingen kunnen bij een preset horen of bij een onderliggend instrumentlaag dat door meerdere presets wordt gedeeld. Ze zijn niet per se een instelling die elk geluid in de bank beïnvloedt.

| Presetfunctie                      | Huidige vertaling                                                                                   |
|-----------------------------------|---------------------------------------------------------------------------------------------------|
| Sample/toets/velocity mapping, root pitch en tuning | Geïmplementeerd; geauthorde bereiken worden behouden. Velocity zones en gewone tracker velocity werken nog steeds. |
| Sample offsets, ondersteunde loops, gain/pan en exclusive groups | Geïmplementeerd binnen parser en voice-limieten.                                                  |
| Statische low-pass cutoff/resonantie en volume-envelope | Geïmplementeerd via de tracker-renderer; geen garantie op identieke respons als een andere SoundFont-speler. |
| Aangepaste preset/instrument modulatie routes (`pmod` / `imod`) | Niet geïmplementeerd. Geauthorde controller-naar-parameter mappings, hoeveelheden en responscurves worden genegeerd. |
| SoundFont modulatie/vibrato LFO’s en modulatie envelope-routing | Niet geïmplementeerd; geprogrammeerde beweging van pitch, filter of volume ontbreekt mogelijk.      |
| Key-scaled volume-envelope hold/decay, fixed-key/fixed-velocity generators | Niet geïmplementeerd.                                                                             |
| SoundFont chorus/reverb sends     | Niet geïmplementeerd. Tracker-effecten kunnen handmatig worden ingesteld maar reproduceren niet automatisch de preset-instellingen. |

Het oudere bericht **“Custom bank modulation ignored”** beschrijft een controle op bestandsniveau: de parser vond ergens in de bank aangepaste modulatierecords. Dit betekent **niet** dat de geselecteerde preset ze gebruikt, of dat de hele bank onbruikbaar is. Deze algemene beperking wordt hier gedocumenteerd in plaats van het bericht onder elke preset te herhalen. Gedetailleerde parserdiagnoses bewaren die records; andere laads fouten en compatibiliteitswaarschuwingen verschijnen wanneer van toepassing. De engine claimt ook geen volledige SoundFont default-modulator-gedrag. Tracker PBN, vibrato, Edit en Lane FX zijn onafhankelijke bedieningselementen, geen automatische vertalingen van weggelaten preset-programmering.

Voor het kiezen van een bibliotheek, begin met presets die vooral gebaseerd zijn op sample-mapping, loops en statische envelopes. Geluiden waarvan het karakter afhangt van controller articulaties, geanimeerde filters of geprogrammeerde LFO’s hebben een nader vergelijk nodig in een volledige SoundFont-speler. **Laadt en speelt** betekent niet **volledig reproduceert de preset**. Vergelijk nuttige noten en velocities evenals aangehouden noten en note OFF. Het veranderen van SF2 naar SF3 (of terug) verwijdert deze modulatiebehoeften niet automatisch. Een conversie moet de mappings en het gedrag vertalen naar functies die deze engine ondersteunt; het renderen van een effect in een sample behoudt alleen dat resultaat, niet de interactieve controllerrespons. Zie de [format coverage and limits](soundbanks/README.md#format-coverage) en [validatienotities](soundbanks/validation.md) vóór het kiezen of converteren van een bank. Deze limieten beschrijven de huidige experimentele implementatie, niet restricties inherent aan alle SoundFont-spelers.


#### SoundFont-collecties doorbladeren

In SAMPLE → SOUNDFONT opent **Preset** je collecties. **Right** of **EDIT/A** gaat direct een map in. In een eindcollectie gaat Right naar de presets; nog een Right springt 15 items verder zonder te laden. EDIT/A laadt de gemarkeerde preset. Left gaat terug naar de collecties en dan naar de bovenliggende map; OPT gaat ook terug. PLAY start/stopt de song tijdens het bladeren.

Map- en geluidsiconen identificeren de items. Collecties gebruiken de volledige hoogte van het paneel. De geselecteerde preset blijft onder de acht trackpreviews staan; lange namen scrollen na een seconde stilstand op de selectie en resetten zodra je beweegt. EDIT+Left/Right op de instrumentpagina volgt dezelfde volgorde van geneste mappen, waarbij de geluiden van elke collectie bij elkaar blijven. EDIT+PLAY beluistert.

De derde rij meldt Reading preset, Finding samples, Unzipping, Checking samples of Loading samples. EXS-instrumenten met lege of `(null)` ingebedde namen gebruiken hun bestandsnamen. De bronbanken en audio veranderen niet. Gebruik **Rescan** nadat je de bestanden van je bibliotheek hebt gewijzigd.

Zie [formaatdekking en limieten](soundbanks/README.md#format-coverage) voor het ondersteunde samplergedrag en de sampleformaten.

### AY Classic, AY Plus en AY Sample

Dit zijn de originele ChipNomad AY/YM-engines. AY Classic biedt hardware-achtige toon-, noise- en envelopbedieningselementen. AY Plus voegt software-oscillatoren en rijkere modulatie toe. AY Sample speelt een sample af via AY-stijl volumeregeling en is een apart instrumenttype dan de Sampler-engine.

#### AY Classic

Dit is een legacy instrumenttype uit ChipNomad `v1.0.0`. Het nieuwere AY Plus-instrument biedt meer functies.

AY Classic-instrumenten hebben deze parameters:

- Mixer: toon aan/uit, noise aan/uit, envelopvorm (`0-F`)
- Volume: softwaregegenereerde ADSR-envelope
- Automatische envelopperiode: aan of uit, met een snelheid van `1:1` tot `F:F`

**AY Quality** in Settings beïnvloedt alleen AY/YM-rendering. Het verandert niet de kwaliteit van Braids, Plaits of Sampler. **Sample dithering** geldt voor AY Sample-kwantisatie.

Zie de [ChipNomad AY-3-8910 documentatie](https://chipnomad.org/chips/ay-3-8910/) voor chipdetails.

#### AY Plus

AY Plus past klassieke synthesizerconcepten toe op AY-3-8910/YM2149F-geluid. Je kunt AY beschouwen als een synthesizer met 5 oscillatoren:

- 3 vierkante golf toonoscillatoren
- 1 noise-oscillator
- 1 amplitude-envelopgenerator die ook als oscillator kan dienen

In dit model heeft elk instrument 3 hardware-oscillatoren: toon, noise en envelope. AY Plus voegt een 4e software-oscillator toe. Het sneller updaten van AY-registers dan de project-ticksnelheid opent meer geluidsontwerpmogelijkheden. Door de werking van AY worden de oscillatoren gemixt met ringmodulatie. Probeer verschillende pitch-offsets, detunings en oscillatorcombinaties om te horen wat er uitkomt.

Elke tonale oscillator kan onafhankelijk worden afgestemd met pitch- en fine tune-offsets.

Soorten software-oscillatoren en hun parameters:

- Pulse - vierkante golf met regelbare pulsbreedte
  - FM Depth - eenvoudige FM-diepte
  - Pulsbreedte - bereik `00-FF`, aantal stappen ingesteld op het Project-scherm
  - Pulse low - bepaalt de lage waarde in een herhaalde puls-paar waarvan de hoge waarde altijd `15` is
- Sync Tone - hard-sync-effect voor de toonoscillator
  - FM Depth - eenvoudige FM-diepte
- Sync Env - hard-sync-effect voor de enveloposcillator
  - FM Depth - eenvoudige FM-diepte
  - Pulsbreedte - duurverhouding tussen de 2 vormen in het enveloppe paar
  - Envelope Pair - 2 envelopvormen om tussen te schakelen
- Wavetable - 32-staps wavetable, bewerkt op het Wavetable-scherm
  - FM Depth - eenvoudige FM-diepte
  - Wavetabel index
- Tone FM - eenvoudig FM-achtig effect voor de toonoscillator
  - FM Depth - toon-FM diepte
- Env FM - eenvoudig FM-achtig effect voor de enveloposcillator
  - FM Depth - envelope-FM diepte

Pulse en Wavetable software-oscillatoren kunnen niet samen met de Envelope oscillator worden gebruikt. De Envelope oscillator schakelt automatisch uit.

Simpel FM schakelt snel tussen 2 periodwaarden, 1 lagere en 1 hogere. Dit is vergelijkbaar met het gebruiken van een vierkante golf als FM-modulator. Pulse, Sync Tone, Sync Env en Wavetable gebruiken een `1:1` verhouding tussen FM-carrier en modulator. Tone FM en Env FM kunnen andere verhoudingen imiteren via software oscillator pitch-offsets.

#### AY Sample

Je kunt mono- of stereo WAV-bestanden laden in `8-bit`, `16-bit`, `24-bit` of `32-bit`. Ze worden geconverteerd naar `8-bit` en gekort tot `16 KB`. Deze limiet komt overeen met de RAM-pagina grootte van de ZX Spectrum.

AY speelt samples via snelle schrijfacties naar het volumeregister. De output is unipolair, dus samples moeten "opgetild naar nul" worden voor schonere weergave. Dit voorkomt klikjes aan het einde van de sample en verbetert stille uitklanken.

AY heeft een `4-bit` DAC-resolutie met een niet-lineaire volumeschaal, dus `8-bit` samples worden tijdens het afspelen geconverteerd naar `4-bit`. De Sample dithering-optie in Settings kan het waargenomen bitdiepte verbeteren. Dithering is niet praktisch op retroplatforms zoals de ZX Spectrum en Atari ST, dus zet het uit bij het schrijven voor echte hardware of bij het nastreven van een “krokant” lo-fi geluid.

Samples gedragen zich als een extra AY Plus software-oscillator, maar hun extra parameters maken ze makkelijker te behandelen als een apart instrumenttype.

Sample-parameters:

- Sample rate – afspelen op native snelheid geeft `C-4`
- Sample start
- Sample lengte
- Sample loop start
- Sample pitch offset
- Sample fine pitch offset

De secties voor Tone en Noise oscillator zijn gelijk aan die van AY Plus. Door de werking van AY worden alle oscillatoren gemixt via ringmodulatie.

### Braids

Braids biedt 47 synthesemodellen. De hoofd bedieningselementen zijn:

- **Model**: oscillator of synthesemodel
- **Timbre** en **Color**: modelafhankelijke macro-parameters
- **Filter**: extra LP/BP/HP-filter, `12` of `24 dB` helling, cutoff en resonantie
- **ADSR**: attack, decay, sustain en release

Tik op **Model** om te kiezen uit gecategoriseerde modellenlijsten.

| Categorie       | Modellen                                                                                                   |
|-----------------|-----------------------------------------------------------------------------------------------------------|
| Analog          | `00 CSAW`, `01 MORPH`, `02 SAW-SQUARE`, `03 SINE-TRI`, `04 BUZZ`, `05 SQUARE-SUB`, `06 SAW-SUB`, `07 SQUARE-SYNC`, `08 SAW-SYNC` |
| Multi Osc       | `09 TRIPLE-SAW`, `10 TRIPLE-SQR`, `11 TRIPLE-TRI`, `12 TRIPLE-SINE`, `13 TRIPLE-RING`, `14 SAW-SWARM`, `15 SAW-COMB`, `16 TOY` |
| Filter / Stem   | `17 FILTER-LP`, `18 FILTER-PEAK`, `19 FILTER-BP`, `20 FILTER-HP`, `21 VOSIM`, `22 VOWEL`, `23 VOWEL-FOF`, `24 HARMONICS` |
| FM / Chaos      | `25 FM`, `26 FEEDBACK-FM`, `27 CHAOTIC-FM`                                                                 |
| Physical        | `28 PLUCKED`, `29 BOWED`, `30 BLOWN`, `31 FLUTED`, `32 STRUCK-BELL`, `33 STRUCK-DRUM`                        |
| Drums           | `34 KICK`, `35 CYMBAL`, `36 SNARE`                                                                          |
| Wavetables      | `37 WAVETABLES`, `38 WAVE-MAP`, `39 WAVE-LINE`, `40 WAVE-PARA`                                              |
| Noise / Granular| `41 FILTER-NOISE`, `42 TWIN-PEAKS`, `43 CLOCK-NOISE`, `44 GRAN-CLOUD`, `45 PARTICLE`, `46 DIGI-MOD`          |

#### Parameters

Timbre en Color hangen af van het geselecteerde model. Raadpleeg het originele Braids-handboek van Mutable Instruments voor de volledige modellenlijst en details, of speel gewoon op gehoor.

#### VCF en VCA

Braids voert de standaard ChooChooTracker multimode-filter en ADSR-envelope aan.

#### Instellingen

Globale Braids-instellingen zijn beschikbaar in de app-instellingen:

- **BITS**: verlaagt de uitvoerresolutie
- **DRFT**: voegt oscillator pitch-instabiliteit toe
- **SIGN**: reproduceert het originele Braids golfvorm-imperfectie-algoritme

### aChChid**aChChid** is een monofonische acid bas engine gebaseerd op Open303. `Square` en `Saw` gebruiken de native TB-303 oscillator, filter, envelope en accent-gedrag. `Braids` vervangt alleen de oscillator, en gaat daarna door hetzelfde 303 filter en versterkerpad. Het biedt Model, Timbre, Color en een extra waveshaper. De shaper gaat van zachte saturatie naar wavefolding.

Cutoff en Decay gebruiken logaritmische aanpassing: **EDIT + LINKS/RECHTS** beweegt één muzikale stap, terwijl **EDIT + OMHOOG/OMLAAG** zestien stappen beweegt. Cutoff loopt van 200 Hz tot 20 kHz; Decay van 200 ms tot 2 s.

Een `F` in de noot-volume kolom triggert een accent. `ASL` slidet naar die noot vanaf de vorige toon zonder de 303 envelope te retriggeren; het houdt automatisch de voorgaande gate vast, en `ASL 00` geeft een `60 ms` glide. `ATY` houdt de noot open voor de hele huidige stap. Noten zonder `ASL` retriggeren altijd.

Modulatiebestemmingen omvatten Decay en Accent, plus Timbre en Color in de Braids golfmodus.

### Bogie

**Bogie** is een native one-shot drum synthesizer. Tracker-noten bepalen de pitch en de tracker/instrument volume stelt het niveau in; elke trigger heeft zijn eigen interne decay, dus er is geen ADSR pagina. Het heeft 12 modellen: `Kick`, `Snare`, `Hat`, `Clap`, `Tom`, `Rim`, `FM`, `Noise`, `Cowbell`, `Cymbal`, `Shaker` en `Clave`.

De zes macro’s zijn **Decay**, **Tone**, **Sweep**, **Noise**, **FM** en **Drive**. De labels passen zich aan waar nuttig (bijvoorbeeld, Kick’s Noise is `Click`, en Cowbell’s FM is metalen cross-modulatie). Elke macro is actief op elk model, maar de muzikale rol verandert met het model: Sweep kan pitch, burst spacing of metallic spreiding wijzigen; FM voegt cross-modulatie, een metalen laag of een ringachtige boventoon toe. Het onderste deel van elk bereik is bedoeld voor conventionele drumsounds; het bovenste deel opent bewust bredere timbrale morfen en synthetische resultaten.

Elk model kan de gedeelde LP/BP/HP multimode filter gebruiken, met Clean, Classic, Aggro of Acid karakter, `12`/`24 dB` slope, cutoff en resonance. Bogie ondersteunt al zijn actieve macro’s plus Volume, Pitch, Cutoff en Resonance als modulatie- en motion-opnamebestemmingen. Gebruik korte Decay met Cowbell voor scherpe phonk attacks; verhoog FM voor een hardere, meer metalen bel.

Op het Modulatiescherm, druk **EDIT** op een bron om een familie te kiezen (Envelopes, LFO of Sticks), daarna de bron. **Links** en **Rechts** wisselen nog steeds direct tussen bronnen.

### Sintered

**Sintered** is een native one-shot digitale percussie synth. Noten bepalen de fundamentele pitch; de zes modellen zijn `Knot`, `Shard`, `Burst`, `Comb`, `Logic` en `Melt`. Elk model heeft **Decay**, **Mod** en **Motion**, plus drie model-specifieke parameters: Knot gebruikt Ratio/Spread/Fold, Shard Ratio/Feedback/Bite, Burst Noise/Color/Feedback, Comb Time/Damping/Regen, Logic Rate/Pattern/Crush en Melt Ratio/Chaos/Drive. **Mod** is altijd de interne koppeldiepte.

Motion is een interne macro envelope: waarden onder `80` gebruiken een AD vorm, `80` schakelt uit, en waarden boven `80` gebruiken een decay-only vorm. Verder van het midden maakt de envelope sneller. De bestemmingen zijn per model vast, dus Sintered blijft voorspelbaar in trackerpatronen: Knot animeert Mod/Fold; Shard Feedback/Bite; Burst Noise/Color/Feedback; Comb Damping/Regen; Logic Rate/Crush; Melt Chaos/Drive. Alle modellen resetten hun staat bij een noot-trigger en begrenzen interne feedback voor herhaalbare hits.

### MonkSynth

MonkSynth is een vocale synthesizer. Hij modelleert klinker, stemkarakter, ademruis, glide, vibrato, unison en delay vanaf de Instrument-pagina. EDIT opent alle regelaars en de patchnaam.

Kies een bank en een preset op de VOICE-regelaars. ALL omvat FACTORY en USER; EDIT+PLAY beluistert de gemarkeerde preset, terwijl EDIT hem selecteert. Sla je eigen geluiden op in `instruments/USER/monk/`. Opgeslagen instrumenten en songs behouden hun MonkSynth-instellingen.

Waarden zijn hexadecimaal `00..FF`, behalve Voices (`01..0A`, van één tot tien). Attack, decay en release beslaan 0..5 seconden; detune 0..50 cent en voice spread 0..0,5. Volume, Pitch en de zes hoofdregelaars van de stem zijn beschikbaar als modulatiebestemmingen en voor motion recording.

Alle zestien regelaars staan samen in **MonkSynth FX**: `MWV` (klinker), `MIN` (stem), `MAM` (ademruis), `MFL` (glide), `MFB` (vibratodiepte), `MSH` (vibratosnelheid), `MUV` (stemmen), `MDT` (detune), `MVS` (spread), `MDM` (delay-mix), `MDR` (delaysnelheid), `MLV` (niveau) en `EAT`/`EDC`/`ESU`/`ERL` (ADSR). Deze betekenissen gelden wanneer MONK het actieve instrument is; de bestaande betekenissen van de MME-commando's veranderen niet. De gedeelde Trigger FX-groep behoudt TDC/TCL zonder dubbele envelope-items.

### MME

**MME** (Multi Modulation Engine) is een tweedosillator synth gericht op interacties tussen oscillatoren. De modellen zijn `Ring`, `Fold`, `Cross`, `VPM`, `Sync`, `Logic` en `Vocode`. Beide oscillatoren zijn intern en volgen tracker-noten; `Interval` stelt hun relatie in.

De zes macro's zijn **Waves**, **Interval**, **Amount**, **Flow**, **Feedback** en **Shaper**. Waves interpoleren tussen muzikale A/B golfvormparen. Amount regelt de kerninteractie van het model, Flow verandert de richting of algoritme-variant, en Feedback is begrensd maar wordt bewust wild aan het einde van het bereik. Shaper is een gedeelde laatste fase die gaat van clean via saturatie naar wavefolding.

MME gebruikt de gedeelde LP/BP/HP multimode filter en ADSR. Alle zes macro’s, Cutoff en Resonance zijn beschikbaar voor modulatie en motion-opname. Houd **EDIT** op Model vast om het model-popup te openen.

### MIDI Out

**MIDI Out** (desktop, PortMaster en Web MIDI browsers) stuurt een extern MIDI-apparaat aan in plaats van audio te synthesizen: het triggeren van een noot verzendt een echte MIDI Note On op het instrument’s **Channel** (`1-16`) en bij het beëindigen van de noot een bijbehorende Note Off. Volume wordt velocity, en een akkoordtrack’s stemmen krijgen elk hun eigen Note On/Off. Er is geen audio te horen vanuit ChooChooTracker zelf — selecteer eerst het MIDI-uitvoerapparaat onder [Settings](#13-settings). Zie [MIDI](#15-midi) voor het complete plaatje, inclusief geluid preview vanaf een MIDI-keyboard.

**Program** en **Bank high/low** (CC0/CC32) zijn optioneel — worden weergegeven als `--` als ze uitstaan, `Clear` schakelt een veld uit en onthoudt de waarde. Als ze zijn ingesteld, worden ze één keer verstuurd, vlak voor de volgende noot, elke keer als ze anders zijn dan de laatste opdracht aan dat kanaal (niet voor elke noot, wat de envelop/patch van het ontvangende apparaat zou retriggeren).

**MC1 nummer**-**MC4 nummer** kiezen welke CC nummers (`00-7F`) elk van de vier MC1-MC4 rij FX verstuurt — zie [MIDI Out FX](#midi-out-fx). Ook `--`/optioneel; een slot dat niet is ingesteld maakt zijn FX inactief.

### Classic architectuur engines

De engines in deze categorie delen een VCO naar VCF naar VCA architectuur.

Meerdere VCO types zijn beschikbaar:

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **Sampler**
- **2xSCWF**
- **BYOWTBL**

Elk instrument voert zijn VCO naar een multimode filter.

Het filter heeft een schakelbare `12 dB` of `24 dB` slope en kan functioneren als low-pass (LP), band-pass (BP) of high-pass (HP) filter.

Verschillende filter-karakters zijn beschikbaar:

- **Clean**: steriel en digitaal
- **Classic**: geïnspireerd op klassieke Amerikaanse analoge synths
- **Aggro**: geïnspireerd op bulderende Japanse analoge synths
- **Acid**: dubbele agressiviteit van aggro

Classic, Aggro en Acid kunnen behoorlijk resonant worden. Begin laag en draai ze voorzichtig omhoog.

Het filter voedt een VCA die wordt aangestuurd door een ADSR-envelope. De envelope-vorm kan morphen tussen exponentiële, lineaire en logaritmische curves.

#### Plaits en Plaits-Alt

**Plaits** biedt 24 engines, gegroepeerd in de selectie-popup als volgt:

| Categorie | Engines |
|---|---|
| Analog / Waves | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digitaal | `07 CHIPTUNE`, `15 SPEECH` |
| Texture / Noise | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Fysiek | `19 STRING`, `20 MODAL` |
| Drums | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** is een verzameling alternatieve modellen voor de originele Plaits module. De meeste zijn vreemder of experimenteler dan de gemiddelde VCO.

| Categorie | Engines |
|---|---|
| Granulair / Micro | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| Fase / Harmonisch | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| Akoestisch / Fysiek | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| Polyfonie / Harmonie | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| Digitaal / Vreemd | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

De parameters volgen het ontwerp van Mutable:

- **Harmonic** bestuurt harmonische relaties, balans of modelkeuze binnen een engine.
- **Timbre** beweegt over het algemeen van donker/sparse naar helder/dicht spectrum.
- **Morph** verkent een andere timbrale dimensie.
- **Main/Aux** mengt de hoofdoutput met de alternatieve output van de engine.

Hun precieze betekenis hangt af van de engine. Bijvoorbeeld, akkoord-engines gebruiken ze voor akkoordtype, inversie en golfvorm. Fysieke modellen gebruiken ze voor materiaal, excitatie en decay, terwijl drum-engines ze gebruiken voor toon, karakter en decay. Raadpleeg de Plaits gebruikershandleiding voor details.

Voor `CHORD`, `DIATONIC CHORD` en `WT DIATONIC CHORD` toont Harmonic de geselecteerde akkoordnaam (`MIN`, `MAJ`, `MIN7`, `MAJ7`, enz.) in plaats van de ruwe regelwaarde. Braids `WAVE PARA` geeft vergelijkbaar namen voor zijn Color stemmingen.

Tik op **Engine** om te kiezen uit gecategoriseerde enginelijsten.

**Env Mode** heeft 2 routeringen:

- `TRIG` reproduceert de module met TRIG aangesloten en LEVEL niet gepatched
- `VCA` houdt LEVEL open en past de tracker ADSR toe na de stem

#### Sampler

Deze schone Sample engine speelt mono of stereo PCM samples geladen in RAM.- Tik op **Sample** om een ongecomprimeerde `8-bit`, `16-bit` of `24-bit` PCM WAV te laden. Druk op **PLAY** in de browser om het geselecteerde bestand te beluisteren.
- Wanneer een sample is geladen, verschijnt **EDIT** naast de naam. Tik erop om het Sample Edit-scherm te openen. **OPT** of **SELECT + [LINKS]** gaat terug naar het instrument.
- Het Sample Edit-scherm toont de bestandsnaam (met een `*` marker wanneer de sample in RAM afwijkt van het bestand op schijf), een format uitlezing (sample rate, kanalen), een hoge golfvorm en de velden **Region**, **Select**, **Slice**, **Process** en **File**.
- Bewerkingen zijn alleen tijdelijk in RAM totdat ze via een save-proces worden weggeschreven: het project slaat het pad van het samplebestand op en laad het WAV opnieuw van schijf bij het volgende projectladen, dus het verlaten van de tracker zonder **Save**/**Save As** verwerpt bewerkingen (de `*` marker waarschuwt hiervoor). Het laden van een andere sample in het instrument verwerpt ook ongeslagen bewerkingen en zet de Region terug naar de volledige sample (Start `00`, Eind `FF`).
- **Region** zet de afspeel Start/End markeringen, ook beschikbaar op het Sampler instrument scherm. Ze stellen genormaliseerde speelgrenzen in (`00-FF`); als Start na End ligt, speelt de sample achteruit. **EDIT + [LINKS/RECHTS]** verfijnt de marker per stap van één (`01`) en zoomt de golfvorm rond de marker terwijl **EDIT** wordt ingedrukt; **EDIT + [OMHOOG/OMLAAG]** maakt grove aanpassingen (stap 16) en herstelt het overzicht op de hele sample. **EDIT + OPT** zet de marker terug naar de standaard (Start `00`, Eind `FF`).
- De golfvorm zoomt in op een vaste periode van één seconde audio rond de bewerkte marker en schuift mee om deze zichtbaar te houden; samples die in het venster passen blijven op de volledige 1:1 weergave. De zoom is tijdelijk: deze is actief zolang **EDIT** wordt vastgehouden, loslaten herstelt de 1:1 weergave, net als grove stappen. Het openen van het scherm reset altijd naar de volledige sampleweergave.
- **Select** stelt een verwerkingsselectie in absolute frames in, onafhankelijk van de afspeel Start/End markeringen. **EDIT + [LINKS/RECHTS]** verplaatst een hendel twintig frames en zoomt erop terwijl **EDIT** wordt vastgehouden; **EDIT + [OMHOOG/OMLAAG]** springt per `frameCount/64` (minimaal 16) en herstelt de volledige sampleweergave. **EDIT (tik)** op een hendel kopieert de huidige Start of End marker positie naar die hendel. **EDIT + OPT** op een van beide hendels maakt de hele selectie leeg. Is de selectie leeg, dan tonen beide hendels `-`; bij omgekeerde hendels wisselen ze automatisch van plaats. De selectie is een sessie-only editorstatus: deze wordt niet opgeslagen in het project, en bij openen wordt deze geïnitialiseerd met het Region bereik (de hele sample met standaard markers).
- **Slice** is één rij met vier waardecellen - **Mode**, **Count**, **Slice** en **Frame** - en wordt samen met het instrument opgeslagen. Uit staat het Start/End venster afspelen. Phrase-noten selecteren slices chromatisch vanaf **C-0**, en noten voorbij de laatste slice herhalen vanaf het begin (noot N speelt slice `N % count`, een loop voorbij de laatste slice begint opnieuw bij de eerste). Gesneden noten transponeren de toonhoogte niet en gebruiken geen Scale-quantisatie, maar **CRD** houdt de geselecteerde slice aan en transponeert zijn stemmen als een akkoord. Slice starts worden op de golfvorm weergegeven met korte lijntjes van twee pixels breed (donker oranje; de actuele slice lijn is helderder), en de slice onder de cursor krijgt een zwarte achtergrondband op de golfvorm (de selectie-tint wordt niet over deze band getekend, zodat deze volledig zwart en duidelijk zichtbaar blijft).
  - **Mode** wisselt `OFF` / `EQUAL` / `AUTO` / `LAZY` (EDIT + links/rechts of tik; EDIT + OPT schakelt slicing uit). Wisselen tussen modi initialiseert de slicepunten: **EQUAL** verdeelt het Start/End venster gelijkmatig over **Count** delen; **AUTO** voert een transient-detectie uit over het venster en plaatst een slice op elke vondst (tot maximaal **Count**); **LAZY** start met één slice die het hele venster beslaat. Wisselen naar LAZY bewaart de huidige EQUAL/AUTO instelling (count en grenzen), en terug naar EQUAL of AUTO herstelt deze exact - er is geen bevestiging en er gaat niets verloren. Slice en Stretch sluiten elkaar uit: het inschakelen van de één schakelt de ander uit, en de Slice cellen worden gedimd terwijl Stretch de duur regelt.
  - **Count** toont het totaal aantal slices (`01`..`64`). **EDIT + [LINKS/RECHTS]** verhoogt of verlaagt het met één (minimum 1, maximum 64), **EDIT + [OMHOOG/OMLAAG]** doorloopt de machten van twee `2 / 4 / 8 / 16 / 32 / 64` met wrap-around. Elke wijziging berekent de verdeling opnieuw: **EQUAL** verdeelt het venster gelijkmatig, **AUTO** voert detectie opnieuw uit met de nieuwe doelwaarde (de detectiegevoeligheid is afgeleid van het count - meer slices betekent een lagere drempel). Het vak toont `-` als de modus OFF is. In **LAZY** blijft het vak verlicht (telt de hand-geplaatste slices) maar is alleen display: de cursor slaat het over — naar rechts van **Mode** of links van **Slice** gaat direct naar **Slice**, omlaag vanuit **Select** rij's **END** marker komt ook daar terecht, en tikken doet niets (hand-geplaatste slices kennen geen verdeling om te herberekenen). **EDIT + OPT** en een gewone **EDIT** tik doen hier niets: uitschakelen van slicing gebeurt op **Mode**, verwijderen van een slice op **Slice**.
  - **Slice** is de browser: toont de 1-geïndexeerde huidige slice (`01`..count). **EDIT + [LINKS/RECHTS]** schakelt tussen slices één voor één, **EDIT + [OMHOOG/OMLAAG]** springt vier slices tegelijk (beiden begrensd, geen wrap-around); de geselecteerde slice wordt op de golfvorm aangegeven met een helderdere marker en zwarte band, en de zoom her-centreert op het slice begin. **EDIT + OPT** verwijdert de huidige slice (deze wordt samengevoegd met de vorige; de eerste slice kan niet verwijderd worden, en het verwijderen van de laatste slice schakelt slicing uit - grenzen blijven behouden bij uit- en aanschakelen en komen terug bij kiezen van een modus). **EDIT (tik)** speelt de geselecteerde slice één keer af (werkt in alle slice-modi; de preview stopt als de toets losgelaten wordt of bij verlaten van het scherm). Het vak toont `-` als mode OFF is.
  - **Frame** toont het startframe in hex van de huidige slice. **EDIT + [LINKS/RECHTS]** schuift het met tien frames (zoomt in op de marker zolang **EDIT** is gedrukt), **EDIT + [OMHOOG/OMLAAG]** schuift met honderd frames en herstelt het overzicht. Een marker blijft binnen zijn eigen slice: het kan de vorige of volgende slice start niet passeren. **EDIT + OPT** verwijdert de slice.
  - **B** verlaat de Slice rij en gaat terug naar het instrument scherm zoals overal op dit scherm; **SHIFT + LINKS** verlaat vanuit elke plek.
  - **LAZY workflow**: met LAZY geselecteerd, tik op **PLAY** voor eenmalige volledige sample-afspelen - het speelt één keer vanaf het begin en stopt vanzelf op de originele toonhoogte en snelheid van de sample (een groene marker van twee pixels volgt de positie op de golfvorm); opnieuw tikken op **PLAY** stopt het voortijdig. Tijdens het afspelen voegt elke **EDIT** klik een slice toe op de afspeelpositie, ongeacht waar de cursor staat (slices dichter dan 50 ms bij een bestaande worden geweigerd met `Too close to slice`). Opnieuw afspelen behoudt de bestaande sneden en voegt nieuwe toe op elke getikte positie, in volgorde - de **Count** box wordt ververst bij iedere slice en de **Slice** browser volgt de nieuwste. De regels voor browsen en bewerken gelden ook voor hand-geplaatste slices: **EDIT + richting** op **Slice** scrollt door ze, **Frame** past het geselecteerde begin aan, **EDIT + OPT** verwijdert de huidige slice. Het Frame vak wordt gedimd als afspelen en slice toevoegen actief is. **SHIFT + PLAY** wordt in LAZY niet onderschept: het start phrase-afspelen zoals op elk ander scherm. LAZY slices spelen net als EQUAL en AUTO in de sequencer: phrase-noten mappen chromatisch van **C-0** naar de hand-geplaatste slices, met wrap-around over de laatste heen.
  - **Combo hints**: zolang de cursor op een Slice rij cel rust en er geen ander bericht wordt getoond, toont de statusbalk de hint van die cel en houdt deze zichtbaar. Bij **Mode** is de hint gebonden aan de actieve modus en toont enkel de uitleg (de modenaam staat al in de cel): `Divides sample in equal parts`, `Divides sample based on transients`, `Press Play and add slices with EDIT` - bij wisseling wordt de tekst direct aangepast, OFF maakt de balk leeg. Bij **Count** staat `Adjust the number of slices`, bij **Slice** `Browse slices`, bij **Frame** `Adjust slice start`. Elk actiebericht vervangt de hint tot het bericht verloopt.
- **Process** selecteert een destructieve bewerking: **Crop**, **Normalize**, **Delete**, **Silence**, **Fade In**, **Fade Out** of **Reverse** (EDIT + links/rechts wisselt, tik wisselt vooruit, EDIT + OPT zet geen). **GO** (dezelfde rij) voert de geselecteerde bewerking uit op de huidige selectie, of op de hele sample als er geen selectie is. **Crop** houdt alleen de selectie over; **Normalize** schaalt de selectie zodat de piek volle sterkte bereikt (beide kanalen delen één gain zodat stereobeeld intact blijft); **Delete** verwijdert de selectie en sluit de gaten (het verwijderen van de hele sample wordt geweigerd); **Silence** maakt het stil (nulwaarden); **Fade In**/**Fade Out** laten de selectie lineair in/uitfaden; **Reverse** speelt de selectie achteruit af (frames worden op hun plek omgewisseld, beide kanalen van een frame verplaatsen samen, lengte en markers blijven gelijk). Crop en Delete vereisen een selectie – zonder selectie tonen ze `Select region first`. Elke bewerking pauzeert audio kort, houdt één undo-niveau bij, en markeert de sample als gewijzigd in RAM: het bestand op schijf blijft onaangeroerd tot Save wordt uitgevoerd (zie hieronder), en het verlaten van het scherm verliest de undo status.
- **UNDO** (naast GO) herstelt de sample naar de staat vóór de laatste bewerking. Het is gedimd totdat een bewerking wordt uitgevoerd, schakelt tussen pre- en post-op status bij herhaald indrukken, en wordt gereset bij herbetreden van het scherm.
- **File** bevat de save-opties. Het instrument slaat het volledige pad van de geladen WAV op; deze functies schrijven dat bestand weg of wijzen het instrument naar een nieuw bestand. Ze veranderen nooit de instrumentnaam.
  - Het File veld wisselt tussen **Save** en **Save As** (EDIT + links/rechts of tik); **GO** voert de getoonde actie uit.
  - **Save** overschrijft de WAV waar de sample van geladen werd. Voor een sample zonder slices vraagt het eerst `Overwrite <name>?`. Een gesneden sample opent eerst de **SAVE SLICES** dialoog waar gevraagd wordt waar de slicepunten bewaard moeten worden:
    **SAVE TO SAMPLE** schrijft ze als cue chunks in de WAV (compatibel – DAW's tonen ze als markers) en bewaart ze ook in het project,
    **SAVE TO PROJECT** schrijft een gewone WAV en bewaart de punten alleen in het project,
    **CANCEL** annuleert. Een checkbox `Don't ask again in this project` bewaart de keuze in het project en slaat de dialoog over bij volgende saves (deze keuze wordt opgeslagen als `- Sample save choice:` in het .cct bestand en kan alleen via projectbewerking worden aangepast). De dialoog wordt overgeslagen voor samples zonder slices. **Save** is gedimd (en overgeslagen in navigatie) zolang er geen sample met bestandspad is geladen. Mislukt schrijven behoudt de `*` marker en toont de fout.
  - **Save As** schrijft de huidige sample naar een nieuw bestand: voer een bestandsnaam in (vooraf ingevuld met de huidige naam zonder extensie) en kies een map in de browser. De sample wordt weggeschreven als `<map>/<naam>.wav`, het instrument wijst naar dat nieuwe bestand, en de map wordt onthouden als standaard samplemap. Het is gedimd (en overgeslagen in navigatie) zolang er geen sample data is geladen, zodat een zojuist geladen sample een bestandslocatie kan krijgen. Paden langer dan 255 tekens worden geweigerd.
  - De `*` marker vóór de bestandsnaam betekent dat de sample in RAM afwijkt van het bestand op schijf (elke bewerking zet hem, Save en Save As wissen hem). De marker geldt alleen voor de sessie: wordt niet opgeslagen in het project en verdwijnt bij herbetreden van het scherm. Als meerdere instrumenten naar dezelfde WAV verwijzen, overschrijft opslaan van één het bestand voor alle instrumenten.
- Op het Sample instrument scherm gebruik **EDIT + [LINKS/RECHTS]** om de vorige of volgende WAV in dezelfde map te laden.
- **Pitch** transposeert in halve tonen (`-48` tot `+48`).
- **Loop** kiest uit Off, Loop of Ping-Pong.
- **Speed** regelt granulaire time-stretching van `0%` tot `500%` (`100%` is normaal).Niet-ondersteunde WAV-formaten tonen een foutmelding. Converteer ongebruikelijke bestanden naar `PCM8`, `PCM16` of `PCM24` WAV voordat je ze importeert.

##### Audio-ingang

Open **Settings → Audio → Audio Input** voordat je opneemt vanaf een microfoon of USB-audio-interface. De lijst toont de ingangen die op dat moment op het apparaat beschikbaar zijn. Als een gearmde interface wordt losgekoppeld, wordt de opname onderbroken en herstart de audiomanager zodat de app blijft werken. Als je aan het opnemen was, druk dan op **STOP** om de onderbroken take af te sluiten. Selecteer na het opnieuw aansluiten **[Refresh]** en controleer of de gewenste ingang gearmd is voordat je weer opneemt; alleen opnieuw aansluiten armt hem niet betrouwbaar. **[List]** leest de huidige ingangenlijst opnieuw in zonder de audio te herstarten.

Vink het vakje **ARM** van een ingang aan om hem beschikbaar te maken voor de Sampler-recorder; de kleine meter toont het ingangsniveau. Arm één interface tegelijk: deze versie opent alleen de eerste gearmde interface, ook als meerdere vakjes zijn aangevinkt. Je selectie wordt onthouden op apparaatnaam, zodat een niet-beschikbare interface ongearmd blijft in plaats van door een andere ingang te worden vervangen. Druk op **OPT** om terug te keren naar de Audio-instellingen.

##### Audio-ingang opnemen

Op het Sample-instrumentscherm opent **REC** (of **EDIT/REC** wanneer een sample is geladen) de sample-editor, die opnemen en bewerken op één scherm samenbrengt. Arm eerst de ingangen op het scherm **Settings → Audio → Audio Input** (zie [Audio-ingang](#audio-ingang)). De bovenste rijen van de editor besturen het opnemen:

- **Input** bladert door de opnamebron: `T1`..`T8` nemen de acht trackertracks op, gevolgd door de gearmde microfoon of USB-interface. Ga voor een externe bron voorbij `T8` tot de apparaatnaam verschijnt. `-` betekent dat er geen bron expliciet is gekozen; **REC** gebruikt dan de eerste gearmde ingang en toont de naam. Houd **EDIT** ingedrukt en druk op **LEFT**/**RIGHT** om terug/vooruit te gaan, of **EDIT+OPT** om terug te keren naar `-`. Tikken op Input gaat direct vooruit.
- De niveaumeter naast **Input** toont het live niveau van de gearmde ingang (alleen externe bronnen). **MON** schakelt ingangsmonitoring in: de gearmde ingang gaat met -6 dB naar de uitgang om feedback te beperken. De instelling wordt in het instellingenbestand opgeslagen.
- **REC** start het opnemen. Externe bronnen nemen direct op; een trackbron toont **ARMED** en de opname begint wanneer de playback start (druk op **PLAY**). **STOP** beëindigt de opname: de WAV wordt naar de exportmap van het project geschreven (`recNNN.wav`), in het huidige Sampler-instrument geladen en de Region wordt weer de volledige sample. Het scherm verlaten met **OPT** tijdens een lopende opname beëindigt die op dezelfde manier.
- **PLAY** (getoond na een opname) beluistert de take; wordt **STOP** terwijl de preview speelt. **CLEAR** (ernaast) verwijdert het WAV-bestand en laadt de sample uit het instrument; na het wissen neemt **REC** zijn plaats weer in.
- De golfvorm toont de live ingang tijdens het opnemen en de volledige golfvorm daarna. De statusregel toont de bestandsnaam tijdens het opnemen en het aantal frames erna; een externe take toont ook `drop 0` wanneer er geen audioframe verloren is gegaan. Een opgenomen sample is volledig bewerkbaar: loopregio, selectie, slicing, bewerking en opslaan werken als bij elk geladen bestand.

Opnames worden geschreven met de geselecteerde audio-samplefrequentie, als 16-bit stereo WAV. Trackbronnen leggen vast wat de instrumenten van de track renderen (na FX); externe bronnen leggen de apparaatingang direct vast.

#### 2xSCWF

2xSCWF is een dual single-cycle golfvormoscillator. Laad 1 mono WAV met precies 1 periode in elke oscillator. Beide oscillatoren lezen vooruit en wikkelen aan het einde van hun cyclus.

Mix crossfades A en B.

Detune is fijn en exponentieel van unisono tot `+200 ct`, en daarna in halve toonstapjes van `+3 st` tot `+24 st`.

2xSCWF behandelt elk bestand als een 1-cyclus tabel en leest deze precies één keer per oscillatorcyclus. Dit maakt het mogelijk om 2 single-cycle golven te mengen en te detunen.

De fabriek `waveforms/AKWF/` map bevat golfvormen uit de Adventure Kid-bibliotheek. Er zijn online nog veel meer beschikbaar om de collectie uit te breiden.

#### BYOWTBL

BYOWTBL is een dual wavetable oscillator compatibel met Serum tabellen. Elke oscillator laadt een mono WAV met opeenvolgende single-cycle frames. `Pos A` en `Pos B` doorzoeken hun tabellen onafhankelijk van `00` tot `FF`. De engine interpolateert lineair binnen elk frame en tussen aangrenzende frames. `Mix` crossfade de oscillatoren, en `Detune` verschuift oscillator B.

### FM instrumentenfamilie

#### Native OPLL en VRC7 instrumenten

De Instrument Type selector heeft een FM groep met **OPLL / MSX** (YM2413) en **VRC7** (DS1001). Elk biedt nu **73 programma’s** in scrollende Bank / Preset lijsten: de originele 15 tonen, 40 extra onderscheidende tonen uit de emu2413 YM2413/VRC7/YMF281B tabellen, en 18 door ChooChoo gemaakte tweedelige operatorprogramma’s. Exacte duplicaten van tonen worden binnen elke engine weggelaten. De extra paletten gebruiken het programmeerbare toongat van de chip; ze breiden de fysieke ROM niet uit. Programma nul identificeert een aangepast geluid. EDIT + links/rechts selecteert de vorige of volgende preset binnen het bankfilter. Fine ct stelt de afstemming bij van -100 tot +100 cent.

In de programma lijst auditieert EDIT + PLAY het gemarkeerde geluid; laat de knoppen los om te stoppen. EDIT bevestigt en OPT annuleert. Bladeren/auditie verandert het instrument, de tabel of het nummer niet. Het gebruikelijke auditioneer-geste op de Instrument-pagina werkt na selectie nog steeds. Native chip envelopes verzorgen attack/release; de optionele Amp env voegt een software ADSR toe (zie Native chip controls hieronder). Er is geen volledige operator-editor.

#### AdLib / OPL2 en OPL3

FM bevat ook AdLib / OPL2 (YM3812) en OPL3 (YMF262). Bank filtert de fabriekslijst; Preset opent categorieën en een Alle-weergave. EDIT + PLAY auditioneert voor selectie, EDIT selecteert, OPT annuleert, en EDIT + links/rechts op Preset laadt de vorige/volgende overeenkomstige entry. Fine ct stelt lokale tuning bij. Mode toont 2 operator, 4 operator, of Dual voice. OPL2 verbergt incompatibele OPL3 patches; OPL3 kan de gedeelde twee-operator collectie afspelen.

Pop-up titels van Bank en Preset identificeren de huidige engine. Wisselen van engine of instrumentplek zet het bankfilter terug op **Alle banken**, ook bij wissel tussen compatibele AdLib en OPL3 engines. Terugkeren van een pop-up binnen dezelfde plek behoudt het gekozen filter. Afbreken van een DX7 bank import herstelt het vorige filter.

Preset bevestiging publiceert ook bewerkingen gemaakt bij loslaten van een knop direct naar de volgende UI/audio-tick. Afspelen van songs vereist geen extra knopdruk of transport herstart om de patch te ontvangen. Output volgt nog steeds de geconfigureerde audiobuffer en het envelope/retrigger gedrag van de engine.

De fabriekscollectie bevat 697 bronitems van The Fat Man 2-op, The Fat Man 4-op en DMXOPL3, met 589 genormaliseerde unieke patch-identiteiten. Aliassen behouden bron-namen en toeschrijvingen. Bron-native operator niveaus, LFO-diepteflags, nootoffset, vaste percussie-pitchen en dual-voice tuning blijven behouden. Tracker volume gebruikt software amplitude; het reproduceert niet de originele MIDI-spelers volume-curves of velocity-offset regels. Bron release-duur schattingen knippen geen sustain-noten af. Sommige effecten hebben trage attacks: houd Seashore vast in plaats van te verwachten dat een korte klik hem onthult.

Selecteren van een preset laat de tabel van de plek en track-gebruikte inserts onaangeroerd.

#### Sega PSG en Game Boy native instrumenten

CHIP bevat Sega PSG, GB Pulse en GB Noise. Bij Sega PSG houdt **Bass range: Extended** lage noten onderscheidend. Kies **Chip** voor het originele beperkte bereik, waarin de laagste noten samenvallen. Fixed-rate noise behoudt zijn originele pitch; toon-afgeleide noise volgt de gekozen bass range.
De GB instrumenten gebruiken DMG pulse/noise registers; er wordt geen Game Boy wavekanaal blootgesteld. Hun samengestelde presetlijsten bevatten nu **24, 24 en 28** presets respectievelijk. De toevoegingen omvatten gelinkte/periodieke Sega noise, GB pitch sweeps, korte percussie, metalen noise, drones en stijgingen. Dit zijn originele programma’s, geen geëxtraheerde gamegeluiden.
De pagina biedt native mode/duty/width, envelope/sweep/noise bediening waar van toepassing, plus een software amplitude ADSR.

#### DX7 FM

Selecteer **Type → FM → DX7 FM**; DX7 is de zevende optie, na Arcade / YM2151.
DX7 FM gebruikt zes operators. De pagina biedt Bank, Preset en Fine ct naast de gebruikelijke instrumentinstellingen. Preset bladeren gebruikt bank/categorie lijsten. Houd EDIT+PLAY ingedrukt om het gemarkeerde geluid te horen; loslaten stopt auditie. Alleen EDIT bevestigt bij loslaten; OPT annuleert. Het laden van een geluid kopieert de complete patch zonder tracer-tabellen of track insert FX te veranderen.

Voor een blijvende persoonlijke bibliotheek plaats `.syx` bestanden in **`instruments/banks/dx7/`** naast de bestaande instrumentbibliotheek. Submappen worden ondersteund. Open **DX7 → Bank**: elk bestand verschijnt als een benoemde bank, klaar voor Preset bladeren. Heropen Bank na toevoegen of verwijderen van bestanden; geen conversie of Load Instrument stap nodig. Een standaard originele DX7/TX7 bank bevat **32 stemmen**. Een bestand met vier bankberichten verschijnt als vier genummerde banken (in totaal 128 stemmen). Enkel-stem bestanden worden ook geaccepteerd. De gekozen patch wordt samen met het instrument opgeslagen, dus verwijderen van de bronbank verandert het nummer niet.

**Load Instrument** accepteert ook originele DX7/TX7 `.syx` single voices en 32-voice banken. Selecteer een patch om die te gebruiken, of annuleer om het huidige instrument te behouden. Gebruik de Bank map hierboven voor een blijvende bibliotheek; sla de song of een `.cni` op om een bewerkte patch te bewaren. Niet-ondersteunde Yamaha-formaten worden geweigerd.

DX7 heeft een limiet van **16 actieve noten door het hele nummer heen**, inclusief akkoordstemmen en release tails. Wanneer de limiet bereikt is, verdwijnen eerst release tails, daarna stille aangehouden noten, zodat nieuwe aanslagen bruikbaar blijven. Preset auditie is niet beschikbaar terwijl een nummer speelt.

#### FM operator-editor

Genesis FM, Arcade FM, OPL2, OPL3, OPLL en VRC7 instrumenten hebben een **EDIT** knop rechts van Fine ct. Deze opent een tabel van de patch parameters:

- Genesis FM / Arcade FM
- OPL2 / OPL3: op OPL3 kan hier de modus (2op, 4op, dual) worden gewijzigd.
- OPLL / VRC7: een ROM-preset bewerken maakt er een "Custom" van.

Links/Rechts en de gebruikelijke bewerkingsknoppen wijzigen waarden; het instrument blijft spelen zodat je wijzigingen hoort. De statusbalk beschrijft de parameter onder de cursor. OPT of Links+Shift keert terug naar het instrumentenscherm. DX7 en SID hebben geen operator-editor.

Genesis FM (YM2612) en Arcade FM (YM2151) gebruiken dezelfde Bank/Preset-browser, auditie- en fijnafstemmingsknoppen als de andere FM-engines. Hun native envelopes regelen de release; wijziging van tracker volume of pitch herstart ze niet.

#### FM fabrieks- en GEBRUIKERSpresets

Native chip instrumenten gebruiken **Bank:** om ALLE, een meegeleverde collectie of GEBRUIKER te kiezen. **Preset:** kiest een geluid binnen die selectie. Houd EDIT ingedrukt en druk Links of Rechts op één van de rijen om te cyclen; een korte EDIT tik opent de keuze.

Meegeleverde collecties zijn ZIP-pakketten onder `instruments/FACTORY/`. GEBRUIKER bestanden gaan in `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse`, of `gb-noise`.
Blader direct door folders of plaats er ZIPs met compatibele presets. ZIPs blijven intact op schijf. Al deze mappen accepteren compatibele `.cni` bestanden; CNI is ChooChoo/ChipNomad’s draagbaar instrumentformaat, geen chipfabrikantstandaard. De GEBRUIKER browser leest ook de volgende bronformaten, inclusief in ZIPs:

| GEBRUIKER-map | Aanvullende formaten |
| --- | --- |
| `dx7` | Yamaha DX7/TX7 `.syx` single voices en 32-stem banken |
| `genesis` | `.tfi`, DefleMask `.dmp`, Furnace `.fui` |
| `arcade` | VOPM `.opm` banken, DefleMask `.dmp`, Furnace `.fui` |
| `opl2`, `opl3` | `.wopl`, `.opli`, `.woplx`, `.sbi`, Furnace `.fui` |
| `opll`, `vrc7` | Furnace `.fui` custom patches en vaste ROM-selecties |
| `sega`, `gb-pulse`, `gb-noise` | Furnace `.fui`, DefleMask `.dmp` |
| `sid` | GoatTracker 2 `.ins` (GTI5), Furnace `.fui` |

Een bank opent in compatibele stemmen. Niet-ondersteunde bestanden of functies melden een reden; incompatibele stemmen in een gemengde bank worden uitgesloten terwijl hun bron-nummers stabiel blijven. Laden is transactioneel: een afgewezen preset laat het huidige instrument ongewijzigd. Bronpresets vervangen de tracer-tabel van het instrument door een lege, behalve DX7 SysEx die de bestaande tabel bewaart. CNI herstelt zijn opgeslagen tabel. Geladen patches zijn eigendom van het nummer en blijven werken na verplaatsen of verwijderen van het bronpakket.

WOPL banken kunnen ongebruikte plekken bevatten zonder een expliciete blanco vlag. De browser negeert onbenoemde plekken waarvan de actieve operators volledig zijn gedempt en geen attack hebben. Onbenoemde bespeelbare stemmen blijven beschikbaar als `Program N [Bbank]` of `Drum N [Bbank]`, gebruikmakend van de bron 0-gebaseerde programma- en banknummers. Een onbenoemd enkel-OPLI instrument gebruikt zijn bestandsnaam. Genoemde stille patches blijven behouden.

Furnace, DefleMask en GoatTracker bestanden worden geïmporteerd als instrumenten, niet als complete nummers. Geanimeerde bron-macro’s, samples en nummerspecifieke afspeelinstellingen worden mogelijk niet overgezet. Plaats Game Boy bestanden in de bedoelde Pulse of Noise map.

Cyclen van GEBRUIKER presets gaat door over bestanden, mappen, banken en ZIP-pakketten in beide richtingen, met wrap bij de uiteinden. Heropenen van de GEBRUIKER browser volgt de huidige selectie. ALLES bevat GEBRUIKER geluiden onder Ongesorteerd voor gecategoriseerde engines; GEBRUIKER blijft een aparte top-level keuze. Sega PSG en Game Boy gebruiken platte ALLES/Fabriek-lijsten en een hiërarchische GEBRUIKER browser, zonder een Ongesorteerd paneel. Oudere DX7 bestanden in `instruments/banks/dx7/` blijven toegankelijk in GEBRUIKER.

Een engine toont alleen haar compatibele meegeleverde collecties. Fabrieks-Presets combineren de meegeleverde OPLL/VRC7 toonsets voor de geselecteerde engine; DX7 combineert ChooChoo en YSE originals. Gedownloade benoemde collecties blijven apart. Fat Man 2-op staat onder OPL2 en Fat Man 4-op onder OPL3. Compatibele OPL2 CNI bestanden kunnen nog steeds in OPL3 GEBRUIKER mappen gebruikt worden.

#### Native chip bediening

Native instrument golfvormvoorbeelden vernieuwen wanneer hun bedieningselementen worden hertekend.
FM instrumenten kunnen hun optionele Amp env curve overleggen; SID behoudt zijn golfvormvoorbeeld zonder die FM-only overlay, ook tijdens bladeren door banken en presets.

Alle zeven FM engines bieden **Bright**, **Feedback**, en **Amp env**. Bright loopt van -63 tot +63, met nul ter behoud van de patch; het wijzigt modulatie-operator niveaus terwijl carrier niveaus behouden blijven. Het hoorbare effect hangt af van het algoritme. Feedback staat standaard op **Preset**, of overschrijft de native feedback met 0–7. Deze bedieningen herschrijven de opgeslagen native operator bytes niet.Amp env staat standaard op **Bypass**. Selecteer **ADSR** om attack, decay, sustain, release en vorm toe te voegen rondom het native geluid. A/D/R gebruiken het gedeelde kwadratische bereik van 0–5 seconden; sustain loopt van stilte tot volledig niveau. Native envelopes blijven actief, dus deze envelope kan een geluid niet verlengen voorbij zijn native release. Preset bladeren behoudt de FM amp en toonregelaars van de slot.

De volgende phrase FX verschijnen ook als ondersteunde Modulation- en motion-recording bestemmingen. Waarden in de FX-kolom zijn hexadecimaal. Instellingen gelden voor playback zonder het opgeslagen instrument te wijzigen. Native GB sweep en envelope-regelaars hangen vast op de volgende noottrigger; duty/width en noise-frequentie kunnen live veranderen.

| Engines | FX | Control |
|---|---|---|
| Native FM | `OL1`–`OL6` | Absolute operator output levels: OPLL/VRC7 modulator en OPL `00–3F`; OPLL/VRC7 carrier `00–0F`; Genesis/Arcade `00–7F`; DX7 `00–63` (0–99). Hoe hoger, hoe sterker de output. Alleen ondersteunde operators worden getoond. |
| OPLL/VRC7/OPL2/OPL3 | `OAR`, `ODR`, `ORR`, `OSL` | Operator 1 attack, decay, release en sustain-atuering, `00–0F`. |
| OPLL/VRC7/OPL2/OPL3/Genesis/Arcade | `OMU 00–0F` | Operator 1 frequentiemultiplier. |
| Genesis/Arcade | `LFR` | Native LFO-snelheid: Genesis `00–07`, Arcade `00–FF`. |
| Arcade | `LAD`, `LPD 00–7F` | Aparte LFO amplitude- en pitch-dieptes. |
| Genesis/Arcade | `LAS`, `LEN` | Native amplitudegevoeligheid `00–03` en LFO inschakelen `00–01`. |
| Genesis/Arcade | `LPS 00–07` | Native pitchgevoeligheid. |
| SID | `SAT`, `SDE`, `SSU`, `SRL 00–0F` | Native attack, decay, sustain en release. Tijdswaarden lopen op richting `0F`; sustain naar vol niveau. |
| SID | `SPR 01–10` | Silent partner frequentie van 1× tot 16×; beïnvloedt ringmodulatie en hard sync. |
| Alle native FM | `FBK 00–07` | Absolute native feedback, geïnitialiseerd vanuit het instrument. |
| Alle native FM met Amp env aan; Sega/GB | `EAT`, `EDC`, `ESU`, `ERL`, `ESH 00–FF` | Attack, decay, sustain, release, vorm. Deze schakelen geen bypassed FM amp in. |
| Sega PSG | `CMD 00–02`, `CNR 00–03` | Toon / witte ruis / periodieke ruis; drie vaste noise-snelheden of toon-afgeleide snelheid. |
| GB Pulse | `CMD 00–03` | Native pulse duty. |
| GB Noise | `CMD 00–01`, `CND 00–07`, `CNS 00–0D` | Noise breedte, clock divisor en shift. |
| GB Pulse | `CSP 00–07`, `CSS 00–07`, `CSD 00–01` | Sweep-periode, shift, neerwaartse richting. |
| GB Pulse / Noise | `CEI 00–0F`, `CEP 00–07`, `CED 00–01` | Native envelope beginwaarde, periode, stijgende richting. |

Native FX selectie gebruikt het instrument in de phrase-rij `I` kolom, of het actieve instrument gevonden door achteruit te zoeken wanneer `I` leeg is. De FX popup toont een korte bedieningbeschrijving, de huidige presetwaarde en valide commandomarge in een compact blok. Selecteren van een ander native effect start bij die waarde; opnieuw openen van hetzelfde effect behoudt de bewerkte waarde. Een multi-rij selectie lost het instrument van elke rij apart op. Tabellen gebruiken hun instrumentcontext. Native waarde-bewerkingen stoppen op hun wettelijke grenzen, inclusief duty, noise, sweep, ADSR, feedback en operator niveaus. De waarden zijn hexadecimaal: bijvoorbeeld DX7’s maximum `63` betekent decimaal 99.

#### SID-instrumenten en Phrase FX

Kies **CHIP → SID**. Bank biedt **ChooChoo SID Originals** en **SIDkit Effects**. Blader, beluister en sla ze op als de andere native-chip presets.

De instrumentpagina toont waveform/pulse width, filtermodus/cutoff/resonantie en native ADSR-regelaars. **EDIT**, naast de presetnaam, opent **SID EDIT**. Wave en PW hebben aparte labels; Filter heeft een modusrij, gevolgd door Cutoff en Reso; ADSR labels staan naast hun waarden. Het aanraken van elk veld selecteert die precieze regelaar. Bestaande parameters en fijn/grof afstel gedrag blijven behouden.

SID EDIT gebruikt de compacte rijen van de FM-editor, thema kleuren, hexadecimale waarden en EDIT+richting-regelaars. OPT of Links+Shift keert terug; EDIT+PLAY hoort het instrument. De pagina bewerkt rechtstreeks de bestaande patch, wijzigingen worden opgeslagen met instrument en project.

SID EDIT groepeert waveform, ring/sync, ADSR, pulse width, filter, motion, arpeggio, pitch sweep en vibrato. De statusbalk identificeert elke regelaar en bereik. Ring en sync gebruiken elk 2 van de 4 beschikbare SID stemmen, dus een SID-akkoord kan minder noten nodig hebben als een van beide actief is.

| FX | Bereik | Actie |
| --- | --- | --- |
| `SCP` | `00–FF` | Pulse-width basis, geschaald naar native 12-bit breedte; pulse motion blijft actief. |
| `SCT` | `00–FF` | Cutoff basis, geschaald naar native 11-bit cutoff; filter motion blijft actief. |
| `SRN` | `00–0F` | Native resonantie. |
| `SWV` | `00–07` | Triangle, saw, tri+saw, pulse, tri+pulse, saw+pulse, tri+saw+pulse, noise. |
| `SFI` | `00–07` | Filtermodus bits: 1 low-pass, 2 band-pass, 4 high-pass; 0 bypass. |
| `SMR` | `00–FF` | Recipe macro klok, geschaald van 1 tot 200 Hz. |
| `SRG` | `00–01` | Triangle ringmodulatie met silent partner. |
| `SSY` | `00–01` | Oscillator sync met silent partner. |

Pulse width en cutoff ondersteunen `SLE`. Discrete golven, schakelaars, resonantie en FM feedback behouden hun nuttige native stappen; bredere byte-waardes zouden geen meer hardware staten creëren. `FBK` selecteert direct feedback `00–07`. Operator niveaus gebruiken de hierboven genoemde engine-specifieke bereiken. Brightness en master-aanpassingen behouden het volledige byte-bereik.

#### SID en native-chip factory en USER presets

Native chip instrumenten gebruiken **Bank:** om ALL, een inbegrepen collectie of USER te kiezen. **Preset:** selecteert een geluid binnen die selectie. Houd EDIT ingedrukt en druk op Links of Rechts op een rij om te cyclus; een korte EDIT-tap opent de kiezer.

Inbegrepen collecties zijn ZIP-packs onder `instruments/FACTORY/`. USER-bestanden gaan onder `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse`, of `gb-noise`. Blader direct in je mappen, of plaats een ZIP met mappen en compatibele `.cni` presets daar. DX7 accepteert ook ondersteunde `.syx` single voices en 32-stemmige banks, inclusief in ZIPs. ZIPs blijven intact op schijf. Direct laden van WOPL/WOPLX wordt niet ondersteund; gebruik compatibele CNI presets of converteer ondersteunde bronformaten offline eerst.

USER preset cyclus gaat door over bestanden, mappen, banken en ZIP packs in beide richtingen, met wrap-around aan beide uiteinden. Het opnieuw openen van de USER browser volgt de huidige selectie. ALL bevat USER geluiden onder Unsorted voor gecategoriseerde engines; USER blijft een afzonderlijke top-level keuze. Sega PSG en Game Boy gebruiken platte ALL/Factory lijsten en een hiërarchische USER browser, zonder Unsorted-paneel. Oudere DX7-bestanden in `instruments/banks/dx7/` blijven toegankelijk in USER.

Een engine toont alleen zijn compatibele inbegrepen collecties. Factory Presets combineert de inbegrepen OPLL/VRC7 tons sets voor de gekozen engine; DX7 combineert ChooChoo en YSE originals. Gedownloade benoemde collecties blijven apart. Fat Man 2-op staat onder OPL2 en Fat Man 4-op onder OPL3. Compatibele OPL2 CNI-bestanden kunnen nog steeds in OPL3 USER-mappen gebruikt worden.

## 6. Modulatie en motion recording

Elk instrument kan tot 4 modulatieslots hebben. Audio-instrumenten kunnen targeten op **Instrument Pan** of **Track Pan**; beiden gebruiken `00` voor links, `80` voor midden en `FF` voor rechts.

De beschikbare modulatie types zijn:

- ADSR - klassieke attack-decay-sustain-release envelope
- AHD - attack-hold-decay envelope
- LFO - low frequency oscillator met verschillende vormen
- syncLFO (voorheen SLFO) - LFO met een ticks-per-eenheid-instelling en een periode-multiplier
- slowLFO - lange cycles met een proportionele periode-regelaar, die net voorbij het bereik van de gewone LFO begint
- FLFO - audio-rate LFO die het kHz-bereik kan bereiken; FLFO kan CPU-intensief zijn
- SLIN - lineaire joystick bediening
- SRAT - rate-based joystick bediening
- Sequence - leest de trackersequentie als modulatiesignaal: **PITCH** is de uiteindelijke gesequencete toonhoogte, **VOL** het uiteindelijke gesequencete volume en **TRANCE GATE** een onafhankelijke ritmische modulator. Motif, Rate en Length kiezen het gate-patroon; Trigger bepaalt of het vrij loopt, per noot opnieuw triggert of reset op phrase- of chain-grenzen. Gesloten gates geven nul, open gates een half niveau, en Accents verdeelt Euclidisch gates op volledig niveau over de actieve gates. Het gebruikt of wijzigt geen track-insert.

De lijst van modulatie bestemmingen hangt af van het instrumenttype.

Wanneer een ADSR of AHD envelope Volume target, wordt het de volume-envelope van het instrument. Een LFO die Volume target, verplaatst in plaats daarvan het huidige output volume.

De modulatie-hoeveelheid kan positief of negatief zijn. Het bereik hangt af van de bestemming. Als het volledige bereik van de bestemming `127` of minder is, zoals bij de meeste parameters, definieert de hoeveelheid een absoluut bereik. Brede bestemmingen zoals pitch gebruiken een geschaalde hoeveelheid.

De Sequence-bronnen PITCH en VOL hebben een **Mode**. **Normal** houdt de trackerwaarde hoorbaar en gebruikt hem tegelijk als modulatie. **Bypass** maakt hem alleen een bron: PITCH triggert opnieuw op de laatste toonhoogte (of C-4 bij de eerste noot) en VOL houdt het laatste hoorbare niveau vast. Trance Gate is al alleen een bron. Als een actieve Sequence-slot PITCH of VOL op bypass zet, wint bypass. Om feedback te voorkomen kunnen PITCH en VOL niet naar Pitch of Volume wijzen.

Alle duurparameters, inclusief attack, hold, decay en periode, worden gemeten in ticks.

**syncLFO** is de voormalige SLFO: stel zijn ticks en multiplier in voor tempogesynchroniseerde beweging. Bestaande SLFO-instellingen en songs behouden dezelfde timing.

Gebruik **slowLFO** voor sweeps die langer zijn dan de gewone LFO aankan. De Period-regelaar loopt van 256 tot 131.072 ticks. Alle tick-gebaseerde LFO's volgen het songtempo en delen dezelfde vormen, wavetable-keuze en triggermodi. Kies Free voor een sweep die tussen noten doorloopt; Retrig, Phrase en Chain herstarten hem op hun respectievelijke grenzen.

LFO-vormen:

- Tri - bipolaire trianglegolf
- Sin - bipolaire sinusgolf
- UniTri - unipolaire trianglegolf
- UniSin - unipolaire sinusgolf
- RampDn - lineaire dalende ramp (zaagtandgolf)
- RampUp - lineaire stijgende ramp (zaagtandgolf)
- ExpDn - exponentiële dalende ramp (exponentiële zaagtand)
- ExpUp - exponentiële stijgende ramp (exponentiële zaagtand)
- Square - blokgolf
- Random - sample-and-hold willekeurige golf
- Wavetable - leest een van de 32-staps AY wavetables van het project. Waarden `00` tot `0F` mappen van `-100%` tot `+100%` zonder interpolatie.

LFO trigger types:

- Free - start LFO alleen opnieuw als het instrument verandert
- Retrig - start LFO opnieuw bij elke noot
- Phrase - start LFO, syncLFO en slowLFO opnieuw wanneer playback een nieuwe phrase ingaat
- Chain - start LFO, syncLFO en slowLFO opnieuw wanneer playback een nieuwe chain ingaat
- Hold - stopt na 1 cycle en houdt de laatste waarde vast
- Once - stopt na 1 cycle en keert terug naar nul

### Motion recording

- **STICK LIVE** (`L1` standaard) past stick-modulatie toe zonder de phrase te veranderen. In **Settings > Stick live mode** schakelt `HOLD` (standaard) het in zolang ingedrukt; `TOGGLE` schakelt aan bij één druk en uit bij de volgende; `FREE` laat het permanent aan staan. Loslaten bij `TOGGLE` laat het aanstaan.
- Hou tijdens playback **MOTION RECORD** (`L2`) ingedrukt om stick-modulatie toe te passen en veranderde bestemmingen als absolute FX-waarden op te nemen.
- Hou **MOTION ERASE** (`R2`) ingedrukt om overeenkomende destination FX van de huidige rij te verwijderen.

De rechtsonder aanwezige `~` geeft actieve Stick live aan. Record (`*`, of `!` bij overflow) en Erase (`x`) krijgen indicatorprioriteit en blijven momentaan in beide modi. Beide schakelen live stick verwerking in zolang ingedrukt; loslaten laat een aparte live toggle intact.

De modus wordt opgeslagen met app-instellingen, maar de live toggle gaat altijd uit bij opstarten. Modus wissels resetten de toggle: `HOLD` volgt de op dat moment ingedrukte Stick live knop, `TOGGLE` wacht op een nieuwe druk, `FREE` is direct actief.

Motion recording schrijft track FX in de phrase die nu speelt. Het werkt eerst overeenkomende FX bij, daarna vult het lege slots van rechts naar links. Het overschrijft nooit een andere FX. Als alle 3 slots vol zijn, wordt die motion niet opgenomen op die stap. Een `!` rechtsonder geeft aan dat er meer destinations veranderden dan in de 3 FX-kolommen passen.

Motion recording ondersteunt Braids, Plaits, Sampler, 2xSCWF, BYOWTBL en Bogie bestemmingen, inclusief actieve Bogie macros en filterregelaars waar van toepassing.


## 7. Track insert effecten

Elk van de acht tracks heeft twee seriële slots, **TF1 → TF2**. Vanuit Instrument, gebruik het gebruikelijke Shift+Up gebaar om MOD te openen, dan Shift+Up nogmaals voor **F: Insert FX**. Shift+Down keert terug naar MOD. OPT+Links/Rechts selecteert de track (1–8), zonder het instrument te wijzigen. De module-header opent de bestaande kiezer; bypass behoudt de module, ingestelde waarden en automatisering. Parameters zijn byte-waarden: linker kolom 1–4, rechter kolom 5–8. Contexttekst toont volledige namen en gedecodeerde waarden. Oude projecten laden met beide slots OFF.

De navigatieweergave houdt de volledige F–M–I–P kolom zichtbaar in alle instrumentenschermen, inclusief Insert FX en de instrumentenpool, en markeert het huidige scherm.| Module | Parameters in volgorde |
| --- | --- |
| Work Compressor | Threshold, Attack, Release, Makeup, Ratio, Detector source, Detector filter, Mix |
| Airwindows Distortion | Input, Mode, Output, Mix |
| Airwindows StereoDoubler | Detune, Mix |
| TAPESCAM | Input, Drive, Color, Wobble, Tone, Output |
| OTT (Rui-727) | Depth, Time, Upward, Downward, Input, Output |
| Chorus | Rate, Depth, Tone, Mix, Feedback |
| Flanger | Rate, Depth, Feedback, Mix |
| Phaser | Rate, Depth, Feedback, Mix, Stages |
| Rotary | Speed, Depth, Drive, Mix |
| Saturation | Drive, Tone, Level, Mix |
| Bitcrusher | Bit depth, Rate reduction, Tone, Mix |
| Destruction | Mode, Amount, Tone, Mix |
| FORMANT | Mode, Vowel, First formant, Second formant, Resonance, Mix |
| Trance Gate | Pattern, Rate, Length, Gate, Phase, Mode, Smooth, Mix |
| Beads (Mutable Instruments) | Time, Size, Density, Pitch, Scatter, Feedback, Shape/Freeze, Mix |

Continue variabelen gebruiken 00–FF, inclusief geldige nul en maximale FF, behalve FORMANT VOW (00–7F). Bipolaire
variabelen hebben een exact neutraal punt op 80. Distortion modi zijn 00 Density, 01 Drive,
02 Spiral, 03 Mojo, 04 Dyno. Compressorratio's 00–07 zijn 1.5, 2, 3, 4, 6, 8, 16,
20:1; Compressor Detect 00–02 luistert naar zijn eigen stereo-ingang, links of rechts; 03–0A luistert in plaats daarvan naar tracks 1–8. Detectorfilter 80 is
neutraal, daaronder is het low-pass, daarboven high-pass. Een trackbron wordt genomen na zijn volume, pan en Tilt EQ maar vóór zijn inserts; een gemute of ontbrekende bron levert stilte.
Elke slot kan elk module bevatten; herhaalde modules zijn onafhankelijk.
Chorus en Phaser starten met hoorbare maar gecontroleerde modulatie. Verlaag Mix,
Depth of Feedback voor een bijna transparant effect; hoge waarden van Depth, Feedback en Mix
worden bewust extreem. Flanger en Rotary gebruiken zachte standaardmodulatie.
Rotary Speed
gebruikt het volledige **00-FF** bereik: **00 = 0.01 Hz** (één omwenteling per 100 seconden),
**FF = 25 Hz** (de voormalige Chaos maximum). Exponentiële spreiding geeft fijnere
regeling bij langzame LFO-beweging. Nieuwe Rotary invoegingen standaard op **8F**, ongeveer
0.8 Hz. Dit vervangt de vier discrete snelheidswaarden; bestaande Rotary instellingen
en F11/F21 snelheidsautomatisering moeten opnieuw worden afgestemd op deze nieuwe schaal (oud Slow/Fast/Hyper/
Chaos komt ongeveer overeen met **8F/CB/E7/FF**). Andere parameters blijven ongewijzigd.
Bitcrusher bitdiepte varieert van 4 tot 16 bits en
rate reduction van 1x tot 32x. Destruction modi zijn Fold, Clip en Crush.
Saturation en distortion bevatten output level of mix-controles om ze
tegen het onbewerkte signaal te balanceren.
Trance Gate is een tempo-gesynchroniseerde amplitude- sequencer voor pads, leads en
stoten. Pattern waarden 00–3F selecteren benoemde muzikale motieven; 00–1F zijn kern grooves
en 20–3F zijn experimentele variaties. Rate biedt rechte, triplet en gedoteerde waarden van 1/2 tot 1/32. Length verkort het onafhankelijk geroteerde 16-stappenmotief tot 1–16
stappen. Elke track note-on herstart de sequence; note-off laat deze doorlopen
gedurende de release. Forward, Reverse en Ping-Pong definiëren de afspeelrichting; Chaos
gebruikt een herhaalbare mutatie die de dichtheid behoudt elke vier loops. Op de master
insert volgt het de transporttijd zonder nootretriggers. Gate bepaalt de breedte van elke open stap, Smooth rondt de randen af en behoudt een klikveilige minimale fade,
en Mix crossfade tussen droge en gated audio.

**Beads** gebruikt de originele Mutable Instruments granular DSP als stereo insert.
Kies Beads in een track-insert slot. De acht bedieningselementen gebruiken ook
bestaande F11–F18 / F21–F28 commando’s en insert MOD bestemmingen; er zijn geen nieuwe FX IDs
of projectformaten nodig. Korrels (grains) lopen continu zonder extra events.

- **Time:** positie in de vier seconden opnamebuffer.
- **Size:** lange reverse grains bij 00, kortste grains rond 55, dan progressief
  langere forward grains. FF schakelt na overgang naar de originele delay-modus.
- **Density:** regelmatige grains onder het midden, willekeurige timing erboven; het midden
  is een dode zone waar geen nieuwe grains starten. Van het midden afbewegen verhoogt de snelheid.
- **Pitch:** −24 tot +24 semitonen; 80 is exact unisono.
- **Scatter:** 00 is geen parameter willekeur. Verhoging activeert de interne
  uniforme Time-willekeur van Beads op volle sterkte en Size-willekeur op halve sterkte,
  onafhankelijk per korrel. Pitch en Shape blijven staan op ingestelde waarden.
- **Feedback:** voert bewerkte grains terug in het opnamepad.
- **Shape/Freeze:** 00–7F neemt op, 80–FF bevriest. Binnen beide bereiken sweepen de lage zeven
  bits dezelfde envelope van rechthoekig via decay en bell naar reverse.
  Voeg 80 toe/trek af om freeze te schakelen zonder Shape te veranderen: 40 en C0 hebben dezelfde
  envelope. De waarde-hint toont enveloppepositie en Opname/Gevroren status.
  Houd EDIT met Up/Down vast om met 10 hex (16) te wijzigen; Left/Right wijzigt met 1.
  Deze gecombineerde bediening is discreet zodat de bitfreeze niet wordt doorlopen bij sweep;
  modulatie van het hele byte kan bewust wisselen tussen beide toestanden.
- **Mix:** 00 is exact droog, FF volledig bewerkt. Wijzigingen worden gladgestreken.

Freeze houdt de audio vast die door het effect is vastgelegd; het selecteren van een ander
module of resetten van Beads maakt het vrij. Track reverb sends blijven beschikbaar.

### Een kick de mix of een andere track laten pompen

Voor een pompende master zet je de kick op track 1, selecteer je Compressor in MASTER FX, zet je Detect op 03 (Track 1), Ratio op 04 (6:1), Attack rond 60 (ongeveer 1,6 ms), Release rond C0 (ongeveer 115 ms), Makeup op 00 en Mix op FF. Verlaag Threshold tot de andere geluiden bij elke kick wegzakken en stel daarna Release bij zodat ze herstellen vóór de volgende. Het directe kickgeluid behoudt zijn eigen track-inserts maar omzeilt de twee master-insertslots; zijn reverb- en delay-returns pompen alsnog.

Om alleen een bas, pad of synth te verlagen, zet je zijn Compressor op die track en selecteer je de kicktrack met Detect. De bronkick blijft hoorbaar en zijn volume wordt niet gewijzigd door de compressor van die track. Een kick en bas die op dezelfde track zijn gecombineerd, kunnen met deze routing niet worden gescheiden.

### FORMANT

Selecteer **FORMANT** op TF1 of TF2 om een geluid te vormen tot klinkerachtige tonen. Werkt het best met een aanhoudende, obertoonrijke bron zoals een zaagtand.

Zet MODE op `00` VOW en gebruik **VOW** om van A (`00`) naar U (`7F`) te sweepen via E, I en O. Zet MODE op `01` CUSTOM om **F1** en **F2** zelf af te stellen. **RES** versmalt of verbreedt het klinkerkarakter; **MIX** mengt het effect met het oorspronkelijke signaal. Gebruik MOD → INSERT FX om de actieve regelaars te bewegen. MODE is geen MOD-bestemming.

### Lane FX voor Inserts

Insert-commando's gebruiken **F11–F18** voor TF1 en **F21–F28** voor TF2. De FX-keuze en Phrase/Table popup tonen alleen parameters gebruikt door de geselecteerde insert in de track. De popup groepeert ze als **TF1: effect naam** en **TF2: effect naam**; OFF slots worden verborgen. Bypassed inserts blijven bewerkbaar.
In de Phrase en Table FX popup volgt de omschrijving de geselecteerde track insert slot: het toont module, parameternaam, ontcijferde bereikgrenzen en opgeslagen basiswaarde. Bestaande song-commando’s blijven intact als een modulewijziging ze overbodig maakt; het openen van de picker start bij een beschikbare controle.
Tijdens het bewerken toont de onderhint de ontcijferde waarde en eenheid.
Elke Phrase of Table FX kolom kan een van beide slots adresseren; het Phrase grid behoudt zijn drie kolommen. Deze absolute runtime waarden blijven bewaard over noten, instrumenten,
phrases, chains en gewone loops. Nieuwe playback/hard stop reset ze. Het bewerken van een basiswaarde op de F-pagina wist alleen de override van die parameter. MOD werkt op de
effectieve basis/automatiseringswaarde zonder de offsets op te slaan. Het selecteren van een nieuwe
module herstelt standaardwaarden en wist die slot’s overrides. Bypass behoudt
bestaande automatisering terwijl verwerking wordt uitgezet na een korte fade.

**Adressen verwijzen naar trackslots/parameterposities, niet naar benoemde effecten.** Een
Phrase hergebruikt op een andere track gebruikt die track’s modules. Wijzigingen van modules kunnen
de betekenis van een commando veranderen; commando’s worden niet herschreven. OFF/niet gebruikte adressen
zijn veilige no-ops en blijven ongewijzigd in opgeslagen Phrase/Table data.

De instrument MOD-chooser voegt een Insert FX-categorie toe. Elke instrumentbron
bedient inserts op de track waar het speelt, inclusief het gecombineerde akkoordsignaal.
Hetzelfde instrument op twee tracks beïnvloedt onafhankelijke chains. Bestaande timing van bronnen, signed amounts, combinatieregels van bronnen, stick modi en motion recording
blijven van toepassing. Discrete controles zijn gekwantiseerd en beperkt tot geldige keuzes. Er is geen
nieuwe track-eigen modulatie bank.

Begin met een paar inserts, let op overbelasting of gekraak, en bypass effecten
die het nummer niet versterken. Drive- en gain-instellingen kunnen de mix clippen, gebruik daarom
de Mixer clip-indicator tijdens het instellen.

## 8. Tabellen

Tabellen zijn een kerninstrument voor geluidsontwerp in trackers. In Vortex Tracker termen combineren ze instrumenten en ornamenten, maar ze kunnen veel meer. Wie LSDj of NerdSEQ kent, herkent het principe al.

De Pitch-kolom accepteert relatieve (`~`) of absolute (`=`) toonhoogte in halve tonen. De Volume-kolom is `00`-`0F`, geldt voor elke engine bovenop zijn envelope, en kan gebruikt worden als gate: alterneer `0F` en `00` en herhaal met `HOP 00`. De 4 FX-lanes komen grotendeels overeen met de lanes in een phrase, hoewel enkele FX anders werken in tabellen.

Een `TIC` FX op de laatste tabelrij zet de snelheid van die kolom en overschrijft de standaard instrument tabelsnelheid. Elke FX-kolom kan op een andere tick-snelheid lopen. De Pitch- en Volumekolommen volgen de snelheid van de 1e FX-kolom.

Een tabel van 16 rijen lijkt kort naast een lang Vortex Tracker instrument, maar het `HOP` FX kan voorwaardelijke loops creëren zoals "herhaal deze rijen 5 keer", evenals geneste loops. Met loops en onafhankelijke FX-kolomsnelheden kunnen die 16 rijen verrassend lang doorgaan.

Het is mogelijk lane FX te kopiëren/plakken tussen phrases en tabellen ;)

### Retrigger modus

Het `Retrig` veld bovenin het Table-scherm kiest wanneer een al actieve tabel herstart. `Inst` is de standaard en compatibele modus: een phrase-rij met een instrument selecteert de standaard tabel van dat instrument en start deze op rij `00`. `Phrase` herstart een actieve tabel bij betreden van een phrase, `Chain` bij betreden van een chain, en `Free` laat de tabel doorlopen tot de track stopt of een expliciet commando deze vervangt.

Een structurele modus kijkt nooit terug in de tijd. Als een track geen actieve instrumenttabel heeft, activeert de eerste phrase-rij die een instrument introduceert de tabel op exact die rij, ook als dat rij `4`, `8` of `F` van een phrase is. Latere instrumentnummers vervangen geen actieve `Phrase`, `Chain` of `Free` tabel. Dit maakt tabellen geschikt als LFO-achtige automatisering zonder stilte simulatie van eerdere phrase-stappen.

`TBL`, `TBX` en `RET` zijn altijd expliciete acties: ze selecteren of herstarten onmiddellijk, ongeacht modus. Op een Phrase- of Chain-grens vindt herstart plaats vóór het lezen van de nieuwe phrase-rij, dus een `TBL` of `TBX` op die rij heeft het laatste woord. Table-rij `00` FX lopen dus normaal weer; cumulatieve FX behouden gebruikelijke accumulatie.

Tabellen `00-7F` zijn gereserveerd voor standaard instrumenttabellen. Tabellen `80-FE` zijn bedoeld als hulptabellen gestart door het `TBX` effect. Het lagere bereik werkt ook voor hulptabellen, maar gebruik daarvan kan onverwachte conflicten veroorzaken.

### Bediening

- **EDIT + [UP/DOWN]** op een FX-naamkolom: open FX-selectiescherm
- **OPT + [RICHTING]**: navigeer tussen tabellen

## 9. Instrument Pool en AY Wavetable

### Instrument Pool

De Instrument Pool toont elk instrument in het project en laat je ze herschikken. Tijdens playback toont het ook welke instrumenten actief zijn.

#### Bediening

- **EDIT**: bewerk het instrument en spring naar het Instrument-scherm
- **SHIFT + OPT**: kopieer het instrument
- **SHIFT + EDIT**: plak het instrument
- **EDIT + [UP/DOWN]**: herschik instrumenten
- **EDIT + PLAY**: preview het instrument

### AY Wavetable

Wavetable is een AY Plus software-oscillatortype. Een project mag tot 256 wavetables bevatten, elk 32 stappen lang. Alle wavetable-instrumenten delen dezelfde set golven. Hier bewerk je ze.

AY-volumelevels zijn niet-lineair, dus het Standard-view golfvorm wordt getekend naar werkelijke outputlevels. Dubbelklik **OPT** om te wisselen tussen Standard view en het opgeslagen **LFO view**, met dezelfde shortcut als Song/Live Mode: de LFO view toont de ruwe `0`–`15` stappen lineair, met de middenlijn tussen `7` en `8` als modulatie-nulpunt.

Het scherm heeft 2 logische rijen. De bovenste rij bevat **Load** en **Save** knoppen, de tweede rij de wavetable-editor.

#### Bediening

- **OPT + [LINKS/RECHTS]**: navigeer door de lijst met wavetables
- **OPT + [OMHOOG/OMLAAG]**: navigeer 16 golven tegelijk
- **SHIFT + OPT**: kopieer de wavetable
- **SHIFT + EDIT**: plak de wavetable
- **OPT + PLAY**: wissel tussen Standard en LFO view

## 10. Tracker FX

Track FX forceren sequencer- of sound-engine waarden op individuele stappen. Andere grooveboxes noemen dit vaak "parameter locks". Elke Phrase-rij heeft 3 FX-slots.### Sequencer FX

| FX | Waarde | Gedetailleerd gedrag |
|---|---|---|
| `ARP` | `XY` | Arpeggieert de basnoot, `+X` stappen en `+Y` stappen. `37` produceert een mineur-akkoordpatroon. |
| `ARC` | `XY` | `X` kiest de arpeggio richting of bereikmodus; `Y` is de snelheid in ticks. |
| `PVB` | `XY` | Pitch vibrato: `X` is snelheid en `Y` is diepte. In Linear modus gebruikt diepte `10-cent` stappen. |
| `PBN` | signed `XX` | Voegt elke phrase/table rij `XX` pitch-eenheden toe; `FF` betekent `-1`. Gebruik `00` om te stoppen. |
| `PSL` | `XX` ticks | Glijdt vanaf de vorige pitch naar de nieuwe noot over `XX` ticks. |
| `PIT` | signed `XX` | Geaccumuleerde relatieve offset in pitch-table stappen. |
| `FIN` | signed `XX` | Geaccumuleerde fijne offset in cents bij Linear pitch, anders in period-eenheden. |
| `PRD` | signed `XX` | Geaccumuleerde relatieve oscillator-periode offset. |
| `VOL` | signed `XX` | Geaccumuleerde relatieve volume-offset (`FF` is -1, niet volledig volume). |
| `VSL` | signed `XX` | Voegt `XX` toe aan volume bij elke phrase/table rij. Gebruik `00` om te stoppen. |
| `PAN` | `00-FF` | Absoluut instrument panorama: links, midden (`80`), rechts. |
| `TPN` | `00-FF` | Absoluut mixer-track panorama: links, midden (`80`), rechts. |
| `RET` | `XY` | Retrigger elke `Y` ticks; `X` past een volumeverandering toe. `Y=0` stopt retriggeren. |
| `DEL` | `XX` ticks | Vertraagt note-on. Een vertraging langer dan de huidige groove-stap overslaat de noot. |
| `OFF` | `XX` ticks | Stuurt note-off na `XX` ticks en gaat in ADSR release stage. |
| `KIL` | `XX` ticks | Hard killt de stem na `XX` ticks zonder ADSR release te doorlopen. |
| `TIC` | `XX` ticks | Stelt table ticks per rij in. In een table verandert dit de snelheid van die FX-kolom. |
| `TBL` | `00-FE`, `FF` uit | Vervangt en herstart de instrumenttabel onmiddellijk; `FF` stopt het. |
| `TBX` | `00-FE`, `FF` uit | Start of vervangt een hulptabel onmiddellijk; `FF` stopt het. |
| `THO` | rij `XX` | Springt alle instrument-table kolommen naar rij `XX`. |
| `TXH` | rij `XX` | Springt alle hulptabel-kolommen naar rij `XX`; wordt niet gebruikt binnen een tabel. |
| `GRV` | groove `XX` | Selecteert een groove voor de huidige track. |
| `GGR` | groove `XX` | Selecteert een groove voor elke track. |
| `HOP` | `XY` | Springt naar rij `Y`, `X` keer; `X=0` loop oneindig. Table HOP beïnvloedt zijn eigen kolom. |
| `SNG` | signed `XX` | Verplaatst playback met `XX` songrijen. |
| `PRO` | `00-64` | Bereken een absolute triggerkans van `0%` tot `100%`. |
| `MOD` | `AB` | Trigger op doorgang `A` van een `B`-doorgangen cyclus. |
| `RNN` | `MX` | Speelt een willekeurige noot. `M` kiest de modus: `0` altijd willekeurig, `1` willekeurig per PLAY, `2` kiest en houdt vast tot de waarde verandert. `X` kiest het bereik: `0` elke noot, `1` +3 st, `2` +6 st, `3` +12 st, `4` -3 st, `5` -6 st, `6` -12 st, `7` +/-3 st, `8` +/-6 st, `9` +/-12 st. Willekeurige keuzes volgen de projectschaal wanneer de schaal-engine op de track wordt toegepast. |
| `SPD` | signed `XX` | Selecteert een persistente per-track klokratio; zie de snelhedentabel hieronder. |
| `SLE` | `00-FF` ticks | Stelt persistente per-track glide tijd in voor continue engine FX. `00` is direct. Reset naar `00` bij stoppen playback. |

### Audio FX sends

| FX | Waarde | Gedetailleerd gedrag |
|---|---|---|
| `RSN` | `00-FF` | Stelt deze track's reverb send in tot de volgende noottrigger. |
| `DSN` | `00-FF` | Stelt deze track's delay send in tot de volgende noottrigger. |

### ADSR / Trigger FX

| FX | Waarde | Gedetailleerd gedrag |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | Overschrijft attack, decay, sustain, release of envelope vorm. |
| `TDC` / `TCL` | `00-FF` | Overschrijft Trigger-mode Decay of Color op Plaits en Plaits-Alt. |

#### Condities

Geïnspireerd door Zweedse "trig conditions", ondersteunt de tracker triggerkansen en modulo-condities.

| FX | Waarde | Betekenis |
|---|---|---|
| `PRO` | `00-64` | Absolute triggerkans van `0%` tot `100%` |
| `MOD` | `AB` | Trigger op iteratie `A` van `B`; bijvoorbeeld `12`, `22`, `14`, `34` |

`MOD` tellers zijn lokaal per track en phrase. Ongeldige combinaties, zoals `A` groter dan `B`, triggeren niet.

`MOD34` triggert op de 3e doorgang van elke 4-doorgangen cyclus. `MOD1F` triggert op de 1e doorgang van elke 16-doorgangen cyclus.

#### Afspelsnelheid

Net als NerdSEQ ondersteunt ChooChooTracker een onafhankelijke afspeelsnelheid per track.

`SPD` leest zijn waarde als signed byte. `00` is normale snelheid, positieve waarden maken de track sneller, negatieve waarden maken hem langzamer. De instelling blijft actief totdat een ander `SPD`-commando het verandert.

| Waarde | Snelheid |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

Oudere projecten zonder signed `SPD` ondersteuning behouden de originele `00-10` mapping, dus ze spelen nog steeds zoals opgeslagen.

Zeer hoge vermenigvuldigers kunnen de resolutie van de tracker tick scheduler overschrijden en zijn niet hardware-getest.

### Scale & chords FX

#### Scale automatisering

`SCL XY` verandert de globale afspeelschaal en root zonder de in frases opgeslagen noten te wijzigen. `X` kiest een schaal (`0` Chromatisch, `1` Major, `2` Minor, tot `C` Custom); `Y` kiest de root (`0` C tot `B` B). Het commando geldt alleen voor phrase FX, niet voor tables. Als meerdere tracks `SCL` uitdelen op dezelfde tick, wint de track met het laagste nummer.

`SCL` behoort tot Quantizer modus. Met **Note Lock** actief wordt het commando niet aangeboden in de FX-lijst, en elke al geschreven `SCL` in frases wordt genegeerd zolang de modus aanstaat.

#### Akkoorden`CRD XY` zet de noot in zijn eigen Phrase-rij om in een akkoord. `Y` selecteert: `0` Major, `1` Minor, `2` Dim, `3` Aug, `4` Sus2, `5` Sus4, `6` Power, `7` Maj7, `8` Min7, `9` Dom7, `A` Min7b5, `B` Dim7, `C` Add9, `D` MinAdd9, `E` Maj9, of `F` Min9. `X` selecteert een van 16 voicings: `0-3` zijn een oplopende gesloten inversiecyclus; `4-7` voegen drop-2 toe; `8-B` voegen drop-3 toe; en `C-F` combineren beide drops. De cyclus zet zich voort door een extra octaaf voor triaden en power akkoorden, dus elke waarde is uniek; hogere waarden worden progressief breder en minder conventioneel. Bijvoorbeeld, `CRD 4Y` is drop-2 hoofdpositie en `CRD DY` is eerste inversie met beide drops. Onderaan of bovenaan de pitchtabel worden stemmen beperkt tot het bereik. De opdracht is niet persistent: schrijf het op elke akkoord-rij. Elke gegenereerde noot wordt onafhankelijk gekwantiseerd wanneer Scale actief is, zowel in Quantizer- als Note Lock-modus. Het is alleen beschikbaar voor software engines; AY-instrumenten negeren het en blijven monofonisch.

### Modulation FX

Phrase- en table-FX kunnen een modulatieslot veranderen zonder het instrument te bewerken:

| FX patroon | Betekenis |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Relatieve Amount-offset voor modulatiesloten 1-4 |
| `M11`, `M12`, `M13`, `M14` | Relatieve P1-P4 offset voor modulatieslot 1 |
| `M21`, `M22`, `M23`, `M24` | Relatieve P1-P4 offset voor modulatieslot 2 |
| `M31`, `M32`, `M33`, `M34` | Relatieve P1-P4 offset voor modulatieslot 3 |
| `M41`, `M42`, `M43`, `M44` | Relatieve P1-P4 offset voor modulatieslot 4 |

De waarde wordt geïnterpreteerd als een signed `8-bit` relatieve verandering (`01` telt op met `1`, `FF` trekt `1` af). Herhaalde opdrachten stapelen op. Effectieve waarden worden begrensd tot hun geldige bereik.

`P5` is niet beschikbaar als phrase- of table-FX. Het is beschikbaar als de `M1–M4 Wavetable` modulatiebestemming, zodat één modulator de AYWavetable index van een andere kan scannen.

| Modulatietype | P1 | P2 | P3 | P4 | P5 wanneer Shape Wavetable is |
|---|---|---|---|---|---|
| ADSR | Attack | Decay | Sustain | Release | Ongebruikt |
| AHD | Attack | Hold | Decay | Ongebruikt | Ongebruikt |
| LFO | Shape | Trigger mode | Period | Ongebruikt | AY wavetable index |
| syncLFO | Shape | Trigger mode | Ticks | Multiplier | AY wavetable index |
| slowLFO | Shape | Trigger mode | Periodestap (00–FF) | - | AY wavetable index |
| FLFO | Shape | Trigger mode | Frequency (`1 Hz` tot `20 kHz`) | Ongebruikt | AY wavetable index |

### Braids FX

| FX | Waarde | Betekenis |
|---|---|---|
| `BMD` | `00-2E` | Braids model |
| `BTM` | `00-FF` | Absolute genormaliseerde Timbre |
| `BCL` | `00-FF` | Absolute genormaliseerde Color |
| `BCF` | `00-FF` | Exponentiële cutoff, `20 Hz` tot `20 kHz` |
| `BRS` | `00-FF` | Exponentiële filterresonantie |

### aChChid FX

| FX | Waarde | Betekenis |
|---|---|---|
| `ASL` | `00-FF` | Slide naar de noot zonder retrigger. `00` is `60 ms`; hogere waarden verlengen de glide. |
| `ADC` | `00-FF` | Decay override: `200 ms` tot `2 s`. |
| `AAC` | `00-FF` | Accent hoeveelheid: geen tot maximaal. |
| `ATM` | `00-FF` | Braids Timbre override; actief alleen in Braids wave mode. |
| `ACL` | `00-FF` | Braids Color override; actief alleen in Braids wave mode. |
| `ACF` | `00-FF` | Exponentiële 303 filter cutoff, `20 Hz` tot `20 kHz`. |
| `ARS` | `00-FF` | 303 filterresonantie, geen tot maximaal. |
| `AEM` | `00-FF` | 303 filter envelope modulatie, geen tot maximaal. |

### Bogie FX

| FX | Waarde | Betekenis |
|---|---|---|
| `DMD` | `00-0B` | Selecteert het Bogie-model tot de volgende trigger. |
| `DDC`, `DTO`, `DSW`, `DNO`, `DFM`, `DDR` | `00-FF` | Override Decay, Tone, Sweep, Noise, FM of Drive tot de volgende trigger. Alle zes macro’s zijn beschikbaar op elk model. |
| `DCF` | `00-FF` | Filter cutoff, logaritmisch gemapt. |
| `DRS` | `00-FF` | Filterresonantie. |

### Sintered FX

| FX | Waarde | Betekenis |
|---|---|---|
| `SMD` | `00-05` | Selecteert het Sintered-model tot de volgende trigger. |
| `SDC`, `SMP`, `SMA`, `SMB`, `SMO`, `SMC` | `00-FF` | Override Decay, Mod, de eerste twee modelmacro’s, Motion of de laatste modelmacro tot de volgende trigger. |
| `SCF`, `SRS` | `00-FF` | Filter cutoff en resonantie. |

### MIDI Out FX

| FX | Waarde | Betekenis |
|---|---|---|
| `MC1`-`MC4` | `00-FF` | Stuurt een MIDI CC, herschaald naar `0-127`. Welk CC-nummer elk slot stuurt, wordt per instrument ingesteld (`MC1 number`-`MC4 number` op het instrumentenscherm), niet door de FX zelf – een slot zonder nummer is inactief. |

### Plaits FX

| FX | Waarde | Betekenis |
|---|---|---|
| `PMD` | `00-17` | Plaits engine |
| `PHA` | `00-FF` | Absolute genormaliseerde Harmonischen |
| `PTM` | `00-FF` | Absolute genormaliseerde Timbre |
| `PMO` | `00-FF` | Absolute genormaliseerde Morph |
| `PAX` | `00-FF` | Main/Aux blend: `00` Main, `FF` Aux |
| `PCF` | `00-FF` | Exponentiële cutoff, `20 Hz` tot `20 kHz` |
| `PRS` | `00-FF` | Exponentiële filterresonantie |

### Sample FX

| FX | Waarde | Betekenis |
|---|---|---|
| `SPL` | `00-03` | Afspelen modus: `00` Vooruit, `01` Achteruit, `02` Loop, `03` Ping-Pong |
| `SLI` | `00-FF` | Speel de genummerde slice (`01` = eerste slice) ongeacht de noot; `00` behoudt normale nootmapping. Vereist slice modus ingeschakeld op het instrument |
| `SPT` | signed `XX` | Sample transpositie in halve tonen |
| `SST` | `00-FF` | Genormaliseerde afspeelstart |
| `STA` | `00-FF` | Alias van `SST` (genormaliseerde afspeelstart) |
| `SEN` | `00-FF` | Genormaliseerd afspeeleinde |
| `SVL` | `00-FF` | Absoluut samplevolume |
| `SCF` | `00-FF` | Exponentiële cutoff, `20 Hz` tot `20 kHz` |
| `SRS` | `00-FF` | Exponentiële filterresonantie |
| `SSP` | `00-FF` | Samplesnelheid, gemapt van `0%` tot `500%` |
| `RNS` | `M0` | Speelt een willekeurige slice van een gesliced sample in plaats van de aan de noot toegewezen slice. `M` kiest de modus: `0` altijd willekeurig, `1` willekeurig per PLAY, `2` kiest en houdt vast tot de waarde verandert. Vereist dat slice-modus in het instrument aan staat; de lage nibble wordt niet gebruikt. |

### AY FX gedeeld door AY-instrumenten

| FX | Waarde | Gedetailleerd gedrag |
|---|---|---|
| `AYM` | `XY` | `X` is envelope shape; `Y` selecteert uit off/tone/noise/tone+noise (`0/1/2/3`). |
| `NOI` | signed `XX` | Opgehoopte relatieve noise-period offset. |
| `NOA` | `00-1F`, `FF` | Absolute noise periode; `FF` geeft noise-period prioriteit aan eerdere tracks. |
| `ERT` | ieder | Retriggert de huidige hardware envelope shape. |
| `EAU` | `XY` | Automatische envelope ratio `X:Y`; `X=0` schakelt uit. |

### AY Classic-only FX| FX | Waarde | Gedetailleerd gedrag |
|---|---|---|
| `EVB` | `XY` | Envelope-periode vibrato: `X` snelheid, `Y` diepte. |
| `EBN` | signed `XX` | Voegt `XX` toe aan de envelope-periode bij elke phrase/table-rij. |
| `ESL` | `XX` ticks | Glijdt van de vorige envelope-periode naar de nieuwe waarde. |
| `ENT` | noot `XX` | Stelt de envelope-periode in op basis van de toon uit de pitch-table die in de UI wordt getoond. |
| `EPT` | signed `XX` | Geaccumuleerde relatieve envelope-periode offset. |
| `EPL` | byte `XX` | Stelt de lage byte van de envelope-periode in. |
| `EPH` | byte `XX` | Stelt de hoge byte van de envelope-periode in. |

### AY Plus FX| FX | Waarde | Gedetailleerd gedrag |
|---|---|---|
| `TNN` | noot `XX` | Zet de tone-oscillator op een pitch-table noot, negeert de rijnoot. |
| `TNP` / `TNF` | gesigneerd `XX` | Geaccumuleerde tone pitch-stap / fijne offset. |
| `TRT` | elk | Hervat de fase van de tone-oscillator waar ondersteund. |
| `ENN` | noot `XX` | Zet de envelope-oscillator op een pitch-table noot. |
| `ENP` / `ENF` | gesigneerd `XX` | Geaccumuleerde envelope pitch-stap / fijne offset. |
| `SFT` | `00-07` | Software type: Geen, Pulse, Sync Tone, Sync Env, Wavetable, Tone FM, Env FM, Sample. |
| `SFN` | noot `XX` | Zet de software-oscillator op een pitch-table noot. |
| `SFP` / `SFF` | gesigneerd `XX` | Geaccumuleerde software-oscillator pitch-stap / fijne offset. |
| `SRT` | elk | Reset de fase van de software-oscillator. |
| `SFM` | gesigneerd `XX` | Geaccumuleerde FM-diepte offset. |
| `PWM` | gesigneerd `XX` | Geaccumuleerde pulse-width offset. |
| `SPL` | gesigneerd `XX` | Geaccumuleerde pulse low-level offset. |
| `SWT` | gesigneerd `XX` | Geaccumuleerde wavetable-index offset. |

### AY Sample FX

AY Sample ondersteunt de gedeelde AY-commando’s plus `TNN`, `TNP`, `TNF`, `TRT`, `SFN`, `SFP` en `SFF` zoals hierboven beschreven.

| FX | Waarde | Gedetailleerd gedrag |
|---|---|---|
| `SMS` | `00-FF` | Zet legacy AY Sample afspeelstart op `XX x 64` bronmonsters. |

Engine parameter FX gelden voor hun bijbehorend instrumenttype en worden gereset bij de volgende noot-trigger tenzij anders vermeld. `SPD` is bewust persistent.

De FX-selector filtert deze referentie tot veelvoorkomende commando’s plus de groep die door het actieve instrument wordt ondersteund. Het contextuele help-paneel toont waardedetails tijdens het bewerken.

## 11. Mixer, Reverb en Delay

De Mixer is het meest linkse hoofdscherm. Het toont CPU-belasting en overstuurbaarheidswaarschuwingen. Een rode `!` in de **CLIP**-kolom markeert de track waarvan het droge signaal de mix buiten het veilige bereik duwde.

Gebruik het om de 8 tracks in balans te brengen, elke track vorm te geven met Tilt EQ, en audio naar de reverb of delay te sturen:

| Veld | Betekenis |
|---|---|
| LVL | Post-engine trackniveau, `000-100` |
| REV | Send naar de gedeelde reverb, `000-100` |
| DLY | Send naar de gedeelde ping-pong delay, `000-100` |
| PAN | Vouwt de audio naar links of rechts |
| TLT | Per-track Tilt EQ, `00-FF`; `80` is neutraal, lage waarden benadrukken bas en hoge waarden benadrukken hoge tonen. Lage waarden introduceren een zachte overdrive. |
| M | `*` dempt deze track |
| S | `*` solo’s deze track |

De mixer is trackgebaseerd, niet instrumentgebaseerd. Als een track van instrument verandert, blijven het niveau, PAN, Tilt en sends aan de track gekoppeld. Instrumenten hebben ook hun eigen `00-FF` **Vol** en **Pan** in de bovenste rij van het Instrument-scherm. Instrument PAN wordt eerst toegepast, daarna track PAN. Beide behouden een bestaande stereo-beeld in het midden; ze verzwakken progressief de tegenovergestelde kant bij verplaatsing naar links of rechts.

### Auto Mix

De Mixer bevat een **AUTO MIX**-knop. Deze rendert offline `6 seconden`, balanceert het trackvolume, vergelijkt daarna 8 octaafbanden met een roze-ruisprofiel en verlaagt tracks die die banden overbelasten zachtjes. Hij stelt voorzichtige niveauwijzigingen voor, laat wat piekruimte over en start een tijdelijke preview. **Apply** behoudt de voorgestelde niveaus, terwijl **Cancel** de vorige 8 niveaus herstelt. Het is een goed startpunt, maar je zult het resultaat waarschijnlijk willen bijstellen.

### Reverb

Gebruik **SELECT + [UP]** vanuit Mixer om het reverb-instelmenu te openen.

- **Algorithm**: kiest tussen Clouds of Beads. Elke algoritme behoudt zijn eigen bedieningselementen.
- **Return**: nat reverbniveau in de mastermix.
- **Clouds**: **Time**, **Damping**, plus pre-reverb **LP** en **HP** filters.
- **Beads**: **Time**, native feedback-filter **Tone**, en een pre-reverb **HP** filter.

Dit is alleen het send reverbgedeelte van Clouds of Beads, niet de granular processor.

### Ping-pong Delay

Gebruik **SELECT + [DOWN]** vanuit Mixer om Delay te openen, daarna **SELECT + [UP]** om terug te keren. Reverb werkt in de tegenovergestelde volgorde: **SELECT + [UP]** opent deze en **SELECT + [DOWN]** keert terug naar Mixer.

- **Return**: nat delayniveau.
- **To Reverb**: hoeveelheid van het natte delaysignaal dat naar de gedeelde reverb wordt gestuurd, `0-100%`.
- **Ticks**: delaytijd in tracker ticks (`9 ticks` = `1 beat` standaard).
- **Feedback**: cross-feedback hoeveelheid, beperkt tot `95%`.
- **Filter**: low-pass filter in het delayed signaalpad.

De 1e herhaling volgt de stereo-ingang. Latere feedback kruist tussen linker- en rechterkanaal.

## 12. Project scherm

Het Project scherm biedt **Load**, **Save**, **New**, **Export**, **Manage** en **Scale** commando’s, samen met bestandsnaam, titel en auteur metadata.

### Export locatie

Exports, stems en bounces worden weggeschreven naar een project-specifieke map binnen de bestaande samples directory van de app:

- **Standaard**: `<app folder>/samples/Exports/<project name>/`. Terwijl het project geen naam heeft, is de map `current-project`.
- In de webversie is de standaard `/user/samples/Exports/<project name>/`.
- Op Android bevindt de samples-map zich in de workspace van de app: privé voor de app in de Play Store-versie, `samples` in de map die je koos in de directe APK-build.

De **Folder** rij in het EXPORT-scherm toont de actieve bestemming:

- Tik op **EDIT** om een eigen map te kiezen met de folderbrowser. De gekozen map wordt exact zo gebruikt (er wordt geen project-submap toegevoegd) en onthouden in de instellingen.
- **EDIT + OPT** (wissen) zet terug naar de standaardlocatie.

Als een genoemd project wordt opgeslagen onder een nieuwe naam, wordt de standaard exportmap hernoemd om te matchen, waarbij eerder geëxporteerde bestanden bij het project blijven. Hernoemen geldt niet voor een aangepaste map en de map wordt niet hernoemd bij autosave. Als een map met de doelnaam al bestaat, blijven beide mappen bestaan. Als de tracker de map kwijt is geraakt (bijv. na een herstart), hernoemt het opslaan van een genoemd project de bestaande `current-project` map. Instrumenten blijven werken na een hernoemen: geladen samplepaden die naar de hernoemde map wijzen worden automatisch bijgewerkt.**Load** accepteert `.mid`/`.midi` bestanden, geïmporteerd als een nieuw project: noten worden gegroepeerd per MIDI-kanaal (één kanaal per track, tot het maximum aantal tracks), gequantiseerd op 4 rijen per beat, en geplaatst op één standaard AY instrument - MIDI programmanummers hebben geen chiptune-equivalent, kies er dus later echte instrumenten voor. Alleen het eerste tempo van het bestand wordt gebruikt (één globale tick-snelheid, geen tempoveranderingen per sectie). De **MIDI** rij in het Export scherm doet het omgekeerde: schrijft de noten, volume en tempo/groove van de huidige arrangement als een Standard MIDI File (één MIDI track per tracker track); [MIDI Out](#midi-out) instrumenten en andere per-rij FX buiten volume en de globale groove hebben geen MIDI-equivalent en worden niet vertaald.

**Load** accepteert Dirtywave M8 songs (`.m8s`, firmware 2.x-4.x), geïmporteerd als een nieuw project. Alleen de structuur en noten worden overgenomen (songrijen, chains met transpositie, phrases met noot, velocity en instrumentnummer, note-offs, tempo en songnaam); noten behouden hun echte toonhoogte (de M8 noemt octaven twee hoger: zijn `C-6` is hier `C-4`). Elk instrument dat door een phrase gebruikt wordt, wordt geïmporteerd onder de naam van het M8 instrument. M8 **MACROSYN** instrumenten worden Braids instrumenten (de 47 shapes zijn de Braids modellen, met dezelfde nummers; timbre, kleur, filtertype, cutoff, resonantie en pan worden overgenomen, de cutoff curve is een benadering). **SAMPLER** instrumenten worden Sample instrumenten: de `.m8s` slaat slechts het pad van de WAV op de M8 kaart op, daarom wordt het bestand gezocht in de map van de `.m8s` en de ouders daarvan (een kopie van de kaartindeling werkt ook) en in je huidige samplemap (`Samples` of `samples` beide bruikbaar); als het niet gevonden wordt, behoudt het instrument het M8 pad en blijft stil. Geïmporteerde samples krijgen +24 halve tonen in hun Pitch instelling, omdat de M8 een sample in zijn originele toonhoogte twee octaven lager afspeelt dan deze app. **FMSYNTH** wordt een Genesis FM instrument (ruwe benadering), **WAVSYNTH** en **HYPERSYNTH** worden aChChid. **MIDI OUT** wordt een MIDI Out instrument (kanaal, bank, programma en de eerste vier CC nummers). Andere types (extern) worden een standaard AY instrument - kies later echte geluiden. FX en tabellen worden genegeerd.

**Load** accepteert Little Sound Dj songs (`.lsdsng`, één song per bestand), geïmporteerd als nieuw project. Het kiezen van een hele LSDj save (`.sav`, tot 32 songs) opent een lijst van de songs (slot, naam, versie, grootte); kies één om te importeren (de song die bewerkt werd toen de cartridge werd opgeslagen staat er niet in). Alleen de structuur en noten worden overgenomen (songrijen, chains met transpositie, phrases met noot en instrumentnummer, tempo en songnaam); elke noot krijgt vol volume, want LSDj heeft geen nootvolume, en de vier LSDj kanalen PU1, PU2, WAV en NOI worden tracks 1-4. Pulse instrumenten worden GB Pulse (duty, envelope en panning worden overgenomen; pulse sweep niet), noise instrumenten worden GB Noise (envelope, stabiliteit en panning), wave instrumenten worden de GB Pulse Hollow Lead preset en kits een standaard GB Noise. De grooves worden geïmporteerd, en de phrase commands `C` (akkoord), `D` (delay), `E` (pulse- en noise-envelope, niet op het wave-kanaal), `G` (groove), `K` (kill) en `V` (vibrato) worden `ARP`, `DEL`, `CEI`/`CEP`/`CED`, `GGR`, `KIL` en `PVB`. De andere commands (`A`, `F`, `H`, `L`, `M`, `O`, `P`, `R`, `S`, `T`, `W`, `Z`), tabellen, golf frames, kit samples en synth instellingen worden genegeerd, dus een song die daarop leunt zal anders klinken. Het tempo wordt omgerekend voor een 6-tick groove.

Op desktop en Android doet de **LSDJ** rij in het Export scherm het omgekeerde, beperkt: tik erop en kies een bestaand `.lsdsng` als **template** (onthouden tot afsluiten, **EDIT + OPT** vergeet het). De songrijen, chains, phrases, tempo en titel worden vervangen, de rest (tabellen, grooves, waves, kits, synth instellingen) blijft hetzelfde. Tracks 1-4 worden PU1, PU2, WAV en NOI; GB Pulse en GB Noise instrumenten worden teruggeschreven als LSDj pulse en noise instrumenten, andere instrumenttypes als standaard pulse instrument. FX, nootvolumes en note-offs worden niet geëxporteerd, en een song met chain `80` of hoger of phrase `FF` of hoger kan niet geëxporteerd worden (LSDj heeft 128 chains en 255 phrases). Het geëxporteerde bestand is nog niet getest in LSDj zelf.

Op desktop doet de **M8S** rij in het Export scherm het omgekeerde, beperkt: tik erop en kies een bestaand `.m8s` als **template** (onthouden tot afsluiten, **EDIT + OPT** vergeet het). Song, chains, phrases, tempo en titel worden vervangen, de rest blijft hetzelfde, behalve dat Braids instrumenten worden teruggeschreven als MacroSynth en Sample instrumenten als Sampler (pad `/Samples/<bestandsnaam>`: kopieer zelf de WAV daarheen op de M8), mits de template een instrument van dat type heeft om andere parameters van te kopiëren. FX worden niet geëxporteerd, en songs met phrase `FF` of hoger kunnen niet geëxporteerd worden (de M8 heeft 255 phrases). Het geëxporteerde bestand is nog niet getest op een echte M8. Zie [M8 songs](m8s-format.md) voor details.

Op desktop kun je met key jazz de bestandsnaam, titel en auteur direct op je toetsenbord typen in plaats van het virtuele toetsenbord op het scherm te gebruiken — zie [Key jazz](#key-jazz-desktop-only).

### Scale / Quantize

Het Scale scherm bestuurt de globale 12-TET scale engine. De **Mode** rij selecteert tussen twee modi:- **Quantizer** (standaard): frase-invoer blijft chromatisch; wanneer ingeschakeld worden noten direct vóór het geluidssysteem naar beneden afgerond, zodat het veranderen van de grondtoon of toonladder een nummer reharmoniseert zonder de frases te bewerken.
- **Note Lock**: noten die in frases worden getypt, worden bij invoer vastgezet op de dichtstbijzijnde toon van de toonladder, zodat alleen toonladdernoten kunnen worden geschreven. Nieuwe invoer en verlagen sluiten naar beneden aan; `EDIT + RIGHT` / `EDIT + UP` verhogen de noot naar de volgende toonladdernoot erboven. Invoer volgt altijd de projecttoonladder op dit scherm: verandering van grondtoon of toonladder beïnvloedt direct nieuw ingevoerde noten op ingeschakelde tracks, terwijl reeds opgeslagen noten onaangeroerd blijven. `SCL` FX is niet beschikbaar in deze modus en elk reeds geschreven `SCL` wordt genegeerd. Afspelen-kwantisering van gewone noten wordt overgeslagen (invoer is al vergrendeld); akkoordkwantisering via `CRD` blijft van kracht.

- **Clock source** kiest tussen **Internal** (het eigen tempo van het project), **Midi Clock** (de sequencer volgt een binnenkomende MIDI-clock) en **Ableton Link** (de sequencer volgt het tempo van een Link-sessie op het lokale netwerk). Bij **Midi Clock** is de **Tempo**-rij gedimd en toont het gemeten tempo van de binnenkomende clock; het transport (play/stop) wordt bestuurd door de Start/Continue/Stop-berichten van de zender, dus de PLAY-bediening van de app is niet nodig. Bij **Ableton Link** wordt de rij de **Link**-rij: die bevat de verbind/verbreek-schakelaar voor de sessie in plaats van een tempoveld (zie hieronder). De clockbron wordt in settings.txt opgeslagen en geldt voor alle projecten.
- **Tempo** stelt het projecttempo in hele BPM in (`3-500`). Intern slaat de tracker zijn tick rate nog steeds op in Hz (`tick rate = BPM x 0,4`), dus bestaande projecten laden en bewaren ongewijzigd; een project met een fractioneel tempo toont het afgerond en snapt naar een hele BPM bij de eerste tempobewerking. Grove aanpassing gaat in stappen van 5 BPM, fijne in stappen van 1; wissen herstelt de standaard van 125 BPM.

ChooChooTracker slaat projecten op als `.cct`. Dit is een eigen formaat dat zelfs voor mensen makkelijk leesbaar is.

Gebruik **Save** voordat je instrumenttypes verandert of een ander project laadt.

### Ableton Link

Ableton Link laat ChooChooTracker het tempo volgen van andere Link-compatibele apps (Ableton Live, LinkHut, enz.) op hetzelfde lokale netwerk (Wi-Fi of Ethernet). Het is alleen een ontvanger: de tracker past zijn eigen tempo aan de sessie aan en stuurt nooit tempo of transport naar andere deelnemers.

**Wat wordt gesynchroniseerd:**

- Sessietempo - de tracker volgt continu het tempo van de Link-sessie (binnen het 3-500 BPM-bereik van de tracker).
- Beatfase - op PLAY drukken lijnt de playback uit op het beatraster van de sessie: de song begint op de volgende maat (quantum van 4 beats).

**Wat niet wordt gesynchroniseerd:**

Tijdens playback met Link controleert ChooChooTracker de gedeelde beatfase eens per 4/4-maat en past zijn eigen tick rate zachtjes aan wanneer nodig. Dit compenseert kleine klokverschillen tussen apparaten zonder trackerticks over te slaan of te herhalen, en wijzigt nooit de Link-sessie van een andere deelnemer.

- Grooves en `SPD`- en `DELAY`-waarden worden niet beïnvloed - ze blijven relatief aan het gevolgde tempo.
- Cursorposities in song/chain/phrase en mixerinstellingen worden nooit gedeeld.

**Hoe te gebruiken:**

1. Open het Project-scherm en zet **Clock source** op **Ableton Link**. De rij eronder wordt de **Link**-rij.
2. Druk op EDIT bij **[Connect]** om de sessie te betreden. Het statusbericht rechts toont de voortgang: `Wait...` tijdens verbinden, `No net` als er geen netwerk is, `No sess` als er geen sessie wordt gevonden, `Err` bij een interne fout. Zolang je verbonden bent toont de knop **[Disconnect]** en wisselt de status ongeveer elke seconde tussen `connected` en `Peers: N` (N = aantal peers).
3. Wacht op `connected` en druk op PLAY - de playback begint uitgelijnd op de volgende maat van de sessie.
4. Druk op EDIT bij **[Disconnect]** (of zet de clockbron terug op Internal) om de sessie te verlaten.

In Link-modus is de sessie eigenaar van het tempo: de rij toont geen BPM-veld en het sessietempo wordt automatisch op het project toegepast (binnen het 3-500 BPM-bereik van de tracker).

**Beschikbaarheid per platform:** PortMaster (hoofddoel), Windows, Linux, macOS en Android worden ondersteund. De Web- en Miyoo-versies bevatten geen Link.

**PortMaster-opmerking:** op PortMaster-apparaten is de audiobuffer groter (512 frames bij 48 kHz), dus de beatuitlijning is bij benadering, binnen ongeveer 10-15 ms, en niet sample-exact. De tempo-synchronisatie zelf wordt niet beïnvloed.

## 13. Settings

Gebruik Settings voor voorkeuren die aan de app blijven verbonden, niet aan het huidige project. Projectbrede instellingen zoals tempo, groove en mixerniveau’s blijven op hun eigen schermen.

### Controls

- **Repeat delay** en **Repeat speed** bepalen hoe snel en hoe vaak een ingedrukte navigatie- of bewerkingsknop herhaalt. Verhoog de delay als lang ingedrukt houden te snel gaat; verlaag hem voor snelle trackerinvoer.
- **Stick live mode** bepaalt hoe de gemapte **STICK LIVE** knop werkt: `HOLD` is actief zolang je hem ingedrukt houdt, `TOGGLE` blijft actief na één druk en `FREE` is altijd actief. Zie [Motion recording](#motion-recording).
- **Key mapping** wijst toetsenbord- of controllerbesturingen toe. Kies een commando, druk op de nieuwe bediening, en selecteer daarna **Done** om de mapping op te slaan.

### Audio

**Sample rate** biedt `32 kHz` of `48 kHz` (standaard). Wijzigingen gelden direct en worden opgeslagen. De samplefrequentie wijzigen stopt playback en samplepreview terwijl de geluidsengines en elke gearmde audio-ingang op de nieuwe frequentie opnieuw worden geopend. Als een apparaat de nieuwe frequentie weigert, blijft de vorige instelling actief. `32 kHz` verlaagt de verwerkingsbelasting en begrenst de hoogste afgespeelde frequentie op 16 kHz. De CPU-belastingsindicator toont de kosten van audiorendering.

**Buffer size** ruilt reactietijd in voor veiligheid. Gebruik een kleinere waarde voor strakker live spelen; verhoog hem als het geluid kraakt of de CPU-meter consistent hoog staat. Beschikbare waarden zijn `32`, `64`, `128`, `256`, `512` en `1024` samples. Het CPU-belastingsdisplay helpt bij het kiezen van een stabiele instelling.

### MIDI

Kies hier in- en uitgangen, wijs binnenkomende MIDI-kanalen toe aan instrumenten, en map MIDI-CC-besturingen. Zie [MIDI](#15-midi) voor het aansluiten van een keyboard, het bedienen van de mixer of het sturen van noten naar externe hardware.

### Synths

- **AY Quality** kiest het kwaliteitsniveau voor AY/YM-instrumenten. **AY Sample dithering** kan AY-sample-afspelen verzachten; laat uit voor een directer, lo-fi resultaat of bij gebruik op originele hardware.
- **Braids BITS**, **DRFT** en **SIGN** zijn globale Braids-karakterinstellingen, gedeeld door alle Braids-instrumenten. Gebruik ze om resolutie, drift en kenmerk vóór het fijn afstellen van individuele geluiden in te stellen. Zie [Braids](#braids).

### Mixer

- **Mix volume** is het masteruitgangsniveau van de applicatie. Zet deze lager bij gebruik van een externe mixer of recorder; het verandert niets aan de trackmix van het nummer.
- **Tilt pivot** stelt de middenfrequentie in voor elke Mixer **Tilt**-regelaar in dit project, variërend van `250 Hz` tot `4000 Hz` (standaard `1 kHz`).

### Graphics

- **Edit color theme** verandert de interfacekleuren. **Load font** laadt een `.cnfont` weergavelettertype.
- **Song waveform** toont of verbergt de mixgolfvorm op de Song-pagina; zie [Song waveform](#song-waveform).
- **Track visuals** kiest per track tussen **Detailed** (het instrumentgolfvorm plus overlays) of **Audio waveform** (de uiteindelijke trackaudio). Gebruik **All detailed** of **All audio** om alle tracks tegelijk te wijzigen.
- **Waveform FPS** stelt in hoe vaak het instrumentscope- en mixervisualisatiescherm wordt vernieuwd, tussen `1` en `60` frames per seconde. Verlaag voor een rustiger scherm of meer headroom.
- **Renderer** toont of de applicatie `GPU`- of `Software`-rendering gebruikt.

### Support report and quit

**Support report** maakt een klein diagnostisch bestand aan voor een bugrapport. Voeg dit toe met het `.cct`-project als een probleem aan een specifiek nummer gekoppeld is. **Quit ChooChooTracker** sluit de applicatie netjes af.

### Song waveform

Graphics → Song waveform staat standaard UIT. Tik op EDIT om te schakelen, of gebruik EDIT + richting; de keuze wordt opgeslagen als app-voorkeur. Is het ingeschakeld, dan toont een tweerijige golfvorm boven de SONG-pagina de uiteindelijke mix mono opgeteld. Alle overige pagina’s behouden hun volledige hoogte, normale marges en instrumentvoorvertoningen ongeacht de instelling. De opgeslagen voorkeur blijft compatibel met eerdere versies. SONG-aanraakzones volgen de weergegeven rijen in beide standen.

## 14. Troubleshooting

### CPU

CPU-belasting hangt af van de actieve engines en effecten. Plaits fysieke modellen en Clouds Reverb zijn zwaarder dan eenvoudige AY-stemmen. Controleer prestaties op de doelconsole, zeker met 8 Plaits-stemmen en beide sends actief. Een CPU-meter rond `100%` kan kraakjes of gemiste deadlines veroorzaken.

Lopen er problemen met knappen, crashes of traagheid, gebruik **Settings > Support report** en voeg het gegenereerde `support-report.txt` toe aan het bugrapport. Gaat het om een specifiek nummer, voeg dan ook het `.cct` bestand toe.

### Geen geluid

Controleer het instrumentnummer, track-mute of solo-status, track LVL, applicatie Mix volume en instrumentenvelop. Voor samples: controleer of de originele WAV nog op de opgeslagen locatie staat.

Op ArkOS druk je **MENU + L3** om de systeemmute aan of uit te zetten. Helpt dat niet, start de hardware opnieuw en sluit de audio-interface opnieuw aan. Probeer de optie "repair audio" in ArkOS; dat kan helpen.

### Input voelt verkeerd

Stel Repeat delay en Repeat speed bij in Settings. Als een enkele druk dubbel registreert, controleer dan of slechts 1 fysieke bediening op die richting gemapt is en meld het exacte scherm en de sneltoets.

## 15. MIDISettings > **MIDI** opent dit submenu: **MIDI In** / **MIDI Out** kies een verbonden apparaat, waarbij je door gedetecteerde poorten bladert met `OFF` aan beide uiteinden; voor beide richtingen moet een apparaat geselecteerd zijn.

- **Geluid preview vanaf een MIDI-keyboard**: met een geselecteerde MIDI In device speelt het spelen van noten op het verbonden keyboard een instrument af op elk scherm, net als de on-screen **EDIT + PLAY** snelkoppeling op het [Instrument](#5-instruments)-scherm. Dit is alleen voor preview - het voert geen noten in in het nummer.
- **Kanaaltoewijzing**: opent een lijst van de 16 MIDI-kanalen; wijs elk kanaal een instrument toe (`OFF` standaard) zodat noten die op dat kanaal binnenkomen dat instrument afspelen, ongeacht welk instrument op het Instrument-scherm geselecteerd is - bijvoorbeeld kanaal `01` -> instrument `05`. Een kanaal op `OFF` valt terug op het momenteel geselecteerde instrument, het oorspronkelijke gedrag. Deze toewijzing wordt opgeslagen in settings.txt.
- **MIDI CC-toewijzing**: wijst een binnenkomende controller toe aan een instrumentparameter, inclusief de **Volume**, of aan globale controls. Globale rijen tonen `TRK 1`-`TRK 8` in plaats van een instrument en bieden track **Mute**, **Solo**, **Volume**, **Reverb send** en **Delay send**; **Song play/stop** heeft geen doel. Een nieuwe toewijzing negeert de CC die het heeft geleerd/ingevoerd en wordt actief bij de volgende fysieke beweging, waardoor het instellen van toewijzingen tijdens het afspelen een patch niet op nul kan zetten. Mute en solo gebruiken `0-63` = uit, `64-127` = aan.
- **MIDI-clocksynchronisatie**: met een MIDI In-apparaat geselecteerd zet je **Clock source** op het Project-scherm op **Midi Clock** om een extern sequencer te volgen. De tracker meet het tempo van de binnenkomende clock (24 pulsen per kwartnoot) en toont het in de gedimde **Tempo**-rij; de berichten **Start**, **Continue** en **Stop** besturen de playback. De clockbron wordt onthouden in settings.txt.
- **Een extern MIDI-apparaat aansturen**: geef een track het [MIDI Out](#midi-out) instrumenttype en stel het kanaal in; het triggeren van noten op die track stuurt echte MIDI naar het geselecteerde MIDI Out-apparaat in plaats van geluid te maken in ChooChooTracker.

## 16. Credits en licenties

ChooChooTracker is een fork van ChipNomad en behoudt de MIT-licentie aanpak. Braids, Plaits, Plaits-Alt, Clouds DSP, Warps-afgeleide MME DSP en stmlib-code zijn afgeleid van Mutable Instruments’ open-source releases onder hun toepasselijke MIT-licentie. Plaits-Alt is afkomstig uit de lylepmills/eurorack Plaits Lab-fork; de daarbij behorende bronvermeldingen blijven gelden. De aChChid-engine gebruikt Open303 door Robin Schmidt, copyright 2009, onder de MIT-licentie. MonkSynth gebruikt de native DSP van JonET onder de MIT-licentie; zie `LICENSE.monksynth.txt`. Bogie en Sintered zijn originele native implementaties. MIDI I/O gebruikt RtMidi door Gary P. Scavone, onder een MIT-achtige licentie. Zie de meegeleverde licentiebestanden voor exacte vermelding. Ableton Link wordt geleverd met een speciale licentieovereenkomst van Ableton, valt niet onder de MIT-licentie, en onze licentie geldt niet voor jouw forks of andere projecten dan ChooChooTracker.