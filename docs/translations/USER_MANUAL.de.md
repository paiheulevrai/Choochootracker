# ChooChooTracker Benutzerhandbuch
> Dies ist eine automatisch erzeugte Übersetzung von `docs/USER_MANUAL.md` aus dem aktuellen englischen Handbuch. Weicht sie vom Englischen ab, gilt der englische Text.
>
> **Konventionen.** Die Benutzeroberfläche der Anwendung ist auf Englisch. Bildschirm-, Menü-, Feld- und Buttonnamen, FX-Codes, Modul- und Engine-Namen sowie Dateiendungen bleiben auf Englisch.

ChooChooTracker ist ein 8-Spur-Music-Tracker für Handheld-Konsolen.

Es folgt dem klassischen LSDj-Arbeitsablauf und Navigationssystem, mit mehreren Sounddesign-Optionen:

- Vintage Chip- und Konsolen-Emulation
- Soundfonts
- Mutable Instruments Braids, Plaits und Plaits-Alt Oszillatoren mit einem Multimode-Filter
- PCM-Samples, Single-Cycle-Wellenformen und Wavetable-Oszillatoren mit Multimode-Filter
- aChChid, ein vielseitiger Acid-Synth basierend auf Open303
- Bogie, ein nativer 12-Modelle-Drumsynthesizer mit Multimode-Filter
- MonkSynth, ein Vokal-Synthesizer mit zehnstimmigem Unisono und Stereo-Delay
- MME, ein Zwei-Oszillator Multi-Modulationssynthesizer mit Multimode-Filter
- Sintered, ein nativer sechsstufiger experimenteller digitaler Drumsynthesizer

## 1. Installation und Dateien

### ArkOS / PortMaster

Installiere `choochootracker.zip` über PortMaster oder kopiere das entpackte Paket in das `ports`-Verzeichnis der Konsole. Starte **ChooChooTracker** aus dem Ports-Menü.

### Miyoo Mini / Mini Plus (Onion OS)

Entpacke das MiyooPorts-Archiv im Root-Verzeichnis der SD-Karte, sodass sich die `Roms/PORTS/`-Ordner mit den bestehenden Onion-Ordnern zusammenfügen. Starte **ChooChooTracker** im Ports-Menü nach der Aktualisierung der Sammlung. Sichere den `projects/`-Ordner der App, bevor du eine vorhandene Installation ersetzt.

Die Ladezeit eines Projekts kann je nach den im Song verwendeten Instrumententypen variieren. Wenn Start oder Projektladen ungewöhnlich langsam wirkt, melde die jeweilige Dauer zusammen mit dem Konsolenmodell und der Onion-OS-Version.

### Raspberry Pi OS 64-bit

Entpacke das Raspberry-Pi-AArch64-Archiv, installiere die SDL2-Runtime (`sudo apt install libsdl2-2.0-0`) und starte dann `./chipnomad.sh` aus dem entpackten Verzeichnis. Lasse die mitgelieferten Instrumente und anderen Assets neben der ausführbaren Datei liegen.

### Windows

Starte `choochootracker.exe` mit `SDL2.dll` und `libwinpthread-1.dll` im selben Verzeichnis.

Windows unterstützt Tastatur-Eingabe (siehe Jazz-Modus Abschnitt für Details) oder Gamepad.

### Web

Du kannst Choochootracker online unter https://choochootracker.vercel.app/ nutzen.

Diese Version kann mit Tastatur, Onscreen-Steuerungen (mobil) oder Gamepad gesteuert werden.

Gespeicherte Einstellungen, Samples, Themes, Projekte usw. werden im Browser-Speicher abgelegt.

Hinweis: Aufgrund von iOS-Beschränkungen ist die Midi-Nutzung in der Webversion auf iPhone unter Umständen nicht möglich.

### Verzeichnisübersicht

| Ordner | Inhalt |
|---|---|
| `AY_Wavetables` | `.aywave` AY Chip Wavetables |
| `fonts` | `.cnfont` ChipNomad Font-Dateien |
| `instruments` | `.cni` Instrument-Presets sowie FM-Bänke und Soundfonts |
| `pitch-tables` | `.csv` Alternative Stimmungen |
| `projects` | `.cct` Song- und Projektdateien |
| `samples` | `.wav` Dateien für die Sampler-Engine |
| `SR_wavetables` | Serum-Format Wavetables für die BYOWTBL-Engine |
| `themes` | `.cth` ChipNomad Themes |
| `title` | Assets für den Title-Screen |
| `waveforms` | `.wav` Single-Cycle Wellenformen für die 2xSCWF-Engine |

## 2. Steuerung

ChooChooTracker funktioniert am besten mit einem Gamepad, das ein D-pad, 2 Analoge Sticks und 7 Tasten hat.

| Logische Steuerung | Handheld Standard | Windows Standard |
|---|---|---|
| **[LEFT/RIGHT/UP/DOWN]** | D-pad | Pfeiltasten |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Space |
| **SELECT** | Select | Shift |
| Modulationsachsen | Linker und rechter Stick | Game-Controller Sticks |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

Belegungen können in **Settings > Controls > Key mapping** geändert werden, auch unter Android mit einem externen Gamepad. Speichere unter PortMaster eine Belegung mit **Done**, dann lasse den Button los, bevor du die Einstellungen fortsetzt.

### Navigation

- Nutze **[UP/DOWN/LEFT/RIGHT]**, um den Cursor zu bewegen.
- Halte **SELECT + [Richtung]**, um zwischen Bildschirmen zu wechseln.
- Halte **OPT + [Richtung]** für bildschirm-spezifische Navigation.
- Auf einem Wert nutze **EDIT + [LEFT/RIGHT]** für feine Änderungen, **EDIT + [UP/DOWN]** für grobe Änderungen.
- Tippe **EDIT**, um einen Wert einzugeben oder einen Befehl zu aktivieren.

### Auswahl und Zwischenablage

- **SELECT + OPT** aktiviert den Auswahlmodus und wechselt durch nützliche Auswahlbereiche.
- Nutze **[Richtung]**, um den Bereich zu erweitern.
- Tippe und lasse **OPT** los, um die Auswahl zu kopieren und den Auswahlmodus zu verlassen.
- **OPT + EDIT** schneidet die Auswahl aus.
- Außerhalb des Auswahlmodus halte **SELECT** und drücke **EDIT**, um einzufügen.
- **EDIT + [Richtung]** bearbeitet gemeinsam die ausgewählten Zellen.

### Wiedergabe

- **PLAY** startet an der Cursor-Position. Nochmal drücken stoppt.
- Du kannst Noten, Instrumente und Chains während der Wiedergabe bearbeiten; Änderungen sind beim nächsten Sequencer-Tick hörbar.
- **SELECT + PLAY** startet alle Spuren außerhalb des Song-Bildschirms.
- **EDIT + PLAY** spielt ein Instrument aus dem Instrument Pool vor.

### Global Bounce

Wähle einen Bereich von Song-, Chain- oder Phrase-Zeilen und Spalten, dann doppeltippe **EDIT**, um den BOUNCE TO SAMPLE-Bildschirm zu öffnen. Das Bounce umfasst alle ausgewählten Spuren: Spuren, deren erste Chain später in der Auswahl beginnt, warten stumm und starten an ihrer ersten Chain, sodass die Datei alle Chains der ausgewählten Spuren enthält.

Der Bildschirm bietet:

- **File name**: Bearbeite mit der Zeichen-Tastatur. Beim Öffnen wird die nächste freie Sequenznummer (`001`, `002`, ...) angeboten: die erste Nummer, deren `.wav` Datei noch nicht im Export-Ordner existiert. Du kannst frei bearbeiten – der Name wird unverändert verwendet. Existiert die Datei bereits, wird ein `_001`, `_002`, ... Suffix hinzugefügt, um Überschreiben zu vermeiden. Der Zähler wird zurückgesetzt, wenn du ein Projekt lädst oder erstellst.
- **Sample rate** und **Bit depth**: dieselben Optionen wie im Export-Bildschirm
- **Include in a sample name**: drei Kontrollkästchen, die beim Start mit Kontext-Tags im Dateinamen vorangestellt werden (standardmäßig alle aus):
  - **[BPM]** — das aktuelle Tempo des Projekts, z.B. `[120]`
  - **[Key]** — die Tonart und Skala des Projekts, z.B. `[Cmaj]`, `[F#min Pent]`
  - **[Bars:Beats:16ths]** — die gerenderte Länge des Bounces: eine Phrase entspricht einem Takt (16 Sechzehntel), ein Beat sind 4 Sechzehntel. Ganze Phrasen zeigen nur Takte, z.B. `[2]`; Teil-Phrasen zeigen `[0:2]` (zwei Beats) oder `[0:1:3]` (ein Beat plus drei Sechzehntel). Bei Multi-Track-Bounces zählt die längste Spur. Eine Auswahl, die in der Mitte eines Beats endet, zählt ein Sechzehntel weniger: die letzte ausgewählte Zeile ist der Schnittpunkt, ihr Note-Off landet am Ende des vorherigen Sechzehntels.
  - Die Tags werden in der Reihenfolge `[BPM][Key][Length]` vor dem Dateinamen angewendet.
- **Start**: rendert die Auswahl in eine WAV-Datei im Export-Ordner (siehe *Export location* im Project-Bildschirm)
- **Cancel**: zurück zum vorigen Bildschirm ohne Bounce

Während das Bounce rendert, bricht **OPT** den Vorgang ab. Nach Abschluss oder Abbruch kehrt der vorherige Bildschirm zurück.

## 3. Song-Struktur und Bildschirmübersicht

Halte **SELECT + [Richtung]**, um zwischen Bildschirmen zu wechseln.

Die Haupt-Navigationsreihe ist:

<pre>
        I
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

Mittlere Reihe:

- **M**: Mixer (weiter zu Reverb, Delay)
- **S**: Song (weiter zu Project, Settings)
- **C**: Chain
- **P**: Phrase (weiter zu Groove)
- **I**: Instrument (weiter zu Modulation und Instrument Pool)
- **T**: Table (weiter zum AY Wavetable Editor)

Obere Reihe:

- **R**: Reverb
- **P**: Project
- **G**: Groove
- **M**: Modulation
- **I**: Insert FX

Untere Reihe:

- **D**: Delay
- **S**: Settings
- **P**: Instrument Pool
- **W**: AY Wavetable Editor

Ein ChooChooTracker-Song folgt folgender Hierarchie:

```text
Song -> Chain -> Phrase Reihe -> Note + Instrument + FX
```

Ein Song hat 8 Spuren. Jede Spur enthält eine Folge von Chains, jede Chain eine Folge von Phrases. Phrases sind 16-Step-Notensequenzen. Eine Note enthält Pitch, Instrument, Velocity und bis zu 3 Spur-FX. Ein Instrument ist das, was den Klang erzeugt.

## 4. Song, Chain und Phrase

### Song

Der Song-Bildschirm enthält 8 Spalten, je eine pro Audiospur. Jede Zelle verweist auf eine Chain. Ein Song kann bis zu 256 Reihen (`00-FF`) enthalten.

Spuren verlaufen von links nach rechts, ihre Chains vertikal. Jede Spur ist unabhängig und Chains können zwischen 1 und 16 Phrasen enthalten, so können Spuren asynchron laufen. Entwerfe deine Chains so, dass sie synchron bleiben, oder lasse sie auseinanderdriften, wenn es zum Track passt.

Es ist in der Tracker-Community üblich, eine Chain für leere Phrasen zu reservieren, meist `00` oder `FE`.

Du kannst in einem Projekt mehrere Sub-Songs durch „Inseln“ erstellen: Bereiche, die durch leere Reihen getrennt sind.

#### Live Modus

Doppeltippe **OPT**, um den Live Mode ein- oder auszuschalten; `LIVE` ersetzt den Song-Titel. Im Live Mode startet **PLAY** die ausgewählte Chain auf ihrer Spur, falls diese gestoppt ist. Auf einer spielenden Spur wird die ausgewählte Chain zur nächsten Chain-Grenze vorgemerkt; zweimaliges Drücken von **PLAY** bewirkt einen dringenden Wechsel beim nächsten Phrase-Grenzpunkt. Eine ausgewählte leere Song-Zelle setzt stattdessen einen quantisierten Stop. `+` markiert vorgemerkte Chains, `!` dringende Chains und `-` vorgemerkte Stops.

Wähle eine Song-Zeile über mehrere Spalten, um diese Spuren gemeinsam zu starten oder zu stoppen. Mehrfachzeilen-Auswahl ist absichtlich nicht erlaubt. Live-Queues werden gelöscht, wenn eine Song-Bereich-Schleife aktiv ist. Live ist ein Performance-Modus und wird im Projekt nicht gespeichert.

#### Steuerung

- **OPT + [UP/DOWN]**: springt 16 Positionen nach oben oder unten
- Wähle einen Bereich und nutze **SHIFT + EDIT**: klont Chains flach, behält aber die Original-Phrasen
- Wähle einen Bereich und doppeltippe **SHIFT + EDIT**: klont Chains und Phrasen tief
- Wähle einen Bereich und nutze **EDIT + [UP/DOWN]**: verschiebt die Auswahl nach oben oder unten
- Tippe **OPT** dreimal: fügt eine visuelle Hervorhebung hinzu oder entfernt sie
- **OPT + SHIFT**: mute die aktuelle Spur oder ausgewählte Spalten (zuerst **OPT** loslassen, um den Mute zu aktivieren)
- **OPT + PLAY**: solo die aktuelle Spur oder ausgewählte Spalten (zuerst **OPT** loslassen, um das Solo zu aktivieren)
- **OPT + [LEFT/RIGHT]**: solo alle Spuren links oder rechts der aktuellen Spur
- Wähle einen Bereich und doppeltippe **EDIT**: öffnet den BOUNCE TO SAMPLE Bildschirm (siehe Bounce oben)

Das Stoppen der Wiedergabe sowie Änderungen von Mute und Solo nutzen eine kurze 3-ms-Ausgangskorrektur, um Knackser zu reduzieren. Das gilt auch für das Aufheben von Mute und Solo während der Wiedergabe.

### Chain

Eine Chain ist eine Liste geordneter 16-Step-Phrasen mit optionaler Transposition. Sie kann bis zu 16 Phrasen enthalten, wobei Phrasen mehrfach vorkommen können. Die 2. Spalte bestimmt die Transposition in Halbtonschritten.

Ein Stern (`*`) neben einer Chain zeigt an, dass sie im Projekt mehrfach verwendet wird. Chains können im Song-Bildschirm geklont werden.

#### Steuerung

- **OPT + [LEFT/RIGHT]**: wechselt zwischen Spuren
- **OPT + [UP/DOWN]**: wechselt zwischen Chains in der aktuellen Spur
- Wähle einen Bereich und nutze **SHIFT + EDIT**: klont Phrasen
- Wähle einen Bereich und doppeltippe **EDIT**: öffnet den BOUNCE TO SAMPLE Bildschirm für die ausgewählten Chain-Zeilen der aktuellen Spur (siehe Bounce unter Song)

### Phrase

Eine Phrase ist das Spur-Pattern im klassischen Step-Sequencer-Stil.

Eine Phrase ist 16 Schritte lang, mit je einer Zeile pro Schritt. Jede Zeile enthält Note, Instrument, Volumen von `00` bis `7F` und FX-Spalten. Noten verwenden Tracker-Notation wie `C-4` (Note und Oktave).

Zum Stoppen einer Note füge `NOTE OFF` ein, nutze kill-note FX oder spiele eine andere Note ohne Instrument.

Wenn eine Chain mehrere Phrasen enthält, arbeitet der Phrase-Bildschirm als kontinuierlicher Pattern-Editor. Bewegst du dich unter Zeile `F` oder über Zeile `0`, öffnet sich die nächste bzw. vorherige Phrase in der Chain.

Ein Stern (`*`) neben einer Phrase zeigt an, dass sie im Projekt mehrfach verwendet wird. Phrasen können im Chain-Bildschirm geklont werden.

Der FX-Selektor zeigt gebräuchliche Befehle plus die vom Instrument auf dieser Zeile unterstützten. So entfällt eine lange globale Liste enginespezifischer Befehle. ChooChooTracker unterstützt mehr FX als ChipNomad.

### Steuerung- **OPT + EDIT** auf einer leeren Note: fügt `NOTE OFF` ein
- **EDIT + [UP/DOWN]** in der FX-Namensspalte: öffnet den FX-Auswahlbildschirm
- **OPT + [LEFT/RIGHT]**: wechselt zwischen Tracks
- **OPT + [UP/DOWN]**: wechselt zwischen Phrasen in der aktuellen Chain
- Wähle einen Bereich in der Instrumentenspalte, dann **SHIFT + EDIT**: kopiert Instrumente
- Wähle einen Bereich, dann **EDIT + [UP/DOWN]**: rotiert die Phrase-Zeilen
- Wähle einen Bereich, dann doppeltes Tippen auf **EDIT**: öffnet den BOUNCE TO SAMPLE-Bildschirm für die markierten Phrasenzeilen des aktuellen Tracks (siehe Bounce unter Song)

Auf diesem Bildschirm kann bei aktiver Auswahl die Notenspalte
mit Rhythmusmustern gefüllt werden:

- **OPT + LEFT** füllt die Notenzellen der Auswahl mit einem festen 16-Stufen-Muster
  und wechselt bei jedem Druck zum nächsten Muster. Nach dem letzten Muster
  werden die Noten gelöscht und der Zyklus beginnt erneut. Muster sind absolut
  an Phrase-Zeilen ausgerichtet, daher erhält eine Teil-Auswahl nur die Schritte,
  die hineinfallen.
- **OPT + RIGHT** füllt die Notenzellen der Auswahl mit einem zufälligen Pattern aus
  3–8 Noten (7 und 8 seltener), das beim Halten von **OPT** endlos wiederholt wird.
- **OPT + UP** verändert die bereits gesetzten Noten in der Auswahl: bei jedem Druck
  werden 1–3 Noten ausgewählt und jede wird innerhalb einer Oktave zufällig
  verschoben (kleine Verschiebungen sind wahrscheinlicher). Die 8. Betätigung
  stellt die Noten wieder her, die zu Beginn der Aktion in der Auswahl lagen, danach beginnt der Zyklus von vorn.
- **OPT + UP** bei einer Auswahl, die nur die Lautstärkespalte umfasst (keine Note,
  Instrument oder FX-Zellen), randomisiert stattdessen die Velocity:
  der erste Druck wendet das Minimal-Muster an (Volumen verschiebt sich bis ±20),
  der zweite Medium (bis ±50), der dritte komplett zufällige Werte, der vierte stellt
  die ursprünglichen Werte wieder her. Das Loslassen von **OPT** setzt den Zyklus zurück auf Minimal.
  Reihen ohne Lautstärke bleiben unverändert.
- **OPT + DOWN** verteilt die gesampelten Noten geschnittener Sampler-Instrumente in
  der Auswahl chromatisch: die erste Note jedes Instruments (höchste im Sequencer)
  behält ihren Wert, jede folgende Note steigt um einen Halbton. So werden Stapel gleicher
  Noten in aufeinanderfolgende Slice-Triggers verwandelt. Jedes geschnittene Instrument
  verteilt sich unabhängig anhand seiner ersten Note, Noten von Instrumenten ohne Slicing bleiben unverändert.
  Das Ergebnis wird niemals an die Projekt-Tonleiter angepasst. Die letzte platzierte Note wird als letzte benutzte Note zum Einspielen gespeichert, sodass man die Reihe manuell mit dem nächsten Slice fortsetzt.
  Eine Auswahl mit nur geschnittenen Instrumenten verteilt bei jedem Druck; wenn nicht-geschnittene Noten dazukommen, läuft beim ersten Druck das Spread, ab dem zweiten das zufällige Arpeggio.
- **OPT + DOWN** bei einer Auswahl ohne geschnittene Instrumentnoten ist ein zufälliger Arpeggiator für die dort platzierten Noten. Ein Akkord wird aus der CRD-Palette ausgewählt — 3-Noten-Akkorde für 1–2 Noten, 3+ Noten-Akkorde bei 3, beliebige bei 4 oder mehr — mit der Grundnote auf einer zufälligen Tonleiter-Note im Register der Auswahl (chromatische Noten, wenn Skala aus oder Track ausgeschlossen ist). Die Töne werden in die Skala quantisiert wie bei der Wiedergabe von CRD. Jeder Druck verteilt den Akkord auf die platzierten Noten neu: zufällig, von tief nach hoch, von hoch nach tief, dann auf- und absteigend; der 5. Druck stellt die Originalnoten her, der nächste wählt einen neuen Akkord. Der Akkordname erscheint in der Statusleiste während des Arps, es werden keine Noten hinzugefügt oder entfernt, das Loslassen von **OPT** setzt alles zurück.
- Jeder Füllvorgang löscht die Noten der Auswahl vor dem Schreiben, platzierte Noten verwenden das zuletzt genutzte Instrument; Loslassen von **OPT** setzt die Zyklen auf den ersten Schritt zurück.

### Key Jazz (nur Desktop)

Drücke **Esc** im Phrase-Bildschirm, um Key Jazz ein- oder auszuschalten: ein QWERTY-pianostil Noteneingabemodus (ähnlich [m8c](https://github.com/laamaa/m8c)):

- `Z S X D C V G B H N J M` und `Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P` schreiben chromatisch über mehr als zwei Oktaven, basierend auf der physischen Position der Tasten, sodass das Layout auf AZERTY, QWERTY und QWERTZ Tastaturen gleich ist.
- Jede Taste schreibt die Note auf die aktuelle Zeile und rückt zur nächsten Zeile vor.
- `[` / `]` verschieben die Oktave.
- **Shift + Pfeiltasten** wählen einen Bereich von Zeilen und Spalten aus; eine einfache Pfeiltaste danach leert die Auswahl.
- **Ctrl+C** / **Ctrl+X** kopieren oder schneiden die markierten Spalten oder die Note/Instrument/Lautstärke der aktuellen Zeile, wenn nichts markiert ist; **Ctrl+V** fügt am Cursor ein.
- **Delete** löscht die ganze(n) Zeile(n) (alle Spalten) und schiebt den Rest nach oben; der Cursor bleibt in der selben Zeile.
- **Backspace** löscht das Element unter dem Cursor in der aktuellen Spalte (oder den Spalten der Auswahl, wenn aktiv), schiebt den Rest darunter nach oben und rückt den Cursor eine Zeile zurück, wie in einem Texteditor.
- **Insert** fügt eine leere Zeile ein und verschiebt den Rest nach unten.
- **Ctrl+S** speichert das Projekt. Außerhalb von Key Jazz macht Ctrl+S nichts.
- Nochmals Esc schaltet Key Jazz aus.

Während Key Jazz aktiv ist, übernimmt es die Notentasten im Phrase-Bildschirm vollständig, inklusive der Edit-/Opt-/Motion-Kürzel, die dieselben Tasten benutzen — schalte es mit Esc aus, um diese wieder zu verwenden.

Key Jazz funktioniert unabhängig davon, ebenfalls mit Esc schaltbar, auch in:

- **Song** und **Chain**: tippe die Hex-Indizes von Chains oder Phrasen direkt (`0-9`, `A-F`), anstatt mit Up/Down zu inkrementieren. Die erste Eingabe auf einer Zelle ersetzt ihren Wert; weitere Eingaben überschreiben sie (eingeben von „3“ und dann „F“ setzt 3F). In Song wählen **Shift+Pfeile** Bereiche, **Delete** löscht komplette Zeilen und schiebt nach oben, **Backspace** löscht aktuelle/markierte Spalten und schiebt nach oben, **Insert** fügt eine Zeile ein, **Ctrl+C**/**Ctrl+X**/**Ctrl+V** kopieren/schneiden/einfügen markierte Zeilen (oder aktuelle Zeile, wenn nichts markiert) — dieselben Struktur-Kurzbefehle wie im Phrase-Bildschirm. Das überschreibt Shift, weshalb Shift+Right/Up während Key Jazz hier nicht mehr zu Chain/Project springt — Esc bringt die Kürzel zurück.
- **Jeder Bildschirm mit Texteingabefeld**: tippe direkt auf der Tastatur, statt der Bildschirmtastatur. Gilt für Projektname, Titel, Autor, Instrumentnamen, Farbthema, Pitch-Table, Bounce-Dateinamen und Namensdialoge (Name eingeben, Ordner erstellen). Backspace funktioniert normal, Shift schreibt Großbuchstaben. Esc schaltet Key Jazz für alle diese Eingaben gleichzeitig ein/aus; die virtuelle Tastatur erscheint weiter normal, wenn Key Jazz aus ist.
- **Instrument**: Mit Key Jazz spielen die Notentasten das Instrument des aktuellen Tracks zur Vorschau, ohne dass Noten eingetragen werden (gleiches Piano-Layout wie Phrase-Bildschirm; letzte Taste klingt, bei Loslassen Stopp). Die Oktav-Tasten verschieben die Oktave, ausgehend von der Oktave der ersten Note des Instruments. Im **Name**-Feld schreiben die Tasten stattdessen Text. Während der Song diesen Track spielt, wird nichts abgespielt.

## 5. Instrumente

Alle Instrumente haben einige gemeinsame Parameter:

- Instrumenttyp
- Name, bis zu 15 Zeichen
- Standard-Tabellengeschwindigkeit, in Ticks pro Tabellenzeile
- Lautstärke
- Panning
- Transpose an oder aus

Jedes Instrument hat eine Standard-Tabelle mit derselben Nummer im Bereich `00-7F`. Diese können als Hilfstabellen verwendet werden, aber Hilfstabellen im Bereich `80-FE` zu halten vermeidet Konflikte und Verwirrung. Tabellen sind eines der Hauptsounddesign-Werkzeuge in ChooChooTracker.

### Steuerung

- **OPT + [LEFT/RIGHT]**: zwischen Instrumenten wechseln
- **EDIT + PLAY**: Instrument vorspielen
- **SHIFT + OPT**: Instrument kopieren
- **SHIFT + EDIT**: Instrument einfügen

### SOUNDFONT (experimentell)

Original SF2-/SF3-Bänke, SFZ/EXS-Instrumente mit Samples oder ZIP-Archive nach
`instruments/USER/soundfont` kopieren. **SOUNDFONT** im Instrument auswählen.
Die Scan-Zeile zeigt **Not scanned**, **Scanning...** oder **Scanned** mit **Rescan**-Funktion. Der Scan liest Metadaten, lädt aber nicht alle Preset-Samples in den Speicher.

Die **Preset**-Zeile zeigt das aktive Preset. Tippe EDIT kurz, um den Browser am Preset zu öffnen: Links erscheinen Kollektionen, rechts deren Presets. Jede Kollektion zeigt eine flache Liste, inklusive Presets in Bank-Dateien und ZIP-Archiven. WAV/AIFF-Dateien bleiben bei ihren gemappten Instrumenten; Kollektionen mit einzelnen Samples listen diese als Presets. Download-IDs und Archiv-Hashes werden aus Anzeige-Labels entfernt; Noten, BPM und Take-Nummern bleiben erhalten. EDIT wählt ein Preset aus; EDIT+PLAY spielt die geladene Auswahl vor.

Halte EDIT und drücke Links/Rechts auf Preset, um sequenziell in Browser-Reihenfolge zu blättern, über die Kollektionen hinaus und mit Wrap an beiden Enden. Schnelles Blättern lädt das zuletzt angeforderte Preset vor. Das aktuelle Preset bleibt spielbar während ein neues geladen wird. Scheitert das Laden, bleibt das gewünschte Preset ausgewählt und das Instrument stumm. **Error:** und Grund erscheinen unten im Instrument; Blättern läuft von dort aus weiter. Nach Import oder Änderung der Bibliothek Dateien neu scannen. Status von Presets steht direkt unter Preset mit festem Farbcode. Laden gibt keine Nachrichten auf SONG oder anderen Seiten aus.

**Native keys** zeigt die ausgewählten Note-On-Bereiche des Presets, mit C4 als MIDI-Note 60. **(gaps)** bedeutet, dass es innerhalb des Bereichs nicht für alle Noten eine Zone gibt. Dies ist die Vereinigung aller Velocity-Layer; nicht alle Velocity- oder Artikulationsstufen müssen bei jeder dargestellten Taste klingen. Release-only Samples erweitern den Bereich nicht. Tuning und Pitch FX ändern diesen Bereich nicht und erweitern die Mappings nicht.

**Edit**, rechts von Preset, öffnet den experimentellen SoundFont-Editor:
Tune (−2400 bis +2400 Cent), Cutoff (20–20000 Hz), Resonanz (0–40 dB), Hüllkurve Verzögerung/Attack/Halten/Decay/Release (0–20000 ms), Sustain (0–100 %), Lautstärke, Pan und **Bend step** (1–100 Cent pro PBN Einheit). Neue Instrumente nutzen einen Bend-Schritt von 1 Cent. Ältere Dateien zeigen **Stock**, behalten ihre bisherige PBN-Skala (25 Cent bei linearem Pitch, 1 bei Period Mode); stelle Bend step auf 1 für feinere Biegungen. Clear setzt auf 1 Cent zurück. Das betrifft nur SoundFont PBN; FPT bleibt fix getuned. Lautstärke und Pan sind dieselben Instrumentwerte wie auf der Startseite. Ton- und Hüllkurvenfelder standardmäßig auf **Bank**, behalten für jede Layer den Autorenwert. Ein direkter Wert überschreibt den Parameter in allen Layern. Clear setzt auf **Bank**, 0 Cent für Tune, volle Lautstärke oder zentriertes Pan zurück. EDIT+Links/Rechts macht feine Änderungen, EDIT+Oben/Unten größere. OPT springt zurück zu Instrument; EDIT+PLAY spielt das Instrument mit der Songnote oder Standardnote vor. Das bearbeitet Steuerungen der Instrumentwiedergabe, nicht die bankinternen Mapping-, Key-/Velocityzonen oder Loop-Punkte der Samples.

Tuning, Cutoff und Resonanz wirken live. Attack, Delay und Hold werden bei Note-On gesetzt. Decay, Sustain und Release können während des Spielens aktualisiert werden; Release startet mit Note OFF, nicht nur auf leeren Zeilen. Decay ist bei 100 % Sustain nicht hörbar. Release-Zeit kann keine Probe über ihr Audio hinaus verlängern. Eine explizite Release-Überschreibung lässt ein One-Shot-Sample auf Note OFF reagieren. Bei Bank-Einstellungen bleibt das original One-Shot-Verhalten. Die Key- und Velocitybereiche des Presets bleiben verbindlich; Noten außerhalb sind stumm. FPT kann ein schon ausgewähltes Sample außerhalb dieses Bereichs pitchen ohne Mapping-Änderung. Änderungen werden in Instrument- und Projektdateien gespeichert, ohne die Originalbank zu verändern; ältere Songs ohne Overrides behalten die originalen Banks.Eine WAV mit einem unbekannten sampler-spezifischen Loop-Typ kann ihr Audio ohne diesen Loop abspielen. Die Instrument-Warnung nennt den ignorierten Loop-Typ; die Quelldatei bleibt unverändert. Ein vorwärts gerichteter WAV-Loop, der genau ein Sample über das Audioende hinaus geht, wird als exklusives Ende an der Sample-Grenze mit Warnung akzeptiert; die PCM- und expliziten SFZ/EXS Loop-Zuordnungen bleiben unverändert. Andere ungültige Loop-Grenzen und Root-Noten melden spezifische Fehler. Ein einzelnes null RIFF-Ausrichtungsbyte, das irrtümlich in einem ungeraden PCM-Frame-Layout mitgezählt wurde, wird mit Warnung ignoriert; andere partielle PCM-Frames bleiben Fehler. Standardmäßige abwechselnde/rückwärts WAV-Loops werden weiterhin nicht unterstützt.

**SoundFont FX** stellt acht Regler im kontextbezogenen Lane FX Picker bereit. Alle Werte unten verwenden Tracker-Hexadezimalnotation. Die FX beeinflussen nur ihre eigene Spur und verändern die gespeicherten Instrument-Steuerungen nicht. Jeder neue Ton startet mit den Instrumenteneinstellungen und wendet dann Befehle in seiner Zeile und den nachfolgenden Zeilen an.

| FX | Steuerung | Werte |
| --- | --- | --- |
| FPT | Pitch | `00` = −24 Halbtöne, `18` = null, `30` = +24; exakte Halbtonschritte |
| FCF | Cutoff | `00`–`FE`: 20 Hz–20 kHz, logarithmisch |
| FRS | Resonance | `00`–`FE`: 0–40 dB |
| FAT | Attack | `00`–`FE`: 0–20 Sekunden, feinere Abstufungen bei kurzen Zeiten |
| FHD | Hold | `00`–`FE`: 0–20 Sekunden vor Decay, auf der Notenzeile gesetzt |
| FSU | Sustain | `00`–`FE`: 0–100% gehaltener Pegel |
| FDC | Decay | `00`–`FE`: 0–20 Sekunden, feinere Abstufungen bei kurzen Zeiten |
| FRL | Release | `00`–`FE`: 0–20 Sekunden, feinere Abstufungen bei kurzen Zeiten |

Für FCF/FRS/FAT/FHD/FDC/FSU/FRL setzt `FF` die Einstellung auf EDIT zurück (inklusive **Bank**). Attack/Hold auf der Notenzeile platzieren. Decay/Sustain/Release können die aktive Note verändern; nutze OFF, um Release zu hören. Lautstärke und Pan verwenden die gemeinsamen Regler; veraltete FVL/FPN Befehle werden weiterhin geladen und abgespielt, sind aber nicht mehr verfügbar. Pitch-FX ändern die Stimmung ohne die ausgewählte Key-Zone der Bank zu verändern. Vorhandene Sequenzer-Pitch-/Fine-Pitch-Befehle, MODs und Track-Effekte bleiben verfügbar. Projekte speichern stabile Bank-/Preset-Referenzen und FX-Befehle; Sample-Bibliotheken bleiben extern.

Alle Bänke sind USER-Bänke. Große Instrumente können den Standard-64-MiB PCM-Cache überschreiten, und nicht unterstützte Quellparameter erzeugen Warnungen. Siehe den [SoundBanks-Guide](soundbanks/README.md) für Plattform-Pfade, Browser-Import, Cache-Konfiguration, Formatabdeckung und aktuelle Validierungsgrenzen.

#### SoundFont-Kompatibilität: Bänke, Presets und Modulation

Eine SF2- oder SF3-**Bankdatei** kann viele **Presets** zusammen mit ihren Samples enthalten. Eine Bibliotheks-**Sammlung** ist der Ordner, der im Browser zum Gruppieren dieser Dateien verwendet wird. Diese sind unterschiedliche Ebenen: Ein Preset kann ein nicht unterstütztes Verhalten zeigen, auch wenn andere Presets in derselben Datei gut übersetzt werden.

**Preset-Modulation** bedeutet programmierte Verbindungen von Eingängen wie Velocity, Key, Mod Wheel oder Pressure zu einem Soundparameter. Diese Verbindungen können einem Preset oder einer zugrunde liegenden Instrumentschicht gehören, die von mehreren Presets geteilt wird. Sie sind nicht zwangsläufig eine Einstellung, die jeden Sound in der Bank betrifft.

| Preset-Feature | Aktuelle Übersetzung |
| --- | --- |
| Sample-/Key-/Velocity-Zuordnung, Grundtonhöhe und Stimmung | Implementiert; autorisierte Bereiche bleiben erhalten. Velocity-Zonen und gewöhnliche Tracker-Velocity funktionieren weiterhin. |
| Sample-Offsets, unterstützte Loops, Gain/Pan und Exklusiv-Gruppen | Innerhalb des Parsers und der Stimmenbeschränkungen implementiert. |
| Statischer Low-Pass Cutoff/Resonanz und Lautstärkehüllkurve | Durch den Tracker-Renderer implementiert; keine Garantie für identische Reaktion zu anderem SoundFont-Player. |
| Benutzerdefinierte Preset/Instrument-Modulationspfade (`pmod` / `imod`) | Nicht implementiert. Autorisierte Controller-zu-Parameter-Zuordnungen, Beträge und Antwortkurven werden ignoriert. |
| SoundFont Modulation/Vibrato-LFOs und Modulationshüllkurven-Routing | Nicht implementiert; programmierte Bewegung von Pitch, Filter oder Lautstärke kann fehlen. |
| Key-skaliertes Lautstärkehüllkurven-Halten/Decay, feste Tastatur-/Velocity-Generatoren | Nicht implementiert. |
| SoundFont Chorus/Reverb Sends | Nicht implementiert. Tracker-Effekte können manuell gesetzt werden, reproduzieren aber nicht automatisch die Preset-Einstellungen. |

Die ältere Meldung **„Custom bank modulation ignored“** beschreibt eine prüfung auf Datei-Ebene: Der Parser fand benutzerdefinierte Modulationsdaten irgendwo in der Bank. Es besagt **nicht**, dass das gewählte Preset diese nutzt oder dass die gesamte Bank unbrauchbar ist. Diese allgemeine Einschränkung wird hier dokumentiert, statt mehrfach unter jedem Preset angezeigt. Detaillierte Parserdiagnosen speichern die Datensätze; andere Ladefehler und Kompatibilitätswarnungen erscheinen weiterhin bei Bedarf. Die Engine beansprucht auch kein vollständiges SoundFont-Default-Modulator-Verhalten. Tracker PBN, Vibrato, Edit und Lane FX sind unabhängige Steuerelemente, keine automatische Übersetzung ausgelassener Preset-Programmierung.

Für die Wahl einer Bibliothek beginne mit Presets, die vor allem auf Sample-Zuordnungen, Loops und statischen Hüllkurven basieren. Klänge, deren Charakter von Controller-Artikulationen, animierten Filtern oder programmierten LFOs abhängt, benötigen genaueren Vergleich in einem kompletten SoundFont-Player. **Lädt und spielt** bedeutet nicht **Preset vollständig reproduziert**. Vergleiche nützliche Noten und Velocity-Bereiche sowie gehaltene Noten und Note OFF. Ein Wechsel von SF2 zu SF3 (oder zurück) entfernt diese Modulationsbedürfnisse nicht automatisch. Eine Konvertierung muss Zuordnungen und Verhalten in von dieser Engine unterstützte Funktionen übersetzen; das Rendern eines Effekts in ein Sample erhält nur das gerenderte Ergebnis, nicht seine interaktive Controller-Reaktion. Siehe [Formatabdeckung und Grenzen](soundbanks/README.md#format-coverage) sowie [Validierungsnotizen](soundbanks/validation.md), bevor du eine Bank wählst oder konvertierst. Diese Grenzen beschreiben die aktuelle experimentelle Implementierung, nicht inhärente Einschränkungen aller SoundFont-Player.

#### SoundFont-Sammlungen durchsuchen

In SAMPLE → SOUNDFONT öffnet **Preset** deine Sammlungen. **Right** oder **EDIT/A** betritt direkt einen Ordner. In einer Blatt-Sammlung wechselt Right zu ihren Presets; ein weiteres Right blättert um 15 Einträge vor, ohne zu laden. EDIT/A lädt das markierte Preset. Left geht zurück zu den Sammlungen und dann zum übergeordneten Ordner; auch OPT geht zurück. PLAY startet/stoppt den Song beim Durchsuchen.

Ordner- und Sound-Symbole kennzeichnen die Einträge. Sammlungen nutzen die volle Höhe des Panels. Das gewählte Preset bleibt unter den acht Spurvorschauen; lange Namen scrollen nach einer Sekunde Ruhe in der Auswahl und starten neu, wenn du dich bewegst. EDIT+Left/Right auf der Instrumentenseite folgt derselben verschachtelten Ordnerreihenfolge und hält die Sounds jeder Sammlung zusammen. EDIT+PLAY hört an.

Die dritte Zeile meldet Reading preset, Finding samples, Unzipping, Checking samples oder Loading samples. EXS-Instrumente mit leeren oder `(null)`-eingebetteten Namen nutzen ihre Dateinamen. Quellbänke und Audio bleiben unverändert. Nutze **Rescan**, nachdem du die Dateien deiner Bibliothek geändert hast.

Siehe [Formatabdeckung und Grenzen](soundbanks/README.md#format-coverage) für unterstütztes Sampler-Verhalten und Sample-Formate.

### AY Classic, AY Plus und AY Sample

Dies sind die originalen ChipNomad AY/YM-Engines. AY Classic bietet hardware-ähnliche Ton-, Rausch- und Hüllkurven-Steuerungen. AY Plus erweitert um Software-Oszillatoren und reichere Modulation. AY Sample gibt Samples über AY-ähnliche Lautstärkelevel wieder und ist vom Sampler-Engine-Typ getrennt.

#### AY Classic

Dies ist ein Legacy-Instrumenttyp aus ChipNomad `v1.0.0`. Das neuere AY Plus-Instrument bietet mehr Features.

AY Classic Instrumente haben diese Parameter:

- Mixer: Ton an/aus, Rauschen an/aus, Hüllkurvenform (`0-F`)
- Lautstärke: software-generierte ADSR-Hüllkurve
- Automatischer Hüllkurvenzyklus: an/aus mit Rate von `1:1` bis `F:F`

**AY Quality** in den Settings beeinflusst nur AY/YM-Rendering. Es ändert nicht die Qualität von Braids, Plaits oder Sampler. **Sample-Dithering** wirkt auf die Quantisierung von AY Sample.

Siehe die [ChipNomad AY-3-8910 Dokumentation](https://chipnomad.org/chips/ay-3-8910/) für Chip-Details.

#### AY Plus

AY Plus wendet klassische Synthesizer-Konzepte auf den AY-3-8910/YM2149F Sound-Design an. Man kann AY als Synthesizer mit 5 Oszillatoren betrachten:

- 3 Rechteckton-Oszillatoren
- 1 Rausch-Oszillator
- 1 Amplitudenhüllkurvengenerator, der auch als Oszillator verwendet werden kann

Nach diesem Modell hat jedes Instrument 3 Hardware-Oszillatoren: Ton, Rauschen und Hüllkurve. AY Plus fügt einen 4. Software-Oszillator hinzu. Die schnellere Aktualisierung der AY-Register als der Projekttakt eröffnet mehr Klangdesignmöglichkeiten. Aufgrund der Funktionsweise von AY werden die Oszillatoren mittels Ringmodulation gemischt. Probiere verschiedene Pitch-Offsets, Detuning und Oszillatorkombinationen, um neue Klangfarben zu erzeugen.

Jeder tonale Oszillator kann unabhängig mit Pitch- und Feinstimmungs-Offsets eingestellt werden.

Software-Oszillatortypen und ihre Parameter:

- Pulse – Rechteckwelle mit kontrollierbarer Pulsbreite
  - FM Depth – einfache FM-Tiefe
  - Pulsbreite – Bereich `00-FF`, Schrittanzahl auf Projekt-Bildschirm einstellbar
  - Pulse Low – steuert den niedrigen Wert in einem sich wiederholenden Paar, dessen hoher Wert immer `15` ist
- Sync Tone – Hard-Sync Effekt für den Tonoszillator
  - FM Depth – einfache FM-Tiefe
- Sync Env – Hard-Sync Effekt für den Hüllkurvenoszillator
  - FM Depth – einfache FM-Tiefe
  - Pulsbreite – Dauerverhältnis der 2 Formen im Hüllkurvenpaar
  - Envelope Pair – 2 wählbare Hüllkurvenformen
- Wavetable – 32-stufige Wavetables, bearbeitet auf dem Wavetable-Bildschirm
  - FM Depth – einfache FM-Tiefe
  - Wavetable-Index
- Tone FM – einfacher FM-ähnlicher Effekt für den Tonoszillator
  - FM Depth – FM-Tiefe für den Ton
- Env FM – einfacher FM-ähnlicher Effekt für den Hüllkurvenoszillator
  - FM Depth – FM-Tiefe für die Hüllkurve

Pulse- und Wavetable-Softwareoszillatoren können nicht mit dem Hüllkurvenoszillator kombiniert werden. Der Hüllkurvenoszillator schaltet sich automatisch aus.

Simple FM wechselt schnell zwischen 2 Periodenwerten, einem niedrigeren und einem höheren. Das ähnelt der Verwendung einer Rechteckwelle als FM-Modulator. Pulse, Sync Tone, Sync Env und Wavetable verwenden ein 1:1-Verhältnis zwischen FM-Träger und Modulator. Tone FM und Env FM können andere Verhältnisse über Pitch-Offsets der Softwareoszillatoren simulieren.

#### AY Sample

Du kannst mono oder stereo WAV-Dateien in `8-bit`, `16-bit`, `24-bit` oder `32-bit` laden. Sie werden in `8-bit` konvertiert und auf 16 KB gekürzt. Dieses Limit entspricht der RAM-Seitengröße des ZX Spectrum.

AY spielt Samples durch schnelle Schreibzugriffe auf das Lautstärkeregister ab. Die Ausgabe ist unipolar, daher müssen Samples für saubereren Klang „auf Null gehoben“ werden. Dies entfernt Klicks am Sample-Ende und verbessert leise Ausklänge.

AY hat eine `4-bit` DAC-Auflösung mit nichtlinearer Lautstärkeskala, daher werden `8-bit` Samples während der Wiedergabe auf `4-bit` reduziert. Die Sample-Dithering-Option in den Settings kann die wahrgenommene Bit-Tiefe verbessern. Dithering ist auf Retro-Plattformen wie ZX Spectrum und Atari ST nicht praktikabel, daher solltest du es ausschalten, wenn du für echte Hardware schreibst oder einen knusprigen Lo-Fi-Klang möchtest.

Samples verhalten sich wie ein weiterer AY Plus-Softwareoszillator, aber ihre zusätzlichen Parameter machen sie leichter handhabbar als separater Instrumenttyp.

Sample-Parameter:

- Sample-Rate – Abspielen mit nativer Rate ergibt ein `C-4`
- Sample-Start
- Sample-Länge
- Sample-Loop-Start
- Sample-Pitch-Offset
- Sample-Fein-Pitch-Offset

Die Ton- und Rauschoszillator-Sektionen entsprechen denen von AY Plus. Aufgrund der Funktionsweise von AY werden alle Oszillatoren mittels Ringmodulation gemischt.

### Braids

Braids bietet 47 Synthese-Modelle. Die Hauptregler sind:

- **Model**: Oszillator- oder Synthesealgorithmus
- **Timbre** und **Color**: modellabhängige Makro-Parameter
- **Filter**: zusätzlicher LP/BP/HP Filter, `12` oder `24 dB` Flankensteilheit, Cutoff und Resonanz
- **ADSR**: Attack, Decay, Sustain und Release

Tippe auf **Model**, um aus kategorisierten Modelllisten zu wählen.

| Kategorie | Modelle |
|---|---|
| Analog | `00 CSAW`, `01 MORPH`, `02 SAW-SQUARE`, `03 SINE-TRI`, `04 BUZZ`, `05 SQUARE-SUB`, `06 SAW-SUB`, `07 SQUARE-SYNC`, `08 SAW-SYNC` |
| Multi Osc | `09 TRIPLE-SAW`, `10 TRIPLE-SQR`, `11 TRIPLE-TRI`, `12 TRIPLE-SINE`, `13 TRIPLE-RING`, `14 SAW-SWARM`, `15 SAW-COMB`, `16 TOY` |
| Filter / Voice | `17 FILTER-LP`, `18 FILTER-PEAK`, `19 FILTER-BP`, `20 FILTER-HP`, `21 VOSIM`, `22 VOWEL`, `23 VOWEL-FOF`, `24 HARMONICS` |
| FM / Chaos | `25 FM`, `26 FEEDBACK-FM`, `27 CHAOTIC-FM` |
| Physical | `28 PLUCKED`, `29 BOWED`, `30 BLOWN`, `31 FLUTED`, `32 STRUCK-BELL`, `33 STRUCK-DRUM` |
| Drums | `34 KICK`, `35 CYMBAL`, `36 SNARE` |
| Wavetables | `37 WAVETABLES`, `38 WAVE-MAP`, `39 WAVE-LINE`, `40 WAVE-PARA` |
| Noise / Granular | `41 FILTER-NOISE`, `42 TWIN-PEAKS`, `43 CLOCK-NOISE`, `44 GRAN-CLOUD`, `45 PARTICLE`, `46 DIGI-MOD` |

#### Parameter

Timbre und Color hängen vom ausgewählten Modell ab. Siehe das originale Braids-Handbuch von Mutable Instruments für die vollständige Modellauswahl und Details oder probiere sie einfach aus.

#### VCF und VCA

Braids speist den standardmäßigen ChooChooTracker Multimode-Filter und ADSR-Hüllkurve.

#### Einstellungen

Globale Braids-Einstellungen sind in den App-Settings verfügbar:

- **BITS**: reduziert die Ausgangsauflösung
- **DRFT**: fügt Oszillator-Pitch-Instabilität hinzu
- **SIGN**: reproduziert den originalen Braids-Wellenform-Unschärfe-Algorithmus

### aChChid**aChChid** ist eine monophone Acid-Bass-Engine basierend auf Open303. `Square` und `Saw` nutzen den nativen TB-303 Oszillator, Filter, Hüllkurve und Akzent-Verhalten. `Braids` ersetzt nur den Oszillator und geht dann denselben 303 Filter- und Verstärkerweg. Es bietet Model, Timbre, Color und einen zusätzlichen Waveshaper, der von sanfter Sättigung zu Wavefolding übergeht.

Cutoff und Decay verwenden logarithmische Bearbeitung: **EDIT + LINKS/RECHTS** verschiebt um einen musikalischen Steuerungsschritt, **EDIT + OBEN/UNTEN** um sechzehn Schritte. Cutoff reicht von 200 Hz bis 20 kHz; Decay von 200 ms bis 2 s.

Ein `F` in der Lautstärkespalte löst einen Akzent aus. `ASL` gleitet zu der Note aus der vorherigen Tonhöhe ohne Neuauslösung der 303-Hüllkurve; es hält automatisch das vorherige Gate, und `ASL 00` ergibt einen `60 ms` Glide. `ATY` hält die Note für den ganzen aktuellen Schritt offen. Noten ohne `ASL` werden immer neu ausgelöst.

Modulationsziele umfassen Decay und Accent sowie Timbre und Color im Braids Wave-Modus.

### Bogie

**Bogie** ist ein nativer One-Shot Schlagzeugsynthesizer. Tracker-Noten setzen die Tonhöhe, die Tracker/Instrument Lautstärke die Pegel; jeder Trigger hat seine eigene interne Decay-Phase, daher gibt es keine ADSR Seite. Es gibt 12 Modelle: `Kick`, `Snare`, `Hat`, `Clap`, `Tom`, `Rim`, `FM`, `Noise`, `Cowbell`, `Cymbal`, `Shaker` und `Clave`.

Sechs Makros steuern **Decay**, **Tone**, **Sweep**, **Noise**, **FM** und **Drive**. Die Bezeichnungen passen sich je nach Modell an (z.B. ist Kick’s Noise ein `Click`, Cowbell's FM metallische Kreuzmodulation). Jedes Makro ist bei jedem Modell aktiv, deren musikalische Funktion ändert sich aber je nach Klang: Sweep kann Tonhöhe, Burstabstand oder metallische Streuung verändern; FM fügt Kreuzmodulation, eine metallische Schicht oder einen Ring-Obertton hinzu. Die untere Hälfte jeder Regelung ist für konventionelle Drum-Klänge gedacht; die obere Hälfte öffnet breitere klangliche Morphs und synthetischere Resultate.

Jedes Modell kann den gemeinsamen LP/BP/HP Multimode-Filter mit Clean, Classic, Aggro oder Acid Charakter, `12`/`24 dB` Flankensteilheit, Cutoff und Resonanz verwenden. Bogie unterstützt seine aktiven Makros plus Volume, Pitch, Cutoff und Resonance als Modulations- und Motion-Recording-Ziele. Nutze kurze Decay mit Cowbell für scharfe Phonk-Attacks; erhöhe FM für härtere, metallischere Glocken.

Im Modulationsbildschirm drücke **EDIT** auf eine Quelle, um eine Familie zu wählen (Envelopes, LFO oder Sticks) und dann die Quelle. **Links** und **Rechts** blättern weiterhin direkt durch Quellen.

### Sintered

**Sintered** ist ein nativer One-Shot Digital-Percussion-Synth. Noten bestimmen die Grundtonhöhe; die sechs Modelle sind `Knot`, `Shard`, `Burst`, `Comb`, `Logic` und `Melt`. Jedes Modell hat **Decay**, **Mod** und **Motion** sowie drei modell-spezifische Regler: Knot nutzt Ratio/Spread/Fold, Shard Ratio/Feedback/Bite, Burst Noise/Color/Feedback, Comb Time/Damping/Regen, Logic Rate/Pattern/Crush und Melt Ratio/Chaos/Drive. **Mod** steuert immer die interne Kopplungstiefe.

Motion ist eine interne Makro-Hüllkurve: Werte unter `80` nutzen AD-Form, `80` deaktiviert sie, Werte über `80` Decay-only Form. Weiter weg vom Zentrum wird die Hüllkurve schneller. Die Ziele sind modellabhängig und machen Sintered im Tracker verlässlich: Knot animiert Mod/Fold; Shard Feedback/Bite; Burst Noise/Color/Feedback; Comb Damping/Regen; Logic Rate/Crush; Melt Chaos/Drive. Alle Modelle setzen ihren Zustand bei einer Notentriggerung zurück und begrenzen internes Feedback für reproduzierbare Schläge.

### MonkSynth

MonkSynth ist ein Vokal-Synthesizer. Gestalte Vokal, Stimmcharakter, Atem, Glide, Vibrato, Unisono und Delay auf der Instrumentenseite. EDIT öffnet alle Regler und den Patchnamen.

Wähle über den VOICE-Reglern eine Bank und ein Preset. ALL enthält FACTORY und USER; EDIT+PLAY hört das markierte Preset an, EDIT wählt es aus. Eigene Sounds speicherst du unter `instruments/USER/monk/`. Gespeicherte Instrumente und Songs behalten ihre MonkSynth-Einstellungen.

Werte sind hexadezimal `00..FF`, außer Voices (`01..0A`, eine bis zehn). Attack, Decay und Release umfassen 0..5 Sekunden; Detune 0..50 Cent und der Stimmenabstand 0..0,5. Volume, Pitch und die sechs Haupt-Stimmregler sind als Modulations- und Motion-Recording-Ziele verfügbar.

Alle sechzehn Regler erscheinen gemeinsam unter **MonkSynth FX**: `MWV` (Vokal), `MIN` (Stimme), `MAM` (Atem), `MFL` (Glide), `MFB` (Vibrato-Tiefe), `MSH` (Vibrato-Rate), `MUV` (Stimmen), `MDT` (Detune), `MVS` (Spreizung), `MDM` (Delay-Mix), `MDR` (Delay-Rate), `MLV` (Pegel) und `EAT`/`EDC`/`ESU`/`ERL` (ADSR). Diese Bedeutungen gelten, wenn MONK das aktive Instrument ist; die bestehenden MME-Befehlsbedeutungen bleiben unverändert. Die gemeinsame Trigger-FX-Gruppe behält TDC/TCL ohne doppelte Hüllkurven-Einträge.

### MME

Wähle **SYNTH → MME** im Instrumententyp-Menü.

**MME** (Multi Modulation Engine) ist ein Zwei-Oszillator Synth, der auf Interaktionen zwischen Oszillatoren spezialisiert ist. Die Modelle sind `Ring`, `Fold`, `Cross`, `VPM`, `Sync`, `Logic` und `Vocode`. Beide Oszillatoren sind intern und folgen Tracker-Noten; `Interval` bestimmt ihr Verhältnis.

Die sechs Makros sind **Waves**, **Interval**, **Amount**, **Flow**, **Feedback** und **Shaper**. Waves interpolieren zwischen musikalischen A/B-Wellenformen. Amount steuert die zentrale Interaktion, Flow ändert Richtung oder Algorithmusvariante, Feedback ist begrenzt, wird am Ende seines Bereichs aber bewusst wild. Shaper ist die gemeinsame Endstufe, die von sauber über Sättigung zu Wavefolding wechselt.

MME nutzt den gemeinsamen LP/BP/HP Multimode-Filter plus ADSR. Alle sechs Makros, Cutoff und Resonance sind Modulations- und Motion-Recording-fähig. Halte **EDIT** auf Model gedrückt, um das Modell-Popup zu öffnen.

### MIDI Out

**MIDI Out** (Desktop, PortMaster und Web MIDI Browser) steuert ein externes MIDI-Gerät anstatt Audio zu erzeugen: Ein Notentrigger sendet ein echtes MIDI Note On auf dem Instrument-**Kanal** (`1-16`) und bei Notenende das passende Note Off. Volume wird zu Velocity, und Stimmen im Akkord-Track erhalten jeweils eigene Note On/Off. ChooChooTracker selbst erzeugt keinen hörbaren Sound – wähle zuerst das MIDI-Ausgabegerät unter [Settings](#13-settings). Siehe [MIDI](#15-midi) für komplette Infos inklusive Soundvorschau über ein MIDI-Keyboard.

**Program** und **Bank high/low** (CC0/CC32) sind optional – werden als `--` angezeigt, wenn deaktiviert, `Clear` schaltet ein Feld aus und merkt sich den Wert. Wenn gesetzt, werden sie einmalig gesendet, kurz vor der nächsten Note, sofern sie sich vom letzten Wert auf diesem Kanal unterscheiden (nicht vor jeder Note, um keine Reset-Effekte im Empfangsgerät auszulösen).

**MC1 number**-**MC4 number** wählen die CC-Nummer (`00-7F`), die jeweils die vier MC1-MC4 FX-Reihen senden – siehe [MIDI Out FX](#midi-out-fx). Auch `--`/optional; ein nicht gesetzter Slot macht das entsprechende FX inaktiv.

### Classic architecture engines

Die Engines in dieser Kategorie teilen eine VCO-zu-VCF-zu-VCA Architektur.

Es sind mehrere VCO-Typen verfügbar:

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **Sampler**
- **2xSCWF**
- **BYOWTBL**

Jedes Instrument speist seinen VCO in einen Multimode-Filter.

Der Filter hat wählbare `12 dB` oder `24 dB` Flankensteilheit und kann als Tiefpass (LP), Bandpass (BP) oder Hochpass (HP) arbeiten.

Folgende Filtercharaktere sind verfügbar:

- **Clean**: steril und digital
- **Classic**: inspiriert von klassischen amerikanischen Analogsynthesizern
- **Aggro**: inspiriert von dröhnenden japanischen Analogsynthesizern
- **Acid**: doppeltes Aggro von Aggro

Classic, Aggro und Acid können sehr resonant werden. Starte niedrig und erhöhe sanft.

Der Filter speist einen VCA, gesteuert von einer ADSR-Hüllkurve. Die Hüllkurvenform kann zwischen exponentiellen, linearen und logarithmischen Kurven morphen.

#### Plaits und Plaits-Alt

**Plaits** bietet 24 Engines, gruppiert im Auswahl-Popup wie folgt:

| Kategorie | Engines |
|---|---|
| Analog / Waves | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digital | `07 CHIPTUNE`, `15 SPEECH` |
| Textur / Noise | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Physisch | `19 STRING`, `20 MODAL` |
| Drums | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** ist eine Sammlung alternativer Modelle für das originale Plaits-Modul. Die meisten sind ungewöhnlicher oder experimenteller als der Durchschnitts-VCO.

| Kategorie | Engines |
|---|---|
| Granular / Micro | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| Phase / Harmonic | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| Akustisch / Physisch | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| Polyphonie / Harmonie | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| Digital / Weird | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

Die Parameter folgen Mutables Design:

- **Harmonic** steuert harmonische Beziehungen, Balance oder Modelauswahl innerhalb eines Engines.
- **Timbre** bewegt sich allgemein von dunkel/spärlich zu hell/dicht im Spektrum.
- **Morph** erweitert eine weitere klangliche Dimension.
- **Main/Aux** mischt den Hauptausgang mit der alternativen Engine-Ausgabe.

Die genaue Bedeutung hängt vom Engine ab. Beispielsweise nutzen Akkord-Engines sie für Akkordtypen, Umkehrungen und Wellenform. Physikalische Modelle steuern Material, Anregung und Abklingzeit, während Drum-Engines Ton, Charakter und Decay beeinflussen. Siehe Plaits-Handbuch für Details.

Für `CHORD`, `DIATONIC CHORD` und `WT DIATONIC CHORD` zeigt Harmonic den gewählten Akkordnamen (`MIN`, `MAJ`, `MIN7`, `MAJ7` usw.) statt des rohen Steuerwerts an. Braids `WAVE PARA` benennt ähnlich seine Color-Voicings.

Tippe auf **Engine**, um aus kategorisierten Engine-Listen zu wählen.

**Env Mode** hat 2 Routings:

- `TRIG` reproduziert das Modul mit verbundenem TRIG und unverbundenem LEVEL
- `VCA` hält LEVEL offen und wendet die Tracker-ADSR-Hüllkurve nach der Stimme an

#### Sampler

Dieser saubere Sample-Engine spielt mono- oder stereo PCM-Samples ab, die in den RAM geladen wurden.- Tippe auf **Sample**, um eine unkomprimierte `8-bit`, `16-bit` oder `24-bit` PCM WAV zu laden. Drücke **PLAY** im Browser, um die markierte Datei anzuhören.
- Wenn ein Sample geladen ist, erscheint neben dem Namen **EDIT**. Tippe darauf, um den Sample Edit Bildschirm zu öffnen. **OPT** oder **SELECT + [LEFT]** kehrt zum Instrument zurück.
- Der Sample Edit Bildschirm zeigt den Dateinamen (mit einem `*` Marker, wenn das Sample im RAM vom Speicher auf der Festplatte abweicht), eine Formatanzeige (Sample-Rate, Kanäle), eine große Wellenform und die Felder **Region**, **Select**, **Slice**, **Process** und **File**.
- Änderungen existieren nur im RAM, bis sie gespeichert werden: Das Projekt speichert den Pfad der Sample-Datei und lädt die WAV beim nächsten Laden des Projekts erneut von der Festplatte, daher werden nicht gespeicherte Bearbeitungen verworfen, wenn man den Tracker ohne **Save**/**Save As** verlässt (der `*` Marker warnt davor). Ein anderes Sample ins Instrument zu laden verwirft ebenfalls ungespeicherte Änderungen und setzt die Region auf das ganze Sample zurück (Start `00`, End `FF`).
- **Region** setzt die Wiedergabe-Start-/Endmarkierungen, die auch im Sampler Instrument-Bildschirm verfügbar sind. Sie definieren normalisierte Wiedergabegrenzen (`00-FF`); liegt Start hinter End, wird rückwärts abgespielt. **EDIT + [LEFT/RIGHT]** justiert die Markierung um eine Einheit (`01`) und zoomt die Wellenform um sie, solange **EDIT** gehalten wird; **EDIT + [UP/DOWN]** justiert grob (Schritt 16) und stellt die Ansicht auf das ganze Sample zurück. **EDIT + OPT** setzt die Markierung auf den Standard zurück (Start `00`, End `FF`).
- Die Wellenform-Ansicht zoomt auf ein festes Fenster von einer Sekunde Audio um die bearbeitete Markierung und verschwenkt, um sie sichtbar zu halten; Samples, die ins Fenster passen, bleiben in voller 1:1-Ansicht. Der Zoom ist temporär: Er hält nur, solange **EDIT** gedrückt wird; loslassen stellt auf 1:1 zurück, ebenso grobe Schritte. Beim Eintritt in den Bildschirm wird immer die Ansicht auf das ganze Sample zurückgesetzt.
- **Select** legt eine Bearbeitungs-Auswahl in absoluten Frames fest, unabhängig von den Wiedergabe Start/End-Marken. **EDIT + [LEFT/RIGHT]** verschiebt einen Griff zwanzig Frames und zoomt darauf, solange **EDIT** gehalten wird; **EDIT + [UP/DOWN]** springt um `frameCount/64` (mindestens 16) und stellt auf volle Ansicht zurück. **EDIT (Tippen)** auf einen Griff kopiert die aktuelle Start- oder End-Markerposition auf diesen Griff. **EDIT + OPT** leert die gesamte Auswahl. Ist die Auswahl leer, zeigen beide Griffe `-`; bei invertierten Griffen tauschen sie automatisch die Seiten. Die Auswahl ist nur Sitzungszustand im Editor: Sie wird mit dem Projekt nicht gespeichert, beim Betreten des Bildschirms wird sie mit dem Wiedergabe-Region-Bereich initialisiert (das ganze Sample mit Standardmarken).
- **Slice** ist eine einzelne Zeile mit vier Wertzellen – **Mode**, **Count**, **Slice** und **Frame** – und wird mit dem Instrument gespeichert. Off spielt das Start/End-Fenster ab. Phrase-Noten wählen Slices chromatisch ab **C-0**, und Noten über das letzte Slice hinaus springen zum Anfang zurück (Note N spielt Slice `N % count`, ein Durchlauf nach dem letzten Slice beginnt wieder mit dem ersten). Geslicete Noten transponieren nicht die Tonhöhe und verwenden keine Scale-Quantisierung, aber **CRD** hält das gewählte Slice und transponiert seine Stimmen als Akkord. Slice-Anfänge sind in der Wellenform mit kurzen zwei Pixel breiten Linien markiert (dunkelorange; das aktuelle Slice hellerer Marker), das Slice unter dem Cursor bekommt einen schwarzen Hintergrundstreifen in der Wellenform (die Auswahlfärbung wird nicht über diesen Streifen gemalt, er bleibt komplett schwarz und klar unterscheidbar vom Rest der Vorschau).
  - **Mode** wechselt zyklisch durch `OFF` / `EQUAL` / `AUTO` / `LAZY` (EDIT + links/rechts oder Tippen; EDIT + OPT schaltet Slicing aus). Moduswechsel initialisiert die Slice-Punkte: **EQUAL** teilt das Start/End-Fenster gleichmäßig in **Count** Teile; **AUTO** führt eine Transientenerkennung im Fenster durch und setzt Slices an jedem erkannten Anschlag (bis zu **Count**); **LAZY** startet mit einem Slice, das das ganze Fenster abdeckt. Beim Wechsel zu LAZY wird die aktuelle EQUAL/AUTO-Einstellung (Count und Grenzen) zwischengespeichert, der Wechsel zurück stellt sie unverändert wieder her – keine Nachfrage, kein Datenverlust. Slice und Stretch schließen sich gegenseitig aus: Aktiviert man das eine, wird das andere deaktiviert, und während Stretch aktiv ist, sind die Slice-Zellen abgedunkelt.
  - **Count** zeigt die Slice-Anzahl (`01`..`64`). **EDIT + [LEFT/RIGHT]** erhöht oder verringert sie um eins (Minimum 1, Maximum 64), **EDIT + [UP/DOWN]** wechselt zyklisch durch Potenzen von zwei: `2 / 4 / 8 / 16 / 32 / 64`. Jede Änderung berechnet die Aufteilung neu: **EQUAL** teilt das Fenster neu gleichmäßig, **AUTO** führt eine neue Detektion mit dem neuen Zielwert aus (Empfindlichkeit der Detektion ergibt sich aus Count – mehr Slices bedeuten niedrigere Schwelle). Die Zelle zeigt `-` bei OFF. Im **LAZY** Modus bleibt die Anzeige hell (zählt die handplatzierten Slices) ist aber nur Display-Funktion: Der Cursor überspringt sie – ein Rechts-/Linkswechsel von **Mode** oder **Slice** landet auf **Slice**, ein Herunterkommen vom **Select**-Endmarker landet ebenfalls dort, ein Tippen auf das Feld hat keine Auswirkung (handplace Slices haben keine weitere Aufteilung zum Neuberechnen). **EDIT + OPT** oder ein einfaches Tippen tun hier nichts: Slice aus/an ist nur bei **Mode**, Löschen eines Slice bei **Slice**.
  - **Slice** ist der Browser: Zeigt das 1-basierte aktuelle Slice (`01`..Anzahl). **EDIT + [LEFT/RIGHT]** wechselt jeweils ein Slice vor/zurück, **EDIT + [UP/DOWN]** springt vier Slices (beides begrenzt, kein Wrap). Das aktuelle Slice ist in der Wellenform durch helleren Marker und den schwarzen Streifen markiert, die Ansicht zentriert auf den Slice-Start. **EDIT + OPT** löscht das aktuelle Slice (es verschmilzt mit dem vorherigen; das erste Slice kann nicht gelöscht werden, und führt das Löschen des letzten verbliebenen Slice zum Ausschalten von Slicing – Grenzen bleiben bei OFF erhalten und kehren beim erneuten Moduswechsel zurück). **EDIT (Tippen)** spielt das aktuell ausgewählte Slice als One-Shot ab (funktioniert in jedem Slice-Modus; Vorschau stoppt bei Tastenloslassen oder Verlassen des Bildschirms). Zeigt `-` wenn Modus OFF.
  - **Frame** zeigt den Startframe des aktuellen Slices in Hex. **EDIT + [LEFT/RIGHT]** verschiebt ihn um zehn Frames (zoomt auf Marker bei gehaltenem **EDIT**), **EDIT + [UP/DOWN]** verschiebt um hundert Frames und stellt auf die Komplettansicht zurück. Marker bleiben im eigenen Slice: Sie können nicht über den vorherigen oder nächsten Slice-Start hinausgehen. **EDIT + OPT** löscht den Slice.
  - **B** verlässt die Slice-Zeile zum Instrument ähnlicher wie in den anderen Bereichen; **SHIFT + LEFT** verlässt den Bildschirm von überall.
  - **LAZY Arbeitsablauf**: Mit ausgewähltem LAZY tippe **PLAY** für eine One-Shot Vollsample-Wiedergabe – es spielt einmal vom Start und stoppt selbstständig in original Tonhöhe und Geschwindigkeit (ein grüner zweipixeliger Marker folgt der Position in der Wellenform); nochmaliges Tippen stoppt vorzeitig. Während dem Abspielen bewirkt jeder **EDIT** Klick das Hinzufügen eines Slice an der Wiedergabeposition – egal, wo sich der Cursor befindet (Slices näher als 50 ms zu einem bestehenden werden mit `Too close to slice` abgewiesen). Eine erneute Wiedergabe behält bestehende Slices und fügt neue an jeder Tipp-Position hinzu, in der Reihenfolge – der **Count** Wert aktualisiert sich bei jedem hinzugefügten Slice und der **Slice** Browser folgt dem neuen Slice. Für handplatzierte Slices gelten die gleichen Browsing- und Bearbeitungsregeln wie bei den anderen Modi: **EDIT + Richtung** auf **Slice** browsed sie, **Frame** passt den Start an, **EDIT + OPT** löscht das aktuelle Slice. Die Frame-Zelle dimmt, während das Playback-Drop aktiv ist. **SHIFT + PLAY** wird in LAZY nicht abgefangen: Startet Phrasen-Wiedergabe wie überall. LAZY Slices spielen im Sequenzer ebenso wie EQUAL und AUTO: Phrase-Noten werden chromatisch ab **C-0** auf die handplatzierten Slices abgebildet, mit Wrap nach dem letzten Slice.
  - **Kombinationshinweise**: Solange der Cursor auf einer Slice-Zellen ruht und keine andere Nachricht gezeigt wird, zeigt die Statusleiste den Hinweis dieser Zelle und bleibt sichtbar. Bei **Mode** ist der Hinweis an den aktiven Modus gebunden und zeigt nur die Beschreibung (der Modusname ist schon in der Zelle): `Divides sample in equal parts`, `Divides sample based on transients`, `Press Play and add slices with EDIT` – Moduswechsel ändern den Text sofort, OFF leert die Leiste. Bei **Count** steht `Adjust the number of slices`, bei **Slice** `Browse slices`, bei **Frame** `Adjust slice start`. Jede Aktionsmeldung ersetzt den Hinweis bis zum Ablauf.
- **Process** wählt eine destruktive Bearbeitungsoperation: **Crop**, **Normalize**, **Delete**, **Silence**, **Fade In**, **Fade Out** oder **Reverse** (EDIT + links/rechts wechselt, tippen wechselt vorwärts, EDIT + OPT setzt keine). **GO** (gleiche Reihe) führt die gewählte Operation auf der aktuellen Auswahl aus, oder auf dem ganzen Sample, wenn die Auswahl leer ist. **Crop** behält nur die Auswahl; **Normalize** skaliert die Auswahl so, dass der Peak voller Pegel erreicht (beide Kanäle teilen Gain, Stereo-Bild bleibt erhalten); **Delete** entfernt die Auswahl und fügt die Reste zusammen (Löschen des kompletten Samples wird abgelehnt); **Silence** setzt Nullen in die Auswahl; **Fade In**/**Fade Out** laufen die Auswahl linear von/towards Stille an; **Reverse** spielt die Auswahl rückwärts (Frames werden in-place getauscht, beide Kanäle eines Frames bleiben zusammen, Länge und Marker bleiben gleich). Crop und Delete brauchen eine Auswahl – ist die Auswahl leer kommt die Meldung `Select region first`. Jede Operation pausiert kurz den Ton, speichert einen Undo-Schritt, und markiert das Sample als verändert im RAM: Die Datei auf der Festplatte bleibt unberührt bis zum Speichern (siehe unten) und das Verlassen des Bildschirms verwirft den Undo-Slot.
- **UNDO** (neben GO) setzt das Sample auf den Zustand vor der letzten Operation zurück. Es ist bis zur ersten Operation ausgegraut, wechselt bei wiederholtem Drücken zwischen Vor- und Nachzustand, und wird beim Betreten des Bildschirms zurückgesetzt.
- **File** verwaltet die Speicheraktionen. Das Instrument speichert den kompletten Pfad der geladenen WAV; diese Aktionen schreiben diese Datei oder verweisen auf eine neue. Der Instrumentname wird nie verändert.
  - Das File-Feld wechselt zwischen **Save** und **Save As** (EDIT + links/rechts oder tippen); **GO** (gleiche Reihe) führt die angezeigte Aktion aus.
  - **Save** überschreibt die WAV, von der das Sample geladen wurde. Für ein Sample ohne Slices fragt es zuerst `Overwrite <name>?`. Bei einem gesliceten Sample öffnet zuerst der **SAVE SLICES** Dialog und fragt, wo die Slice-Punkte gespeichert werden sollen: **SAVE TO SAMPLE** schreibt sie als Cue-Chunks in die WAV (portabel – DAWs zeigen sie als Marker) und behält sie im Projekt, **SAVE TO PROJECT** speichert eine reine WAV und hält die Punkte nur im Projekt, **CANCEL** bricht ab. Eine Checkbox `Don't ask again in this project` speichert die Wahl mit dem Projekt und überspringt den Dialog künftig (die Einstellung steht als `- Sample save choice:` in der .cct Datei und kann nur durch Editieren der Projektdatei geändert werden). Der Dialog wird bei Samples ohne Slices übersprungen. **Save** ist ausgegraut (und im Menü übersprungen), bis ein Sample mit Dateipfad geladen ist. Ein Schreibfehler behält den Dirty-Marker und zeigt den Fehler an.
  - **Save As** speichert das aktuelle Sample in eine neue Datei: Gib einen Dateinamen ein (vorausgefüllt mit dem aktuellen Namen ohne Endung) und wähle dann einen Ordner im Browser. Das Sample wird als `<folder>/<name>.wav` gespeichert, das Instrument zeigt auf die neue Datei, und der Ordner wird als Standard-Sample-Ordner gemerkt. Es ist ausgegraut (und im Menü übersprungen), bis Sample-Daten geladen sind, damit frische Samples erst einen Pfad bekommen können. Pfade über 255 Zeichen werden abgelehnt.
  - Der `*` Marker vor dem Dateinamen bedeutet, dass das Sample im RAM vom Speicher auf der Festplatte abweicht (jede Prozess-Operation setzt ihn, Save und Save As löschen ihn). Der Marker ist nur Sitzungsstatus: Er wird nicht im Projekt gespeichert und beim Wechsel in den Bildschirm zurückgesetzt. Wenn mehrere Instrumente auf dieselbe WAV-Datei verweisen, überschreibt das Speichern einer Datei diese für alle.
- Im Sample Instrument Bildschirm lässt sich mit **EDIT + [LEFT/RIGHT]** die vorherige oder nächste WAV im selben Ordner laden.
- **Pitch** transponiert in Halbtonschritten (`-48` bis `+48`).
- **Loop** wählt Aus, Loop oder Ping-Pong.
- **Speed** steuert granularen Time-Stretch von `0%` bis `500%` (`100%` ist normal).Nicht unterstützte WAV-Formate zeigen einen Fehler an. Konvertieren Sie ungewöhnliche Dateien vor dem Import in `PCM8`, `PCM16` oder `PCM24` WAV.

##### Audio-Eingang

Öffne **Settings → Audio → Audio Input**, bevor du von einem Mikrofon oder USB-Audio-Interface aufnimmst. Die Liste zeigt die aktuell am Gerät verfügbaren Eingänge. Wird ein aktiviertes Interface getrennt, wird die Aufnahme unterbrochen und der Audio-Handler startet neu, damit die App weiterläuft. Falls du aufgenommen hast, drücke **STOP**, um die unterbrochene Aufnahme abzuschließen. Wähle nach dem Wiederverbinden **[Refresh]** und prüfe, dass der gewünschte Eingang aktiviert ist, bevor du erneut aufnimmst; Wiederverbinden allein aktiviert ihn nicht zuverlässig. **[List]** liest die aktuelle Eingangsliste ohne Neustart des Audios neu ein.

Wähle das **ARM**-Kästchen eines Eingangs, um ihn dem Sampler-Recorder zur Verfügung zu stellen; sein kleiner Pegelmesser zeigt den eingehenden Pegel. Aktiviere jeweils nur ein Interface: Diese Version öffnet nur das erste aktivierte Interface, auch wenn mehrere Kästchen markiert sind. Deine Auswahl wird nach Gerätenamen gespeichert, ein nicht verfügbares Interface bleibt also deaktiviert, statt durch einen anderen Eingang ersetzt zu werden. Drücke **OPT**, um zu den Audio-Einstellungen zurückzukehren.

##### Audio-Eingang aufnehmen

Im Sample-Instrumentenbildschirm öffnet **REC** (oder **EDIT/REC**, wenn ein Sample geladen ist) den Sample-Editor, der Aufnahme und Bearbeitung in einem Bildschirm vereint. Aktiviere zuerst Eingänge im Bildschirm **Settings → Audio → Audio Input** (siehe [Audio-Eingang](#audio-eingang)). Die obersten Zeilen des Editors steuern die Aufnahme:

- **Input** schaltet die Aufnahmequelle durch: `T1`..`T8` nehmen die acht Spuren des Trackers auf, danach folgt das aktivierte Mikrofon bzw. USB-Interface. Für externe Aufnahmen schalte über `T8` hinaus weiter, bis der Gerätename erscheint. `-` bedeutet, dass keine Quelle ausdrücklich gewählt wurde; **REC** nutzt dann den ersten aktivierten Eingang und zeigt seinen Namen. Halte **EDIT** und drücke **LEFT**/**RIGHT**, um rückwärts/vorwärts zu schalten, oder **EDIT+OPT**, um auf `-` zurückzusetzen. Antippen von Input schaltet direkt weiter.
- Der Pegelmesser neben **Input** zeigt den Live-Pegel des aktivierten Eingangs (nur externe Quellen). **MON** schaltet das Eingangs-Monitoring: Der aktivierte Eingang wird mit -6 dB auf den Ausgang gelegt, um Rückkopplung zu verringern. Die Einstellung wird in der Einstellungsdatei gespeichert.
- **REC** startet die Aufnahme. Externe Quellen nehmen sofort auf; eine Spurquelle zeigt **ARMED** und die Aufnahme beginnt, wenn die Wiedergabe startet (drücke **PLAY**). **STOP** beendet die Aufnahme: Die WAV wird in den Export-Ordner des Projekts geschrieben (`recNNN.wav`), in das aktuelle Sampler-Instrument geladen, und die Region wird auf das ganze Sample zurückgesetzt. Verlässt du den Bildschirm mit **OPT** während einer laufenden Aufnahme, wird sie auf dieselbe Weise abgeschlossen.
- **PLAY** (nach einer Aufnahme angezeigt) hört die Aufnahme an; während der Vorschau wird es zu **STOP**. **CLEAR** (daneben) löscht die WAV-Datei und entlädt das Sample aus dem Instrument; nach dem Löschen nimmt **REC** wieder seinen Platz ein.
- Die Wellenform zeigt während der Aufnahme den Live-Eingang und nach dem Stopp die vollständige aufgenommene Wellenform. Die Statuszeile zeigt während der Aufnahme den Dateinamen und danach die Frame-Anzahl; eine externe Aufnahme zeigt außerdem `drop 0`, wenn keine Audio-Frames verloren gingen. Ein aufgenommenes Sample ist voll bearbeitbar: Loop-Region, Auswahl, Slicing, Bearbeitung und Speichern funktionieren wie bei jeder geladenen Datei.

Aufnahmen werden mit der gewählten Audio-Samplerate als 16-Bit-Stereo-WAV geschrieben. Spurquellen erfassen, was die Instrumente der Spur rendern (nach den FX); externe Quellen erfassen den Geräteeingang direkt.

#### 2xSCWF

2xSCWF ist ein dualer Single-Cycle-Wellenformoszillator. Laden Sie je einen Mono-WAV mit genau 1 Periode in jeden Oszillator. Beide Oszillatoren lesen vorwärts und springen am Ende ihres Zyklus zum Anfang zurück.

Mischen Sie die Crossfades A und B.

Detune ist fein und exponentiell von Unison bis `+200 ct`, danach in Halbtonschritten von `+3 st` bis `+24 st`.

2xSCWF behandelt jede Datei als 1-Zyklus-Tabelle und liest sie genau einmal pro Oszillatorzyklus aus. So können Sie 2 Single-Cycle-Wellen mischen und verstimmen.

Der Werkordner `waveforms/AKWF/` enthält Wellenformen der Adventure Kid Bibliothek. Online sind viele weitere verfügbar, wenn Sie Ihre Sammlung erweitern wollen.

#### BYOWTBL

BYOWTBL ist ein dualer Wavetable-Oszillator, kompatibel mit Serum-Tabellen. Jeder Oszillator lädt ein Mono-WAV mit aufeinanderfolgenden Single-Cycle-Frames. `Pos A` und `Pos B` scannen ihre Tabellen unabhängig von `00` bis `FF`. Der Motor interpoliert linear innerhalb jedes Frames und zwischen benachbarten Frames. `Mix` crossfadet die Oszillatoren, `Detune` versetzt Oszillator B.

### FM-Instrumentenfamilie

#### Native OPLL- und VRC7-Instrumente

Der Instrument Type-Selector hat eine FM-Gruppe mit **OPLL / MSX** (YM2413) und **VRC7** (DS1001). Beide bieten nun **73 Programme** in scrollbaren Bank-/Preset-Listen: die ursprünglichen 15 Töne, 40 zusätzliche verschiedene Töne aus emu2413s YM2413/VRC7/YMF281B Tabellen und 18 von ChooChoo erstellte Zwei-Operator-Programme. Exakte Duplikate von Tonbytes werden innerhalb jedes Engines ausgelassen. Die zusätzlichen Paletten nutzen den programmierbaren Tone-Slot des Chips; sie erweitern nicht den physischen ROM-Inhalt. Programm Null identifiziert einen benutzerdefinierten Ton. EDIT + links/rechts wählt das vorherige oder nächste Preset innerhalb des Bankfilters. Fine ct passt die Stimmung von -100 bis +100 Cents an.

In der Programmliste spielt EDIT + PLAY den markierten Sound ab; lassen Sie die Tasten los, um zu stoppen. EDIT bestätigt, OPT bricht ab. Das Durchsuchen/Auskoppeln ändert nicht das Instrument, die Tabelle oder das Songprojekt. Die normale Audition-Geste der Instrumentenseite funktioniert auch nach der Auswahl weiterhin. Native Chip-Hüllkurven steuern Attack/Release; die optionale Amp-Hüllkurve fügt eine Software-ADSR hinzu (siehe Native Chip Controls unten). Einen vollständigen Operator-Editor gibt es nicht.

#### AdLib / OPL2 und OPL3

FM umfasst auch AdLib / OPL2 (YM3812) und OPL3 (YMF262). Bank filtert die Werksliste; Preset öffnet Kategorien und die Alle-Ansicht. EDIT + PLAY spielt vor Auswahl ab, EDIT wählt, OPT bricht ab, EDIT + links/rechts auf Preset lädt den vorherigen/nächsten passenden Eintrag. Fine ct passt die lokale Stimmung an. Mode zeigt 2 Operator, 4 Operator oder Dual Voice. OPL2 blendet inkompatible OPL3-Patches aus; OPL3 kann die gemeinsame Zwei-Operator-Kollektion spielen.

Bank- und Preset-Popup-Titel zeigen die aktuelle Engine an. Ein Wechsel der Engine oder des Instrumenten-Slots setzt den Bankfilter auf **Alle Banken** zurück, auch beim Wechsel zwischen kompatiblen AdLib- und OPL3-Engines. Ein Rücksprung aus einem Popup im selben Slot behält den gewählten Filter. Das Abbrechen eines DX7-Bankimports stellt den vorherigen Filter wieder her.

Die Preset-Bestätigung veröffentlicht auch Änderungen, die beim Loslassen einer Taste gemacht wurden, zum nächsten UI-/Audio-Tick. Songs, die abgespielt werden, benötigen keinen weiteren Tastendruck oder Transport-Neustart, um den Patch zu übernehmen. Die Ausgabe folgt weiterhin dem konfigurierten Audiopuffer und dem Hüllkurven-/Retrigger-Verhalten der Engine.

Die Standardkollektion enthält 697 Quell-Entries von The Fat Man 2-op, The Fat Man 4-op und DMXOPL3 mit 589 normalisierten, einzigartigen Patch-Identitäten. Aliase behalten Quellnamen und Attribution. Quell-native Operator-Level, LFO-Tiefenflags, Notenversatz, festgelegte Percussion-Tonhöhen und Dual-Voice-Abstimmung bleiben erhalten. Tracker-Lautstärke verwendet Software-Amplitude; sie reproduziert nicht die Original-MIDI-Player-Lautstärkekurven oder Velocity-Offset-Richtlinien. Quell-Release-Dauer-Schätzungen schneiden sustainte Noten nicht ab. Einige Effekte haben langsame Attack-Phasen: Halten Sie Seashore, statt auf ein kurzes Klickgeräusch zu warten.

Das Auswählen eines Presets ändert nicht die Slot-Tabelle oder die von der Spur besessenen Inserts.

#### Sega PSG und native Game Boy Instrumente

CHIP umfasst Sega PSG, GB Pulse und GB Noise. Beim Sega PSG hält **Bassbereich: Erweitert** tiefe Noten klar unterscheidbar. Wählen Sie **Chip** für die ursprüngliche begrenzte Reichweite, in der die tiefsten Noten ineinanderlaufen. Festfrequenzrauschen behält seine ursprüngliche Tonhöhe; Ton-basiertes Rauschen folgt dem gewählten Bassbereich.
Die GB-Instrumente verwenden DMG Pulse/Noise-Register; kein Game Boy Wellenkanal wird freigegeben. Ihre voreingestellten Listen enthalten jetzt **24, 24 und 28** Presets jeweils. Die Ergänzungen umfassen periodisches/verbundenes Sega-Rauschen, GB Pitch-Sweeps, kurze Percussion, metallisches Rauschen, Drone- und Steigerungseffekte. Dies sind Originalprogramme, keine extrahierten Spielklänge.
Die Seite bietet native Modus-/Duty-/Width-, Hüllkurven-/Sweep-/Noise-Kontrollen nach Bedarf sowie eine Software-Amplituden-ADSR.

#### DX7 FM

Wählen Sie **Type → FM → DX7 FM**; DX7 ist der siebte Eintrag nach Arcade / YM2151. DX7 FM nutzt sechs Operatoren. Die Seite bietet Bank, Preset und Fine ct sowie die üblichen Instrumenteneinstellungen. Preset-Browsing nutzt Bank-/Kategorielisten. Halten Sie EDIT+PLAY, um den markierten Sound zu hören; loslassen stoppt die Wiedergabe. EDIT alleine bestätigt beim Loslassen; OPT bricht ab. Ein geladenes Preset kopiert den vollständigen Patch, ohne Tracker-Tabellen oder Spur-Inserts zu verändern.

Für eine permanente persönliche Bibliothek legen Sie `.syx`-Dateien in
**`instruments/banks/dx7/`** neben die vorhandene Instrumentenbibliothek. Unterordner werden unterstützt. Öffnen Sie **DX7 → Bank**: Jede Datei erscheint als benannte Bank, bereit zum Preset-Browsing. Öffnen Sie den Bank-Dialog nach dem Hinzufügen oder Entfernen von Dateien erneut; keine Konvertierung oder Load Instrument ist erforderlich. Eine Standard-Original-DX7/TX7-Bank enthält **32 Stimmen**. Eine Datei mit vier Banknachrichten erscheint als vier nummerierte Banken (insgesamt 128 Stimmen). Einzelstimmen-Dateien werden ebenfalls akzeptiert. Das ausgewählte Preset wird mit dem Instrument gespeichert, sodass das Entfernen der Quellbank den Song nicht verändert.

**Load Instrument** akzeptiert ebenfalls originale DX7/TX7 `.syx` Einzelstimmen und 32-Stimmen-Bänke. Wählen Sie einen Patch zum Verwenden aus oder brechen Sie ab, um das aktuelle Instrument beizubehalten. Verwenden Sie die oben genannte Bank-Ordnerstruktur für eine dauerhafte Bibliothek; speichern Sie das Songprojekt oder eine `.cni`, um einen bearbeiteten Patch zu sichern. Nicht unterstützte Yamaha-Formate werden abgelehnt.

DX7 hat eine Begrenzung von **16 aktiven Noten im Song**, inklusive Akkordstimmen und Release-Tails. Wenn das Limit erreicht ist, werden zuerst Release-Tails geopfert, dann gehaltene, leise Noten, damit neue Attacke spielbar bleiben. Preset-Audition ist nicht verfügbar, während der Song läuft.

#### FM Operator Editor

Genesis FM, Arcade FM, OPL2, OPL3, OPLL und VRC7 Instrumente besitzen einen **EDIT**-Knopf rechts neben Fine ct. Er öffnet eine Tabelle mit den Patch-Parametern:

- Genesis FM / Arcade FM
- OPL2 / OPL3: Bei OPL3 kann hier der Modus (2op, 4op, Dual) geändert werden.
- OPLL / VRC7: Das Bearbeiten eines ROM-Presets wandelt es in „Custom“.

Links/Rechts und die üblichen Edit-Tasten ändern Werte; das Instrument spielt weiter, sodass Sie Änderungen hören können. Die Statusleiste beschreibt den Parameter unter dem Cursor. OPT oder Links+Shift kehren zum Instrumentenbildschirm zurück. DX7 und SID haben keinen Operator-Editor.

Genesis FM (YM2612) und Arcade FM (YM2151) nutzen den selben Bank-/Preset-Browser, Audition- und Feinstimmungssteuerungen wie die anderen FM-Motoren. Ihre nativen Hüllkurven steuern den Release; Änderung von Tracker-Lautstärke oder -Pitch triggert sie nicht neu.

#### FM Werk- und USER-Presets

Native Chip-Instrumente nutzen **Bank:** zur Auswahl von ALL, einer enthaltenen Sammlung oder USER. **Preset:** wählt einen Sound innerhalb der Auswahl. Halten Sie EDIT und drücken Sie links oder rechts in einer Zeile, um zyklisch zu wechseln; kurzes Tippen auf EDIT öffnet den Auswahldialog.

Inklusive Sammlungen sind ZIP-Pakete unter `instruments/FACTORY/`. USER-Dateien liegen unter `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse` oder `gb-noise`.
Durchsuchen Sie Ordner direkt oder legen Sie eine ZIP kompatibler Presets dort ab. ZIPs bleiben unverändert auf der Festplatte. Alle diese Ordner akzeptieren kompatible `.cni`-Dateien; CNI ist ChooChoo/ChipNomads portables Instrumentenformat, kein Chip-Herstellerstandard. Der USER-Browser liest außerdem folgende Quellformate, auch innerhalb von ZIPs:

| USER-Ordner       | Zusätzliche Formate                                                     |
| ---               | ---                                                                    |
| `dx7`             | Yamaha DX7/TX7 `.syx` Einzelstimmen und 32-Stimmen-Bänke              |
| `genesis`          | `.tfi`, DefleMask `.dmp`, Furnace `.fui`                              |
| `arcade`          | VOPM `.opm` Bänke, DefleMask `.dmp`, Furnace `.fui`                   |
| `opl2`, `opl3`    | `.wopl`, `.opli`, `.woplx`, `.sbi`, Furnace `.fui`                    |
| `opll`, `vrc7`    | Furnace `.fui` Custom Patches und festgelegte ROM-Auswahlen           |
| `sega`, `gb-pulse`, `gb-noise` | Furnace `.fui`, DefleMask `.dmp`                                 |
| `sid`             | GoatTracker 2 `.ins` (GTI5), Furnace `.fui`                           |

Eine Bank öffnet mit kompatiblen Stimmen. Nicht unterstützte Dateien oder Features geben einen Grund an; inkompatible Stimmen in einer gemischten Bank sind ausgeschlossen, während ihre Quellnummern stabil bleiben. Das Laden ist transaktional: ein abgelehntes Preset belässt das aktuelle Instrument unverändert. Quell-Presets ersetzen die Tracker-Tabelle des Instruments durch eine leere, außer DX7-SysEx, das seine bestehende Tabelle bewahrt. CNI stellt gespeicherte Tabellen wieder her. Geladene Patches gehören dem Song und funktionieren weiter, auch wenn die Quellpakete verschoben oder gelöscht werden.

WOPL-Bänke können ungenutzte Slots ohne explizites Leer-Flag enthalten. Der Browser ignoriert unbenannte Slots, deren aktive Operatoren vollständig gedämpft sind und keinen Attack haben. Nicht benannte, spielbare Stimmen bleiben als `Program N [Bbank]` oder `Drum N [Bbank]` verfügbar, basierend auf ihrer nullbasierten Programm- und Banknummer der Quelle. Ein unbenanntes einzelnes OPLI-Instrument nutzt den Dateinamen. Benannte stille Patches bleiben erhalten.

Furnace-, DefleMask- und GoatTracker-Dateien werden als Instrumente importiert, nicht als ganze Songs. Animierte Quellmakros, Samples und Song-spezifische Wiedergabeeinstellungen können verloren gehen. Game Boy-Dateien legen Sie in den vorgesehenen Pulse- oder Noise-Ordner.

Die USER-Preset-Zyklisierung läuft über Dateien, Ordner, Bänke und ZIP-Pakete in beiden Richtungen und springt am Ende um. Nach erneutem Öffnen des USER-Browsers wird die aktuelle Auswahl beibehalten. ALL schließt USER-Sounds unter Unsortiert für kategorisierte Engines ein; USER bleibt eine separate Top-Level-Auswahl. Sega PSG und Game Boy nutzen flache ALL/Factory-Listen und einen hierarchischen USER-Browser ohne Unsortiert-Panel. Ältere DX7-Dateien in `instruments/banks/dx7/` bleiben in USER zugänglich.

Eine Engine zeigt nur ihre kompatiblen enthaltenen Sammlungen an. Factory Presets kombiniert die enthaltenen OPLL/VRC7-Tonesets für die ausgewählte Engine; DX7 kombiniert ChooChoo- und YSE-Originale. Heruntergeladene benannte Sammlungen bleiben separat. Fat Man 2-op ist unter OPL2 gelistet, Fat Man 4-op unter OPL3. Kompatible OPL2 CNI-Dateien können weiterhin in OPL3 USER-Ordnern verwendet werden.

#### Native Chip Controls

Native Instrumenten-Wellenformvorschauen aktualisieren sich beim Neuzeichnen ihrer Regler. FM-Instrumente können ihre optionale Amp-Hüllkurve überlagern; SID behält seine Wellenformvorschau ohne die FM-exklusive Overlay, auch beim Browsen durch Banken und Presets.

Alle sieben FM-Engines bieten **Bright**, **Feedback** und **Amp env**. Bright reicht von -63 bis +63, wobei Null das Patch unverändert lässt; es ändert Pegel der Modulationsoperatoren, behält aber den Träger-Level bei. Die hörbare Wirkung hängt vom Algorithmus ab. Feedback ist standardmäßig auf **Preset**, oder überschreibt das native Feedback mit 0–7. Diese Regler ändern die gespeicherten nativen Operator-Bytes nicht.Amp env ist standardmäßig auf **Bypass** eingestellt. Wähle **ADSR**, um Attack, Decay, Sustain, Release und Form zur nativen Klangquelle hinzuzufügen. A/D/R verwenden den gemeinsamen 0–5 Sekunden quadratischen Bereich; Sustain reicht von Stille bis Vollpegel. Native Hüllkurven laufen weiterhin, daher kann diese Hüllkurve einen Ton nicht über seine native Release-Zeit hinaus verlängern. Das Preset-Browsing bewahrt die FM-Amp- und Tonregler des Slots.

Die folgenden Phrase-FX erscheinen auch als unterstützte Modulations- und Bewegungsaufnahmeziele. Werte in der FX-Spalte sind hexadezimal. Einstellungen wirken sich nur auf die Wiedergabe aus, ohne das gespeicherte Instrument zu ändern. Native GB Sweep- und Hüllkurvensteuerungen werden beim nächsten Note-Trigger verriegelt; Duty/Width und Noise-Frequenz können live geändert werden.

| Engines | FX | Steuerung |
|---|---|---|
| Native FM | `OL1`–`OL6` | Absolute Operator-Ausgangspegel: OPLL/VRC7 Modulator und OPL `00–3F`; OPLL/VRC7 Träger `00–0F`; Genesis/Arcade `00–7F`; DX7 `00–63` (0–99). Höher bedeutet mehr Lautstärke. Nur unterstützte Operatoren erscheinen. |
| OPLL/VRC7/OPL2/OPL3 | `OAR`, `ODR`, `ORR`, `OSL` | Operator 1 Attack, Decay, Release und Sustain-Abschwächung, `00–0F`. |
| OPLL/VRC7/OPL2/OPL3/Genesis/Arcade | `OMU 00–0F` | Operator 1 Frequenz-Multiplier. |
| Genesis/Arcade | `LFR` | Native LFO-Rate: Genesis `00–07`, Arcade `00–FF`. |
| Arcade | `LAD`, `LPD 00–7F` | Separate LFO-Amplituden- und Pitch-Tiefen. |
| Genesis/Arcade | `LAS`, `LEN` | Native Amplituden-Sensitivität `00–03` und LFO-An/Aus `00–01`. |
| Genesis/Arcade | `LPS 00–07` | Native Pitch-Sensitivität. |
| SID | `SAT`, `SDE`, `SSU`, `SRL 00–0F` | Native Attack, Decay, Sustain und Release. Zeitwerte steigen bis `0F`; Sustain bis Vollpegel. |
| SID | `SPR 01–10` | Silent Partner Frequenz von 1× bis 16×; beeinflusst Ringmodulation und Hard Sync. |
| Alle native FM | `FBK 00–07` | Absolute native Feedback, vom Instrument initialisiert. |
| Alle native FM mit aktiviertem Amp env; Sega/GB | `EAT`, `EDC`, `ESU`, `ERL`, `ESH 00–FF` | Attack, Decay, Sustain, Release, Form. Diese aktivieren keinen bypassed FM Amp. |
| Sega PSG | `CMD 00–02`, `CNR 00–03` | Ton / weißes Rauschen / periodisches Rauschen; drei feste Rauschraten oder ton-abgeleitete Rate. |
| GB Pulse | `CMD 00–03` | Native Puls-Duty. |
| GB Noise | `CMD 00–01`, `CND 00–07`, `CNS 00–0D` | Rauschbreite, Clock-Divisor und Shift. |
| GB Pulse | `CSP 00–07`, `CSS 00–07`, `CSD 00–01` | Sweep-Periode, Shift, Abwärtsrichtung. |
| GB Pulse / Noise | `CEI 00–0F`, `CEP 00–07`, `CED 00–01` | Native Hüllkurven Anfangspegel, Periode, Aufwärtsrichtung. |

Die Auswahl eines nativen FX benutzt das Instrument in der Phrase-Zeile `I`-Spalte oder das aktive Instrument, das beim Zurückschauen gefunden wird, falls `I` leer ist. Das FX-Popup zeigt eine kurze Beschreibung der Steuerung, ihren aktuellen Preset-Wert und den gültigen Befehlsbereich in einem kompakten Block. Die Auswahl eines anderen nativen Effekts startet bei dessen Wert; das erneute Öffnen desselben Effekts bewahrt den bearbeiteten Wert. Eine Mehrfachauswahl in Zeilen löst separat die Instrumente jeder Zeile auf. Tabellen verwenden ihren Instrument-Kontext. Native Wertänderungen stoppen an den erlaubten Endpunkten, inklusive Duty, Noise, Sweep, ADSR, Feedback und Operator-Leveln. Die Werte sind hexadezimal: Beispielsweise bedeutet DX7s maximaler `63` dezimal 99.

#### SID-Instrumente und Phrase FX

Wähle **CHIP → SID**. Bank bietet **ChooChoo SID Originals** und **SIDkit Effects**. Sie können wie andere native Chip-Presets durchsucht, angehört und gespeichert werden.

Die Instrumentenseite zeigt Wellenform/Pulsbreite, Filtermodus/Cutoff/Resonanz und native ADSR-Steuerungen. **EDIT**, neben dem Presetnamen, öffnet **SID EDIT**. Wave und PW haben getrennte Labels; Filter hat eine Moduszeile, dann Cutoff und Reso; ADSR-Labels sind neben den Werten. Antippen eines Feldes selektiert genau diese Steuerung. Vorhandene Parameterbereiche und feine/grobe Anpassungsverhalten bleiben erhalten.

SID EDIT nutzt die kompakten FM Editor Reihen, Theme-Farben, hexadezimale Werte und EDIT+Richtungs-Steuerung. OPT oder Links+Shift kehrt zurück; EDIT+PLAY hört das Instrument an. Die Seite editiert den vorhandenen gespeicherten Patch direkt, Änderungen werden mit Instrument und Projekt gespeichert.

SID EDIT gruppiert Wellenform, Ring/Sync, ADSR, Pulsbreite, Filter, Motion, Arpeggio, Pitch Sweep und Vibrato. Die Statusleiste zeigt jede Steuerung mit ihrem Bereich an. Ring und Sync verwenden jeweils zwei der vier verfügbaren SID-Stimmen, daher braucht ein SID-Akkord weniger Noten bei Aktivierung von Ring oder Sync.

| FX | Bereich | Aktion |
| --- | --- | --- |
| `SCP` | `00–FF` | Pulsbreitenbasis, skaliert zur nativen 12-Bit Breite; Rezept-Pulsbewegung bleibt aktiv. |
| `SCT` | `00–FF` | Cutoff-Basis, skaliert zum nativen 11-Bit Cutoff; Rezept-Filterbewegung bleibt aktiv. |
| `SRN` | `00–0F` | Native Resonanz. |
| `SWV` | `00–07` | Triangle, Saw, Tri+Saw, Pulse, Tri+Pulse, Saw+Pulse, Tri+Saw+Pulse, Noise. |
| `SFI` | `00–07` | Filtermodus-Bits: 1 Low-Pass, 2 Band-Pass, 4 High-Pass; 0 Bypass. |
| `SMR` | `00–FF` | Rezept-Makrouhr, skaliert von 1 bis 200 Hz. |
| `SRG` | `00–01` | Triangle Ring Modulation mit Silent Partner. |
| `SSY` | `00–01` | Oszillatorsync mit Silent Partner. |

Pulsbreite und Cutoff unterstützen `SLE`. Diskrete Wellenformen, Schalter, Resonanz und FM-Feedback behalten ihre sinnvollen nativen Schritte; größere Bytewerte würden keine weiteren Hardwarezustände erzeugen. `FBK` wählt direkt Feedback `00–07`. Operator-Level verwenden die oben genannten enginespezifischen Bereiche. Helligkeit und Master-Anpassungen nutzen den vollen Bytebereich.

#### SID- und native Chip Factory- und USER-Presets

Native Chip-Instrumente nutzen **Bank:** zur Auswahl von ALL, einer enthaltenen Sammlung oder USER. **Preset:** wählt einen Klang innerhalb der Auswahl. Halten von EDIT und Drücken von Links oder Rechts in einer Zeile wechselt den Preset; ein kurzes EDIT-Tippen öffnet den Auswahldialog.

Beigefügte Sammlungen sind ZIP-Pakete unter `instruments/FACTORY/`. USER-Dateien liegen unter `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse` oder `gb-noise`.
Durchsuche deine Ordner direkt oder lege ZIPs mit Ordnern und kompatiblen `.cni`-Presets dort ab. DX7 akzeptiert zudem unterstützte `.syx` Einzelstimmen und 32-Stimmen-Bänke, auch innerhalb von ZIPs. ZIPs bleiben auf der Festplatte intakt. Direktes Laden von WOPL/WOPLX wird nicht unterstützt; nutze kompatible CNI-Presets oder konvertiere unterstützte Formate zuerst offline.

Das USER-Preset-Cycling läuft über Dateien, Ordner, Banken und ZIP-Pakete in beide Richtungen, mit Umwicklung am Ende. Das erneute Öffnen des USER-Browsers folgt der aktuellen Auswahl. ALL enthält USER-Sounds unter Unsorted für kategorisierte Engines; USER bleibt eine eigene, oberste Wahl. Sega PSG und Game Boy nutzen flache ALL/Factory-Listen und einen hierarchischen USER-Browser ohne Unsorted-Panel. Ältere DX7-Dateien in `instruments/banks/dx7/` bleiben über USER zugänglich.

Eine Engine zeigt nur ihre kompatiblen enthaltenen Sammlungen. Factory Presets kombinieren die enthaltenen OPLL/VRC7-Tonsets der ausgewählten Engine; DX7 kombiniert ChooChoo und YSE Originals. Heruntergeladene benannte Sammlungen bleiben separat. Fat Man 2-op erscheint unter OPL2, Fat Man 4-op unter OPL3. Kompatible OPL2 CNI-Dateien können weiterhin in OPL3 USER-Ordnern verwendet werden.

## 6. Modulation und Bewegungsaufnahme

Jedes Instrument kann bis zu 4 Modulations-Slots haben. Audio-Instrumente können **Instrument Pan** oder **Track Pan** ansteuern; beide verwenden `00` für links, `80` für Mitte und `FF` für rechts.

Verfügbare Modulationstypen:

- ADSR – klassischer Attack-Decay-Sustain-Release-Hüllkurve
- AHD – Attack-Hold-Decay-Hüllkurve
- LFO – Niederfrequenzoszillator mit mehreren Formen
- syncLFO (früher SLFO) – LFO mit Ticks-pro-Einheit-Einstellung und Perioden-Multiplikator
- slowLFO – lange Zyklen mit gleichmäßig proportionalem Periodenregler, beginnend knapp oberhalb des Bereichs des normalen LFO
- FLFO – Audio-Rate LFO, der den kHz-Bereich erreichen kann; FLFO kann CPU-intensiv sein
- SLIN – lineare Joysticksteuerung
- SRAT – geschwindigkeitsbasierte Joysticksteuerung
- Sequence – liest die Tracker-Sequenz als Modulationssignal: **PITCH** ist die finale sequenzierte Tonhöhe, **VOL** die finale sequenzierte Lautstärke und **TRANCE GATE** ein unabhängiger rhythmischer Modulator. Motif, Rate und Length wählen das Gate-Muster; Trigger legt fest, ob es frei läuft, pro Note neu getriggert oder an Phrase- bzw. Chain-Grenzen neu gestartet wird. Geschlossene Gates geben null aus, offene Gates halben Pegel, und Accents verteilt Gates mit vollem Pegel euklidisch auf die aktiven Gates. Es nutzt oder verändert keinen Track-Insert.

Die Liste der Modulationsziele variiert je nach Instrumenttyp.

Wenn eine ADSR- oder AHD-Hüllkurve das Volume ansteuert, wird sie zur Lautstärkehüllkurve des Instruments. Ein LFO auf Volume verschiebt stattdessen die aktuelle Ausgabelaustärke.

Die Modulationsmenge kann positiv oder negativ sein. Ihr Bereich hängt vom Ziel ab. Wenn der volle Bereich des Ziels `127` oder weniger beträgt (wie bei den meisten Parametern), definiert die Menge einen absoluten Bereich. Breitere Ziele wie Pitch verwenden eine skalierte Menge.

Die Sequence-Quellen PITCH und VOL haben einen **Mode**. **Normal** lässt den Tracker-Wert hörbar und nutzt ihn zugleich als Modulation. **Bypass** macht ihn nur zur Quelle: PITCH triggert erneut auf der letzten Tonhöhe (bzw. C-4 bei der ersten Note), und VOL behält den letzten hörbaren Pegel. Trance Gate ist bereits reine Quelle. Setzt ein aktiver Sequence-Slot PITCH oder VOL auf Bypass, gewinnt Bypass. Um Rückkopplung zu vermeiden, können PITCH und VOL nicht Pitch oder Volume ansteuern.

Alle Dauerparameter, inklusive Attack, Hold, Decay und Periode, werden in Ticks gemessen.

**syncLFO** ist der frühere SLFO: Stelle Ticks und Multiplikator für tempo-synchrone Bewegung ein. Bestehende SLFO-Einstellungen und Songs behalten dasselbe Timing.

Verwende **slowLFO** für Sweeps, die länger sind, als der normale LFO erzeugen kann. Sein Period-Regler reicht von 256 bis 131.072 Ticks. Alle tick-basierten LFOs folgen dem Song-Tempo und teilen dieselben Formen, dieselbe Wavetable-Auswahl und dieselben Trigger-Modi. Wähle Free für einen Sweep, der über Noten hinweg weiterläuft; Retrig, Phrase und Chain starten ihn an der jeweiligen Grenze neu.

LFO-Formen:

- Tri – bipolare Dreieckswelle
- Sin – bipolare Sinuswelle
- UniTri – unipolare Dreieckswelle
- UniSin – unipolare Sinuswelle
- RampDn – linearer Abwärtssprung (Sägezahnwelle)
- RampUp – linearer Aufwärtssprung (Sägezahnwelle)
- ExpDn – exponentieller Abwärtssprung (exponentielle Sägezahnwelle)
- ExpUp – exponentieller Aufwärtssprung (exponentielle Sägezahnwelle)
- Square – Rechteckwelle
- Random – Sample-and-Hold Zufallswelle
- Wavetable – liest eine der 32-stufigen AY-Waves des Projekts. Werte `00` bis `0F` mappen von `-100%` bis `+100%` ohne Interpolation.

LFO-Auslöserarten:

- Free – startet den LFO nur neu, wenn das Instrument wechselt
- Retrig – startet den LFO bei jeder Note neu
- Phrase – startet LFO, syncLFO und slowLFO bei Wiedergabeneubeginn eines neuen Phrases
- Chain – startet LFO, syncLFO und slowLFO beim Wiedergabeneubeginn einer neuen Chain
- Hold – stoppt nach 1 Zyklus und hält den letzten Wert
- Once – stoppt nach 1 Zyklus und kehrt zu Null zurück

### Bewegungsaufnahme

- **STICK LIVE** (`L1` standardmäßig) wendet Stick-Modulation an, ohne die Phrase zu ändern. In **Settings > Stick live mode** aktiviert `HOLD` (Standard) die Modulation nur während Taste gehalten wird; `TOGGLE` schaltet bei einem Druck ein und beim nächsten Druck aus; `FREE` lässt sie permanent aktiv. Loslassen bei `TOGGLE` lässt sie aktiviert.
- Halte während der Wiedergabe **MOTION RECORD** (`L2`), um Stick-Modulation anzuwenden und veränderte Ziele als absolute FX-Werte aufzunehmen.
- Halte **MOTION ERASE** (`R2`), um passende Ziel-FX aus der aktuellen Zeile zu entfernen.

Das untere rechte `~` zeigt aktives Stick Live an. Record (`*`, oder `!` bei Overflow) und Erase (`x`) haben Priorität und sind in beiden Modi nur momentant aktiv. Beide aktivieren live Stick-Verarbeitung nur beim Halten; Loslassen lässt eine separat aktivierte Live-Umschaltung unverändert.

Der Modus wird mit den Anwendungseinstellungen gespeichert, die Live-Umschaltung startet aber immer aus. Moduswechsel löschen die Umschaltung: `HOLD` folgt einer gedrückten Stick-Live-Taste, `TOGGLE` wartet auf neuen Tastendruck, `FREE` ist sofort aktiv.

Bewegungsaufnahme schreibt Track-FX in die aktuell gespielte Phrase. Übereinstimmende FX werden zuerst aktualisiert, danach werden leere Slots von rechts nach links vergeben. Ein anderer FX-Typ wird nie überschrieben. Sind alle 3 Slots voll, wird die Bewegung auf diesem Step nicht aufgenommen. Ein `!` unten rechts zeigt an, dass mehr Ziele geändert wurden als in die 3 FX-Spalten passen.

Bewegungsaufnahme unterstützt Braids, Plaits, Sampler, 2xSCWF, BYOWTBL und Bogie-Ziele, inklusive aktiver Bogie-Makros und Filtersteuerungen, wo anwendbar.

## 7. Track Insert Effekte

Jeder der acht Tracks hat zwei serielle Slots, **TF1 → TF2**. Aus Instrument öffne mit Shift+Up das MOD und erneut Shift+Up für **F: Insert FX**. Shift+Down kehrt zu MOD zurück. OPT+Links/Rechts wählt den Track (1–8) ohne Instrumentauswahl zu ändern. Der Modul-Kopf öffnet den vorhandenen Auswahldialog; Bypass behält Modul, konfigurierte Werte und Automation bei. Parameter sind Bytewerte: linke Spalte 1–4, rechte Spalte 5–8. Kontextanzeige zeigt volle Namen und dekodierte Werte. Alte Projekte laden mit beiden Slots OFF.

Die Navigationsanzeige behält die volle F–M–I–P-Spalte während der Instrumentenbildschirme bei, inklusive Insert FX und Instrumentenpool, und markiert den aktuellen Bildschirm.| Modul | Parameter in Reihenfolge |
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

Kontinuierliche Regler verwenden 00–FF, außer FORMANT VOW (00–7F). Bipolare Regler haben ein exaktes Neutral bei 80. Distortion-Modi sind 00 Density, 01 Drive, 02 Spiral, 03 Mojo, 04 Dyno. Compressor-Ratios 00–07 sind 1.5, 2, 3, 4, 6, 8, 16, 20:1. Compressor Detect 00–02 hört auf den eigenen Stereo-Eingang, links oder rechts; 03–0A hört stattdessen auf die Spuren 1–8. Der Detector filter 80 ist neutral, darunter Tiefpass, darüber Hochpass. Eine Spurquelle wird nach Lautstärke, Pan und Tilt EQ, aber vor den Inserts abgegriffen; eine stumme oder fehlende Quelle liefert Stille. Jeder Slot kann jedes Modul enthalten; wiederholte Module sind unabhängig. Chorus und Phaser starten mit hörbarer, aber kontrollierter Modulation. Verringere Mix, Depth oder Feedback für einen fast transparenten Effekt; hohe Einstellungen bei Depth, Feedback und Mix sind absichtlich extrem. Flanger und Rotary nutzen sanfte Modulations-Defaults. Rotary Speed verwendet den vollen **00-FF** Bereich: **00 = 0.01 Hz** (eine Umdrehung pro 100 Sekunden), **FF = 25 Hz** (das bisherige Chaos-Maximum). Exponentielle Verteilung ermöglicht feinere Kontrolle langsamer LFO-Bewegung. Neue Rotary-Einsätze starten standardmäßig bei **8F**, etwa 0.8 Hz. Dies ersetzt die vier diskreten Geschwindigkeiten; bestehende Rotary-Einstellungen und F11/F21 Speed-Automations müssen auf die neue Skala angepasst werden (alt Slow/Fast/Hyper/Chaos entsprechen ungefähr **8F/CB/E7/FF**). Andere Parameter bleiben unverändert. Bitcrusher-Bittiefe reicht von 4 bis 16 Bit, Rate Reduction von 1x bis 32x. Destruction-Modi sind Fold, Clip und Crush. Saturation und Distortion bieten Ausgangspegel- oder Mix-Regler zur Balance mit dem unverarbeiteten Signal.
Trance Gate ist ein tempo-synchronisierter Amplitudensequencer für Pads, Leads und Stabs. Patternwerte 00–3F wählen benannte musikalische Motive; 00–1F sind Core-Grooves, 20–3F experimentelle Variationen. Rate bietet gerade, Triolen- und punktierte Werte von 1/2 bis 1/32. Length kürzt das unabhängig rotierte 16-Schritte-Motiv auf 1–16 Schritte. Jeder Note-On im Track startet die Sequenz neu; Note-Off lässt sie während des Releases weiterlaufen. Forward, Reverse und Ping-Pong definieren die Abspielrichtung; Chaos nutzt eine wiederholbare dichte-erhaltende Mutation alle vier Loops. Im Master-Insert folgt es der Transportzeit ohne Noten-Retrigger. Gate setzt die Breite jedes offenen Schritts; Smooth rundet die Kanten und erhält einen klickfreien Minimal-Fade, Mix crossfadet zwischen trockenem und gated Audio.

**Beads** nutzt den originalen Mutable Instruments granularen DSP als Stereo-Insert. Wähle Beads in einem Track-Insert-Slot. Seine acht Regler verwenden bestehende F11–F18 / F21–F28 Befehle und Insert-MOD-Ziele; keine neuen FX-IDs oder Projektformate nötig. Grains laufen kontinuierlich, keine zusätzlichen Events erforderlich.

- **Time:** Position im 4-Sekunden Aufnahmepuffer.
- **Size:** lange Reverse-Grains bei 00, kürzeste Grains um 55, dann zunehmend längere Vorwärts-Grains. FF wechselt nach der Überblendung in den Original-Delay-Modus.
- **Density:** unterhalb der Mitte regelmäßige Grains, oberhalb zufällige Timing; die tote Zone in der Mitte stoppt neue Grains. Entfernt man sich vom Zentrum, steigt die Rate.
- **Pitch:** −24 bis +24 Halbtöne; 80 ist exakte Oktavgleichheit.
- **Scatter:** 00 bedeutet keine Parameterrandomisierung. Höhere Werte aktivieren die interne einheitliche Time-Randomness von Beads in voller Stärke und Size-Randomness in halber Stärke, unabhängig für jedes Grain. Pitch und Shape bleiben auf ihren gesetzten Werten.
- **Feedback:** speist verarbeitete Grains zurück in den Aufnahmeweg.
- **Shape/Freeze:** 00–7F nimmt auf, 80–FF friert ein. Innerhalb jeder Skala steuert die unteren 7 Bits denselben Hüllkurvenverlauf von Rechteck über Decay und Glocke bis Reverse. Durch Addieren/Subtrahieren von 80 wechselt man zwischen Freeze und Shape ohne den Hüllkurvenwert zu verändern: 40 und C0 haben dieselbe Hüllkurve. Der Wertehinweis zeigt Hüllkurvenposition und Aufnahme-/Freeze-Status. Halte EDIT mit Up/Down gedrückt für Sprünge um 10 hex (16); Links/Rechts ändert um 1. Diese komprimierte Steuerung ist diskret, Schleifen durch das Freeze-Bit sind so nicht möglich; Modulation des ganzen Bytes kann bewusst zwischen den Zuständen wechseln.
- **Mix:** 00 ist exakt trocken, FF vollständig verarbeitet. Änderungen werden geglättet.

Freeze hält das vom Effekt aktuell erfasste Audio; Modulwechsel oder Reset von Beads löschen es. Spurreverb-Sends bleiben verfügbar.

### Einen Kick den Mix oder eine andere Spur pumpen lassen

Für einen pumpenden Master lege den Kick auf Spur 1, wähle Compressor in MASTER FX, setze Detect auf 03 (Track 1), Ratio auf 04 (6:1), Attack auf etwa 60 (ca. 1,6 ms), Release auf etwa C0 (ca. 115 ms), Makeup auf 00 und Mix auf FF. Senke Threshold, bis die anderen Sounds bei jedem Kick eintauchen, und passe dann Release an, damit sie vor dem nächsten wieder oben sind. Der Direktsound des Kicks behält seine eigenen Track-Inserts, umgeht aber beide Master-Insert-Slots; seine Reverb- und Delay-Rückführungen pumpen trotzdem.

Um nur einen Bass, Pad oder Synth abzusenken, setze den Compressor auf dessen Spur und wähle mit Detect die Kick-Spur. Der Quell-Kick bleibt hörbar, und seine Lautstärke wird vom Compressor dieser Spur nicht verändert. Ein Kick und ein Bass auf derselben Spur lassen sich mit diesem Routing nicht trennen.

### FORMANT

Wähle **FORMANT** in TF1 oder TF2, um einen Klang zu vokalartigen Tönen zu formen. Es funktioniert am besten mit einer gehaltenen, obertonreichen Quelle wie einer Sägezahnwelle.

Setze **MODE** auf `00` VOW und nutze **VOW**, um von A (`00`) über E, I und O bis U (`7F`) zu fahren. Setze MODE auf `01` CUSTOM, um **F1** und **F2** selbst abzustimmen. **RES** verengt oder erweitert den Vokalcharakter; **MIX** mischt den Effekt mit dem Originalsignal. Nutze MOD → INSERT FX, um die aktiven Regler zu bewegen. MODE ist kein MOD-Ziel.

### Lane FX für Inserts

Insert-Befehle nutzen **F11–F18** für TF1 und **F21–F28** für TF2. Die FX-Auswahl im Roll und das Phrase/Table-Popup zeigen nur Parameter für die eingesetzten Inserts des ausgewählten Tracks. Die Gruppen heißen **TF1: Effektname** und **TF2: Effektname**; OFF-Slots sind ausgeblendet. Bypassete Inserts bleiben editierbar. Im Phrase- und Table-FX-Popup folgt die Beschreibung dem Insert-Slot des ausgewählten Tracks: Sie zeigt Modul, Parametername, decodierte Wertebereiche und gespeicherten Basiswert. Bestehende Songbefehle bleiben erhalten, falls Modulwechsel sie unbenutzt macht; der Picker startet dann mit einem verfügbaren Regler.
Beim Werteditieren zeigt der untere Hinweis den decodierten Wert und die Einheit. Jeder Phrase- oder Table-FX-Spalte kann ein jeder Slot zugeordnet werden; das Phrase-Raster behält seine drei Spalten. Diese absoluten Laufzeitwerte bleiben über Noten, Instrumente, Phrases, Chains und normale Loops erhalten. Frisches Abspielen oder harter Stop setzt sie zurück. Das Editieren eines Basiswertes auf der F-Seite löscht nur das Override dieses Parameters. MOD wirkt auf den effektiven Basis-/Automationswert ohne dessen Offsets zu speichern. Ein neuer Modulwähler setzt Defaults und löscht die Overrides dieses Slots. Bypass behält bestehende Automation, stoppt aber Verarbeitung nach kurzem Ausblenden.

**Adressen beziehen sich auf Track-Slots/Parameterpositionen, nicht benannte Effekte.** Ein auf einem anderen Track wiederverwendetes Phrase nutzt dessen Module. Modulwechsel können den Befehlsinhalt ändern; Befehle werden nicht umgeschrieben. OFF/nicht genutzte Adressen sind sichere No-Ops und bleiben in Phrase/Table-Daten gespeichert.

Der Instrument-MOD-Wähler ergänzt eine Insert-FX-Kategorie. Quellen jedes Instruments steuern Inserts auf allen Tracks, die es spielen, inklusive kombinierter Akkordsignale. Gleiches Instrument auf zwei Tracks betrifft unabhängige Ketten. Bestehende Quell-Timings, signierte Werte, Quellkombinationsregeln, Stick-Modi und Bewegungsaufzeichnung gelten. Diskrete Regler sind quantisiert und auf gültige Werte begrenzt. Es gibt keine neue track-eigene Modulationsbank.

Starte mit wenigen Inserts, achte auf Übersteuerung oder Knistern und bypass Effekte, die den Song nicht unterstützen. Drive- und Gain-Einstellungen können die Mischung übersteuern, nutze deshalb den Mixer-Clip-Indikator bei der Anpassung.

## 8. Tabellen

Tabellen sind ein zentrales Sound-Design-Werkzeug in Trackern. In Vortex Tracker kombinieren sie Instrumente und Ornamente, können aber noch viel mehr. Wer LSDj oder NerdSEQ kennt, fühlt sich vertraut.

Die Pitch-Spalte akzeptiert relative (`~`) oder absolute (`=`) Tonhöhenwerte in Halbtönen. Die Volume-Spalte ist `00`-`0F`, wirkt sich auf jeden Engine über seinem Hüllkurvenniveau aus und kann fürs Gate genutzt werden: wechselweise `0F` und `00` dann Loop mit `HOP 00`. Die 4 FX-Lanes entsprechen meist den Lanes in einer Phrase, aber einige FX verhalten sich in Tabellen anders.

Ein `TIC` FX in der letzten Tabellenzeile bestimmt die Spalten-Geschwindigkeit und überschreibt die Instrument-Standardgeschwindigkeit der Tabelle. Jede FX-Spalte kann eine eigene Tick-Geschwindigkeit haben. Pitch und Volume folgen der Geschwindigkeit der 1. FX-Spalte.

Eine 16-zeilige Tabelle wirkt im Vergleich zu Instrumenten in Vortex Tracker kurz, doch `HOP` FX erzeugen bedingte Loops wie „wiederhole diese Zeilen 5 Mal“ sowie verschachtelte Loops. Mit Loops und unabhängigen FX-Spaltengeschwindigkeiten reicht die Tabelle trotzdem sehr weit.

Ein Kopieren/Einfügen von Lane-FX zwischen Phrases und Tabellen ist möglich ;)

### Retrigger-Modus

Das `Retrig` Feld oben im Tabellenbildschirm bestimmt, wann eine aktive Tabelle neu gestartet wird. `Inst` ist der Standard- und kompatible Modus: Eine Phrase-Zeile mit Instrument startet die Standardtabelle dieses Instruments ab Zeile `00`. `Phrase` startet eine aktive Tabelle beim Eintritt der Phrase, `Chain` beim Eintritt der Chain, `Free` lässt sie laufen bis Track-Stop oder explizitem Befehl.

Eine strukturelle Methode schaut nie in die Vergangenheit. Wenn ein Track keine aktive Instrumententabelle hat, aktiviert die erste Phrase-Zeile mit Instrument exakt ab dieser Zeile, auch wenn es Zeile `4`, `8` oder `F` ist. Spätere Instrumentnummern ersetzen keine aktive `Phrase`, `Chain` oder `Free` Tabelle. So sind Tabellen als LFO-ähnliche Automationen nutzbar, ohne frühere Phrasenschritte still zu simulieren.

`TBL`, `TBX` und `RET` sind immer explizite Aktionen: sie wählen oder starten sofort, unabhängig vom Modus. An Phrase- oder Chain-Grenzen passiert ihr Neustart vor dem Lesen der neuen Phrase-Zeile, so hat ein `TBL` oder `TBX` in dieser Zeile das letzte Wort. Tabellenzeile `00` FX laufen normal erneut; kumulative FX häufen sich wie gewohnt.

Tabellen `00-7F` sind für Standard-Instrumenttabellen reserviert. Tabellen `80-FE` sind für Hilfstabellen, gestartet durch `TBX`. Der untere Bereich funktioniert auch für Hilfstabellen, kann aber zu Konflikten und Verwirrung führen.

### Steuerung

- **EDIT + [UP/DOWN]** auf einer FX-Namensspalte: FX-Auswahl öffnen
- **OPT + [Richtung]**: zwischen Tabellen wechseln

## 9. Instrument Pool und AY Wavetable

### Instrument Pool

Der Instrument Pool zeigt alle Instrumente im Projekt und erlaubt, sie neu anzuordnen. Während der Wiedergabe zeigt er auch, welche Instrumente aktuell aktiv sind.

#### Steuerung

- **EDIT:** Instrument bearbeiten und zum Instrumentbildschirm springen
- **SHIFT + OPT:** Instrument kopieren
- **SHIFT + EDIT:** Instrument einfügen
- **EDIT + [UP/DOWN]:** Instrumente neu anordnen
- **EDIT + PLAY:** Instrument vorhören

### AY Wavetable

Wavetable ist ein AY Plus Software-Oszillatortyp. Ein Projekt kann bis zu 256 Wavetables mit je 32 Schritten enthalten. Alle Wavetable-Instrumente nutzen dieselben Wellen. Hier bearbeitest du sie.

AY-Lautstärke ist nichtlinear, daher wird die Wellenform im Standard-View an die tatsächlichen Ausgangspegel angepasst. Doppeltippe **OPT** zum Umschalten zwischen Standard- und dauerhaften **LFO-View**, benutzt denselben Shortcut wie Song/Live Mode: Der LFO-View zeigt rohe `0`–`15` Steps linear mit Mittellinie zwischen `7` und `8` als Modulations-Nullpunkt.

Der Bildschirm hat zwei logische Reihen. Oben sind die **Load** und **Save** Buttons, darunter der Wavetable-Editor.

#### Steuerung

- **OPT + [LEFT/RIGHT]:** Wavetable-Liste durchblättern
- **OPT + [UP/DOWN]:** in 16er-Schritten springen
- **SHIFT + OPT:** Wavetable kopieren
- **SHIFT + EDIT:** Wavetable einfügen
- **OPT + PLAY:** Zwischen Standard- und LFO-Ansicht wechseln

## 10. Tracker FX

Track FX erzwingen Sequenzer- oder Sound-Engine-Werte auf einzelnen Schritten. Andere Grooveboxes nennen das „Parameter Locks“. Jede Phrase-Zeile hat 3 FX-Slots.### Sequencer FX

| FX | Wert | Detailliertes Verhalten |
|---|---|---|
| `ARP` | `XY` | Arpeggiert die Grundnote, `+X` Schritte und `+Y` Schritte. `37` erzeugt ein Moll-Akkordmuster. |
| `ARC` | `XY` | `X` wählt die Arpeggi-Richtung oder den Bereichsmodus; `Y` ist die Geschwindigkeit in Ticks. |
| `PVB` | `XY` | Pitch-Vibrato: `X` ist Geschwindigkeit und `Y` die Tiefe. Im Linear-Modus verwendet die Tiefe `10-Cent` Schritte. |
| `PBN` | mit Vorzeichen `XX` | Fügt jede Phrase/Table-Zeile `XX` Pitch-Einheiten hinzu; `FF` bedeutet `-1`. `00` stoppt es. |
| `PSL` | `XX` Ticks | Slidet über `XX` Ticks von der vorherigen Tonhöhe zur neuen Note. |
| `PIT` | mit Vorzeichen `XX` | Akkumulierte relative Abweichung in Pitch-Table-Schritten. |
| `FIN` | mit Vorzeichen `XX` | Akkumulierte Feineinstellung in Cents bei Linearpitch, sonst in Periodeneinheiten. |
| `PRD` | mit Vorzeichen `XX` | Akkumulierte relative Oszillator-Periodenabweichung. |
| `VOL` | mit Vorzeichen `XX` | Akkumulierte relative Lautstärkeänderung (`FF` ist -1, nicht volle Lautstärke). |
| `VSL` | mit Vorzeichen `XX` | Fügt jeder Phrase/Table-Zeile `XX` zur Lautstärke hinzu. `00` stoppt es. |
| `PAN` | `00-FF` | Absolutes Instrumenten-Panorama: links, Mitte (`80`), rechts. |
| `TPN` | `00-FF` | Absolutes Mixer-Track-Panorama: links, Mitte (`80`), rechts. |
| `RET` | `XY` | Retriggert alle `Y` Ticks; `X` ändert die Lautstärke. `Y=0` stoppt Retrigger. |
| `DEL` | `XX` Ticks | Verzögert das Note-On. Eine Verzögerung, die länger als der aktuelle Groove-Schritt ist, überspringt die Note. |
| `OFF` | `XX` Ticks | Sendet Note-Off nach `XX` Ticks und startet die ADSR-Release-Phase. |
| `KIL` | `XX` Ticks | Tötet die Stimme hart nach `XX` Ticks ohne ADSR-Release. |
| `TIC` | `XX` Ticks | Setzt Table-Ticks pro Zeile. In einer Table ändert es die Geschwindigkeit dieser FX-Spalte. |
| `TBL` | `00-FE`, `FF` aus | Ersetzt und startet die Instrumententabelle sofort neu; `FF` stoppt sie. |
| `TBX` | `00-FE`, `FF` aus | Startet oder ersetzt eine Hilfstabelle sofort; `FF` stoppt sie. |
| `THO` | Zeile `XX` | Springt alle Instrumententabellen-Spalten zu Zeile `XX`. |
| `TXH` | Zeile `XX` | Springt alle Hilfstabellen-Spalten zu Zeile `XX`; wird nicht aus Tabellen heraus verwendet. |
| `GRV` | Groove `XX` | Wählt einen Groove für die aktuelle Spur. |
| `GGR` | Groove `XX` | Wählt einen Groove für alle Spuren. |
| `HOP` | `XY` | Springt zu Zeile `Y`, `X`-mal; `X=0` schleift unendlich. Table-HOP beeinflusst die eigene Spalte. |
| `SNG` | mit Vorzeichen `XX` | Bewegt die Wiedergabe um `XX` Song-Zeilen. |
| `PRO` | `00-64` | Legt eine absolute Trigger-Wahrscheinlichkeit von `0%` bis `100%` fest. |
| `MOD` | `AB` | Triggert im Durchgang `A` eines `B`-Durchgangszyklus. |
| `RNN` | `MX` | Spielt eine zufällige Note. `M` wählt den Modus: `0` immer zufällig, `1` zufällig pro PLAY, `2` wählen und behalten, bis sich der Wert ändert. `X` wählt den Bereich: `0` beliebige Note, `1` +3 st, `2` +6 st, `3` +12 st, `4` -3 st, `5` -6 st, `6` -12 st, `7` +/-3 st, `8` +/-6 st, `9` +/-12 st. Zufallswahlen folgen der Projekt-Skala, wenn die Skalen-Engine für die Spur gilt. |
| `SPD` | mit Vorzeichen `XX` | Wählt ein persistent pro-Track-Taktverhältnis; siehe Tabelle unten. |
| `SLE` | `00-FF` Ticks | Setzt persistente Glide-Zeit pro Spur für kontinuierliche Engine-FX. `00` ist sofort. Einstellung wird auf `00` zurückgesetzt, wenn die Wiedergabe stoppt. |

### Audio FX Sends

| FX | Wert | Detailliertes Verhalten |
|---|---|---|
| `RSN` | `00-FF` | Setzt den Reverb-Send dieser Spur bis zum nächsten Note-Trigger. |
| `DSN` | `00-FF` | Setzt den Delay-Send dieser Spur bis zum nächsten Note-Trigger. |

### ADSR / Trigger FX

| FX | Wert | Detailliertes Verhalten |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | Überschreibt Attack, Decay, Sustain, Release oder Hüllkurvenform. |
| `TDC` / `TCL` | `00-FF` | Überschreibt Trigger-Modus Decay oder Color bei Plaits und Plaits-Alt. |

#### Bedingungen

Inspiriert von schwedischen „trig conditions“ unterstützt der Tracker Trigger-Wahrscheinlichkeiten und Modulo-Bedingungen.

| FX | Wert | Bedeutung |
|---|---|---|
| `PRO` | `00-64` | Absolute Trigger-Wahrscheinlichkeit von `0%` bis `100%` |
| `MOD` | `AB` | Trigger im Durchlauf `A` von `B`; z.B. `12`, `22`, `14`, `34` |

`MOD`-Zähler sind lokal für Track und Phrase. Ungültige Kombinationen, wie `A` größer als `B`, triggern nicht.

`MOD34` triggert im 3. Verlauf von jedem 4-Verlauf-Zyklus. `MOD1F` triggert im 1. Verlauf von jedem 16-Verlauf-Zyklus.

#### Wiedergabegeschwindigkeit

Wie NerdSEQ unterstützt ChooChooTracker eine unabhängige Wiedergabegeschwindigkeit für jede Spur.

`SPD` wird als vorzeichenbehaftetes Byte interpretiert. `00` ist Normalgeschwindigkeit, positive Werte beschleunigen die Spur, negative verlangsamen sie. Einstellung bleibt aktiv, bis ein anderer `SPD`-Befehl sie ändert.

| Wert | Geschwindigkeit |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

Ältere Projekte ohne vorzeichenbehaftete `SPD`-Unterstützung behalten die ursprüngliche `00-10` Tabelle, so dass sie wie gespeichert spielen.

Sehr hohe Multiplikatoren können die Auflösung des Tracker-Tick-Zeitplans überschreiten und wurden nicht hardwaregetestet.

### Scale & chords FX

#### Scale Automation

`SCL XY` ändert die globale Playback-Skala und den Root, ohne die in Phrasen gespeicherten Noten zu verändern. `X` wählt eine Skala (`0` Chromatisch, `1` Dur, `2` Moll bis `C` Custom); `Y` wählt den Root (`0` C bis `B` B). Befehl gilt nur für Phrase-FX, nicht für Tabellen. Wenn mehrere Spuren im gleichen Tick `SCL` senden, gewinnt die Spur mit der niedrigeren Nummer.

`SCL` gehört zum Quantizer-Modus. Mit aktiviertem **Note Lock** ist der Befehl im FX-Menü ausgeblendet und alle bereits in Phrasen geschriebenen `SCL`-Befehle werden ignoriert.

#### Akkorde

`CRD XY` wandelt die Note in der aktuellen Phrase-Zeile in einen Akkord um. `Y` wählt: `0` Dur, `1` Moll, `2` Dim, `3` Aug, `4` Sus2, `5` Sus4, `6` Power, `7` Maj7, `8` Min7, `9` Dom7, `A` Min7b5, `B` Dim7, `C` Add9, `D` MinAdd9, `E` Maj9 oder `F` Min9. `X` wählt eine von 16 Voicings: `0-3` sind eine aufsteigende geschlossene Umkehrung; `4-7` fügen Drop-2 hinzu; `8-B` fügen Drop-3 hinzu; und `C-F` kombinieren beide Drops. Der Zyklus setzt sich über eine zusätzliche Oktave für Dreiklänge und Powerchords fort, so dass jeder Wert eindeutig ist; höherwerte werden progressiv breiter und unkonventioneller. Zum Beispiel ist `CRD 4Y` Drop-2 Grundstellung und `CRD DY` erste Umkehrung mit beiden Drops. Am unteren oder oberen Ende der Pitch-Tabelle werden Stimmen auf deren Bereich begrenzt. Der Befehl ist nicht persistent: schreibe ihn auf jede akkordierte Zeile. Jede generierte Note wird beim aktiven Scale unabhängig quantisiert, sowohl im Quantizer- als auch im Note Lock-Modus. Nur für Software-Engines verfügbar; AY-Instrumente ignorieren es und bleiben monophon.

### Modulation FX

Phrase- und Table-FX können einen Modulations-Slot ändern, ohne das Instrument zu editieren:

| FX-Muster | Bedeutung |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Relativer Amount-Offset für Modulations-Slots 1–4 |
| `M11`, `M12`, `M13`, `M14` | Relative P1–P4-Offsets für Modulations-Slot 1 |
| `M21`, `M22`, `M23`, `M24` | Relative P1–P4-Offsets für Modulations-Slot 2 |
| `M31`, `M32`, `M33`, `M34` | Relative P1–P4-Offsets für Modulations-Slot 3 |
| `M41`, `M42`, `M43`, `M44` | Relative P1–P4-Offsets für Modulations-Slot 4 |

Der Wert wird als vorzeichenbehaftete `8-Bit`-relative Änderung interpretiert (`01` addiert `1`, `FF` subtrahiert `1`). Mehrfachbefehle summieren sich. Effektive Werte werden auf gültigen Bereich begrenzt.

`P5` ist als Phrase- oder Table-FX nicht verfügbar. Es ist als `M1–M4 Wavetable` Modulationsziel verfügbar, sodass ein Modulator den AYWavetable Index eines anderen scannen kann.

| Modulationstyp | P1 | P2 | P3 | P4 | P5 (bei Shape Wavetable) |
|---|---|---|---|---|---|
| ADSR | Attack | Decay | Sustain | Release | Unbenutzt |
| AHD | Attack | Hold | Decay | Unbenutzt | Unbenutzt |
| LFO | Shape | Trigger-Modus | Periode | Unbenutzt | AY Wavetable Index |
| syncLFO | Shape | Trigger-Modus | Ticks | Multiplikator | AY Wavetable Index |
| slowLFO | Shape | Trigger-Modus | Periodenschritt (00–FF) | - | AY Wavetable Index |
| FLFO | Shape | Trigger-Modus | Frequenz (`1 Hz` bis `20 kHz`) | Unbenutzt | AY Wavetable Index |

### Braids FX

| FX | Wert | Bedeutung |
|---|---|---|
| `BMD` | `00-2E` | Braids-Modell |
| `BTM` | `00-FF` | Absolut normalisiertes Timbre |
| `BCL` | `00-FF` | Absolut normalisierte Color |
| `BCF` | `00-FF` | Exponentieller Cutoff, `20 Hz` bis `20 kHz` |
| `BRS` | `00-FF` | Exponentielle Filter-Resonanz |

### aChChid FX

| FX | Wert | Bedeutung |
|---|---|---|
| `ASL` | `00-FF` | Slide zur Note ohne Retrigger. `00` sind `60 ms`; höhere Werte verlängern den Glide. |
| `ADC` | `00-FF` | Decay-Override: `200 ms` bis `2 s`. |
| `AAC` | `00-FF` | Akzentstärke: keine bis maximal. |
| `ATM` | `00-FF` | Braids Timbre-Override; nur aktiv im Braids Wave-Modus. |
| `ACL` | `00-FF` | Braids Color-Override; nur aktiv im Braids Wave-Modus. |
| `ACF` | `00-FF` | Exponentieller 303-Filter-Cutoff, `20 Hz` bis `20 kHz`. |
| `ARS` | `00-FF` | 303 Filter-Resonanz, keine bis maximal. |
| `AEM` | `00-FF` | 303 Filter-Hüllkurvenmodulation, keine bis maximal. |

### Bogie FX

| FX | Wert | Bedeutung |
|---|---|---|
| `DMD` | `00-0B` | Wählt das Bogie-Modell bis zum nächsten Trigger. |
| `DDC`, `DTO`, `DSW`, `DNO`, `DFM`, `DDR` | `00-FF` | Überschreibt Decay, Tone, Sweep, Noise, FM oder Drive bis zum nächsten Trigger. Alle sechs Makros sind bei jedem Modell verfügbar. |
| `DCF` | `00-FF` | Filter-Cutoff, logarithmisch gemappt. |
| `DRS` | `00-FF` | Filter-Resonanz. |

### Sintered FX

| FX | Wert | Bedeutung |
|---|---|---|
| `SMD` | `00-05` | Wählt das Sintered-Modell bis zum nächsten Trigger. |
| `SDC`, `SMP`, `SMA`, `SMB`, `SMO`, `SMC` | `00-FF` | Überschreibt Decay, Mod, die ersten zwei Modell-Makros, Motion oder den letzten Modell-Makro bis zum nächsten Trigger. |
| `SCF`, `SRS` | `00-FF` | Filter-Cutoff und Resonanz. |

### MIDI Out FX

| FX | Wert | Bedeutung |
|---|---|---|
| `MC1`-`MC4` | `00-FF` | Sendet einen MIDI CC, skaliert auf `0-127`. Welche CC-Nummer jeder Slot sendet, wird pro Instrument eingestellt (`MC1 number`-`MC4 number` im Instrumentbildschirm), nicht vom FX selbst – ein Slot ohne Nummer ist inaktiv. |

### Plaits FX

| FX | Wert | Bedeutung |
|---|---|---|
| `PMD` | `00-17` | Plaits-Engine |
| `PHA` | `00-FF` | Absolut normalisierte Harmonische |
| `PTM` | `00-FF` | Absolut normalisiertes Timbre |
| `PMO` | `00-FF` | Absolut normalisiertes Morph |
| `PAX` | `00-FF` | Haupt-/Hilfsmischung: `00` Haupt, `FF` Hilf |
| `PCF` | `00-FF` | Exponentieller Cutoff, `20 Hz` bis `20 kHz` |
| `PRS` | `00-FF` | Exponentielle Filter-Resonanz |

### Sample FX

| FX | Wert | Bedeutung |
|---|---|---|
| `SPL` | `00-03` | Wiedergabemodus: `00` Vorwärts, `01` Rückwärts, `02` Loop, `03` Ping-Pong |
| `SLI` | `00-FF` | Spielt den nummerierten Slice (`01` = erster Slice) unabhängig von der Note; `00` behält normale Notenzuordnung. Erfordert Slice-Modus am Instrument aktiviert |
| `SPT` | mit Vorzeichen `XX` | Sample-Transposition in Halbtonschritten |
| `SST` | `00-FF` | Normalisierter Startpunkt der Wiedergabe |
| `STA` | `00-FF` | Alias für `SST` (normalisierter Wiedergabestart) |
| `SEN` | `00-FF` | Normalisiertes Wiedergabeende |
| `SVL` | `00-FF` | Absolute Sample-Lautstärke |
| `SCF` | `00-FF` | Exponentieller Cutoff, `20 Hz` bis `20 kHz` |
| `SRS` | `00-FF` | Exponentielle Filter-Resonanz |
| `SSP` | `00-FF` | Sample-Geschwindigkeit, gemappt von `0%` bis `500%` |
| `RNS` | `M0` | Spielt einen zufälligen Slice eines geslicten Samples statt des per Note zugeordneten. `M` wählt den Modus: `0` immer zufällig, `1` zufällig pro PLAY, `2` wählen und behalten, bis sich der Wert ändert. Erfordert aktivierten Slice-Modus am Instrument; das untere Nibble ist unbenutzt. |

### AY FX shared by AY instruments

| FX | Wert | Detailliertes Verhalten |
|---|---|---|
| `AYM` | `XY` | `X` ist Hüllkurvenform; `Y` wählt off/tone/noise/tone+noise (`0/1/2/3`). |
| `NOI` | mit Vorzeichen `XX` | Akkumulierte relative Noise-Periodenabweichung. |
| `NOA` | `00-1F`, `FF` | Absolute Noise-Periode; `FF` gibt Noise-Perioden-Priorität an frühere Spuren. |
| `ERT` | beliebig | Retriggert die aktuelle Hardware-Hüllkurvenform. |
| `EAU` | `XY` | Automatisches Hüllkurvenverhältnis `X:Y`; `X=0` deaktiviert. |

### AY Classic-only FX

| FX | Wert | Detailliertes Verhalten |
|---|---|---|
| `EVB` | `XY` | Hüllkurvenperioden-Vibrato: `X` Geschwindigkeit, `Y` Tiefe. |
| `EBN` | mit Vorzeichen `XX` | Fügt `XX` der Hüllkurvenperiode jeder Phrase/Table-Zeile hinzu. |
| `ESL` | `XX` Ticks | Slidet von vorheriger Hüllkurvenperiode zum neuen Wert. |
| `ENT` | Note `XX` | Setzt Hüllkurvenperiode aus der in der UI angezeigten Pitch-Table-Note. |
| `EPT` | mit Vorzeichen `XX` | Akkumulierte relative Hüllkurvenperiodenabweichung. |
| `EPL` | Byte `XX` | Setzt das Low-Byte der Hüllkurvenperiode. |
| `EPH` | Byte `XX` | Setzt das High-Byte der Hüllkurvenperiode. |

### AY Plus FX| FX | Wert | Detailliertes Verhalten |
|---|---|---|
| `TNN` | Note `XX` | Setzt den Tone-Oszillator auf eine Tonleiter-Note, ignoriert die Zeilennote. |
| `TNP` / `TNF` | vorzeichenbehaftet `XX` | Angehäufter Tonhöhen-Schritt / Feineinstellung. |
| `TRT` | beliebig | Neustart der Phase des Tone-Oszillators, wenn unterstützt. |
| `ENN` | Note `XX` | Setzt den Envelope-Oszillator auf eine Tonleiter-Note. |
| `ENP` / `ENF` | vorzeichenbehaftet `XX` | Angehäufter Envelope-Höhen-Schritt / Feineinstellung. |
| `SFT` | `00-07` | Software-Typ: None, Pulse, Sync Tone, Sync Env, Wavetable, Tone FM, Env FM, Sample. |
| `SFN` | Note `XX` | Setzt den Software-Oszillator auf eine Tonleiter-Note. |
| `SFP` / `SFF` | vorzeichenbehaftet `XX` | Angehäufter Software-Oszillator Höhen-Schritt / Feineinstellung. |
| `SRT` | beliebig | Setzt die Phase des Software-Oszillators zurück. |
| `SFM` | vorzeichenbehaftet `XX` | Angehäufter FM-Tiefen-Offset. |
| `PWM` | vorzeichenbehaftet `XX` | Angehäufter Pulsweiten-Offset. |
| `SPL` | vorzeichenbehaftet `XX` | Angehäufter Puls-Tiefpegel-Offset. |
| `SWT` | vorzeichenbehaftet `XX` | Angehäufter Wavetable-Index Offset. |

### AY Sample FX

AY Sample akzeptiert die gemeinsamen AY-Befehle plus `TNN`, `TNP`, `TNF`, `TRT`, `SFN`, `SFP` und `SFF` wie oben beschrieben.

| FX | Wert | Detailliertes Verhalten |
|---|---|---|
| `SMS` | `00-FF` | Setzt den Startpunkt der Legacy AY Sample-Wiedergabe auf `XX x 64` Quell-Samples. |

Engine-Parameter-FX gelten für den passenden Instrumenttyp und werden beim nächsten Notentrigger zurückgesetzt, sofern nicht anders angegeben. `SPD` bleibt absichtlich erhalten.

Der FX-Selektor filtert diese Referenz auf gemeinsame Befehle plus jene, die vom aktiven Instrument unterstützt werden. Das Kontext-Hilfefenster zeigt Wertdetails beim Editieren.

## 11. Mixer, Reverb und Delay

Der Mixer ist der ganz linke Hauptbildschirm. Er zeigt CPU-Auslastung und Clipping-Warnungen. Ein rotes `!` in der **CLIP**-Spalte markiert die Spur, deren trockenes Signal den Mix über den sicheren Bereich hinaus getrieben hat.

Benutze ihn, um die 8 Spuren auszubalancieren, jede Spur mit Tilt EQ zu formen und Audio zum Reverb oder Delay zu schicken:

| Feld | Bedeutung |
|---|---|
| LVL | Post-Engine Spurpegel, `000-100` |
| REV | Send an den gemeinsamen Reverb, `000-100` |
| DLY | Send an das gemeinsame Ping-Pong Delay, `000-100` |
| PAN | Verschiebt das Audio nach links oder rechts |
| TLT | Spurbezogener Tilt EQ, `00-FF`; `80` ist neutral, niedrige Werte bevorzugen Bass, hohe Werte Höhen. Niedrige Werte erzeugen sanfte Übersteuerung. |
| M | `*` stummschaltet diese Spur |
| S | `*` solo schaltet diese Spur |

Der Mixer ist spurbasiert, nicht instrumentenbasiert. Wenn ein Spur-Instrument wechselt, bleiben Level, PAN, Tilt und Sends an der Spur gebunden. Instrumente haben außerdem jeweils ihren eigenen `00-FF` **Vol**- und **Pan**-Wert in der oberen Reihe des Instrument-Bildschirms. Instrument-PAN wird zuerst angewendet, danach Spur-PAN. Beide erhalten ein bestehendes Stereo-Bild in der Mitte; sie dämpfen die Gegenseite progressiv beim Verschieben nach links oder rechts.

### Auto Mix

Der Mixer enthält einen **AUTO MIX**-Button. Er rendert `6 Sekunden` offline, gleicht die Spurlautstärken an, vergleicht dann 8 Oktavbänder mit einem Pink-Noise-Profil und senkt Spuren, die diese Bänder überladen, sanft ab. Er schlägt vorsichtige Pegeländerungen vor, lässt etwas Spitzen-Headroom und startet eine temporäre Vorschau. **Apply** übernimmt die vorgeschlagenen Pegel, **Cancel** stellt die vorherigen 8 Pegel wieder her. Das ist ein brauchbarer Ausgangspunkt, du wirst das Ergebnis aber vermutlich noch nachjustieren wollen.

### Reverb

Benutze **SELECT + [UP]** im Mixer, um die Reverb-Einstellungen zu öffnen.

- **Algorithmus**: Wählt Clouds oder Beads. Jeder Algorithmus besitzt eigene Steuerungen.
- **Return**: nasser Reverb-Anteil im Master-Mix.
- **Clouds**: **Time**, **Damping**, sowie Pre-Reverb **LP** und **HP** Filter.
- **Beads**: **Time**, nativer Feedback-Filter **Tone**, und ein Pre-Reverb **HP** Filter.

Dies ist nur der Send-Reverb-Teil von Clouds oder Beads, nicht der Granularprozessor.

### Ping-pong Delay

Benutze **SELECT + [DOWN]** im Mixer, um das Delay zu öffnen, dann **SELECT + [UP]** zum Zurückkehren. Reverb funktioniert entgegengesetzt: **SELECT + [UP]** öffnet ihn, **SELECT + [DOWN]** kehrt zum Mixer zurück.

- **Return**: nasser Delay-Pegel.
- **To Reverb**: Anteil des nassen Delay-Signals, der in den gemeinsamen Reverb geschickt wird, `0-100%`.
- **Ticks**: Delay-Zeit in Tracker-Ticks (`9 ticks` = `1 Beat` standardmäßig).
- **Feedback**: Cross-Feedback-Stärke, begrenzt auf `95%`.
- **Filter**: Tiefpassfilter auf dem verzögerten Signalweg.

Die 1. Wiederholung folgt dem Stereo-Eingang. Spätere Feedbacks kreuzen links und rechts.

## 12. Projekt-Bildschirm

Der Projekt-Bildschirm bietet die Befehle **Load**, **Save**, **New**, **Export**, **Manage** und **Scale** sowie Dateiname, Titel und Autor-Metadaten.

### Export-Ordner

Exports, Stems und Bounces werden in einem pro Projekt angelegten Ordner im vorgegebenen Samples-Verzeichnis der App abgelegt:

- **Standard**: `<app folder>/samples/Exports/<project name>/`. Solange das Projekt unbenannt ist, heißt der Ordner `current-project`.
- Im Web-Build ist der Standard `/user/samples/Exports/<project name>/`.
- Unter Android ist der Samples-Ordner im App-Arbeitsverzeichnis: privat in der Play-Store-Version, `samples` in dem Ordner, den du beim direkten APK-Build ausgewählt hast.

Die **Folder**-Zeile auf dem EXPORT-Bildschirm zeigt das aktive Ziel an:

- Tippe **EDIT**, um mit dem Ordnerbrowser einen benutzerdefinierten Ordner auszuwählen. Der gewählte Ordner wird exakt so verwendet (kein zusätzlicher Projekt-Unterordner) und in den Einstellungen gespeichert.
- **EDIT + OPT** (Löschen) setzt auf den Standardordner zurück.

Wenn ein benanntes Projekt unter neuem Namen gespeichert wird, wird der Standard-Exportordner entsprechend umbenannt, vorher exportierte Dateien bleiben erhalten. Eine Umbenennung betrifft keinen benutzerdefinierten Ordner und wird von Autosave nicht ausgeführt. Existiert bereits ein Ordner mit dem Zielnamen, bleiben beide erhalten. Falls der Tracker den Ordner (zum Beispiel nach einem Neustart) „verloren“ hat, wird beim Speichern eines benannten Projekts stattdessen der existierende `current-project`-Ordner umbenannt. Instrumente funktionieren nach der Umbenennung weiter: geladene Sample-Pfade, die auf den umbenannten Ordner zeigen, werden automatisch aktualisiert.

**Load** akzeptiert `.mid`/`.midi` Dateien und importiert sie als neues Projekt: Noten werden nach MIDI-Kanal gruppiert (je ein Kanal pro Spur, bis zur Anzahl der Spuren), auf 4 Reihen pro Beat quantisiert und einem einzelnen Standard-AY-Instrument zugewiesen – MIDI-Programmnnummern haben kein Chiptune-Äquivalent, wähle danach echte Instrumente. Es wird nur das erste Tempo der Datei verwendet (ein globaler Tick-Rate, keine Abschnittstempo-Änderungen). Die Export-Zeile **MIDI** macht das Gegenteil: schreibt die aktuellen Noten, Lautstärke und Tempo/Groove als Standard MIDI-Datei (ein MIDI-Track pro Tracker-Spur); [MIDI Out](#midi-out) Instrumente und weitere per Reihe FX außer Lautstärke und globalem Groove haben keine MIDI-Entsprechung und werden nicht übersetzt.

**Load** akzeptiert Dirtywave M8 Songs (`.m8s`, Firmware 2.x-4.x) und importiert sie als neues Projekt. Struktur und Noten werden übernommen (Song-Rows, Chains mit deren Transpose, Phrases mit Note, Velocity und Instrumentnummer, Note-Offs, Tempo und Songname); Noten behalten ihre echte Tonhöhe (die M8 benennt Oktaven zwei höher: sein `C-6` ist hier `C-4`). Jedes von einer Phrase genutzte Instrument wird unter dem M8-Instrumentnamen importiert. M8 **MACROSYN** Instrumente werden Braids-Instrumente (die 47 Shapes sind die Braids-Modelle mit gleichen Nummern; Timbre, Color, Filtertyp, Cutoff, Resonanz und Pan werden übernommen, die Cutoff-Kurve ist ungefähr). **SAMPLER** Instrumente werden Sample Instrumente: die `.m8s` speichert nur den Pfad der WAV auf der M8-Karte, die Datei wird im Ordner der `.m8s` und dessen Eltern gesucht (eine Kopie der Kartenstruktur funktioniert) sowie im aktuell benutzten Sample-Ordner (`Samples` oder `samples`); wird sie nicht gefunden, bleibt der M8-Pfad erhalten und das Instrument verstummt. Importierte Samples erhalten +24 Halbtöne in deren Pitch-Einstellung, weil die M8 ein Sample zwei Oktaven tiefer abspielt als diese App. **FMSYNTH** wird zu Genesis FM Instrument (grobe Annäherung), **WAVSYNTH** und **HYPERSYNTH** werden aChChid. **MIDI OUT** wird MIDI Out Instrument (Kanal, Bank, Programm und erste vier CC Nummern). Andere Typen (extern) werden zu Standard-AY-Instrumenten – wähle anschließend echte Klänge. FX und Tables werden ignoriert.

**Load** akzeptiert Little Sound Dj Songs (`.lsdsng`, je eine Datei pro Song) und importiert sie als neues Projekt. Wird ein ganzer LSDj-Save (`.sav`, bis zu 32 Songs) geladen, öffnet sich eine Liste der Songs (Slot, Name, Version, Größe); wähle einen zum Import (der beim Speichern aktiv bearbeitete Song wird nicht gelistet). Struktur und Noten werden übernommen (Song-Rows, Chains mit deren Transpose, Phrases mit Note und Instrumentnummer, Tempo und Song-Name); alle Noten erhalten volle Lautstärke, da LSDj keine Notenlautstärke kennt, und die vier LSDj-Kanäle PU1, PU2, WAV und NOI werden zu Track 1–4. Pulse-Instrumente werden GB Pulse (Duty, Envelope und Panning übernommen; Pulse Sweep nicht), Noise-Instrumente GB Noise (Envelope, Stabilität und Panning), Wave-Instrumente GB Pulse Hollow Lead Preset, Kits ein Standard GB Noise. Grooves werden importiert; Phrase-Befehle `C` (Chord), `D` (Delay), `E` (Pulse- und Noise-Envelope, nicht auf Wave-Kanal), `G` (Groove), `K` (Kill) und `V` (Vibrato) werden zu `ARP`, `DEL`, `CEI`/`CEP`/`CED`, `GGR`, `KIL` und `PVB`. Andere Befehle (`A`, `F`, `H`, `L`, `M`, `O`, `P`, `R`, `S`, `T`, `W`, `Z`), Tables, Wave-Frames, Kit-Samples und Synth-Einstellungen werden ignoriert, daher klingt ein Song, der diese nutzt, anders. Das Tempo wird auf ein 6-Tick-Groove umgerechnet.

Auf Desktop und Android macht die Export-Zeile **LSDJ** das Gegenteil, begrenzt: Tippe sie an und wähle eine vorhandene `.lsdsng` als **Template** (bis zum Beenden gespeichert, **EDIT + OPT** vergisst es). Deren Song-Rows, Chains, Phrases, Tempo und Titel ersetzen die aktuellen, alles andere (Tables, Grooves, Waves, Kits, Synth-Einstellungen) bleibt. Tracks 1-4 werden PU1, PU2, WAV und NOI; GB Pulse und GB Noise Instrumente werden zurückgeschrieben als LSDj Pulse- und Noise-Instrumente, andere Instrumenttypen als Standard-Pulse-Instrument. FX, Notenlautstärken und Note-Offs werden nicht exportiert, und Songs mit Chain `80` oder höher bzw. Phrase `FF` oder höher können nicht exportiert werden (LSDj kennt 128 Chains und 255 Phrases). Die exportierte Datei wurde noch nicht in LSDj selbst getestet.

Auf Desktop macht die Export-Zeile **M8S** likewise das Gegenteil, begrenzt: Tippe sie an und wähle eine vorhandene `.m8s` als **Template** (bis zum Beenden gespeichert, **EDIT + OPT** vergisst es). Song, Chains, Phrases, Tempo und Titel werden ersetzt, alles andere bleibt, außer dass Braids-Instrumente als MacroSynth und Sample-Instrumente als Sampler zurückgeschrieben werden (Pfad `/Samples/<Dateiname>`: die WAV muss auf der M8 selbst dort kopiert werden), vorausgesetzt, das Template hat ein Instrument dieses Typs, um Parameter zu kopieren. FX werden nicht exportiert, und Songs mit Phrase `FF` oder höher können nicht exportiert werden (M8 kennt 255 Phrases). Die exportierte Datei wurde noch nicht auf einer echten M8 getestet. Details siehe [M8 songs](m8s-format.md).

Auf Desktop kannst du mit Key jazz Dateiname, Titel und Autor direkt über die Tastatur eingeben, anstatt die virtuelle Bildschirmtastatur zu nutzen — siehe [Key jazz](#key-jazz-desktop-only).

### Scale / Quantize

Der Scale-Bildschirm steuert die globale 12-TET Skala des Engines. Die **Mode**-Zeile wählt zwischen zwei Modi:- **Quantizer** (Standard): Die Eingabe von Phrasen bleibt chromatisch; wenn aktiviert, werden Noten unmittelbar vor dem Erreichen der Sound-Engine nach unten gerundet. So harmonisiert das Ändern von Tonart oder Skala ein Lied neu, ohne Phrasen editieren zu müssen.
- **Note Lock**: Beim Eintippen werden Noten in Phrasen sofort auf die nächste Note der Skala eingerastet, es können also nur Skalen-Noten geschrieben werden. Neue Eingaben und Verringerungen rasten nach unten; `EDIT + RIGHT` / `EDIT + UP` erhöhen die Note zur nächsthöheren Skalen-Note. Die Eingabe folgt immer der Projektskala auf diesem Bildschirm: Eine Änderung von Tonart oder Skala wirkt sich sofort nur auf neu eingegebene Noten der aktiven Spuren aus, bereits gespeicherte Noten bleiben unverändert. Der `SCL` FX ist in diesem Modus nicht verfügbar und vorhandene `SCL`-Noten werden ignoriert. Die Playback-Quantisierung einfacher Noten wird umgangen (Eingabe ist schon gesperrt); Akkord-Quantisierung per `CRD` bleibt aktiv.

- **Clock source** wählt zwischen **Internal** (das eigene Tempo des Projekts), **Midi Clock** (der Sequencer folgt einer eingehenden MIDI-Clock) und **Ableton Link** (der Sequencer folgt dem Tempo einer Link-Session im lokalen Netzwerk). Bei **Midi Clock** ist die Zeile **Tempo** ausgegraut und zeigt das an der eingehenden Clock gemessene Tempo; der Transport (Play/Stop) wird von den Start/Continue/Stop-Nachrichten des Senders gesteuert, der PLAY-Befehl der App wird also nicht benötigt. Bei **Ableton Link** wird die Zeile zur **Link**-Zeile: Sie enthält statt eines Tempofelds den Verbinden/Trennen-Schalter der Session (siehe unten). Die Clock-Quelle wird in settings.txt gespeichert und gilt für alle Projekte.
- **Tempo** setzt das Projekttempo in ganzen BPM (`3-500`). Intern speichert der Tracker seine Tickrate weiterhin in Hz (`Tickrate = BPM x 0,4`), bestehende Projekte laden und speichern also unverändert; ein Projekt mit gebrochenem Tempo zeigt es gerundet an und rastet bei der ersten Tempo-Änderung auf ganze BPM ein. Grobe Einstellung springt um 5 BPM, feine um 1 BPM; Löschen setzt auf den Standard von 125 BPM zurück.

ChooChooTracker speichert Projekte als `.cct`. Dies ist ein proprietäres Format, das auch für Menschen leicht lesbar ist.

Verwenden Sie **Save**, bevor Sie Instrumententypen ändern oder ein anderes Projekt laden.

### Ableton Link

Mit Ableton Link kann ChooChooTracker dem Tempo anderer Link-fähiger Apps (Ableton Live, LinkHut usw.) im selben lokalen Netzwerk (WLAN oder Ethernet) folgen. Es ist nur ein Empfänger: Der Tracker passt sein eigenes Tempo an die Session an und sendet nie Tempo oder Transport an andere Teilnehmer.

**Was synchronisiert wird:**

- Session-Tempo - der Tracker folgt dem Tempo der Link-Session fortlaufend (innerhalb des Bereichs von 3-500 BPM).
- Beat-Phase - Drücken von PLAY richtet die Wiedergabe am Beat-Raster der Session aus: Der Song startet beim nächsten Takt (Quantum von 4 Beats).

**Was nicht synchronisiert wird:**

Während der Link-Wiedergabe prüft ChooChooTracker die gemeinsame Beat-Phase einmal pro 4/4-Takt und passt bei Bedarf sanft seine eigene Tickrate an. Das gleicht kleine Abweichungen der Gerätetakte aus, ohne Tracker-Ticks zu überspringen oder zu wiederholen, und ändert nie die Link-Session eines anderen Teilnehmers.

- Grooves, `SPD`- und `DELAY`-Werte sind nicht betroffen - sie bleiben relativ zum verfolgten Tempo.
- Cursor-Positionen in Song/Chain/Phrase und Mixer-Einstellungen werden nie geteilt.

**Verwendung:**

1. Öffne den Projekt-Bildschirm und setze **Clock source** auf **Ableton Link**. Die Zeile darunter wird zur **Link**-Zeile.
2. Drücke EDIT auf **[Connect]**, um der Session beizutreten. Die Statusmeldung rechts zeigt den Fortschritt: `Wait...` beim Verbinden, `No net` ohne Netzwerk, `No sess` wenn keine Session gefunden wird, `Err` bei einem internen Fehler. Solange verbunden, zeigt der Button **[Disconnect]** und der Status wechselt etwa einmal pro Sekunde zwischen `connected` und `Peers: N` (N = Anzahl der Peers).
3. Warte auf `connected` und drücke dann PLAY - die Wiedergabe startet am nächsten Takt der Session ausgerichtet.
4. Drücke EDIT auf **[Disconnect]** (oder setze die Clock-Quelle zurück auf Internal), um die Session zu verlassen.

Im Link-Modus besitzt die Session das Tempo: Die Zeile zeigt kein BPM-Feld, und das Session-Tempo wird automatisch auf das Projekt angewendet (innerhalb des Bereichs von 3-500 BPM).

**Plattformverfügbarkeit:** PortMaster (Hauptziel), Windows, Linux, macOS und Android werden unterstützt. Die Web- und Miyoo-Builds enthalten kein Link.

**PortMaster-Hinweis:** Auf PortMaster-Geräten ist der Audiopuffer größer (512 Frames bei 48 kHz), die Beat-Ausrichtung ist daher nur auf etwa 10-15 ms genau und nicht sample-genau. Die Tempo-Synchronisation selbst ist davon nicht betroffen.

## 13. Einstellungen

Einstellungen gelten für Präferenzen, die mit der App bleiben, nicht mit dem aktuellen Projekt. Projektspezifische Steuerungen wie Tempo, Groove und Mixer-Level sind auf eigenen Bildschirmen untergebracht.

### Steuerung

- **Repeat delay** und **Repeat speed** legen fest, wie schnell und wie oft ein gehaltener Navigations- oder Edit-Knopf wiederholt wird. Erhöhen Sie die Verzögerung, wenn gehaltene Eingaben zu schnell laufen; verringern Sie sie für schnelles Tracker-Spiel.
- **Stick live mode** bestimmt das Verhalten des zugeordneten **STICK LIVE**-Buttons: `HOLD` ist aktiv während gehalten, `TOGGLE` bleibt nach Drücken aktiv, `FREE` ist stets aktiv. Siehe [Motion recording](#motion-recording).
- **Key mapping** weist Tastatur- oder Controller-Steuerungen zu. Wählen Sie einen Befehl, drücken Sie die neue Steuerung, dann **Done**, um die Belegung zu speichern.

### Audio

**Sample rate** bietet `32 kHz` oder `48 kHz` (Standard). Änderungen wirken sofort und werden gespeichert. Das Ändern der Samplerate stoppt Wiedergabe und Sample-Vorschau, während die Sound-Engines und jeder aktivierte Audio-Eingang mit der neuen Rate neu geöffnet werden. Lehnt ein Gerät die neue Rate ab, bleibt die vorherige Einstellung aktiv. `32 kHz` verringert den Rechenaufwand und begrenzt die höchste wiedergegebene Frequenz auf 16 kHz. Die CPU-Anzeige zeigt die Kosten des Audio-Renderings.

**Buffergröße** tauscht Reaktionsgeschwindigkeit gegen Stabilität. Für knackfreies Live-Spiel verwenden Sie kleine Werte; bei Knacken oder dauerhaft hohem CPU-Wert erhöhen Sie sie. Verfügbare Werte: `32`, `64`, `128`, `256`, `512` und `1024` Samples. Die CPU-Anzeige hilft bei der Auswahl einer stabilen Einstellung.

### MIDI

Wählen Sie Eingangs- und Ausgangsgeräte aus, ordnen Sie eingehende MIDI-Kanäle Instrumenten zu und konfigurieren Sie MIDI-CC-Zuordnungen. Siehe [MIDI](#15-midi) zum Anschluss eines Keyboards, Mixersteuerung oder Versand von Noten an externe Hardware.

### Synths

- **AY Quality** wählt die Qualitätsstufe für AY/YM-Instrumente. **AY Sample dithering** kann AY-Beispiele glätten; schalten Sie es aus für einen direkteren, lo-fi Klang oder beim Ziel Original-Hardware.
- **Braids BITS**, **DRFT** und **SIGN** sind globale Charaktereinstellungen für alle Braids-Instrumente. Hier legen Sie Auflösung, Drift und Signatur fest, bevor Sie einzelne Sounds verfeinern. Siehe [Braids](#braids).

### Mixer

- **Mix volume** ist die Master-Ausgangslautstärke der Anwendung. Stellen Sie sie niedriger, wenn ein anderer Mixer oder Recorder gespeist wird; sie ändert nicht den Mix der Songspuren.
- **Tilt pivot** legt die Mittenfrequenz für alle Mixer-**Tilt**-Regler im Projekt fest, von `250 Hz` bis `4000 Hz` (Standard `1 kHz`).

### Grafik

- **Edit color theme** ändert die Interface-Farben. **Load font** lädt eine `.cnfont` Anzeige-Schrift.
- **Song waveform** zeigt oder verbirgt die Mix-Wellenform auf der Song-Seite; siehe [Song waveform](#song-waveform).
- **Track visuals** legt je Spur fest, ob **Detailed** (Instrument-Wellenform und Overlays) oder **Audio waveform** (Spur-Ausgang) angezeigt wird. Mit **All detailed** oder **All audio** ändern Sie alle Spuren zusammen.
- **Waveform FPS** bestimmt, wie oft Instrument-Scope und Mixer-Visualisierer neu zeichnen, von `1` bis `60` Bildern pro Sekunde. Verringern Sie den Wert für ruhigere Anzeige oder mehr Leistungsspielraum.
- **Renderer** zeigt an, ob die Anwendung mit `GPU`- oder `Software`-Rendering läuft.

### Support-Report und Beenden

**Support report** erzeugt eine kleine Diagnose-Datei für eine Fehler-Meldung. Hängen Sie diese zusammen mit der `.cct`-Datei an, wenn ein Problem von einem bestimmten Song abhängt. **Quit ChooChooTracker** beendet die Anwendung sauber.

### Song waveform

Grafik → Song waveform ist standardmäßig AUS. Tippen Sie EDIT, um es umzuschalten, oder nutzen Sie EDIT + Richtung; die Wahl wird als App-Präferenz gespeichert. Ist es aktiv, zeigt eine zweizeilige Wellenform über der SONG-Seite die finale Mono-Summe des Mixes. Alle anderen Seiten behalten ihr volles Layout, normale Abstände und Instrumentenvorschau unabhängig dieser Option. Die gespeicherte Präferenz ist abwärtskompatibel. SONG-Touch-Ziele folgen den angezeigten Zeilen in beiden Modi.

## 14. Fehlerbehebung

### CPU

Die CPU-Last hängt von aktiven Engines und Effekten ab. Plaits Physical Models und Clouds Reverb sind rechenintensiver als einfache AY-Stimmen. Prüfen Sie die Performance auf der Zielkonsole, besonders mit 8 Plaits-Stimmen und beiden SENDs aktiv. Eine CPU-Anzeige nahe `100%` kann zu Knacken oder verpassten Audio-Deadlines führen.

Bei Pops, Abstürzen oder Verzögerungen verwenden Sie **Settings > Support report** und fügen die erzeugte `support-report.txt` der Fehler-Meldung bei. Bei song-spezifischen Problemen auch die `.cct` anhängen.

### Kein Sound

Prüfen Sie Instrumentnummer, Stummschaltung oder Solo-Zustand der Spur, Spur-Lautstärke, Anwendung Mix volume und Instrument-Hüllkurve. Bei Samples prüfen Sie, ob die Original-WAV am gespeicherten Pfad noch existiert.

Auf ArkOS schalten Sie mit **MENU + L3** das Betriebssystem-Mute um. Hilft das nicht, starten Sie die Hardware neu und verbinden Sie Ihr Audio-Interface erneut. Versuchen Sie die Option „repair audio“ in ArkOS, sie kann helfen.

### Eingabe fühlt sich falsch an

Passen Sie Repeat delay und Repeat speed in den Einstellungen an. Wenn ein einzelner Tastendruck zweimal reagiert, vergewissern Sie sich, dass nur ein physischer Steuerknopf auf diese Richtung gelegt ist, und melden Sie den genauen Bildschirm und Shortcut.

## 15. MIDI

Settings > **MIDI** öffnet dieses Untermenü: **MIDI In** / **MIDI Out** wählen ein verbundenes Gerät, es wird durch alle erkannten Ports mit `OFF` an den Enden durchgeschaltet; für beide Richtungen muss ein Gerät ausgewählt sein.

- **Sound preview von MIDI-Keyboard**: Ist ein MIDI-In-Gerät gewählt, ertönen auf dem verbundenen Keyboard gespielte Noten auf jedem Bildschirm, genau wie der On-Screen-Shortcut **EDIT + PLAY** auf dem [Instrument](#5-instruments)-Screen. Nur Vorschau – es werden keine Noten in den Song eingegeben.
- **Channel mapping**: öffnet eine Liste der 16 MIDI-Kanäle; jedem kann ein Instrument zugewiesen werden (`OFF` standardmäßig), sodass eingehende Noten auf diesem Kanal dieses Instrument vorhören – egal welches gerade am Instrument-Bildschirm aktiv ist, z.B. Kanal `01` -> Instrument `05`. Ein auf `OFF` belassener Kanal nutzt das gerade ausgewählte Instrument (Standard-Verhalten). Diese Zuweisung wird in settings.txt gespeichert.
- **MIDI CC mapping**: weist einen eingehenden Controller einem Instrumentparameter zu, inklusive **Volume**, oder globalen Steuerungen. Globale Reihen zeigen `TRK 1`-`TRK 8` statt Instrumenten und bieten Spur-**Mute**, **Solo**, **Volume**, **Reverb send** und **Delay send**; **Song play/stop** hat kein Ziel. Neue Belegungen ignorieren den gelernten/configurierten CC und wirken erst bei der nächsten physischen Bewegung – während der Wiedergabe Belegungen setzen also keine Parameter auf null zurück. Mute und Solo verwenden `0-63` = aus, `64-127` = ein.
- **MIDI-Clock-Sync**: Setze bei gewähltem MIDI-In-Gerät **Clock source** im Projekt-Bildschirm auf **Midi Clock**, um einem externen Sequencer zu folgen. Der Tracker misst das Tempo an der eingehenden Clock (24 Impulse pro Viertelnote) und zeigt es in der ausgegrauten **Tempo**-Zeile; **Start**-, **Continue**- und **Stop**-Nachrichten steuern die Wiedergabe. Die Clock-Quelle wird in settings.txt gespeichert.
- **Externe MIDI-Geräte ansteuern**: Vergeben Sie einer Spur den Instrumententyp [MIDI Out](#midi-out) und stellen Sie den Kanal ein; das Abspielen auf dieser Spur sendet echte MIDI-Daten an das gewählte MIDI-Out-Gerät statt Sound in ChooChooTracker zu erzeugen.

## 16. Credits und Lizenzierung

ChooChooTracker ist ein Fork von ChipNomad und behält dessen MIT-Lizenzierung bei. Braids, Plaits, Plaits-Alt, Clouds DSP, Warps-abgeleiteter MME DSP und stmlib-Code stammen aus den Open-Source-Releases von Mutable Instruments unter deren jeweiligen MIT-Hinweisen. Plaits-Alt basiert auf dem lylepmills/eurorack Plaits Lab Fork; die erhaltenen Quellhinweise gelten weiterhin. Die aChChid-Engine nutzt Open303 von Robin Schmidt, Copyright 2009, unter MIT-Lizenz. MonkSynth nutzt JonETs native DSP unter MIT-Lizenz; siehe `LICENSE.monksynth.txt`. Bogie und Sintered sind originale native Implementierungen. MIDI I/O verwendet RtMidi von Gary P. Scavone unter MIT-ähnlicher Lizenz. Details entnehmen Sie den mitgelieferten Lizenzdateien. Ableton Link wird mit einer Sonderlizenz von Ableton bereitgestellt, ist nicht MIT-lizenziert und unsere Lizenz erstreckt sich nicht auf Ihre Forks oder andere Projekte außer ChooChooTracker.