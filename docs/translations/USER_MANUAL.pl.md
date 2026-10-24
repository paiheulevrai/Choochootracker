# ChooChooTracker Podręcznik Użytkownika
> To jest automatycznie wygenerowane tłumaczenie pliku `docs/USER_MANUAL.md` z obecnego angielskiego podręcznika. Jeśli różni się od angielskiego oryginału, obowiązuje tekst angielski.
>
> **Konwencje.** Interfejs aplikacji jest w języku angielskim. Zachowaj nazwy ekranów, menu, pól i przycisków, kody FX, nazwy modułów i silników oraz rozszerzenia plików w języku angielskim.


ChooChooTracker to 8-ścieżkowy tracker muzyczny dla konsol przenośnych.

Bazuje na klasycznym systemie pracy i nawigacji LSDj, oferując kilka opcji kreacji dźwięku:

- Emulacja vintage chipów i konsol
- Soundfonty
- Oscylatory Mutable Instruments Braids, Plaits i Plaits-Alt z filtrem multimode
- Sample PCM, single-cycle waveforms i wavetable z filtrem multimode
- aChChid — wszechstronny acid synth bazujący na Open303
- Bogie — natywny syntezator 12-modelowych bębnów z filtrem multimode
- MonkSynth - syntezator wokalny z dziesięciogłosowym unisono i stereofonicznym delayem
- MME — dwuoscylatorowy syntezator multi-modulacyjny z filtrem multimode
- Sintered — natywny eksperymentalny syntezator perkusyjny 6-modelowy


## 1. Instalacja i pliki

### ArkOS / PortMaster

Zainstaluj `choochootracker.zip` przez PortMaster lub skopiuj rozpakowany pakiet do katalogu `ports` na konsoli. Uruchom **ChooChooTracker** z menu Ports.

### Miyoo Mini / Mini Plus (Onion OS)

Rozpakuj archiwum MiyooPorts w katalogu głównym karty SD tak, aby foldery `Roms/PORTS/` złączyły się z istniejącymi w Onion. Uruchom **ChooChooTracker** z menu Ports po odświeżeniu kolekcji. Przed nadpisaniem istniejącej instalacji zrób kopię zapasową folderu `projects/` aplikacji.

Czas wczytywania projektu może się różnić w zależności od typów instrumentów użytych w utworze. Jeśli uruchamianie lub wczytywanie projektu wydaje się nienormalnie wolne, podaj, ile trwa każde z nich, wraz z modelem konsoli i wersją Onion OS.

### Raspberry Pi OS 64-bit

Rozpakuj archiwum Raspberry Pi AArch64, zainstaluj środowisko SDL2 (`sudo apt install libsdl2-2.0-0`) i uruchom `./chipnomad.sh` z rozpakowanego katalogu. Trzymaj dołączone instrumenty i pozostałe zasoby obok pliku wykonywalnego.

### Windows

Uruchom `choochootracker.exe` wraz z `SDL2.dll` i `libwinpthread-1.dll` w tym samym katalogu.

Obsługa wejścia w Windows to klawiatura (szczegóły w sekcji Jazz mode) lub gamepad.

### Web

Możesz korzystać z Choochootracker online pod adresem https://choochootracker.vercel.app/ .

Sterowanie umożliwia klawiatura, kontrolki ekranowe na urządzeniach mobilnych oraz gamepad.

Zapisane ustawienia, sample, motywy, projekty itd. znajdują się w pamięci przeglądarki.

Użyj **Download .cct** do eksportu projektów.

Uwaga: ze względu na ograniczenia iOS używanie midi w wersji web na iPhonie może być niemożliwe.

### Konfiguracja katalogów

| Folder | Zawartość |
|---|---|
| `AY_Wavetables` | `.aywave` wavetables układu AY |
| `fonts` | pliki czcionek `.cnfont` ChipNomad |
| `instruments` | presety instrumentów `.cni`, banki FM i Soundfonty |
| `pitch-tables` | alternatywne strojenia `.csv` |
| `projects` | pliki piosenek i projektów `.cct` |
| `samples` | pliki `.wav` dla silnika Sampler |
| `SR_wavetables` | wavetable w formacie Serum dla silnika BYOWTBL |
| `themes` | motywy ChipNomad `.cth` |
| `title` | zasoby ekranu tytułowego |
| `waveforms` | single-cycle waveforms `.wav` dla silnika 2xSCWF |

## 2. Sterowanie

ChooChooTracker najlepiej działa z gamepadem wyposażonym w D-pad, 2 analogowe drążki i 7 przycisków.

| Sterowanie logiczne | Domyślne handheld | Domyślne Windows |
|---|---|---|
| **[LEFT/RIGHT/UP/DOWN]** | D-pad | Strzałki |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Spacja |
| **SELECT** | Select | Shift |
| Osie modulacji | lewe i prawe drążki | drążki game-controllera |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

Mapowania można zmienić w **Settings > Controls > Key mapping**, także na Androidzie z zewnętrznym gamepadem. Na PortMaster zapisz mapowanie przyciskiem **Done**, a potem zwolnij przycisk, aby kontynuować w Settings.

### Nawigacja

- Używaj **[UP/DOWN/LEFT/RIGHT]** do przesuwania kursora.
- Przytrzymaj **SELECT + [KIERUNEK]** aby przechodzić między ekranami.
- Przytrzymaj **OPT + [KIERUNEK]** dla nawigacji specyficznej dla ekranu.
- Na wartości użyj **EDIT + [LEFT/RIGHT]** dla drobnych zmian lub **EDIT + [UP/DOWN]** dla większych skoków.
- Kliknij **EDIT**, aby wprowadzić wartość lub wykonać polecenie.

### Zaznaczanie i schowek

- **SELECT + OPT** wchodzi w tryb zaznaczania i cyklicznie przechodzi przez przydatne zakresy.
- Użyj **[KIERUNEK]**, by rozciągnąć zakres.
- Stuknij i puść **OPT**, aby skopiować zaznaczenie i wyjść z trybu zaznaczania.
- **OPT + EDIT** wycina zaznaczenie.
- Poza trybem zaznaczania, przytrzymaj **SELECT** i naciśnij **EDIT**, aby wkleić.
- **EDIT + [KIERUNEK]** edytuje wszystkie wybrane komórki jednocześnie.

### Odtwarzanie

- **PLAY** startuje od kursora. Naciśnij ponownie, aby zatrzymać.
- Możesz edytować nuty, instrumenty i łańcuchy podczas gry; zmiany słychać przy następnym takcie sekwencera.
- **SELECT + PLAY** startuje wszystkie ścieżki poza ekranem Song.
- **EDIT + PLAY** odtwarza podgląd instrumentu z Instrument Pool.

### Global Bounce

Wybierz zakres wierszy i kolumn Song, Chain lub Phrase, następnie dwukrotnie stuknij **EDIT**, aby otworzyć ekran BOUNCE TO SAMPLE. Bounce obejmuje wszystkie wybrane ścieżki: ścieżki, których pierwsza łańcuch zaczyna się później w zaznaczeniu, cicho czekają i dołączają przy pierwszym łańcuchu, więc plik zawiera łańcuchy wszystkich ścieżek.

Ekran umożliwia:

- **File name**: edycja z klawiaturą znakową. Po otwarciu podpowiadany jest kolejny wolny numer sekwencji (`001`, `002`, ...): pierwsza liczba, dla której plik `.wav` nie istnieje jeszcze w folderze eksportu. Nazwę można zmienić dowolnie — jest używana bez modyfikacji. Jeśli plik o tej nazwie istnieje, dodawany jest sufiks `_001`, `_002`, ... aby nie nadpisać istniejących. Licznik resetuje się po załadowaniu lub utworzeniu projektu.
- **Sample rate** i **Bit depth**: identyczne opcje jak na ekranie eksportu
- **Include in a sample name**: trzy pola wyboru, które dołączają do nazwy pliku tagi kontekstowe po naciśnięciu **Start** (domyślnie wyłączone):
  - **[BPM]** — aktualne tempo projektu, np. `[120]`
  - **[Key]** — tonika i skala projektu, np. `[Cmaj]`, `[F#min Pent]`
  - **[Bars:Beats:16ths]** — długość bounce: jedna fraza = jeden takt (16 szesnastek), beat to 4 szesnastki. Pełne frazy pokazują tylko takty, np. `[2]`; częściowe frazy pokazują `[0:2]` (dwa beaty) lub `[0:1:3]` (jeden beat plus trzy szesnastki). Przy multi-track bounce decyduje najdłuższa ścieżka. Zaznaczenie kończące się w połowie beatu traci jedną szesnastkę: ostatni zaznaczony wiersz to punkt cięcia, więc jego note-off wypada na koniec poprzedniej szesnastki.
  - Tagi są dodawane w kolejności `[BPM][Key][Length]` przed nazwą pliku.
- **Start**: renderuje zaznaczenie do pliku WAV w folderze eksportu (patrz *Export location* w ekranie Project)
- **Cancel**: powrót bez renderowania

W trakcie renderowania bounce **OPT** anuluje proces. Po zakończeniu lub anulowaniu ekran z którego powrócono zostaje przywrócony.

## 3. Struktura piosenki i mapa ekranów

Przytrzymaj **SELECT + [KIERUNEK]**, aby przechodzić między ekranami.

Główna nawigacja to:

<pre>
        I
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

Środkowy rząd:

- **M**: Mixer (gałęzie do Reverb, Delay)
- **S**: Song (gałęzie do Project, Settings)
- **C**: Chain
- **P**: Phrase (gałęzie do Groove)
- **I**: Instrument (gałęzie do Modulation i Instrument Pool)
- **T**: Table (gałąź do edytora AY Wavetable)

Górny rząd:

- **R**: Reverb
- **P**: Project
- **G**: Groove
- **M**: Modulation
- **I**: Insert FX

Dolny rząd:

- **D**: Delay
- **S**: Settings
- **P**: Instrument Pool
- **W**: AY Wavetable editor

Piosenka ChooChooTracker ma hierarchię:

```text
Song -> Chain -> Phrase row -> Note + Instrument + FX
```

Piosenka zawiera 8 ścieżek. Każda ścieżka składa się z ciągu łańcuchów, a każdy łańcuch z sekwencji fraz. Frazy to 16-krokowe sekwencje nut. nuta zawiera wysokość dźwięku, instrument, velocity i do 3 FX ścieżki. Instrument jest tym, co generuje dźwięk.



## 4. Song, Chain i Phrase

### Song

Ekran Song zawiera 8 kolumn, po jednej na każdą ścieżkę audio. Każda komórka odnosi się do łańcucha. Piosenka może mieć maksymalnie 256 wierszy (`00-FF`).

Ścieżki układają się poziomo, a łańcuchy pionowo. Każda ścieżka jest niezależna, a łańcuchy mogą mieć od 1 do 16 fraz, więc ścieżki mogą się rozjeżdżać w czasie. Zaprojektuj łańcuchy tak, aby pozostały zsynchronizowane lub pozwól im dryfować, jeśli tak wymaga utwór.

Powszechną praktyką w społeczności trackerów jest rezerwowanie 1 łańcucha na puste frazy, zazwyczaj `00` lub `FE`.

Możesz mieć w projekcie wiele pod-utworów („wyspy”) oddzielone pustymi wierszami.

#### Tryb na żywo

Dwukrotnie stuknij **OPT**, aby wejść lub wyjść z Live Mode; `LIVE` zastępuje tytuł Song. W Live Mode, **PLAY** uruchamia wybrany łańcuch na jego ścieżce, gdy ścieżka jest zatrzymana. Na grającej ścieżce, kolejkowany jest wybrany łańcuch na następny punkt zmiany łańcucha; naciśnij **PLAY** dwukrotnie, aby wymusić szybkie przejście na następnej frazie. Wybrana pusta komórka Song kolejkuje zatrzymanie ze zgraniem (quantized stop). `+` oznacza kolejkowany łańcuch, `!` łańcuch pilny, `-` kolejkowane zatrzymanie.

Wybierz jeden wiersz piosenki przez kilka kolumn, aby kolejkować lub zatrzymać ich ścieżki razem. Zaznaczenia wielowierszowe są celowo niedozwolone. Kolejki live są czyszczone, gdy aktywna jest pętla zakresu Song. Live to tryb performance, niezapisywany w projekcie.

#### Sterowanie

- **OPT + [UP/DOWN]**: skok o 16 pozycji w górę lub w dół
- Wybierz zakres, potem **SHIFT + EDIT**: płytkie kopiowanie łańcuchów z zachowaniem fraz
- Wybierz zakres, potem dwukrotnie **SHIFT + EDIT**: głębokie kopiowanie łańcuchów i fraz
- Wybierz zakres, potem **EDIT + [UP/DOWN]**: przesunięcie zaznaczenia w górę lub w dół
- Trzykrotnie stuknij **OPT**: dodaj lub usunąć podświetlenie dla czytelności
- **OPT + SHIFT**: wycisz wybraną ścieżkę lub kolumny (najpierw zwolnij **OPT**, aby utrzymać wyciszenie)
- **OPT + PLAY**: solo wybranej ścieżki lub kolumn (zwolnij **OPT** przedłużając solo)
- **OPT + [LEFT/RIGHT]**: solo wszystkich ścieżek po lewej lub prawej od bieżącej
- Wybierz zakres, potem dwukrotnie stuknij **EDIT**: otwiera ekran BOUNCE TO SAMPLE (patrz Bounce powyżej)

Zatrzymanie odtwarzania oraz zmiana mute lub solo używają krótkiej korekty wyjścia 3 ms, aby ograniczyć trzaski. Dotyczy to także wyłączania mute i zdejmowania solo podczas odtwarzania.

### Chain

Łańcuch to uporządkowana lista fraz 16-krokowych z opcjonalną transpozycją. Może mieć do 16 fraz, które mogą się powtarzać. Druga kolumna ustawia transpozycję w półtonach.

Obok wykorzystywanego łańcucha pojawia się gwiazdka (`*`). Możesz klonować łańcuchy z ekranu Song.

#### Sterowanie

- **OPT + [LEFT/RIGHT]**: przejście między ścieżkami
- **OPT + [UP/DOWN]**: przejście między łańcuchami na bieżącej ścieżce
- Wybierz zakres, potem **SHIFT + EDIT**: kopiowanie fraz
- Wybierz zakres, potem dwukrotnie stuknij **EDIT**: otwiera ekran BOUNCE TO SAMPLE dla zaznaczonych wierszy łańcucha bieżącej ścieżki (patrz Bounce w Song)

### Phrase

Fraza to wzorzec ścieżki w tradycyjnym step-sequencerze.

Fraza ma 16 kroków, po jednym wierszu na krok. Każdy wiersz zawiera nutę, instrument, głośność od `00` do `7F` oraz kolumny FX. Nuty są w notacji trackerowej jak `C-4` (nazwa nuty i oktawa).

Aby zakończyć nutę, wstaw `NOTE OFF`, użyj effectu kill-note lub zagraj inną nutą bez instr. ustawionego.

Gdy łańcuch zawiera kilka fraz, ekran Phrase działa jak ciągły edytor patternów. Przejście poniżej wiersza `F` lub powyżej `0` otwiera następną lub poprzednią frazę.

Gwiazdka (`*`) pojawia się przy frazie, która jest powtórzona w projekcie. Możesz klonować frazy z ekranu Chain.

Selektor FX pokazuje często używane komendy plus te obsługiwane przez instrument na tym wierszu, unikając długiej globalnej listy specyficznych FX. ChooChooTracker obsługuje więcej FX niż ChipNomad.

### Sterowanie- **OPT + EDIT** na pustej nucie: wstaw `NOTE OFF`
- **EDIT + [GÓRA/DÓŁ]** na kolumnie nazwy FX: otwórz ekran wyboru FX
- **OPT + [LEWO/PRAWO]**: przejdź między ścieżkami
- **OPT + [GÓRA/DÓŁ]**: przejdź między frazami w bieżącym chainie
- Zaznacz zakres w kolumnie instrumentu, potem użyj **SHIFT + EDIT**: klonuj instrumenty
- Zaznacz zakres, potem użyj **EDIT + [GÓRA/DÓŁ]**: rotuj wiersze frazy
- Zaznacz zakres, potem dwukrotnie naciśnij **EDIT**: otwórz ekran BOUNCE TO SAMPLE dla zaznaczonych wierszy frazy na bieżącej ścieżce (zobacz Bounce w Song)

Na tym ekranie, z aktywnym zaznaczeniem, kolumna nut może być wypełniona
wzorcami rytmicznymi:

- **OPT + LEWO** wypełnia zaznaczone komórki nut wzorcem 16-krokowym o stałej sekwencji
  i przechodzi do następnego wzorca przy każdym naciśnięciu. Po ostatnim wzorcu nuty są kasowane i cykl się powtarza. Wzorce są wyrównane do absolutnych wierszy frazy, więc częściowe zaznaczenie otrzymuje tylko kroki mieszczące się w nim.
- **OPT + PRAWO** wypełnia zaznaczone komórki nut wzorcem losowym z 3-8 nut (7 i 8 rzadziej),
  powtarzając się bez końca podczas trzymania **OPT**.
- **OPT + GÓRA** mutuje nuty już wpisane w zaznaczeniu: każde naciśnięcie wybiera 1-3 z nich i przesuwa każdą o losową wartość w obrębie oktawy
  (mniejsze przesunięcia są bardziej prawdopodobne). Ósme naciśnięcie przywraca nuty zaznaczenia ze startu cyklu, po czym cykl się powtarza.
- **OPT + GÓRA** na zaznaczeniu obejmującym tylko kolumnę głośności (bez nut,
  instrumentów lub FX) losowo zmienia velocity zamiast mutować: pierwsze naciśnięcie stosuje wzorzec Minimalny (zmiany do +-20), drugie Średni (do +-50), trzecie Losowy (zupełnie losowe wartości), a czwarte przywraca pierwotne velocity zaznaczenia. Zwolnienie **OPT** resetuje cykl do Minimalnego.
  Wiersze bez głośności pozostają bez zmian.
- **OPT + DÓŁ** rozkłada wpisane nuty instrumentów sliced Samplera w zaznaczeniu na chromatyczną skalę: pierwsza nuta każdego instrumentu (najwyższa w sekwencerze) zostaje bez zmian, a każda kolejna nuta tego instrumentu jest podwyższana o półton względem poprzedniej, zamieniając stos identycznych nut w kolejne wyzwalacze slice’ów. Kilka sliced instrumentów rozkłada się niezależnie względem swoich pierwszych nut, nuty na instrumentach bez slice'owania pozostają bez zmiany, a wynik nie jest nigdy dopasowywany do skali projektu. Ostatnia nuta rozkładu staje się ostatnią używaną nutą przy wpisie, dlatego dalsze granie ręczne będzie kontynuować następnym slicem. Zaznaczenie zawierające tylko sliced instrumenty rozkłada się przy każdym naciśnięciu; gdy są też nuty nie-sliced, rozkład następuje przy pierwszym naciśnięciu, a losowy arp przy drugim i kolejnych.
- **OPT + DÓŁ** na zaznaczeniu bez nut sliced instrumentów to losowy arpeggiator dla już wpisanych nut. Akord jest wybierany z palety CRD — 3-nutowe akordy do zaznaczeń 1-2 nut, 3+ nutowe do 3, dowolne akordy dla 4 i więcej — z toniką na losowej nucie skali projektu w rejestrze zaznaczenia (dowolna chromatyczna przy wyłączonej skali lub wykluczeniu ścieżki), a tony są kwantyzowane do skali tak jak w odtwarzaniu CRD. Każde naciśnięcie zmienia rozstawienie akordu na nutach: losowo, od najniższej do najwyższej, od najwyższej do najniższej, a potem góra-dół; piąte naciśnięcie przywraca oryginalne nuty, a kolejne wybiera nowy akord. Nazwa akordu jest wyświetlana w pasku statusu, gdy arp jest aktywny, nie dodaje ani nie usuwa nut, a zwolnienie **OPT** resetuje cały proces.
- Każde naciśnięcie wypełnienia usuwa nuty zaznaczenia przed wpisaniem, a wstawione nuty zachowują ostatnio używany instrument; zwolnienie **OPT** resetuje cykle do pierwszego kroku.

### Key jazz (tylko na komputerze)

Naciśnij **Esc** na ekranie Phrase, by przełączyć key jazz — tryb wpisywania nut klawiszami QWERTY w stylu pianina (jak [m8c](https://github.com/laamaa/m8c)):

- `Z S X D C V G B H N J M` oraz `Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P` wpisują nuty chromatycznie przez ponad dwie oktawy, korzystając z fizycznego położenia klawiszy, więc układ jest taki sam na klawiaturach AZERTY, QWERTY i QWERTZ.
- Każde naciśnięcie wpisuje nutę w bieżącym wierszu i przechodzi do następnego wiersza.
- `[` / `]` przesuwają oktawę.
- **Shift + strzałki** zaznaczają zakres wierszy i kolumn; zwykła strzałka po tym usuwa zaznaczenie.
- **Ctrl+C** / **Ctrl+X** kopiują lub wycinają zaznaczone kolumny albo nutę/instrument/głośność bieżącego wiersza przy braku zaznaczenia; **Ctrl+V** wkleja pod kursorem.
- **Delete** usuwa cały wiersz/wiersze (wszystkie kolumny) i przesuwa resztę frazy w górę, aby uzupełnić lukę; kursor zostaje na tym samym wierszu.
- **Backspace** usuwa element pod kursorem tylko w bieżącej kolumnie (lub w zaznaczonych kolumnach, jeśli są aktywne), przesuwa wiersze poniżej w górę i cofa kursor o wiersz, jak w edytorze tekstu.
- **Insert** wstawia pusty wiersz, przesuwając resztę frazy w dół.
- **Ctrl+S** zapisuje projekt. Poza key jazz Ctrl+S nie działa.
- Ponowne **Esc** wyłącza key jazz.

Podczas włączonego key jazz, na ekranie Phrase klawisze nut działają wyłącznie dla tego trybu, w tym skróty Edit/Opt/Motion współdzielące te klawisze — wyłącz key jazz Esc, by ich używać ponownie.

Key jazz działa także, z osobnym przełączaniem Escape, na:

- **Song** i **Chain**: wpisuj bezpośrednio indeks szesnastkowy chaina albo frazy (`0-9`, `A-F`) zamiast używać Up/Down. Pierwsza cyfra zastępuje wartość, dalsze wpisy bez przejścia przesuwają ją (wpisanie "3", a potem "F" ustawia 3F). W Song **Shift+strzałki** zaznaczają zakres, **Delete** usuwa wiersze i przesuwa frazę w górę, **Backspace** usuwa bieżące/zaznaczone kolumny i przesuwa pozostałe w górę, **Insert** wstawia pusty wiersz, a **Ctrl+C**/**Ctrl+X**/**Ctrl+V** kopiują/wycinają/wklejają zaznaczone wiersze (lub bieżący, gdy brak zaznaczenia) — te same skróty jak w Phrase. Ten tryb przejmuje Shift, więc Shift+Prawo/Góra nie przełącza już do Chain/Project podczas aktywnego key jazz — Esc przywraca te skróty.
- **Wszystkie ekrany z polami tekstowymi**: wpisuj bezpośrednio z klawiatury zamiast używać wirtualnej klawiatury ekranowej. Działa to w nazwach plików Project, tytule i autorze, nazwie instrumentu, tematowi kolorów, tablicy wysokości, nazwie pliku bounce i dialogach nazw (wpisz nazwę, utwórz folder). Backspace działa jak zwykłe usuwanie tekstu; Shift wpisuje wielkie litery. Esc przełącza tę funkcję jednocześnie na wszystkich tych ekranach; klawiatura wirtualna nadal otwiera się normalnie, gdy key jazz jest wyłączony.
- **Instrument**: z włączonym key jazz klawisze nut grają dźwięk instrumentu na bieżącej ścieżce dla podglądu, bez wpisywania nuty (ten sam układ pianina co w Phrase; ostatni wciśnięty klawisz jest słyszalny, dźwięk zwalniany po puszczeniu). Klawisze oktawy przesuwają oktawę, zaczynając od oktawy pierwszej nuty instrumentu. W polu **Name** klawisze wpisują tekst zamiast grać. Dźwięk nie jest odtwarzany, gdy ścieżka gra utwór.

## 5. Instrumenty

Wszystkie instrumenty mają kilka wspólnych parametrów:

- Typ instrumentu
- Nazwa, do 15 znaków
- Domyślna prędkość tabeli, w tickach na wiersz tabeli
- Głośność
- Panorama (panning)
- Transpozycja włączona lub wyłączona

Każdy instrument ma domyślną tabelę o tym samym numerze w zakresie `00-7F`. Możesz używać ich jako tabel pomocniczych, ale trzymanie tabel pomocniczych w zakresie `80-FE` zapobiega konfliktom i zamieszaniu. Tabele są jednym z głównych narzędzi do kształtowania dźwięku w ChooChooTracker.

### Sterowanie

- **OPT + [LEWO/PRAWO]**: przełączanie między instrumentami
- **EDIT + PLAY**: podgląd instrumentu
- **SHIFT + OPT**: kopiuj instrument
- **SHIFT + EDIT**: wklej instrument

### SOUNDFONT (eksperymentalne)

Skopiuj oryginalne banki SF2/SF3, instrumenty SFZ/EXS z próbkami lub archiwa ZIP
do `instruments/USER/soundfont`. Wybierz **SOUNDFONT** w Instrument. Wiersz skanowania pokazuje **Not scanned**, **Scanning...** lub **Scanned**, z akcją **Rescan**.
Skanowanie odczytuje metadane; nie ładuje próbek wszystkich presetów do pamięci.

Wiersz **Preset** pokazuje aktywny preset. Naciśnij i zwolnij EDIT, by otworzyć przeglądarkę przy tym presecie: kolekcje po lewej i ich presety po prawej. Każda kolekcja to lista płaska, zawierająca presety w plikach banków i archiwach ZIP. Wspierające WAV/AIFF pozostają przy swoich instrumentach; kolekcje zawierające tylko poszczególne próbki pokazują je jako presety.
ID pobrań i skróty archiwów są usuwane z etykiet; zachowywane są numery nut, BPM i take. EDIT wybiera preset; EDIT+PLAY podgląda załadowany wybór.

Przytrzymaj EDIT i naciśnij Left/Right w Preset, by przemieszczać się kolejno w kolejności przeglądarki, przechodząc do następnej kolekcji i zawijając bibliotekę na końcach. Szybkie przewijanie przygotowuje ostatnio żądany wybór. Bieżący preset jest dostępny do grania podczas przygotowywania zamiennika. W razie błędu wybrany preset pozostaje, instrument staje się cichy. **Błąd:** i powód pojawiają się na dole Instrument; przewijanie kontynuuje od tego presetu w obu kierunkach. Po imporcie lub zmianie plików biblioteki wykonaj ponowny skan. Stan presetu pojawia się bezpośrednio pod Preset, w stałym kolorze. Ładowanie nie wyświetla komunikatów na SONG ani innych stronach.

**Native keys** pokazuje zakres nut-on autorsko przypisanych do presetu, używając C4 dla MIDI 60. **(gaps)** oznacza, że niektóre nuty w tym zakresie nie mają przypisanej strefy.
To suma warstw velocity; nie każda prędkość czy artykulacja musi brzmieć na każdym wyświetlanym klawiszu. Sample release-only nie rozszerzają zakresu.
Dostrajanie i pitch FX nie zmieniają tego widoku ani zakresu przypisania.

**Edit**, po prawej stronie Preset, otwiera eksperymentalny edytor SoundFont:
strojenie (−2400 do +2400 centów), cutoff (20–20000 Hz), rezonans (0–40 dB), opóźnienie/attack/hold/decay/release (0–20000 ms), sustain (0–100%), głośność, panorama i
**Bend step** (1–100 centów na jednostkę PBN). Nowe instrumenty używają kroku bend 1 cent.
Starsze pliki wyświetlają **Stock**, zachowujące skalę PBN (25 centów w trybie pitch liniowym, 1 w trybie period); ustaw Bend step na 1, by uzyskać drobniejsze zginanie. Clear ustawia krok bend na 1 cent. Dotyczy to tylko SoundFont PBN; FPT pozostaje z ustalonym strojem.
Głośność i panorama są tymi samymi wartościami instrumentu na stronie głównej.
Pola ton i obwiedni domyślnie mają wartość **Bank**, zachowując autorskie wartości każdej warstwy. Wartość wpisana jawnie nadpisuje parametr dla warstw presetu.
Clear przywraca **Bank**, zero centów dla strojenia, pełną głośność i panoramę na środek.
EDIT+Left/Right wprowadza drobne zmiany; EDIT+Up/Down — większe. OPT wraca do Instrument; EDIT+PLAY odsłuchuje instrument na nucie utworu lub domyślnej.
To edytuje kontrolę odtwarzania instrumentu, nie mapowania każdej próbki w banku, zakresów klawiatury/velocity ani punktów loop.

Strojenie, cutoff i rezonans działają na żywo. Attack, delay i hold ustawiane przy note-on. Decay, sustain i release mogą aktualizować grający głos; release zaczyna się wraz z note OFF, nie tylko na pustym wierszu. Decay nie jest słyszalny przy 100% sustain. Czas release nie może wydłużać próbki poza dostępną długość audio. Jawne nadpisanie release sprawia, że próbka one-shot reaguje na note OFF. Przy ustawieniach Bank zachowany jest autorski tryb one-shot. Zakresy klucz i velocity presetu pozostają wiążące; nuty spoza nich są bez dźwięku. FPT może zmienić wysokość już wybranej próbki poza tym zakresem bez zmiany mapowania.
Zmiany zapisywane są w plikach instrumentu i projektu bez modyfikowania źródłowego banku;
starsze utwory bez nadpisań zachowują oryginalne ustawienia banku.WAV z nieznanym, specyficznym dla samplerów typem pętli może odtwarzać dźwięk bez tej pętli. Ostrzeżenie Instrumentu informuje o zignorowanym typie pętli; plik źródłowy pozostaje niezmieniony. Pętla forward WAV zakończona dokładnie jednym próbką poza granicą audio jest traktowana jako ekskluzywny koniec na granicy próbki, z ostrzeżeniem; mapowania PCM oraz jawne mapowania pętli SFZ/EXS pozostają niezmienione. Inne nieprawidłowe granice pętli i klucz korzeniowy zgłaszają konkretne błędy. Pojedynczy zerowy bajt wyrównania RIFF błędnie uwzględniony w nieparzystej ramce PCM jest ignorowany z ostrzeżeniem; inne niepełne ramki PCM pozostają błędami. Standardowe pętle WAV zmienne/odwrotne nie są jeszcze obsługiwane.

**SoundFont FX** udostępnia osiem kontroli w kontekstowym wyborze Lane FX. Wszystkie wartości poniżej używają notacji szesnastkowej trackera. FX wpływają tylko na swój ślad i pozostawiają zapisane ustawienia instrumentu bez zmian. Każda nowa nuta startuje z ustawień instrumentu, a następnie stosuje polecenia z jej wiersza i kolejnych.

| FX  | Kontrola | Wartości |
| --- | -------- | -------- |
| FPT | Pitch    | `00` = −24 półtonów, `18` = zero, `30` = +24; dokładne półtonowe kroki |
| FCF | Cutoff   | `00`–`FE`: 20 Hz–20 kHz, logarytmicznie |
| FRS | Resonance| `00`–`FE`: 0–40 dB |
| FAT | Attack   | `00`–`FE`: 0–20 sekund, gęstsze odstępy przy krótkich czasach |
| FHD | Hold     | `00`–`FE`: 0–20 sekund przed decay, ustawiane na wierszu nuty |
| FSU | Sustain  | `00`–`FE`: 0–100% poziomu podtrzymania |
| FDC | Decay    | `00`–`FE`: 0–20 sekund, gęstsze odstępy przy krótkich czasach |
| FRL | Release  | `00`–`FE`: 0–20 sekund, gęstsze odstępy przy krótkich czasach |

Dla FCF/FRS/FAT/FHD/FDC/FSU/FRL, `FF` powraca do ustawienia z Edit (włącznie z **Bank**). Umieść Attack/Hold na wierszu nuty. Decay/Sustain/Release mogą zmieniać aktywną nutę; użyj OFF, aby usłyszeć release. Głośność i panorama używają wspólnych kontroli; polecenia FVL/FPN w wersji legacy nadal ładują się i grają, ale nie są już oferowane. FX pitch zmienia strojenie bez zmiany wybranej strefy klawiszy banku. Istniejące polecenia pitch/fine-pitch sekwencera, MODy i efekty ścieżek pozostają dostępne. Projekty zapisują stabilne referencje bank/preset oraz polecenia FX; biblioteki próbek pozostają zewnętrzne.

Wszystkie banki to banki USER. Duże instrumenty mogą przekraczać domyślny bufor PCM o rozmiarze 64 MiB, a nieobsługiwane parametry źródłowe generują ostrzeżenia. Zobacz [przewodnik SoundBanks](soundbanks/README.md) dla ścieżek platformy, importu przez przeglądarkę, konfiguracji bufora, pokrycia formatu i aktualnych ograniczeń walidacji.

#### Kompatybilność SoundFont: banki, presety i modulacja

Jeden plik **banku** SF2 lub SF3 może zawierać wiele **presetów** wraz z ich próbkami. Kolekcja biblioteki to folder używany do grupowania tych plików w przeglądarce. To różne poziomy: preset może mieć nieobsługiwane zachowanie nawet jeśli inne presety w tym samym pliku działają poprawnie.

**Modulacja presetów** oznacza zaprogramowane powiązania z wejściami takimi jak velocity, klawisz, mod wheel czy pressure do parametru dźwięku. Te powiązania mogą należeć do presetu lub do warstwy instrumentu wspólnej dla kilku presetów. Nie muszą stanowić ustawienia mającego wpływ na każdy dźwięk w banku.

| Funkcja presetu                         | Aktualna implementacja |
| ------------------------------------- | ---------------------- |
| Mapowanie próbki/klawisza/velocity, baza dźwięku i strojenie | Zaimplementowane; zachowane range autorskie. Strefy velocity i zwykłe velocity trackera działają nadal. |
| Offsety próbek, obsługiwane pętle, gain/pan i grupy ekskluzywne | Zaimplementowane w parserze i limitach głosu. |
| Statyczny low-pass cutoff/resonance i obwiednia głośności | Zaimplementowane przez silnik trackera; brak gwarancji identycznej reakcji z innym odtwarzaczem SoundFont. |
| Niestandardowe trasy modulacji preset/instrument (`pmod` / `imod`) | Niezaimplementowane. Ignorowane są autorskie mapowania kontroler-parametr, wartości i krzywe odpowiedzi. |
| SoundFont modulacja/vibrato LFO i trasa obwiedni modulacji | Niezaimplementowane; może brakować zaprogramowanych ruchów pitcha, filtru lub głośności. |
| Key-scaled hold/decay obwiedni głośności, generatory fixed-key/fixed-velocity | Niezaimplementowane. |
| SoundFont chorusa/reverbu | Niezaimplementowane. Efekty trackera można ustawić ręcznie, ale nie odtwarzają ustawień presetu automatycznie. |

Starszy komunikat **„Niestandardowa modulacja banku ignorowana”** opisuje sprawdzenie na poziomie pliku: parser znalazł niestandardowe rekordy modulacji gdzieś w banku. Nie oznacza to jednak, że wybrany preset ich używa lub że cały bank jest bezużyteczny. To ogólne ograniczenie jest opisane tutaj zamiast powtarzania tego komunikatu pod każdym presetem. Szczegółowe diagnozy parsera zachowują rekordy; inne błędy ładowania i ostrzeżenia o kompatybilności nadal się pojawiają jeśli mają zastosowanie. Silnik nie twierdzi również pełnej zgodności z domyślnym zachowaniem modulatorów SoundFont. Tracker PBN, vibrato, Edit i Lane FX są niezależnymi kontrolami, a nie automatycznymi tłumaczeniami pominiętej programacji presetów.

Wybierając bibliotekę, zacznij od presetów opartych głównie na mapowaniach próbek, pętlach i statycznych obwiedniach. Dźwięki, których charakter zależy od artykulacji kontrolerów, animowanych filtrów lub zaprogramowanych LFO wymagają bliższej analizy w pełnym odtwarzaczu SoundFont. **Ładuje i gra** nie znaczy **w pełni odtwarza preset**. Porównaj przydatne nuty i velocity, a także trzymane nuty i OFF nuty. Zmiana SF2 na SF3 (lub odwrotnie) sama w sobie nie usuwa tych potrzeb modulacyjnych. Konwersja musi przekładać mapowania i zachowanie na funkcje wspierane przez ten silnik; wyrenderowanie efektu do próbki zachowuje tylko ten efekt, nie interaktywną reakcję kontrolera. Zobacz [pokrycie formatu i limity](soundbanks/README.md#format-coverage) oraz [notatki walidacyjne](soundbanks/validation.md) przed wyborem lub konwersją banku. Te ograniczenia opisują aktualną eksperymentalną implementację, a nie wrodzone ograniczenia wszystkich odtwarzaczy SoundFont.

#### Przeglądanie kolekcji SoundFont

W SAMPLE → SOUNDFONT **Preset** otwiera Twoje kolekcje. **Right** lub **EDIT/A** wchodzi bezpośrednio do folderu. W kolekcji końcowej Right przechodzi do jej presetów; kolejny Right przesuwa o 15 wpisów bez ładowania. EDIT/A ładuje podświetlony preset. Left wraca do kolekcji, potem do folderu nadrzędnego; OPT także cofa. PLAY uruchamia/zatrzymuje utwór podczas przeglądania.

Ikony folderu i dźwięku identyfikują wpisy. Kolekcje używają pełnej wysokości panelu. Wybrany preset pozostaje pod ośmioma podglądami ścieżek; długie nazwy przewijają się po sekundzie bezczynności na zaznaczeniu i resetują przy ruchu. EDIT+Left/Right na stronie instrumentu podąża tą samą kolejnością zagnieżdżonych folderów, trzymając razem brzmienia każdej kolekcji. EDIT+PLAY odsłuchuje.

Trzeci wiersz zgłasza Reading preset, Finding samples, Unzipping, Checking samples lub Loading samples. Instrumenty EXS z pustymi lub `(null)` osadzonymi nazwami używają nazw plików. Banki źródłowe i audio nie zmieniają się. Użyj **Rescan** po zmianie plików biblioteki.

Zobacz [zakres formatów i ograniczenia](soundbanks/README.md#format-coverage) dla obsługiwanego zachowania samplera i formatów sampli.

### AY Classic, AY Plus i AY Sample

To oryginalne silniki ChipNomad AY/YM. AY Classic oferuje kontrolę tonów, szumu i obwiedni sprzętowej. AY Plus dodaje oscylatory programowe i bogatszą modulację. AY Sample odtwarza próbkę przez poziomy głośności w stylu AY i jest odrębny od silnika Sampler.

#### AY Classic

To typ instrumentu legacy z ChipNomad `v1.0.0`. Nowszy instrument AY Plus oferuje więcej funkcji.

Instrumenty AY Classic mają parametry:

- Mixer: ton włączony/wyłączony, szum włączony/wyłączony, kształt obwiedni (`0-F`)
- Głośność: obwiednia ADSR generowana programowo
- Automatyczny okres obwiedni: włączony/wyłączony, z zakresem `1:1` do `F:F`

**AY Quality** w Settings wpływa tylko na rendering AY/YM. Nie zmienia jakości Braids, Plaits ani Samplera. **Dithering próbek** dotyczy kwantyzacji AY Sample.

Zobacz [dokumentację ChipNomad AY-3-8910](https://chipnomad.org/chips/ay-3-8910/) dla szczegółów układu.

#### AY Plus

AY Plus stosuje klasyczne koncepcje syntezatora do designu dźwięku AY-3-8910/YM2149F. Można myśleć o AY jako syntezatorze z 5 oscylatorami:

- 3 oscylatory tonu fal kwadratowych
- 1 oscylator szumu
- 1 generator obwiedni amplitudy, który może działać jako oscylator

Według tego modelu, każdy instrument ma 3 sprzętowe oscylatory: ton, szum i obwiednię. AY Plus dodaje 4. oscylator programowy. Aktualizacja rejestrów AY szybciej niż takt projektu otwiera więcej opcji designu dźwięku. Z powodu działania AY, oscylatory mieszane są modulacją pierścieniową. Eksperymentuj z różnymi przesunięciami pitcha, strojeniem i kombinacjami oscylatorów, aby znaleźć ciekawe brzmienia.

Każdy tonalny oscylator można niezależnie stroić z offsetem pitch i fine tune.

Typy oscylatorów programowych i ich parametry:

- Pulse – fala kwadratowa z kontrolowaną szerokością impulsu
  - FM Depth – podstawowa głębokość FM
  - Pulse width – pełny zakres `00-FF`, liczba kroków ustawiana na ekranie Projektu
  - Pulse low – kontroluje wartość niską w powtarzającej się parze, gdzie wartość wysoka to zawsze `15`
- Sync Tone – efekt twardej synchronizacji dla oscylatora tonów
  - FM Depth – podstawowa głębokość FM
- Sync Env – efekt twardej synchronizacji dla oscylatora obwiedni
  - FM Depth – podstawowa głębokość FM
  - Pulse width – stosunek czasowy dwóch kształtów obwiedni w parze
  - Envelope Pair – 2 kształty obwiedni do przełączania
- Wavetable – 32-stopniowe wavetable, edytowane na ekranie Wavetable
  - FM Depth – podstawowa głębokość FM
  - Wavetable index
- Tone FM – prosty efekt FM dla oscylatora tonów
  - FM Depth – głębokość FM tonu
- Env FM – prosty efekt FM dla oscylatora obwiedni
  - FM Depth – głębokość FM obwiedni

Oscylatory programowe Pulse i Wavetable nie mogą być używane razem z oscylatorem obwiedni. Oscylator obwiedni wyłącza się automatycznie.

Proste FM szybko przełącza się między 2 wartościami okresu, jedną niższą i jedną wyższą. To podobne do używania fali kwadratowej jako modulatora FM. Pulse, Sync Tone, Sync Env i Wavetable stosują stosunek `1:1` pomiędzy nośną FM i modulatorem. Tone FM i Env FM mogą imitować inne stosunki przez offsety strojenia oscylatorów programowych.

#### AY Sample

Można załadować pliki WAV mono lub stereo w formacie `8-bit`, `16-bit`, `24-bit` lub `32-bit`. Są konwertowane do `8-bit` i obcinane do 16 KB. Ten limit odpowiada wielkości strony RAM ZX Spectrum.

AY odtwarza próbki przez szybkie zapisy do rejestru głośności. Wyjście jest jednobiegunowe, więc próbki wymagają „podniesienia do zera” dla czystszego odtwarzania. Usuwa to kliknięcia na końcu próbki i poprawia ciche wygaszenia.

AY ma rozdzielczość DAC `4-bit` z nieliniową skalą głośności, więc próbki `8-bit` są konwertowane do `4-bit` podczas odtwarzania. Opcja ditheringu próbek w Settings może poprawić percepowaną głębię bitową. Dithering nie jest praktyczny na retroplatformach jak ZX Spectrum czy Atari ST, więc wyłącz go pisząc na prawdziwy sprzęt lub dla szorstkiego lo-fi brzmienia.

Próbki działają jak kolejny oscylator programowy AY Plus, ale ich dodatkowe parametry ułatwiają obsługę jako osobny typ instrumentu.

Parametry próbki:

- Sample rate – odtwarzanie próbki z rodzimą szybkością daje `C-4`
- Sample start
- Sample length
- Sample loop start
- Sample pitch offset
- Sample fine pitch offset

Sekcje oscylatorów Tone i Noise odpowiadają tym z AY Plus. Z powodu działania AY, wszystkie oscylatory mieszane są modulacją pierścieniową.

### Braids

Braids oferuje 47 modeli syntezy. Główne kontrolki to:

- **Model**: oscylator lub algorytm syntezy
- **Timbre** i **Color**: parametry makro zależne od modelu
- **Filter**: dodatkowy filtr LP/BP/HP, nachylenie `12` lub `24 dB`, cutoff i rezonans
- **ADSR**: attack, decay, sustain i release

Dotknij **Model**, by wybrać z listy skategoryzowanych modeli.

| Kategoria       | Modele |
|-----------------|--------|
| Analog          | `00 CSAW`, `01 MORPH`, `02 SAW-SQUARE`, `03 SINE-TRI`, `04 BUZZ`, `05 SQUARE-SUB`, `06 SAW-SUB`, `07 SQUARE-SYNC`, `08 SAW-SYNC` |
| Multi Osc       | `09 TRIPLE-SAW`, `10 TRIPLE-SQR`, `11 TRIPLE-TRI`, `12 TRIPLE-SINE`, `13 TRIPLE-RING`, `14 SAW-SWARM`, `15 SAW-COMB`, `16 TOY` |
| Filter / Voice  | `17 FILTER-LP`, `18 FILTER-PEAK`, `19 FILTER-BP`, `20 FILTER-HP`, `21 VOSIM`, `22 VOWEL`, `23 VOWEL-FOF`, `24 HARMONICS` |
| FM / Chaos      | `25 FM`, `26 FEEDBACK-FM`, `27 CHAOTIC-FM` |
| Physical        | `28 PLUCKED`, `29 BOWED`, `30 BLOWN`, `31 FLUTED`, `32 STRUCK-BELL`, `33 STRUCK-DRUM` |
| Drums           | `34 KICK`, `35 CYMBAL`, `36 SNARE` |
| Wavetables      | `37 WAVETABLES`, `38 WAVE-MAP`, `39 WAVE-LINE`, `40 WAVE-PARA` |
| Noise / Granular| `41 FILTER-NOISE`, `42 TWIN-PEAKS`, `43 CLOCK-NOISE`, `44 GRAN-CLOUD`, `45 PARTICLE`, `46 DIGI-MOD` |

#### Parametry

Timbre i Color zależą od wybranego modelu. Odwołaj się do oryginalnej instrukcji Braids od Mutable Instruments po pełną listę modeli i detale lub eksperymentuj na ucho.

#### VCF i VCA

Braids zasila standardowy multimode filtr i obwiednię ADSR ChooChooTracker.

#### Ustawienia

Globalne ustawienia Braids dostępne są w opcjach aplikacji:

- **BITS**: redukuje rozdzielczość wyjścia
- **DRFT**: dodaje niestabilność strojenia oscylatora
- **SIGN**: odtwarza oryginalny algorytm niedoskonałości fali Braids

### aChChid**aChChid** to monofoniczny silnik acid bass oparty na Open303. `Square` i `Saw` korzystają z rodzimych oscylatorów TB-303, filtru, obwiedni i akcentów. `Braids` zastępuje jedynie oscylator, a następnie sygnał przechodzi przez ten sam filtr 303 i ścieżkę wzmacniacza. Umożliwia kontrolę Model, Timbre, Color oraz dodatkowego waveshapera. Waveshaper przechodzi od miękkiej saturacji do wavefoldingu.

Cutoff i Decay edytuje się logarytmicznie: **EDIT + LEFT/RIGHT** przesuwa o jeden krok muzyczny, a **EDIT + UP/DOWN** o szesnaście kroków. Cutoff obejmuje zakres od 200 Hz do 20 kHz; Decay od 200 ms do 2 s.

`F` w kolumnie głośności nuty wyzwala akcent. `ASL` ślizga do tej nuty z poprzedniej wysokości bez ponownego wyzwalania obwiedni 303; automatycznie utrzymuje poprzedni gate, a `ASL 00` daje `60 ms` glide. `ATY` trzyma nutę otwartą przez cały aktualny krok. Nuty bez `ASL` zawsze wyzwalają ponownie.

Cele modulacji obejmują Decay i Accent oraz Timbre i Color w trybie fal Braids.

### Bogie

**Bogie** to natywny syntezator perkusyjny one-shot. Nuty w trackerze ustawiają wysokość, a głośność trackera/instrumentu poziom; każdy trigger ma własny wewnętrzny decay, więc nie ma strony ADSR. Ma 12 modeli: `Kick`, `Snare`, `Hat`, `Clap`, `Tom`, `Rim`, `FM`, `Noise`, `Cowbell`, `Cymbal`, `Shaker` i `Clave`.

Sześć makr to **Decay**, **Tone**, **Sweep**, **Noise**, **FM** i **Drive**. Etykiety dostosowują się tam, gdzie to użyteczne (np. Noise w Kick to `Click`, a FM w Cowbell to metaliczna modulacja krzyżowa). Każde makro działa na każdym modelu, ale jego rola muzyczna zmienia się w zależności od modelu: Sweep może zmieniać wysokość, odstępy wybuchów lub rozproszenie metaliczne; FM dodaje modulację krzyżową, metaliczną warstwę lub dzwonkowaty alikwot. Dolna połowa zakresu służy do tradycyjnych dźwięków perkusyjnych; górna połowa otwiera szersze morfy timbralne i bardziej syntetyczne efekty.

Każdy model może korzystać ze wspólnego filtru multimode LP/BP/HP o charakterze Clean, Classic, Aggro lub Acid, stromiźnie `12`/`24 dB`, cutoff i rezonansie. Bogie obsługuje wszystkie aktywne makra oraz Volume, Pitch, Cutoff i Resonance jako cele modulacji i nagrywania ruchu. Użyj krótkiego Decay z Cowbell dla ostrych ataków phonk; zwiększ FM dla twardszych, bardziej metalicznych dzwonów.

Na ekranie Modulacji, naciśnij **EDIT** na źródle, aby wybrać rodzinę (Envelopes, LFO lub Sticks), a następnie konkretne źródło. **Left** i **Right** cyklują źródła bezpośrednio.

### Sintered

**Sintered** to natywny syntezator digital perkusji one-shot. Nuty ustawiają ton podstawowy; sześć modeli to `Knot`, `Shard`, `Burst`, `Comb`, `Logic` i `Melt`. Każdy model ma **Decay**, **Mod** i **Motion**, plus trzy specyficzne kontrolki: Knot używa Ratio/Spread/Fold, Shard Ratio/Feedback/Bite, Burst Noise/Color/Feedback, Comb Time/Damping/Regen, Logic Rate/Pattern/Crush, Melt Ratio/Chaos/Drive. **Mod** to zawsze głębokość wewnętrznego sprzężenia.

Motion to wewnętrzne makro obwiedni: wartości poniżej `80` dają kształt AD, `80` dezaktywuje, a powyżej `80` aktywuje kształt decay-only. Oddalanie wartości od centrum przyspiesza obwiednię. Cele są stałe dla modelu, więc Sintered pozostaje przewidywalny w patternach trackera: Knot animuje Mod/Fold; Shard Feedback/Bite; Burst Noise/Color/Feedback; Comb Damping/Regen; Logic Rate/Crush; Melt Chaos/Drive. Wszystkie modele resetują stan przy trigwaniu nuty i ograniczają wewnętrzny feedback, by uderzenia były powtarzalne.

### MonkSynth

MonkSynth to syntezator wokalny. Na stronie Instrument modeluje samogłoskę, charakter głosu, oddech, glide, vibrato, unisono i delay. EDIT otwiera wszystkie kontrolki i nazwę patcha.

Wybierz bank i preset na kontrolkach VOICE. ALL obejmuje FACTORY i USER; EDIT+PLAY odsłuchuje podświetlony preset, a EDIT go wybiera. Własne brzmienia zapisuj w `instruments/USER/monk/`. Zapisane instrumenty i utwory zachowują ustawienia MonkSynth.

Wartości są szesnastkowe `00..FF`, z wyjątkiem Voices (`01..0A`, od jednego do dziesięciu). Attack, decay i release obejmują 0..5 sekund; detune 0..50 centów, a rozstaw głosów 0..0,5. Volume, Pitch i sześć głównych kontrolek głosu są dostępne jako destynacje modulacji i motion recording.

Wszystkie szesnaście kontrolek występuje razem w **MonkSynth FX**: `MWV` (samogłoska), `MIN` (głos), `MAM` (oddech), `MFL` (glide), `MFB` (głębokość vibrato), `MSH` (szybkość vibrato), `MUV` (głosy), `MDT` (detune), `MVS` (rozstaw), `MDM` (mix delay), `MDR` (szybkość delay), `MLV` (poziom) i `EAT`/`EDC`/`ESU`/`ERL` (ADSR). Te znaczenia obowiązują, gdy aktywnym instrumentem jest MONK; dotychczasowe znaczenia komend MME się nie zmieniają. Wspólna grupa Trigger FX zachowuje TDC/TCL bez zduplikowanych wpisów obwiedni.

### MME

**MME** (Multi Modulation Engine) to dwuoscylatorowy syntezator dedykowany interakcjom między oscylatorami. Jego modele to `Ring`, `Fold`, `Cross`, `VPM`, `Sync`, `Logic` i `Vocode`. Oba oscylatory są wewnętrzne i podążają za nutami trackera; `Interval` ustawia ich relację.

Sześć makr to **Waves**, **Interval**, **Amount**, **Flow**, **Feedback** i **Shaper**. Waves interpolują muzyczne pary fal A/B. Amount kontroluje podstawową interakcję modelu, Flow zmienia kierunek lub wariant algorytmu, a Feedback jest ograniczony, lecz końcówka zakresu staje się celowo dzika. Shaper to wspólny etap końcowy od czystego przez saturację do wavefoldingu.

MME korzysta ze wspólnego filtru multimode LP/BP/HP i ADSR. Wszystkie sześć makr, Cutoff i Resonance są dostępne jako cele modulacji i nagrywania ruchu. Przytrzymaj **EDIT** na Model, by otworzyć popup wyboru modelu.

### MIDI Out

**MIDI Out** (desktop, PortMaster i przeglądarki Web MIDI) wysyła sygnał do zewnętrznego urządzenia MIDI zamiast syntezować audio: wyzwolenie nuty wysyła prawdziwy Note On na **Channel** instrumentu (`1-16`), a zakończenie nuty – odpowiedni Note Off. Głośność zmienia się w velocity, a głosy ścieżki chord dostają osobne Note On/Off. W ChooChooTracker nie usłyszysz dźwięku – wybierz urządzenie MIDI w [Settings](#13-settings) najpierw. Zobacz [MIDI](#15-midi) dla pełnej informacji, w tym podglądu dźwięku z klawiatury MIDI.

**Program** i **Bank high/low** (CC0/CC32) są opcjonalne – pokazują `--` gdy wyłączone, `Clear` wyłącza pole i pamięta jego wartość. Po ustawieniu wysyłane są raz przed następną nutą, gdy różnią się od ostatniej wartości kanału (nie przed każdą nutą, co powodowałoby ponowne wyzwalanie własnych obwiedni/patchy odbiornika).

**MC1 number**-**MC4 number** wybiera numer CC (`00-7F`), jaki każde z czterech FX MC1-MC4 wysyła – patrz [MIDI Out FX](#midi-out-fx). Także `--`/opcjonalne; nieustawione sloty dezaktywują odpowiadające FX.

### Klasyczna architektura silników

Silniki w tej kategorii mają architekturę VCO → VCF → VCA.

Dostępne są różne rodzaje VCO:

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **Sampler**
- **2xSCWF**
- **BYOWTBL**

Każdy instrument podaje sygnał VCO do filtru multimode.

Filtr ma przełączaną stromiznę `12 dB` lub `24 dB` i może działać jako dolno-, środkowo- lub górnoprzepustowy (LP, BP, HP).

Dostępne charaktery filtra:

- **Clean**: sterylny i cyfrowy
- **Classic**: inspirowany klasycznymi amerykańskimi syntezatorami analogowymi
- **Aggro**: inspirowany ryczącymi japońskimi syntezatorami analogowymi
- **Acid**: podwójna dawka agresji w stosunku do Aggro

Classic, Aggro i Acid mogą osiągać silną rezonansę. Zacznij od niskich ustawień i zwiększaj ostrożnie.

Filtr zasila VCA sterowany obwiednią ADSR. Kształt obwiedni może płynnie zmieniać się między wykładniczym, liniowym i logarytmicznym.

#### Plaits i Plaits-Alt

**Plaits** oferuje 24 silniki, pogrupowane w wyskakującym oknie wyboru tak:

| Kategoria | Silniki |
|---|---|
| Analog / Waves | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digital | `07 CHIPTUNE`, `15 SPEECH` |
| Texture / Noise | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Physical | `19 STRING`, `20 MODAL` |
| Drums | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** to kolekcja alternatywnych modeli dla oryginalnego modułu Plaits. Większość jest dziwniejsza lub bardziej eksperymentalna niż przeciętny VCO.

| Kategoria | Silniki |
|---|---|
| Granular / Micro | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| Phase / Harmonic | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| Acoustic / Physical | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| Polyphony / Harmony | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| Digital / Weird | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

Parametry odpowiadają projektowi Mutable:

- **Harmonic** reguluje relacje harmoniczne, balans lub wybór modelu wewnątrz silnika.
- **Timbre** zmienia się zwykle od ciemnego/ubogiego do jasnego/gęstego spektrum.
- **Morph** eksploruje dodatkowy wymiar timbralny.
- **Main/Aux** miesza główny sygnał z alternatywnym wyjściem silnika.

Dokładne znaczenie zależy od silnika. Np. silniki akordowe używają ich do typu akordu, inwersji i kształtu fali. Modele fizyczne do materiału, excitacji i wybrzmiewania, natomiast modele perkusyjne do tonu, charakteru i wybrzmiewania. Szczegóły znajdziesz w instrukcji Plaits.

Dla `CHORD`, `DIATONIC CHORD` i `WT DIATONIC CHORD`, Harmonic pokazuje nazwę wybranego akordu (`MIN`, `MAJ`, `MIN7`, `MAJ7` itd.), zamiast surowej wartości kontrolera. Braids `WAVE PARA` analogicznie nazywa swoje barwy Color.

Naciśnij **Engine**, aby wybrać z listy silników według kategorii.

**Env Mode** ma 2 ścieżki sygnału:

- `TRIG` emuluje moduł z podłączonym TRIG i odłączonym LEVEL
- `VCA` trzyma LEVEL otwarty i stosuje trackerowy ADSR za głosem

#### Sampler

Ten czysty silnik Sample odtwarza mono lub stereo próbki PCM załadowane do pamięci RAM.- Stuknij **Sample**, aby załadować nieskompresowany `8-bit`, `16-bit` lub `24-bit` PCM WAV. Naciśnij **PLAY** w przeglądarce, aby odsłuchać zaznaczony plik.
- Po załadowaniu sample pojawia się obok nazwy przycisk **EDIT**. Stuknij go, aby otworzyć ekran edycji Sample. **OPT** lub **SELECT + [LEFT]** powraca do instrumentu.
- Ekran edycji Sample pokazuje nazwę pliku (z oznaczeniem `*`, jeśli sample w RAM różni się od pliku na dysku), informacje o formacie (częstotliwość próbkowania, kanały), wysoki wykres fali oraz pola **Region**, **Select**, **Slice**, **Process** i **File**.
- Edycje są przechowywane tylko w RAM do momentu zapisania: projekt przechowuje ścieżkę pliku sample i przy następnym otwarciu projektu ponownie ładuje WAV z dysku, więc opuszczenie trackera bez **Save**/**Save As** powoduje utratę zmian w procesach (oznaczenie `*` to ostrzeżenie). Wczytanie innego sample do instrumentu również odrzuca niezapisane edycje i resetuje Region do całego sample (Start `00`, End `FF`).
- **Region** ustawia znaczniki startu i końca odtwarzania, dostępne także na ekranie instrumentu Sampler. Określają one znormalizowane granice odtwarzania (`00-FF`); jeśli Start jest po End, sample jest odtwarzany od tyłu. **EDIT + [LEFT/RIGHT]** precyzyjnie przesuwa znacznik o jeden krok (`01`) i przybliża falę wokół niego, gdy **EDIT** jest przytrzymany; **EDIT + [UP/DOWN]** przesuwa skokowo (16 kroków) i wraca do widoku całego sample. **EDIT + OPT** resetuje znacznik do wartości domyślnych (Start `00`, End `FF`).
- Widok fali przybliża stałe okno jednej sekundy wokół edytowanego znacznika i przesuwa je tak, aby pozostał widoczny; sample mieszczące się w oknie pozostają w skali 1:1. Przybliżenie jest chwilowe: utrzymuje się podczas przytrzymania **EDIT**, a zwolnienie wraca do widoku 1:1, podobnie jak przesunięcia duże. Wejście na ekran zawsze resetuje widok do całego sample.
- **Select** ustawia zaznaczenie do przetwarzania w absolutnych ramach, niezależne od znaczników Start/End odtwarzania. **EDIT + [LEFT/RIGHT]** przesuwa uchwyt o 20 ramek i przybliża go podczas przytrzymania **EDIT**; **EDIT + [UP/DOWN]** przeskakuje o `frameCount/64` (min. 16) i wraca do widoku całego sample. **EDIT (stuknięcie)** na uchwycie kopiuje pozycję aktualnego znacznika Start lub End. **EDIT + OPT** na uchwycie czyści całe zaznaczenie. Gdy zaznaczenie jest puste, oba uchwyty pokazują `-`; gdy uchwyty są odwrócone, zamieniają się automatycznie. Zaznaczenie jest stanem sesyjnym edytora: nie zapisuje się z projektem i przy wejściu na ekran jest ustalane na podstawie granic Region (cały sample z domyślnymi znacznikami).
- **Slice** to jeden wiersz z czterema polami – **Mode**, **Count**,
  **Slice** i **Frame** – i jest zapisywany z instrumentem. Off odtwarza okno Start/End. Dźwięki Phrase wybierają wycinki chromatycznie od **C-0**, a nuty po ostatnim wycinku zawijają się na początek (nuta N odtwarza wycinek `N % count`, więc przebieg poza ostatni wycinek cyklicznie wraca do pierwszego). Wycinki nie transponują wysokości ani nie korzystają z kwantyzacji Scale, ale **CRD** utrzymuje wybrany wycinek i transponuje jego głosy jak akord. Początki wycinków są oznaczone na fali krótkimi liniami o szerokości dwóch pikseli (ciemny pomarańcz; linia aktualnego wycinka jest jaśniejsza), a wycinek pod kursorem ma czarną opaskę na fali (kolor zaznaczenia nie jest rysowany na tej opasce, więc pozostaje całkowicie czarna i wyraźnie odróżniona od reszty podglądu).
  - **Mode** cykluje `OFF` / `EQUAL` / `AUTO` / `LAZY` (EDIT + lewo/prawo lub stuknięcie; EDIT + OPT wyłącza slicing). Przełączanie trybów inicjuje punkty wycinków: **EQUAL** dzieli okno Start/End równomiernie na **Count** części; **AUTO** wykrywa transienty w oknie i ustawia wycinki na każdy wykryty początek (do **Count**); **LAZY** zaczyna z jednym wycinkiem na całe okno. Przełączenie na LAZY zapisuje bieżące ustawienie EQUAL/AUTO (liczbę i granice), a powrót do EQUAL lub AUTO przywraca je bez zmian – bez pytania i bez utraty danych. Slice i Stretch są wykluczające się: włączenie jednego wyłącza drugi, a pola Slice są przyciemnione, gdy Stretch steruje czasem trwania.
  - **Count** pokazuje całkowitą liczbę wycinków (`01`..`64`). **EDIT + [LEFT/RIGHT]** zmienia je o jeden (min. 1, max. 64), **EDIT + [UP/DOWN]** cykluje przez potęgi dwójki `2 / 4 / 8 / 16 / 32 / 64` z zawijaniem. Każda zmiana przelicza podział: **EQUAL** dzieli równomiernie okno, **AUTO** ponownie wykrywa transienty z nową wartością (czułość wykrywania zależy od liczby wycinków – więcej wycinków to niższy próg). Pole pokazuje `-` gdy tryb jest OFF. W **LAZY** pole pozostaje jasne (liczy wycinki ręcznie dodane), ale jest tylko do odczytu – kursor je pomija – ruch w prawo od **Mode** albo w lewo od **Slice** ląduje na **Slice**, ruch w dół z znacznika **END** wiersza **Select** również; dotknięcie pola nic nie robi (ręcznie dodane wycinki nie mają podziału do przeliczania). **EDIT + OPT** i zwykłe stuknięcie **EDIT** nic tu nie robią: wyłączanie slicing jest na **Mode**, usuwanie wycinka na **Slice**.
  - **Slice** jest przeglądarką: pokazuje obecny wycinek indeksowany od 1 (`01`..liczba wycinków). **EDIT + [LEFT/RIGHT]** przechodzi między wycinkami po jednym, **EDIT + [UP/DOWN]** skacze o cztery wycinki (oba ograniczone, bez zawijania); przeglądany wycinek jest zaznaczony na fali jaśniejszym znacznikiem i czarnym paskiem, a zoom centruje się na początek wycinka. **EDIT + OPT** usuwa obecny wycinek (łączy go z poprzednim; pierwszy wycinek nie może być usunięty, a usunięcie ostatniego wycinka wyłącza slicing – granice pozostają po wyłączeniu i wracają przy ponownym wyborze trybu). **EDIT (stuknięcie)** odtwarza wybrany wycinek jako odtwarzanie jednorazowe (działa we wszystkich trybach slice; podgląd zatrzymuje się po zwolnieniu klawisza lub opuszczeniu ekranu). Pole pokazuje `-` gdy tryb jest OFF.
  - **Frame** pokazuje początkową ramkę wycinka w zapisie szesnastkowym. **EDIT + [LEFT/RIGHT]** przesuwa o dziesięć ramek (przybliżając znacznik podczas trzymania **EDIT**), **EDIT + [UP/DOWN]** przesuwa o sto ramek i wraca do pełnego widoku. Znacznik pozostaje w granicach swojego wycinka – nie może przekroczyć początku poprzedniego ani następnego wycinka. **EDIT + OPT** usuwa wycinek.
  - **B** wychodzi do ekranu instrumentu z wiersza Slice, jak wszędzie indziej na tym ekranie; **SHIFT + LEFT** wychodzi z dowolnego miejsca.
  - **Tryb LAZY**: z wybranym LAZY, stuknij **PLAY**, aby rozpocząć jednorazowe odtwarzanie całego sample od początku — odtwarza raz oryginalnym tonem i tempem (zielony dwupikselowy znacznik podąża za pozycją na fali); ponowne stuknięcie **PLAY** przerywa odtwarzanie. Podczas odtwarzania każde kliknięcie **EDIT** dodaje wycinek w miejscu odtwarzania, bez względu na położenie kursora (wycinki bliższe niż 50 ms od istniejącego są odrzucane z komunikatem `Too close to slice`). Wielokrotne uruchomienie sample zachowuje istniejące wycinki i dodaje nowe przy każdym kliknięciu w kolejności – pole **Count** odświeża się przy każdym dodaniu wycinka, a przeglądarka **Slice** śledzi nowy wycinek. Obowiązują te same zasady przeglądania i edycji co w innych trybach dla ręcznie dodanych wycinków: **EDIT + kierunek** na **Slice** przegląda, **Frame** reguluje wybrany start, **EDIT + OPT** usuwa wycinek. Pole Frame jest przyciemnione podczas uzbrojenia dodawania wycinka. **SHIFT + PLAY** nie jest przechwytywany w LAZY: rozpoczyna odtwarzanie Phrase jak na innych ekranach. Wycinki LAZY odtwarzają się w sequencerze jak w EQUAL i AUTO: nuty Phrase są mapowane chromatycznie od **C-0** na ręcznie dodane wycinki, z zawijaniem.
  - **Podpowiedzi kombinacji**: gdy kursor znajduje się na polu wiersza Slice i nie pokazuje się inny komunikat, pasek statusu wyświetla podpowiedź tego pola i utrzymuje ją podczas postoju kursora. Na **Mode** podpowiedź odpowiada aktywnemu trybowi i pokazuje tylko opis (nazwa trybu jest w polu): `Divides sample in equal parts`, `Divides sample based on transients`, `Press Play and add slices with EDIT` – zmiana trybu natychmiast zmienia tekst, a OFF czyści pasek. Na **Count** pokazuje `Adjust the number of slices`, na **Slice** `Browse slices`, na **Frame** `Adjust slice start`. Każdy komunikat akcji zastępuje podpowiedź do wygaśnięcia.
- **Process** wybiera destrukcyjną operację edycji: **Crop**, **Normalize**, **Delete**, **Silence**, **Fade In**, **Fade Out** lub **Reverse** (EDIT + lewo/prawo zmienia, stuknięcie cyklicznie, EDIT + OPT wyłącza). **GO** (ten sam wiersz) wykonuje wybraną operację na zaznaczeniu lub na całym sample, gdy zaznaczenie jest puste. **Crop** zostawia tylko zaznaczenie; **Normalize** skalują zaznaczenie, aby jego szczyt osiągnął pełną skalę (oba kanały mają tę samą korekcję, co zachowuje scenę stereofoniczną); **Delete** usuwa zaznaczenie i łączy końce (usunięcie całego sample jest zablokowane); **Silence** zeruje go; **Fade In**/**Fade Out** liniowo narastają lub opadają od/do ciszy; **Reverse** odtwarza zaznaczenie od tyłu (ramki zamieniane miejscami, oba kanały ramki razem, długość i markery bez zmian). Crop i Delete wymagają zaznaczenia – przy pustym pokazują `Select region first`. Każda operacja chwilowo zatrzymuje dźwięk, zachowuje jedno poziom cofania, i oznacza sample jako zmieniony w RAM: plik na dysku pozostaje nietknięty do momentu zapisu (patrz niżej), opuszczenie ekranu usuwa schowek undo.
- **UNDO** (obok GO) przywraca sample do stanu sprzed ostatniej operacji. Jest przyciemniony do momentu wykonania operacji, przełącza się między stanami przed i po operacji przy powtarzanych wciśnięciach, i jest kasowany po wejściu na ekran.
- **File** zawiera przepływy zapisu. Instrument przechowuje pełną ścieżkę WAV, z którego został załadowany; te przepływy zapisują ten plik lub wskazują instrumentowi nowy. Nigdy nie zmieniają nazwy instrumentu.
  - Pole File cykluje pomiędzy **Save** i **Save As** (EDIT + lewo/prawo lub stuknięcie); **GO** (ten sam wiersz) wykonuje wyświetlaną akcję.
  - **Save** nadpisuje WAV, z którego sample został załadowany. Dla sample bez wycinków najpierw pyta `Overwrite <name>?`. Sample z wycinkami otwiera dialog **SAVE SLICES**, pytający gdzie zapisać punkty:
    **SAVE TO SAMPLE** – zapisuje je w WAV jako cue chunks (przenośne – DAW pokazują je jako markery) i zachowuje w projekcie,
    **SAVE TO PROJECT** – zapisuje czysty WAV i zachowuje punkty tylko w projekcie,
    **CANCEL** – anuluj.
    Opcja `Don't ask again in this project` zapisuje wybrany wybór w projekcie i pomija dialog dalej (wybór jest zapisany jako `- Sample save choice:` w .cct i można go zmienić tylko ręcznie edytując plik projektu). Dialog jest pomijany dla sampli bez wycinków. **Save** jest przyciemniony (i pomijany przy nawigacji) do czasu wczytania sample z plikiem. Nieudany zapis pozostawia oznaczenie brudnego i pokazuje błąd.
  - **Save As** zapisuje aktualny sample do nowego pliku: wpisz nazwę pliku (wstępnie wypełnioną bieżącą nazwą bez rozszerzenia), a następnie wybierz folder w przeglądarce. Sample zostaje zapisany jako `<folder>/<name>.wav`, instrument wskazuje nowy plik, a folder zostaje zapamiętany jako domyślny katalog próbek. Jest przyciemniony (i pomijany w nawigacji) do czasu wczytania danych sample, aby świeżo załadowany sample mógł dostać ścieżkę pliku. Ścieżki dłuższe niż 255 znaków są odrzucane.
  - Oznaczenie `*` przed nazwą pliku oznacza, że sample w RAM różni się od pliku na dysku (każda operacja w Process ustawia je, Save i Save As kasują). Oznaczenie jest sesyjne: nie zapisuje się z projektem i resetuje przy wejściu na ekran. Jeśli kilka instrumentów odwołuje się do tego samego pliku WAV, zapisanie jednego nadpisuje plik dla wszystkich.
- Na ekranie instrumentu Sample używaj **EDIT + [LEFT/RIGHT]**, aby załadować poprzedni lub następny WAV w tym samym folderze.
- **Pitch** transponuje o półtony (`-48` do `+48`).
- **Loop** wybiera tryb: Off, Loop lub Ping-Pong.
- **Speed** kontroluje granularne rozciąganie czasu od `0%` do `500%` (`100%` to normalna prędkość).Nieobsługiwane formaty WAV wyświetlają błąd. Przekonwertuj nietypowe pliki na `PCM8`, `PCM16` lub `PCM24` WAV przed importem.

##### Wejście audio

Otwórz **Settings → Audio → Audio Input** przed nagrywaniem z mikrofonu lub interfejsu audio USB. Lista pokazuje wejścia dostępne w danej chwili na urządzeniu. Jeśli uzbrojony interfejs zostanie odłączony, przechwytywanie jest przerywane, a menedżer audio restartuje, aby aplikacja działała dalej. Jeśli nagrywałeś, naciśnij **STOP**, aby zakończyć przerwane ujęcie. Po ponownym podłączeniu wybierz **[Refresh]** i sprawdź, czy żądane wejście jest uzbrojone, zanim znów nagrasz; samo ponowne podłączenie nie uzbraja go niezawodnie. **[List]** odczytuje ponownie bieżącą listę wejść bez restartu audio.

Zaznacz pole **ARM** wejścia, aby udostępnić je rejestratorowi Samplera; mały wskaźnik pokazuje poziom wejściowy. Uzbrajaj jeden interfejs naraz: ta wersja otwiera tylko pierwszy uzbrojony interfejs, nawet jeśli zaznaczono kilka pól. Twój wybór jest zapamiętywany według nazwy urządzenia, więc niedostępny interfejs pozostaje nieuzbrojony zamiast zostać zastąpiony innym wejściem. Naciśnij **OPT**, aby wrócić do ustawień Audio.

##### Nagrywanie z wejścia audio

Na ekranie instrumentu Sample **REC** (lub **EDIT/REC**, gdy załadowany jest sample) otwiera edytor sampli, łączący nagrywanie i edycję na jednym ekranie. Najpierw uzbrój wejścia na ekranie **Settings → Audio → Audio Input** (zob. [Wejście audio](#wejście-audio)). Górne wiersze edytora sterują nagrywaniem:

- **Input** przełącza źródło nagrywania: `T1`..`T8` nagrywają osiem ścieżek trackera, a po nich uzbrojony mikrofon lub interfejs USB. Aby nagrać źródło zewnętrzne, przechodź dalej za `T8`, aż pojawi się nazwa urządzenia. `-` oznacza, że źródło nie zostało wybrane jawnie; **REC** używa wtedy pierwszego uzbrojonego wejścia i pokazuje jego nazwę. Przytrzymaj **EDIT** i naciskaj **LEFT**/**RIGHT**, by cofać/przesuwać, albo **EDIT+OPT**, by wrócić do `-`. Stuknięcie Input przesuwa je od razu.
- Wskaźnik poziomu obok **Input** pokazuje poziom uzbrojonego wejścia na żywo (tylko źródła zewnętrzne). **MON** włącza monitorowanie wejścia: uzbrojone wejście jest kierowane na wyjście z -6 dB, aby ograniczyć sprzężenie. Ustawienie jest zapisywane w pliku ustawień.
- **REC** rozpoczyna nagrywanie. Źródła zewnętrzne nagrywają od razu; źródło ścieżki pokazuje **ARMED**, a nagrywanie zaczyna się po starcie odtwarzania (naciśnij **PLAY**). **STOP** kończy nagrywanie: WAV jest zapisywany w folderze eksportu projektu (`recNNN.wav`), ładowany do bieżącego instrumentu Sampler, a Region znów obejmuje cały sample. Wyjście z ekranu przez **OPT** w trakcie nagrywania kończy je tak samo.
- **PLAY** (widoczny po nagraniu) odsłuchuje ujęcie; podczas podglądu zmienia się w **STOP**. **CLEAR** (obok) usuwa plik WAV i zwalnia sample z instrumentu; po usunięciu **REC** wraca na swoje miejsce.
- Przebieg pokazuje wejście na żywo podczas nagrywania, a po zakończeniu pełny przebieg. Wiersz statusu pokazuje nazwę pliku w trakcie nagrywania, a potem liczbę klatek; ujęcie zewnętrzne pokazuje też `drop 0`, gdy nie utracono żadnej klatki audio. Nagrany sample jest w pełni edytowalny: region pętli, zaznaczenie, slicing, przetwarzanie i zapis działają jak dla każdego wczytanego pliku.

Nagrania są zapisywane przy wybranej częstotliwości próbkowania jako 16-bitowy stereofoniczny WAV. Źródła ścieżek przechwytują to, co renderują instrumenty ścieżki (po FX); źródła zewnętrzne przechwytują wejście urządzenia bezpośrednio.

#### 2xSCWF

2xSCWF to podwójny oscylator z jednocykliczną falą. Załaduj 1 mono WAV zawierający dokładnie 1 okres do każdego oscylatora. Oba oscylatory odczytują fale do przodu i zawijają się na końcu cyklu.

Miksuj crossfade A i B.

Detune jest drobny i wykładniczy od unisonu do `+200 ct`, potem przechodzi w półtonowe kroki od `+3 st` do `+24 st`.

2xSCWF traktuje każdy plik jako tabelę 1-cyklową i odczytuje ją dokładnie raz na cykl oscylatora. Pozwala to mieszać i detunować 2 fale jednocykliczne.

Fabryczny folder `waveforms/AKWF/` zawiera waveforms z biblioteki Adventure Kid. Wiele więcej jest dostępnych w sieci, jeśli chcesz rozszerzyć kolekcję.

#### BYOWTBL

BYOWTBL to podwójny oscylator wavetable zgodny z tabelami Serum. Każdy oscylator ładuje mono WAV zawierający kolejne jednocykliczne ramki. `Pos A` i `Pos B` niezależnie skanują swoje tabele od `00` do `FF`. Silnik interpoluje liniowo w ramach każdej ramki i między sąsiednimi ramkami. `Mix` crossfade’uje oscylatory, a `Detune` przesuwa oscylator B.

### Rodzina instrumentów FM

#### Rodzinne instrumenty Native OPLL i VRC7

W selektorze typu instrumentu znajduje się grupa FM z **OPLL / MSX** (YM2413) i **VRC7** (DS1001). Każdy oferuje teraz **73 programy** na przewijalnych listach Bank / Preset: oryginalne 15 tonów, 40 dodatkowych unikatowych tonów z tabel emu2413 YM2413/VRC7/YMF281B oraz 18 programów dwuprocesorowych autorstwa ChooChoo. Dokładne duplikaty bajtów tonu są pomijane w każdym silniku. Dodatkowe palety wykorzystują programowalny slot tonów układu; nie powiększają fizycznego ROMu. Program zero oznacza niestandardowy ton. EDIT + lewo/prawo wybiera poprzedni lub następny preset w filtrze banku. Fine ct reguluje strojenie od -100 do +100 centów.

Na liście programów, EDIT + PLAY odtwarza wybrany dźwięk; zwolnienie przycisków go zatrzymuje. EDIT zatwierdza, OPT anuluje. Przeglądanie/odsłuchiwanie nie zmienia instrumentu, tabeli ani utworu. Standardowy gest odsłuchu na stronie instrumentu działa dalej po wyborze. Natrysk native chipów dostarcza atak/zwolnienie; opcjonalna obwiednia Amp dodaje programową ADSR (patrz Native chip controls poniżej). Brak pełnego edytora operatorów.

#### AdLib / OPL2 i OPL3

FM obejmuje także AdLib / OPL2 (YM3812) i OPL3 (YMF262). Bank filtruje listę fabryczną; Preset otwiera grupy kategorii i Widok Wszystkich. EDIT + PLAY odsłuchuje przed wyborem, EDIT wybiera, OPT anuluje, a EDIT + lewo/prawo na Preset ładuje poprzedni/następny pasujący wpis. Fine ct reguluje strojenie lokalne. Tryb pokazuje 2 operatorów, 4 operatorów lub podwójny głos. OPL2 ukrywa niekompatybilne patche OPL3; OPL3 może odtwarzać wspólną kolekcję dwuprocesorową.

Popupy banku i presetu identyfikują aktualny silnik. Zmiana silnika lub slotu instrumentu resetuje filtr banku do **Wszystkich banków**, także przy przełączaniu między kompatybilnymi silnikami AdLib i OPL3. Powrót z popupu w tym samym slocie zachowuje wybrany filtr. Anulowanie importu banku DX7 przywraca poprzedni filtr.

Potwierdzenie presetu publikuję także zmiany dokonane po zwolnieniu przycisku do następnego ticku UI/audio. Odtwarzanie utworów nie wymaga już kolejnego naciśnięcia przycisku ani restartu transportu, by przełączyć patch. Wyjście wciąż podąża za skonfigurowanym buforem audio i zachowaniem obwiedni/retrygerów silnika.

Kolekcja fabryczna zawiera 697 oryginalnych wpisów z The Fat Man 2-op, The Fat Man 4-op i DMXOPL3, z 589 unikatowymi normalizowanymi identyfikatorami patchy. Aliasy zachowują nazwy i źródła. Rodzinne poziomy operatorów, flagi głębokości LFO, offsety nut, stałe wysokości perkusji i strojenie podwójnego głosu są zachowane. Volume tracker używa programowej amplitudy; nie odtwarza oryginalnych krzywych głośności czy polityk offsetu velocity MIDI. Szacunki czasu zwolnienia ze źródła nie ucinają podtrzymywanych nut. Niektóre efekty mają wolne ataki: przytrzymaj Seashore zamiast oczekiwać krótkiego kliknięcia, by je ujawnić.

Wybranie presetu nie zmienia tabeli slotu ani insertów na ścieżce.

#### Native instrumenty Sega PSG i Game Boy

CHIP zawiera Sega PSG, GB Pulse i GB Noise. W Sega PSG, **Zakres basów: Rozszerzony** zachowuje rozróżnienie niskich nut. Wybierz **Chip** dla oryginalnego ograniczonego zakresu, gdzie najniższe nuty łączą się. Szum stałej częstotliwości zachowuje oryginalną wysokość; generowany z tonu podąża za wybranym zakresem basów. Instrumenty GB używają rejestrów impulsów/szumu DMG; kanał fali Game Boy nie jest udostępniany. Ich listy presetów zawierają teraz **24, 24 i 28** presetów odpowiednio. Dodatki obejmują sprzężone/cykliczne szumy Sega, pitch sweepy GB, krótką perkusję, metaliczny szum, drony i wzniesienia. To oryginalne programy, nie wyciągnięte dźwięki z gier. Strona oferuje natywny tryb/duty/szerokość, kontrolę obwiedni/falowania/szumu oraz programową amplitudę ADSR.

#### DX7 FM

Wybierz **Type → FM → DX7 FM**; DX7 jest siódmym wpisem, po Arcade / YM2151. DX7 FM używa sześciu operatorów. Na stronie znajdują się Bank, Preset i Fine ct wraz ze wspólnymi ustawieniami instrumentu. Przeglądanie presetów używa list banków/kategorii. Przytrzymaj EDIT+PLAY, by odsłuchać wybrany dźwięk; zwolnienie zatrzymuje odsłuch. Sam EDIT zatwierdza przy zwolnieniu; OPT anuluje. Załadowanie dźwięku kopiuje kompletną łatkę bez zmiany tabel trackera lub insertów ścieżki.

Dla trwałej biblioteki osobistej umieść pliki `.syx` w **`instruments/banks/dx7/`** obok istniejącej biblioteki instrumentów. Obsługiwane są podfoldery. Otwórz **DX7 → Bank**: każdy plik pojawia się jako nazwany bank, gotowy do przeglądania presetu. Otwórz Bank ponownie po dodaniu lub usunięciu plików; nie wymagana jest konwersja ani krok Load Instrument. Standardowy oryginalny bank DX7/TX7 zawiera **32 głosy**. Plik z czterema wiadomościami banku pojawia się jako cztery numerowane banki (łącznie 128 głosów). Akceptowane są też pojedyncze głosy. Wybrana łatka jest zapisywana razem z instrumentem, więc usunięcie banku źródłowego nie zmienia utworu.

**Load Instrument** akceptuje także oryginalne pojedyncze głosy i banki 32-głosowe DX7/TX7 `.syx`. Wybierz łatkę, aby jej użyć, lub anuluj, by zachować aktualny instrument. Użyj folderu Bank powyżej dla trwałej biblioteki; zapisz utwór lub `.cni`, by zachować zmodyfikowany patch. Nieobsługiwane formaty Yamaha są odrzucane.

DX7 ma limit **16 aktywnych nut w całym utworze**, łącznie z głosami akordów i końcówkami zwolnienia. Po osiągnięciu limitu pierwsze ustępują końcówki zwolnienia, potem ciche nuty trzymane, aby nowe ataki pozostały grywalne. Odsłuch presetu jest niedostępny podczas odtwarzania utworu.

#### Edytor operatorów FM

Genesis FM, Arcade FM, OPL2, OPL3, OPLL i VRC7 mają przycisk **EDIT** po prawej stronie Fine ct. Otwiera tabelę parametrów łatki:

- Genesis FM / Arcade FM
- OPL2 / OPL3: w OPL3 można tu zmienić tryb (2op, 4op, dual).
- OPLL / VRC7: edycja predefiniowanego ROM-u zmienia go w "Custom".

Strzałki lewo/prawo i zwykłe klawisze edycji zmieniają wartości; instrument gra dalej, więc możesz słyszeć zmiany. Pasek stanu opisuje parametr pod kursorem. OPT lub Lewy+Shift wraca do ekranu instrumentu. DX7 i SID nie mają edytora operatorów.

Genesis FM (YM2612) i Arcade FM (YM2151) używają tego samego przeglądarki Bank/Preset, odsłuchu i strojenia co inne silniki FM. Ich natywne obwiednie kontrolują zwolnienie; zmiana głośności tracker albo pitch nie restartuje ich.

#### Fabryczne i USER presety FM

Native chip instruments używają **Bank:** do wyboru ALL, kolekcji włączonej lub USER. **Preset:** wybiera dźwięk z wybranego zbioru. Przytrzymaj EDIT i naciśnij lewo lub prawo na dowolnym wierszu, by przełączać; krótki dotyk EDIT otwiera selektor.

Dołączone kolekcje to paczki ZIP w `instruments/FACTORY/`. Pliki USER lądują w `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse` lub `gb-noise`.
Przeglądaj foldery bezpośrednio lub umieść tam ZIPy kompatybilnych presetów. ZIPy pozostają nienaruszone na dysku. Wszystkie te foldery akceptują kompatybilne pliki `.cni`; CNI to przenośny format instrumentów ChooChoo/ChipNomad, nie standard producenta chipów.
Przeglądarka USER czyta też poniższe formaty źródłowe, także wewnątrz ZIPów:

| Folder USER | Dodatkowe formaty |
| --- | --- |
| `dx7` | Yamaha DX7/TX7 `.syx` pojedyncze głosy i banki 32-głosowe |
| `genesis` | `.tfi`, DefleMask `.dmp`, Furnace `.fui` |
| `arcade` | VOPM `.opm` banki, DefleMask `.dmp`, Furnace `.fui` |
| `opl2`, `opl3` | `.wopl`, `.opli`, `.woplx`, `.sbi`, Furnace `.fui` |
| `opll`, `vrc7` | Furnace `.fui` łatki niestandardowe i stałe selekcje ROM |
| `sega`, `gb-pulse`, `gb-noise` | Furnace `.fui`, DefleMask `.dmp` |
| `sid` | GoatTracker 2 `.ins` (GTI5), Furnace `.fui` |

Bank otwiera kompatybilne głosy. Nieobsługiwane pliki lub funkcje podają powód; niekompatybilne głosy w mieszanym banku są wykluczone, podczas gdy numery źródłowe pozostają stabilne. Ładowanie jest transakcyjne: odrzucony preset nie zmienia aktualnego instrumentu. Presety źródłowe zastępują tabelę trackera instrumentu pustą, z wyjątkiem SysEx DX7, który zachowuje istniejącą tabelę. CNI przywraca zapisaną tabelę. Załadowane łatki należą do utworu i działają po przeniesieniu lub usunięciu źródłowego pakietu.

Banki WOPL mogą zawierać nieużywane sloty bez jawnej flagi pustki. Przeglądarka pomija niez nazwane sloty, których aktywni operatorzy są całkowicie stłumieni i nie mają ataku. Niez nazwane grywalne głosy pozostają dostępne jako `Program N [Bbank]` lub `Drum N [Bbank]`, używając zeroindeksowanych numerów programu i banku źródła. Niez nazwany pojedynczy instrument OPLI używa nazwy pliku. Nazwane ciche łatki są zachowane.

Furnace, DefleMask i GoatTracker importują pliki jako instrumenty, nie całe utwory. Animowane makra źródłowe, sample i ustawienia odtwarzania specyficzne dla utworu mogą nie przejść. Umieść pliki Game Boy w przeznaczonym folderze Pulse lub Noise.

Cyklowanie presetów USER odbywa się przez pliki, foldery, banki i pakiety ZIP w obu kierunkach, zawijając na końcach. Ponowne otwarcie przeglądarki USER zachowuje obecny wybór. ALL zawiera dźwięki USER w Unsorted dla silników kategoryzowanych; USER pozostaje oddzielnym wyborem na najwyższym poziomie. Sega PSG i Game Boy używają płaskich list ALL/Factory i hierarchicznej przeglądarki USER, bez panelu Unsorted. Starsze pliki DX7 w `instruments/banks/dx7/` pozostają dostępne w USER.

Silnik pokazuje tylko swoje kompatybilne kolekcje fabryczne. Fabryczne Presety łączą włączone zestawy tonów OPLL/VRC7 dla wybranego silnika; DX7 łączy oryginały ChooChoo i YSE. Pobierane nazwane kolekcje pozostają oddzielne. Fat Man 2-op jest wymieniony pod OPL2, a Fat Man 4-op pod OPL3. Kompatybilne pliki OPL2 CNI mogą być używane w folderach USER OPL3.

#### Sterowanie natywnych chipów

Podglądy waveform native instrumentów odświeżają się, gdy ich kontrolki są rysowane ponownie. Instrumenty FM mogą nakładać opcjonalną krzywą Amp env; SID zachowuje podgląd waveform bez nakładki FM, nawet podczas przeglądania banków i presetów.

Wszystkie siedem silników FM oferuje **Bright**, **Feedback** i **Amp env**. Bright zmienia się w zakresie od -63 do +63, zero zachowuje łatkę; zmienia poziomy operatorów modulacji przy zachowaniu poziomów nośnych. Efekt słyszalny zależy od algorytmu. Feedback domyślnie pochodzi z **Presetu**, lub nadpisuje natywny feedback wartościami 0–7. Sterowania te nie zmieniają zapisanych bajtów operatorów native.Amp env domyślnie ustawiony jest na **Bypass**. Wybierz **ADSR**, aby dodać atak, decay, sustain, release i kształt wokół oryginalnego brzmienia. A/D/R korzystają ze wspólnego zakresu kwadratowego 0–5 sekund; sustain działa od ciszy do pełnego poziomu. Rodzime obwiednie działają nadal, więc ta obwiednia nie może wydłużyć dźwięku poza jego natywny release. Przeglądanie presetów zachowuje kontrolę nad wzmacniaczem FM i barwą slotu.

Następujące efekty frazy FX pojawiają się również jako obsługiwane cele modulacji i nagrywania ruchu. Wartości w kolumnie FX są szesnastkowe. Ustawienia mają zastosowanie do odtwarzania bez zmiany zapisanego instrumentu. Rodzinne kontrolery GB sweep i obwiedni przełączają się przy następnym wyzwoleniu nuty; duty/width i częstotliwość szumu mogą zmieniać się na żywo.

| Silniki | FX | Kontrola |
|---|---|---|
| Native FM | `OL1`–`OL6` | Absolutne poziomy operatorów: modulator OPLL/VRC7 i OPL `00–3F`; nośnik OPLL/VRC7 `00–0F`; Genesis/Arcade `00–7F`; DX7 `00–63` (0–99). Wyższa wartość oznacza większą moc wyjścia. Pojawiają się tylko obsługiwane operatorzy. |
| OPLL/VRC7/OPL2/OPL3 | `OAR`, `ODR`, `ORR`, `OSL` | Atak, decay, release i tłumienie sustain operatora 1, `00–0F`. |
| OPLL/VRC7/OPL2/OPL3/Genesis/Arcade | `OMU 00–0F` | Mnożnik częstotliwości operatora 1. |
| Genesis/Arcade | `LFR` | Rodzima prędkość LFO: Genesis `00–07`, Arcade `00–FF`. |
| Arcade | `LAD`, `LPD 00–7F` | Oddzielna amplituda LFO i głębokość pitch. |
| Genesis/Arcade | `LAS`, `LEN` | Wrażliwość amplitudy `00–03` i włączenie LFO `00–01`. |
| Genesis/Arcade | `LPS 00–07` | Wrażliwość pitch. |
| SID | `SAT`, `SDE`, `SSU`, `SRL 00–0F` | Rodzinny atak, decay, sustain i release. Wartości czasowe rosną do `0F`; sustain rośnie do pełnego poziomu. |
| SID | `SPR 01–10` | Częstotliwość partnera cichego od 1× do 16×; wpływa na modulację pierścieniową i hard sync. |
| Wszystkie native FM | `FBK 00–07` | Absolutna natywna sprzężenie zwrotne, inicjalizowane z instrumentu. |
| Wszystkie native FM z włączonym Amp env; Sega/GB | `EAT`, `EDC`, `ESU`, `ERL`, `ESH 00–FF` | Atak, decay, sustain, release, kształt. Nie włączają omijania wzmacniacza FM. |
| Sega PSG | `CMD 00–02`, `CNR 00–03` | Ton / biały szum / okresowy szum; trzy stałe szybkości szumu lub szybkość zależna od tonu. |
| GB Pulse | `CMD 00–03` | Rodziny impulsu natywnych. |
| GB Noise | `CMD 00–01`, `CND 00–07`, `CNS 00–0D` | Szerokość szumu, dzielnik zegara i przesunięcie. |
| GB Pulse | `CSP 00–07`, `CSS 00–07`, `CSD 00–01` | Okres sweep, przesunięcie, kierunek w dół. |
| GB Pulse / Noise | `CEI 00–0F`, `CEP 00–07`, `CED 00–01` | Początkowy poziom obwiedni, okres, kierunek narastania. |

Wybór rodzimych FX używa instrumentu w kolumnie `I` wiersza frazy lub aktywnego instrumentu znalezionego poszukując wstecz, gdy `I` jest puste. Popup FX pokazuje krótki opis kontrolera, jego aktualną wartość presetu i prawidłowy zakres poleceń w zwartej ramce. Wybranie innego natywnego efektu zaczyna od tej wartości; ponowne otwarcie tego samego efektu zachowuje jego zmienioną wartość. Wielowierszowy wybór rozwiązuje oddzielnie instrument każdego wiersza. Tabele używają kontekstu instrumentu. Edycje rodzimych wartości zatrzymują się na dopuszczalnych końcach, włączając duty, szum, sweep, ADSR, sprzężenie zwrotne i poziomy operatorów. Wartości są szesnastkowe: na przykład maksymalne DX7 `63` oznacza dziesiętnie 99.

#### Instrumenty SID i FX frazy

Wybierz **CHIP → SID**. Bank oferuje **ChooChoo SID Originals** i **SIDkit Effects**. Przeglądaj, odsłuchuj i zapisuj je jak inne natywne presety chipowe.

Strona instrumentu udostępnia kontrolę nad falą/szerokością impulsu, trybem filtra/cutoffem/rezonansem i natywnym ADSR. **EDIT**, obok nazwy presetu, otwiera **SID EDIT**. Fala i PW mają oddzielne etykiety; Filtr ma rząd trybu, następnie Cutoff i Rezonans; etykiety ADSR leżą obok wartości. Dotknięcie pola wybiera dokładnie ten kontroler. Zachowane są istniejące zakresy parametrów oraz sposób regulacji drobnej i grubej.

SID EDIT korzysta z kompaktowych rzędów edytora FM, kolorów tematu, wartości szesnastkowych i sterowania EDIT+kierunek. OPT lub Left+Shift wraca; EDIT+PLAY odsłuchuje instrument. Strona edytuje bezpośrednio istniejący zapisany patch, więc zmiany zapisują się z instrumentem i projektem.

SID EDIT grupuje falę, ring/sync, ADSR, szerokość impulsu, filtr, ruch, arpeggio, sweep pitch i vibrato. Pasek stanu identyfikuje każdy kontroler i jego zakres. Ring i sync używają po dwa z czterech dostępnych głosów SID, więc akord SID może wymagać mniej nut, gdy którykolwiek z nich jest aktywny.

| FX | Zakres | Działanie |
| --- | --- | --- |
| `SCP` | `00–FF` | Podstawa szerokości impulsu, skalowana do natywnej 12-bitowej szerokości; ruch szerokości impulsu w przepisach pozostaje aktywny. |
| `SCT` | `00–FF` | Podstawa cutoffu, skalowana do natywnej 11-bitowej wartości; ruch filtra w przepisach pozostaje aktywny. |
| `SRN` | `00–0F` | Rodzima rezonans. |
| `SWV` | `00–07` | Triangle, saw, tri+saw, pulse, tri+pulse, saw+pulse, tri+saw+pulse, noise. |
| `SFI` | `00–07` | Bity trybu filtra: 1 low-pass, 2 band-pass, 4 high-pass; 0 omijanie. |
| `SMR` | `00–FF` | Zegar makro przepisu, skalowany od 1 do 200 Hz. |
| `SRG` | `00–01` | Modułacja pierścieniowa trójkątna z partnerem cichym. |
| `SSY` | `00–01` | Synchronizacja oscylatora z partnerem cichym. |

Szerokość impulsu i cutoff obsługują `SLE`. Dyskretne fale, przełączniki, rezonans i sprzężenie FM zachowują użyteczne natywne kroki; szersze wartości bajtów nie tworzyłyby więcej stanów sprzętu. `FBK` wybiera sprzężenie bezpośrednio `00–07`. Poziomy operatorów korzystają z zakresów specyficznych dla silnika, wymienionych powyżej. Jasność i regulacje główne zachowują pełny zakres bajtowy.

#### Fabryczne i USER presety SID i native-chip

Instrumenty natywne chipowe używają **Bank:**, aby wybrać ALL, wbudowane kolekcje lub USER. **Preset:** wybiera brzmienie w obrębie tego wyboru. Przytrzymaj EDIT i naciśnij Left lub Right na dowolnym wierszu, aby przewijać; krótki klik EDIT otwiera wybieracz.

Dołączone kolekcje to pakiety ZIP w `instruments/FACTORY/`. Pliki USER trafiają do `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse` lub `gb-noise`. Przeglądaj foldery bezpośrednio lub umieść ZIP z folderami i kompatybilnymi presetami `.cni`. DX7 także akceptuje obsługiwane głosy `.syx` i banki 32-głosowe, również wewnątrz ZIPów. ZIPy pozostają nienaruszone na dysku. Bezpośrednie ładowanie WOPL/WOPLX nie jest wspierane; używaj kompatybilnych presetów CNI lub konwertuj obsługiwane formaty źródłowe offline.

Przewijanie presetów USER działa przez pliki, foldery, banki i pakiety ZIP w obu kierunkach, zawijając na końcach. Ponowne otwarcie przeglądarki USER podąża za bieżącym wyborem. ALL obejmuje dźwięki USER pod folderem Unsorted dla kategoryzowanych silników; USER pozostaje osobnym wyborem na najwyższym poziomie. Sega PSG i Game Boy używają płaskich list ALL/Factory i hierarchicznej przeglądarki USER, bez panelu Unsorted. Starsze pliki DX7 w `instruments/banks/dx7/` pozostają dostępne w USER.

Silnik pokazuje tylko kompatybilne dołączone kolekcje. Fabryczne Presety łączą dołączone zestawy tonów OPLL/VRC7 dla wybranego silnika; DX7 łączy oryginały ChooChoo i YSE. Pobierane kolekcje pozostają osobne. Fat Man 2-op jest wymieniany pod OPL2, a Fat Man 4-op pod OPL3. Kompatybilne pliki OPL2 CNI mogą być nadal używane w folderach USER OPL3.

## 6. Modulacja i nagrywanie ruchu

Każdy instrument może mieć do 4 slotów modulacji. Instrumenty audio mogą kierować na **Instrument Pan** lub **Track Pan**; oba używają `00` dla lewo, `80` dla środka i `FF` dla prawo.

Dostępne typy modulacji to:

- ADSR - klasyczna obwiednia atkak-decay-sustain-release
- AHD - obwiednia attack-hold-decay
- LFO - oscylator niskoczęstotliwościowy z kilkoma kształtami
- syncLFO (dawniej SLFO) - LFO z ustawieniem ticków na jednostkę i mnożnikiem okresu
- slowLFO - długie cykle z proporcjonalną kontrolką okresu, zaczynające się tuż za zakresem zwykłego LFO
- FLFO - LFO o częstotliwości audio, może sięgać kHz; FLFO jest zasobożerne CPU
- SLIN - liniowa kontrola joysticka
- SRAT - kontrola joysticka bazująca na szybkości
- Sequence - odczytuje sekwencję trackera jako sygnał modulacji: **PITCH** to końcowa sekwencjonowana wysokość, **VOL** końcowa sekwencjonowana głośność, a **TRANCE GATE** niezależny modulator rytmiczny. Motif, Rate i Length wybierają wzór gate; Trigger decyduje, czy działa swobodnie, wyzwala się ponownie z każdą nutą, czy resetuje na granicach frazy lub łańcucha. Zamknięte gate dają zero, otwarte pół poziomu, a Accents rozdziela euklidesowo gate pełnego poziomu wśród aktywnych. Nie używa ani nie zmienia żadnego insertu ścieżki.

Lista destynacji modulacji zależy od typu instrumentu.

Gdy obwiednia ADSR lub AHD kieruje do Volume, staje się obwiednią głośności instrumentu. LFO kierujący do Volume przesuwa aktualną głośność wyjściową zamiast tego.

Ilość modulacji może być dodatnia lub ujemna. Jej zakres zależy od destynacji. Jeśli pełen zakres destynacji to `127` lub mniej, jak w większości parametrów, ilość definiuje zakres absolutny. Szersze destynacje, np. pitch, używają skalowanej wartości.

Źródła Sequence PITCH i VOL mają **Mode**. **Normal** zostawia wartość trackera słyszalną i jednocześnie używa jej jako modulacji. **Bypass** czyni z niej wyłącznie źródło: PITCH wyzwala ponownie na ostatniej wysokości (lub C-4 przy pierwszej nucie), a VOL utrzymuje ostatni słyszalny poziom. Trance Gate jest już tylko źródłem. Jeśli jakikolwiek aktywny slot Sequence ustawia PITCH lub VOL na bypass, bypass wygrywa. Aby uniknąć sprzężenia, PITCH i VOL nie mogą wskazywać Pitch ani Volume.

Wszystkie parametry czasu, włącznie z atakiem, hold, decay i okresem, mierzone są w taktach (ticks).

**syncLFO** to dawny SLFO: ustaw jego ticki i mnożnik dla ruchu zsynchronizowanego z tempem. Istniejące ustawienia SLFO i utwory zachowują ten sam timing.

Użyj **slowLFO** do przemiatań dłuższych, niż pozwala zwykłe LFO. Jego kontrolka Period obejmuje od 256 do 131 072 ticków. Wszystkie LFO oparte na tickach podążają za tempem utworu i dzielą te same kształty, wybór wavetable i tryby wyzwalania. Wybierz Free dla przemiatania trwającego między nutami; Retrig, Phrase i Chain restartują je na odpowiednich granicach.

Kształty LFO:

- Tri - bipolarna fala trójkątna
- Sin - bipolarna fala sinusoidalna
- UniTri - unipolarna fala trójkątna
- UniSin - unipolarna fala sinusoidalna
- RampDn - liniowy spadek (fala piłokształtna)
- RampUp - liniowy wzrost (fala piłokształtna)
- ExpDn - wykładniczy spadek (wykładnicza fala piłokształtna)
- ExpUp - wykładniczy wzrost (wykładnicza fala piłokształtna)
- Square - fala prostokątna
- Random - próbkowany losowy sygnał podtrzymujący
- Wavetable - odczytuje jedną z 32-stopniowych fal AY projektu. Wartości `00` do `0F` mapują od `-100%` do `+100%` bez interpolacji.

Typy wyzwalania LFO:

- Free - restartuje LFO tylko gdy zmienia się instrument
- Retrig - restartuje LFO przy każdej nucie
- Phrase - restartuje LFO, syncLFO i slowLFO przy wejściu w nową frazę
- Chain - restartuje LFO, syncLFO i slowLFO przy wejściu w nowy łańcuch
- Hold - zatrzymuje po 1 cyklu i utrzymuje ostatnią wartość
- Once - zatrzymuje po 1 cyklu i wraca do zera

### Nagrywanie ruchu

- **STICK LIVE** (`L1` domyślnie) nakłada modulację joysticka bez zmiany frazy. W **Settings > Stick live mode**, `HOLD` (domyślnie) włącza modulację podczas trzymania; `TOGGLE` włącza przy jednym naciśnięciu i wyłącza przy kolejnym; `FREE` zawsze włączony. Zwolnienie przycisku w `TOGGLE` pozostawia funkcję włączoną.
- Podczas odtwarzania, przytrzymaj **MOTION RECORD** (`L2`), by nałożyć modulację joysticka i nagrać zmienione destynacje jako absolutne wartości FX.
- Przytrzymaj **MOTION ERASE** (`R2`), by usunąć pasujące FX z aktualnego wiersza.

Symbol `~` w prawym dolnym rogu oznacza aktywność Stick live. Record (`*`, lub `!` przy przepełnieniu) i Erase (`x`) mają pierwszeństwo i pozostają chwilowe w obu trybach. Oba włączają modulację joysticka podczas trzymania; zwolnienie ich zostawia osobno włączony live toggle.

Tryb jest zapisywany z ustawieniami aplikacji, ale live toggle zawsze startuje wyłączony przy uruchomieniu. Zmiana trybu czyści toggle: `HOLD` podąża za przyciskiem trzymanym na Stick live, `TOGGLE` czeka na nowe naciśnięcie, `FREE` jest aktywny natychmiast.

Nagrywanie ruchu zapisuje track FX we frazie aktualnie odtwarzanej. Najpierw aktualizuje pasujące FX, potem używa pustych slotów od prawej do lewej. Nigdy nie nadpisuje innego FX. Jeśli wszystkie 3 sloty są zajęte, ruch nie zostanie nagrany w tym kroku. `!` w prawym dolnym rogu oznacza, że zmieniono więcej destynacji, niż mieści się w 3 kolumnach FX.

Nagrywanie ruchu obsługuje destynacje Braids, Plaits, Sampler, 2xSCWF, BYOWTBL i Bogie, włącznie z aktywnymi makrami Bogie i kontrolerami filtra tam, gdzie dotyczy.

## 7. Track insert effects

Każdy z ośmiu torów ma dwa szeregowe sloty, **TF1 → TF2**. Z Instrument, użyj zwykłego gestu Shift+Up, aby otworzyć MOD, potem ponownie Shift+Up na **F: Insert FX**. Shift+Down wraca do MOD. OPT+Left/Right wybiera tor (1–8), bez zmiany wyboru instrumentu. Nagłówek modułu otwiera istniejący wybieracz; bypass zachowuje moduł, ustawione wartości i automatyzację. Parametry to wartości bajtowe: lewa kolumna 1–4, prawa 5–8. Tekst kontekstowy pokazuje pełne nazwy i zdekonwertowane wartości. Starsze projekty ładują się z oboma slotami WYŁ.

Wyświetlacz nawigacji zachowuje pełne kolumny F–M–I–P widoczne przez cały czas w ekranach instrumentów, włączając Insert FX i pule instrumentów, i podświetla aktualny ekran.| Moduł | Parametry w kolejności |
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

Kontrolki ciągłe używają zakresu 00–FF, w tym poprawnego zera i maksymalnego FF, z wyjątkiem FORMANT VOW (00–7F). Kontrolki bipolarne mają dokładny punkt neutralny na 80. Tryby distortion to 00 Density, 01 Drive, 02 Spiral, 03 Mojo, 04 Dyno. Kompresor ma ratio 00–07 na poziomach 1.5, 2, 3, 4, 6, 8, 16, 20:1; Compressor Detect 00–02 nasłuchuje własnego wejścia stereo, lewego lub prawego; 03–0A nasłuchuje zamiast tego ścieżek 1–8. Filtr detektora 80 jest neutralny, poniżej to low-pass, powyżej high-pass. Źródło ścieżki jest brane po jej volume, pan i Tilt EQ, ale przed jej insertami; wyciszone lub nieobecne źródło wnosi ciszę. Każdy slot może zawierać dowolny moduł; powtarzające się moduły działają niezależnie. Chorus i Phaser zaczynają z słyszalną, ale kontrolowaną modulacją. Zmniejsz Mix, Depth lub Feedback aby uzyskać prawie przezroczysty efekt; wysokie ustawienia Depth, Feedback i Mix dają celowo ekstremalne efekty. Flanger i Rotary używają delikatnej domyślnej modulacji. Rotary Speed
używa pełnego zakresu **00-FF**: **00 = 0.01 Hz** (jeden obrót na 100 sekund),
**FF = 25 Hz** (maksimum Chaos z poprzednich wersji). Ekspansja wykładnicza daje lepszą kontrolę nad wolnym ruchem LFO. Nowe inserty Rotary domyślnie ustawiają **8F**, około 0.8 Hz. Zastępuje to cztery dyskretne wartości prędkości; istniejące ustawienia Rotary i automatyka prędkości F11/F21 wymagają dopasowania do nowej skali (stare Slow/Fast/Hyper/Chaos odpowiadają mniej więcej **8F/CB/E7/FF**). Inne parametry pozostają bez zmian. Głębia bitcrushera wynosi od 4 do 16 bitów, a redukcja prędkości od 1x do 32x. Tryby Destruction to Fold, Clip i Crush. Saturation i distortion zawierają kontrolę poziomu wyjścia lub mixu dla zbalansowania względem sygnału nieprzetworzonego.
Trance Gate to tempo-synchronizowany sekwenser amplitudy do padów, leadów i uderzeń. Wartości Pattern 00–3F wybierają nazwy motywów muzycznych; 00–1F to główne groove’y, 20–3F to wariacje eksperymentalne. Rate oferuje wartości proste, triolowe i kropkowane od 1/2 do 1/32. Length skraca niezależnie obrócony motyw 16-krokowy do 1–16 kroków. Każde nutowe note-on na ścieżce restartuje sekwencję; note-off zostawia ją działającą przez release. Forward, Reverse i Ping-Pong definiują kierunek, Chaos używa powtarzalnej mutacji zachowującej gęstość co cztery pętle. Na insercie master podąża za czasem transportu bez retriggerów nut. Gate ustawia szerokość każdego otwartego kroku, Smooth zaokrągla jego krawędzie zachowując bezpieczne przed kliknięciem minimum fade, a Mix crossfaduje między suchym a gated sygnałem.

**Beads** wykorzystuje oryginalny granularny DSP Mutable Instruments jako inserty stereofoniczne. Wybierz Beads w slocie insertu ścieżki. Jego osiem kontrolek korzysta też z istniejących komend F11–F18 / F21–F28 oraz insert MOD destinations; nie potrzeba nowych ID FX ani formatu projektu. Ziarna pracują ciągle bez dodatkowych zdarzeń.

- **Time:** pozycja w czterosekundowym buforze nagrania.
- **Size:** długie ziarna odwrócone na 00, najkrótsze ziarna około 55, następnie stopniowo dłuższe ziarna forward. FF wprowadza tryb delay oryginału po przejściu.
- **Density:** regularne ziarna poniżej środka, losowe powyżej; w strefie martwej środka ziarna się nie pojawiają. Oddalenie od środka zwiększa tempo.
- **Pitch:** −24 do +24 półtonów; 80 to dokładna unisono.
- **Scatter:** 00 to brak losowości parametrów. Zwiększenie włącza wewnętrzną jednorodną losowość Time w pełnym zakresie i losowość Size na pół zakresu, niezależnie dla każdego ziarna. Pitch i Shape pozostają ustawione.
- **Feedback:** zwraca przetworzone ziarna do ścieżki nagrania.
- **Shape/Freeze:** 00–7F nagrywa, 80–FF zamraża. W obu zakresach niskie 7 bitów zmienia tę samą obwolutę od prostokątnej, przez decay i dzwon do odwróconej. Dodaj/odejmij 80 aby przełączać freeze bez zmiany Shape: 40 i C0 mają tę samą obwolutę. Podpowiedź wartości pokazuje pozycję obwoluty oraz status Recording/Frozen. Przytrzymaj EDIT z Up/Down aby zmieniać o 10 hex(16); Left/Right zmienia o 1. Ten skompaktowany kontroler jest dyskretny, więc przejście nie zmienia bitu freeze; modulacja całego bajtu może celowo przechodzić między stanami.
- **Mix:** 00 to dokładne dry, FF to pełne przetworzenie. Zmiany są wygładzane.

Freeze zatrzymuje audio aktualnie przechwycone przez efekt; wybór innego modułu lub reset Beads go czyści. Sendy reverbu na ścieżce pozostają dostępne.

### Pompowanie miksu lub innej ścieżki przez kick

Dla pompującego mastera umieść kick na ścieżce 1, wybierz Compressor w MASTER FX, ustaw Detect na 03 (Track 1), Ratio na 04 (6:1), Attack w okolice 60 (ok. 1,6 ms), Release w okolice C0 (ok. 115 ms), Makeup na 00 i Mix na FF. Obniżaj Threshold, aż pozostałe dźwięki będą zapadać się przy każdym kicku, a potem dostrój Release, by wracały przed następnym. Bezpośredni dźwięk kicka zachowuje własne inserty ścieżki, ale omija dwa sloty insertów mastera; jego powroty reverb i delay i tak pompują.

Aby ściszać tylko bas, pad lub syntezator, umieść jego Compressor na tej ścieżce i wybierz ścieżkę kicka przez Detect. Źródłowy kick pozostaje słyszalny, a jego głośność nie jest zmieniana przez kompresor tej ścieżki. Kicka i basu połączonych na jednej ścieżce nie da się rozdzielić tym routingiem.

### FORMANT

Wybierz **FORMANT** w TF1 lub TF2, by kształtować dźwięk w tony przypominające samogłoski. Działa najlepiej na długim źródle bogatym w harmoniczne, jak fala piłokształtna.

Ustaw MODE na `00` VOW i użyj **VOW** do przemiatania od A (`00`) do U (`7F`) przez E, I i O. Ustaw MODE na `01` CUSTOM, aby samodzielnie dostroić **F1** i **F2**. **RES** zwęża lub poszerza charakter samogłoskowy; **MIX** miesza efekt z sygnałem oryginalnym. Użyj MOD → INSERT FX, by poruszać aktywnymi kontrolkami. MODE nie jest destynacją MOD.

### Lane FX dla insertów

Komendy insertów używają **F11–F18** dla TF1 i **F21–F28** dla TF2. Wybór FX na rolce oraz popup Phrase/Table pokazują tylko parametry używane przez ustawione inserty wybranej ścieżki. Popup grupuje je jako **TF1: nazwa efektu** i **TF2: nazwa efektu**; sloty OFF są ukryte. Bypassowane inserty pozostają edytowalne.
W popupie Phrase i Table FX opis podąża za wtyczką inserta ścieżki: pokazuje moduł, nazwę parametru, zdekodowane granice zakresu i zapisaną wartość bazową. Istniejące komendy songa zachowują się jeśli zmiana modułu sprawi, że staną się nieużywane; otwarcie wybieraka zaczyna od dostępnego kontrolera.
Podczas edycji wartości dolna podpowiedź pokazuje dekodowaną wartość i jednostki.
Każda kolumna Phrase lub Table FX może adresować dowolny slot; siatka Phrase utrzymuje swoje trzy kolumny. Te absolutne wartości w czasie działania pozostają między nutami, instrumentami, frazami, chainami i zwykłymi pętlami. Świeże odtwarzanie/twardy stop resetuje je. Edycja wartości bazowej na stronie F czyści jedynie nadpisanie tego parametru. MOD działa na efektywną wartość bazową/automatyzacji bez zapisywania przesunięć. Wybranie nowego modułu przywraca domyślne i czyści nadpisania tego slotu. Bypass zachowuje istniejącą automatykę, zawieszając przetwarzanie po krótkim fade.

**Adresy odnoszą się do slotów ścieżki/pozycji parametrów, nie nazw efektów.** Frazę powtórzoną na innej ścieżce wykorzystuje moduły tej ścieżki. Zmiana modułów może zmienić znaczenie komend; nie są one przepisywane. OFF/nieużywane adresy to bezpieczne no-op i pozostają w danych Phrase/Table.

Wybierak MOD instrumentu dodaje kategorię Insert FX. Źródła każdego instrumentu kontrolują inserty na ścieżce, która go odtwarza, w tym połączony sygnał chordu. Ten sam instrument na dwóch ścieżkach wpływa na niezależne chainy. Stosowane są istniejące zasady timingów, wartości sygnowane, reguły łączenia źródeł, tryby stick i nagrywanie ruchu. Kontrolki dyskretne są kwantyzowane i ograniczone do poprawnych opcji. Nie ma nowej banku modulacji należącej do ścieżki.

Zacznij od kilku insertów, monitoruj przesterowania lub trzaski, i wyłącz efekty, które nie pomagają utworowi. Ustawienia Drive i gain mogą przesterować miks, więc korzystaj z wskaźnika clipu w Mixerze przy ich dostosowywaniu.


## 8. Tabele

Tabele to podstawowe narzędzie do kreowania dźwięku w trackerach. W terminach Vortex Trackera łączą instrumenty i ornamenty, ale robią dużo więcej. Jeśli znasz LSDj lub NerdSEQ, pomysł powinien być już znajomy.

Kolumna Pitch akceptuje względne (`~`) lub bezwzględne (`=`) wartości wysokości dźwięku w półtonach. Kolumna Volume to `00`-`0F`, działa na każdy silnik ponad jego obwiednią i może służyć jako gate: na przemian `0F` i `00` a potem loop z `HOP 00`. 4 lane FX w większości pokrywają się z lane’ami w frazie, choć kilka FX zachowuje się inaczej w tabelach.

Umieszczenie FX `TIC` na ostatnim wierszu tabeli ustala prędkość kolumny i zastępuje domyślną prędkość tabeli instrumentu. Każda kolumna FX może działać z inną prędkością ticka. Kolumny Pitch i Volume podążają za prędkością 1. kolumny FX.

Tabela 16-wierszowa może wyglądać krótko w porównaniu z długim instrumentem Vortex Trackera, ale FX `HOP` pozwala tworzyć pętle warunkowe, takie jak „powtórz te wiersze 5 razy” oraz pętle zagnieżdżone. Z pętlami i niezależnymi prędkościami kolumn FX te 16 wierszy może dać naprawdę długi efekt.

Można kopiować/wklejać lane FX między frazami i tabelami ;)

### Tryb Retrigger

Pole `Retrig` na górze ekranu Table wybiera, kiedy już aktywna tabela się restartuje. `Inst` to domyślny i kompatybilny tryb: wiersz z instrumentem we frazie wybiera domyślną dla tego instrumentu tabelę i startuje ją na wierszu `00`. `Phrase` restartuje aktywną tabelę przy wejściu w frazę, `Chain` przy wejściu w chain, a `Free` pozostawia ją działającą aż do zatrzymania ścieżki lub zastąpienia przez explicit komendę.

Tryb strukturalny nigdy nie sięga wstecz w czasie. Jeśli na ścieżce nie ma aktywnej tabeli instrumentu, pierwszy wiersz frazy wprowadzający instrument aktywuje jej tabelę dokładnie na tym wierszu, nawet jeśli to rząd `4`, `8` lub `F`. Późniejsze numery instrumentów nie zastępują aktywnej tabeli w trybach `Phrase`, `Chain` lub `Free`. To czyni tabele użytecznymi jako automatyzację LFO bez cichego symulowania wcześniejszych kroków frazy.

`TBL`, `TBX` i `RET` to zawsze akcje jawne: wybierają lub restartują od razu, niezależnie od trybu. Na granicy frazy lub chain restart następuje przed czytaniem nowego wiersza, więc `TBL` lub `TBX` na tym wierszu ma ostatnie słowo. FX tabeli wiersza `00` wykonywane są więc ponownie normalnie; skumulowane FX zachowują normalne narastanie.

Tabele `00-7F` są zarezerwowane dla domyślnych tabel instrumentów. Tabele `80-FE` przeznaczone są dla tabel pomocniczych uruchamianych efektem `TBX`. Dolny zakres działa też dla tabel pomocniczych, ale ich użycie w ten sposób może powodować nieoczekiwane konflikty i zamieszanie.

### Kontrolki

- **EDIT + [UP/DOWN]** na kolumnie nazwy FX: otwiera ekran wyboru FX
- **OPT + [KIERUNEK]**: przejście między tabelami

## 9. Instrument Pool i AY Wavetable

### Instrument Pool

Instrument Pool pokazuje wszystkie instrumenty w projekcie i pozwala je przestawiać. Podczas odtwarzania wskazuje również, które instrumenty są aktualnie aktywne.

#### Kontrolki

- **EDIT:** edytuj instrument i przejdź do ekranu Instrument
- **SHIFT + OPT:** kopiuj instrument
- **SHIFT + EDIT:** wklej instrument
- **EDIT + [UP/DOWN]:** zmień kolejność instrumentów
- **EDIT + PLAY:** podgląd instrumentu

### AY Wavetable

Wavetable to typ software’owego oscylatora AY Plus. Projekt może mieć do 256 wavetable, każdy z 32 krokami. Wszystkie instrumenty wavetable dzielą ten sam zestaw fal. Tu je edytujesz.

Poziomy głośności AY są nieliniowe, więc w widoku Standard fala jest rysowana tak, by odpowiadać faktycznym poziomom wyjścia. Podwójne naciśnięcie **OPT** przełącza między Widokiem Standardowym a zapamiętanym **Widokiem LFO**, korzystając z tego samego skrótu co Tryb Song/Live: Widok LFO rysuje surowe kroki `0`–`15` liniowo, ze środkiem między `7` a `8` jako punktem zerowym modulacji.

Ekran ma 2 logiczne rzędy. Górny zawiera przyciski **Load** i **Save**, drugi edytor wavetable.

#### Kontrolki

- **OPT + [LEFT/RIGHT]:** przejście po liście wavetable
- **OPT + [UP/DOWN]:** skok w liście o 16 fal
- **SHIFT + OPT:** kopiuj wavetable
- **SHIFT + EDIT:** wklej wavetable
- **OPT + PLAY:** przełącz Widok Standardowy i LFO

## 10. Tracker FX

Tracker FX wymuszają wartości sekwencera lub silnika dźwięku na pojedynczych krokach. Inne grooveboxy często nazywają to „parametric locks”. Każdy wiersz Phrase ma 3 sloty FX.### Sequencer FX

| FX | Wartość | Szczegółowe działanie |
|---|---|---|
| `ARP` | `XY` | Arpeggiuje nutę bazową, `+X` i `+Y` stopni. `37` tworzy wzór akordu molowego. |
| `ARC` | `XY` | `X` wybiera kierunek arpeggio lub tryb zakresu; `Y` to jego prędkość w tickach. |
| `PVB` | `XY` | Wibrato wysokości dźwięku: `X` to prędkość, `Y` głębokość. W trybie Linear głębokość w skokach `10-cent`. |
| `PBN` | signed `XX` | Dodaje `XX` jednostek wysokości dźwięku na każdy wiersz frazy/tabeli; `FF` oznacza `-1`. Użyj `00` by zatrzymać. |
| `PSL` | `XX` ticków | Slide z poprzedniej wysokości do nowej nuty w `XX` tickach. |
| `PIT` | signed `XX` | Skumulowany względny offset w krokach tabeli wysokości dźwięku. |
| `FIN` | signed `XX` | Skumulowany precyzyjny offset w centach dla Linear pitch, w jednostkach okresu inaczej. |
| `PRD` | signed `XX` | Skumulowany względny offset okresu oscylatora. |
| `VOL` | signed `XX` | Skumulowany względny offset głośności (`FF` to -1, a nie pełna głośność). |
| `VSL` | signed `XX` | Dodaje `XX` do głośności na każdy wiersz frazy/tabeli. `00` zatrzymuje. |
| `PAN` | `00-FF` | Absolutna panorama instrumentu: lewo, centrum (`80`), prawo. |
| `TPN` | `00-FF` | Absolutna panorama ścieżki miksera: lewo, centrum (`80`), prawo. |
| `RET` | `XY` | Retrigger co `Y` ticków; `X` zmienia głośność. `Y=0` zatrzymuje retrigger. |
| `DEL` | `XX` ticków | Opóźnia włączenie nuty. Opóźnienie dłuższe niż krok groove pomija nutę. |
| `OFF` | `XX` ticków | Wysyła note-off po `XX` tickach i uruchamia etap release ADSR. |
| `KIL` | `XX` ticków | Twarde wyłączenie głosu po `XX` tickach bez ADSR release. |
| `TIC` | `XX` ticków | Ustawia ticki na wiersz tabeli. W tabeli zmienia prędkość tej kolumny FX. |
| `TBL` | `00-FE`, `FF` wyłączone | Natychmiast zastępuje i restartuje tabelę instrumentu; `FF` zatrzymuje. |
| `TBX` | `00-FE`, `FF` wyłączone | Natychmiast uruchamia lub zastępuje pomocniczą tabelę; `FF` zatrzymuje. |
| `THO` | wiersz `XX` | Przeskakuje wszystkie kolumny tabeli instrumentu do wiersza `XX`. |
| `TXH` | wiersz `XX` | Przeskakuje wszystkie kolumny tabeli pomocniczej do wiersza `XX`; nie używane z poziomu tabeli. |
| `GRV` | groove `XX` | Wybiera groove dla bieżącej ścieżki. |
| `GGR` | groove `XX` | Wybiera groove dla wszystkich ścieżek. |
| `HOP` | `XY` | Skacze do wiersza `Y`, `X` razy; `X=0` oznacza zapętlenie w nieskończoność. HOP w tabeli działa na własną kolumnę. |
| `SNG` | signed `XX` | Przesuwa odtwarzanie o `XX` wierszy piosenki. |
| `PRO` | `00-64` | Ustawia absolutne prawdopodobieństwo wyzwolenia od `0%` do `100%`. |
| `MOD` | `AB` | Wyzwala na przebiegu `A` cyklu `B` przebiegów. |
| `RNN` | `MX` | Gra losową nutę. `M` wybiera tryb: `0` zawsze losowo, `1` losowo przy każdym PLAY, `2` wybiera i utrzymuje, aż wartość się zmieni. `X` wybiera zakres: `0` dowolna nuta, `1` +3 st, `2` +6 st, `3` +12 st, `4` -3 st, `5` -6 st, `6` -12 st, `7` +/-3 st, `8` +/-6 st, `9` +/-12 st. Losowe wybory podążają za skalą projektu, gdy silnik skali jest zastosowany do ścieżki. |
| `SPD` | signed `XX` | Wybiera trwały współczynnik prędkości zegara na ścieżkę; patrz tabela prędkości poniżej. |
| `SLE` | `00-FF` ticków | Ustawia trwały czas glide na ścieżkę dla ciągłych FX silnika. `00` oznacza natychmiastowe. Ustawienie resetuje się do `00` po zatrzymaniu odtwarzania. |

### Audio FX sends

| FX | Wartość | Szczegółowe działanie |
|---|---|---|
| `RSN` | `00-FF` | Ustawia wysyłkę reverbu na tej ścieżce do następnego wyzwolenia nuty. |
| `DSN` | `00-FF` | Ustawia wysyłkę delay na tej ścieżce do następnego wyzwolenia nuty. |

### ADSR / Trigger FX

| FX | Wartość | Szczegółowe działanie |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | Nadpisuje attack, decay, sustain, release lub kształt obwiedni. |
| `TDC` / `TCL` | `00-FF` | Nadpisuje tryb Trigger Decay lub Color na Plaits i Plaits-Alt. |

#### Warunki

Zainspirowane szwedzkimi "warunkami trig", tracker wspiera prawdopodobieństwa wyzwolenia i warunki modulo.

| FX | Wartość | Znaczenie |
|---|---|---|
| `PRO` | `00-64` | Absolutne prawdopodobieństwo wyzwolenia od `0%` do `100%` |
| `MOD` | `AB` | Wyzwala na iteracji `A` z `B`; np. `12`, `22`, `14`, `34` |

Licznik `MOD` jest lokalny dla ścieżki i frazy. Niepoprawne kombinacje, takie jak `A` większe niż `B`, nie wyzwalają.

`MOD34` wyzwala na 3. przebiegu z cyklu 4-krotnego. `MOD1F` wyzwala na 1. przebiegu z cyklu 16-krotnego.

#### Prędkość odtwarzania

Podobnie jak NerdSEQ, ChooChooTracker wspiera niezależną prędkość odtwarzania dla każdej ścieżki.

`SPD` odczytuje wartość jako signed byte. `00` jest normalną prędkością, wartości dodatnie przyspieszają ścieżkę, ujemne spowalniają. Ustawienie pozostaje aktywne do kolejnej zmiany `SPD`.

| Wartość | Prędkość |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

Starsze projekty bez signed `SPD` zachowują oryginalny zakres `00-10`, więc odtwarzają się jak zapisano.

Bardzo wysokie mnożniki mogą przekroczyć rozdzielczość tick scheduler'a trackera i nie były testowane sprzętowo.

### Scale & chords FX

#### Automatyka skali

`SCL XY` zmienia globalną skalę odtwarzania i tonikę bez zmiany nut w frazach. `X` wybiera skalę (`0` Chromatyczna, `1` Durowa, `2` Molowa, do `C` Własna); `Y` wybiera tonikę (`0` C do `B` B). Komenda działa tylko w FX frazy, nie w tabelach. Jeśli kilka ścieżek da `SCL` jednocześnie, wygrywa ścieżka o najniższym numerze.

`SCL` należy do trybu Quantizer. Z aktywowanym **Note Lock** komenda nie jest oferowana w liście FX, a każde `SCL` już zapisane w frazach jest ignorowane.

#### Akordy

`CRD XY` zmienia nutę na własnym wierszu frazy w akord. `Y` wybiera typ: `0` Durowy, `1` Molowy, `2` Zmniejszony, `3` Zwiększony, `4` Sus2, `5` Sus4, `6` Power, `7` Maj7, `8` Min7, `9` Dom7, `A` Min7b5, `B` Dim7, `C` Add9, `D` MinAdd9, `E` Maj9, `F` Min9. `X` wybiera jeden z 16 głosów: `0-3` cykl zamkniętych inwersji rosnących; `4-7` dodaje drop-2; `8-B` dodaje drop-3; `C-F` łączy oba dropy. Cykl przechodzi przez dodatkową oktawę dla triad i power chordów, więc każda wartość jest unikalna; większe wartości dają bardziej rozwarte, mniej konwencjonalne akordy. Na przykład `CRD 4Y` to drop-2 w położeniu toniki, `CRD DY` to pierwsza inwersja z obiema dropami. Przy dnach lub szczytach tabeli wysokości głosy są dopasowywane do zakresu. Komenda nie jest trwała: wymaga zapisu na każdym wierszu z akordem. Każda generowana nuta jest ilościowo dopasowywana niezależnie, gdy skala jest aktywna, w trybach Quantizer i Note Lock. Dostępne tylko dla silników software’owych; instrumenty AY ignorują ją i pozostają monofoniczne.

### Modulation FX

FX frazy i tabeli mogą zmieniać slot modulacji bez edycji instrumentu:

| Wzorzec FX | Znaczenie |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Względny offset Amount dla slotów modulacji 1-4 |
| `M11`, `M12`, `M13`, `M14` | Względne offsety P1-P4 dla slotu 1 |
| `M21`, `M22`, `M23`, `M24` | Względne offsety P1-P4 dla slotu 2 |
| `M31`, `M32`, `M33`, `M34` | Względne offsety P1-P4 dla slotu 3 |
| `M41`, `M42`, `M43`, `M44` | Względne offsety P1-P4 dla slotu 4 |

Wartość traktowana jest jako signed `8-bit` zmiana względna (`01` dodaje `1`, `FF` odejmuje `1`). Powtarzane komendy kumulują się. Wartości efektywne są ograniczane do prawidłowego zakresu.

`P5` nie jest expose jako FX frazy lub tabeli. Jest dostępny jako cel modulacji `M1–M4 Wavetable`, więc jeden modulator może skanować indeks AYWavetable innego.

| Typ modulacji | P1 | P2 | P3 | P4 | P5 gdy kształt to Wavetable |
|---|---|---|---|---|---|
| ADSR | Attack | Decay | Sustain | Release | Nie używane |
| AHD | Attack | Hold | Decay | Nie używane | Nie używane |
| LFO | Shape | Tryb wyzwalania | Period | Nie używane | Indeks wavetable AY |
| syncLFO | Shape | Tryb wyzwalania | Ticki | Mnożnik | Indeks wavetable AY |
| slowLFO | Shape | Tryb wyzwalania | Krok okresu (00–FF) | - | Indeks wavetable AY |
| FLFO | Shape | Tryb wyzwalania | Częstotliwość (`1 Hz` do `20 kHz`) | Nie używane | Indeks wavetable AY |

### Braids FX

| FX | Wartość | Znaczenie |
|---|---|---|
| `BMD` | `00-2E` | Model Braids |
| `BTM` | `00-FF` | Absolutna znormalizowana barwa (Timbre) |
| `BCL` | `00-FF` | Absolutna znormalizowana barwa (Color) |
| `BCF` | `00-FF` | Wykładnicze cięcie filtra, `20 Hz` do `20 kHz` |
| `BRS` | `00-FF` | Rezonans filtra wykładniczy |

### aChChid FX

| FX | Wartość | Znaczenie |
|---|---|---|
| `ASL` | `00-FF` | Slide do nuty bez retriggera. `00` to `60 ms`; wyższe wartości wydłużają glide. |
| `ADC` | `00-FF` | Nadpisanie decay: `200 ms` do `2 s`. |
| `AAC` | `00-FF` | Ilość akcentu: brak do maksimum. |
| `ATM` | `00-FF` | Nadpisanie barwy Braids; aktywne tylko w trybie fali Braids. |
| `ACL` | `00-FF` | Nadpisanie koloru Braids; aktywne tylko w trybie fali Braids. |
| `ACF` | `00-FF` | Wykładnicze cięcie filtra 303, `20 Hz` do `20 kHz`. |
| `ARS` | `00-FF` | Rezonans filtra 303, od braku do maksimum. |
| `AEM` | `00-FF` | Modulation envelope filtra 303, od braku do maksimum. |

### Bogie FX

| FX | Wartość | Znaczenie |
|---|---|---|
| `DMD` | `00-0B` | Wybiera model Bogie do następnego wyzwolenia. |
| `DDC`, `DTO`, `DSW`, `DNO`, `DFM`, `DDR` | `00-FF` | Nadpisuje decay, tone, sweep, noise, FM lub drive do następnego wyzwolenia. Wszystkie sześć makr jest dostępne w każdym modelu. |
| `DCF` | `00-FF` | Cięcie filtra, mapowane logarytmicznie. |
| `DRS` | `00-FF` | Rezonans filtra. |

### Sintered FX

| FX | Wartość | Znaczenie |
|---|---|---|
| `SMD` | `00-05` | Wybiera model Sintered do następnego wyzwolenia. |
| `SDC`, `SMP`, `SMA`, `SMB`, `SMO`, `SMC` | `00-FF` | Nadpisuje decay, mod, dwa pierwsze makra modelu, motion lub ostatnie makro modelu do następnego wyzwolenia. |
| `SCF`, `SRS` | `00-FF` | Cięcie filtra i rezonans. |

### MIDI Out FX

| FX | Wartość | Znaczenie |
|---|---|---|
| `MC1`-`MC4` | `00-FF` | Wysyła MIDI CC przeskalowany do `0-127`. Numer CC przypisany w instrumencie (`MC1 number`–`MC4 number` na ekranie instrumentu), nie przez FX; slot bez numeru jest nieaktywny. |

### Plaits FX

| FX | Wartość | Znaczenie |
|---|---|---|
| `PMD` | `00-17` | Silnik Plaits |
| `PHA` | `00-FF` | Absolutna znormalizowana ilość harmonicznych |
| `PTM` | `00-FF` | Absolutna znormalizowana barwa (Timbre) |
| `PMO` | `00-FF` | Absolutna znormalizowana morph |
| `PAX` | `00-FF` | Mieszanka Main/Aux: `00` Main, `FF` Aux |
| `PCF` | `00-FF` | Wykładnicze cięcie filtra, `20 Hz` do `20 kHz` |
| `PRS` | `00-FF` | Rezonans filtra wykładniczy |

### Sample FX

| FX | Wartość | Znaczenie |
|---|---|---|
| `SPL` | `00-03` | Tryb odtwarzania: `00` prosto, `01` od tyłu, `02` loop, `03` ping-pong |
| `SLI` | `00-FF` | Odtwarza numerowaną slicę (`01` = pierwsza slice) niezależnie od nuty; `00` zachowuje normalną mapę nut. Wymaga trybu slice w instrumencie. |
| `SPT` | signed `XX` | Transpozycja sample w półtonach |
| `SST` | `00-FF` | Znormalizowany start odtwarzania |
| `STA` | `00-FF` | Alias `SST` (znormalizowany start) |
| `SEN` | `00-FF` | Znormalizowany koniec odtwarzania |
| `SVL` | `00-FF` | Absolutna głośność sample |
| `SCF` | `00-FF` | Wykładnicze cięcie filtra, `20 Hz` do `20 kHz` |
| `SRS` | `00-FF` | Rezonans filtra wykładniczy |
| `SSP` | `00-FF` | Prędkość sample, mapowana od `0%` do `500%` |
| `RNS` | `M0` | Gra losowy slice pociętego sample zamiast przypisanego do nuty. `M` wybiera tryb: `0` zawsze losowo, `1` losowo przy każdym PLAY, `2` wybiera i utrzymuje, aż wartość się zmieni. Wymaga włączonego trybu slice w instrumencie; dolny nibble nie jest używany. |

### AY FX shared by AY instruments

| FX | Wartość | Szczegółowe działanie |
|---|---|---|
| `AYM` | `XY` | `X` to kształt obwiedni; `Y` wybiera off/ton/szum/ton+szum (`0/1/2/3`). |
| `NOI` | signed `XX` | Skumulowany względny offset okresu szumu. |
| `NOA` | `00-1F`, `FF` | Absolutny okres szumu; `FF` daje priorytet sześciokątowi wcześniejszych ścieżek. |
| `ERT` | dowolne | Retriggeruje obecny kształt sprzętowej obwiedni. |
| `EAU` | `XY` | Automatyczne proporcje obwiedni `X:Y`; `X=0` wyłącza. |

### AY Classic-only FX

| FX | Wartość | Szczegółowe działanie |
|---|---|---|
| `EVB` | `XY` | Wibrato okresu obwiedni: `X` prędkość, `Y` głębokość. |
| `EBN` | signed `XX` | Dodaje `XX` do okresu obwiedni na każdy wiersz frazy/tabeli. |
| `ESL` | `XX` ticków | Slide z poprzedniego okresu obwiedni do nowego. |
| `ENT` | nuta `XX` | Ustawia okres obwiedni według nuty z tabeli wysokości widocznej w UI. |
| `EPT` | signed `XX` | Skumulowany względny offset okresu obwiedni. |
| `EPL` | byte `XX` | Ustawia dolny bajt okresu obwiedni. |
| `EPH` | byte `XX` | Ustawia górny bajt okresu obwiedni. |

### AY Plus FX| FX | Wartość | Szczegółowe działanie |
|---|---|---|
| `TNN` | nuta `XX` | Ustawia oscylator tonu na nutę z tabeli wysokości, ignorując nutę z wiersza. |
| `TNP` / `TNF` | ze znakiem `XX` | Skomulowany krok wysokości tonu / drobna korekta. |
| `TRT` | dowolna | Ponowne wyzwolenie fazy oscylatora tonu, jeśli obsługiwane. |
| `ENN` | nuta `XX` | Ustawia oscylator obwiedni na nutę z tabeli wysokości. |
| `ENP` / `ENF` | ze znakiem `XX` | Skomulowany krok wysokości obwiedni / drobna korekta. |
| `SFT` | `00-07` | Typ oprogramowania: Brak, Pulse, Sync Tone, Sync Env, Wavetable, Tone FM, Env FM, Sample. |
| `SFN` | nuta `XX` | Ustawia oscylator programowy na nutę z tabeli wysokości. |
| `SFP` / `SFF` | ze znakiem `XX` | Skomulowany krok wysokości oscylatora programowego / drobna korekta. |
| `SRT` | dowolna | Resetuje fazę oscylatora programowego. |
| `SFM` | ze znakiem `XX` | Skomulowana korekta głębokości FM. |
| `PWM` | ze znakiem `XX` | Skomulowana korekta szerokości impulsu. |
| `SPL` | ze znakiem `XX` | Skomulowana korekta poziomu niskiego impulsu. |
| `SWT` | ze znakiem `XX` | Skomulowana korekta indeksu wavetable. |

### AY Sample FX

AY Sample akceptuje wspólne polecenia AY oraz `TNN`, `TNP`, `TNF`, `TRT`, `SFN`, `SFP` i `SFF` opisane powyżej.

| FX | Wartość | Szczegółowe działanie |
|---|---|---|
| `SMS` | `00-FF` | Ustawia start odtwarzania klasycznego AY Sample na `XX x 64` próbek źródłowych. |

Parametry FX silnika stosują się do odpowiadającego typu instrumentu i są zerowane przy kolejnym wyzwoleniu nuty, chyba że zaznaczono inaczej. `SPD` jest celowo trwałe.

Selektor FX filtruje ten zbiór do wspólnych poleceń oraz grupy wspieranej przez aktywny instrument. Panel pomocy kontekstowej podaje szczegóły wartości podczas edycji.

## 11. Mixer, Reverb i Delay

Mixer to lewy główny ekran. Wyświetla obciążenie CPU i ostrzeżenia o przesterowaniu. Czerwony znak `!` w kolumnie **CLIP** oznacza ślad, którego suchy sygnał przekroczył bezpieczny zakres miksu.

Używaj go do balansowania 8 ścieżek, kształtowania każdej ścieżki za pomocą Tilt EQ oraz wysyłania sygnału do reverb lub delay:

| Pole | Znaczenie |
|---|---|
| LVL | Poziom ścieżki po silniku, `000-100` |
| REV | Wysyłka do wspólnego reverbu, `000-100` |
| DLY | Wysyłka do wspólnego delay ping-pong, `000-100` |
| PAN | Panoramowanie sygnału na lewo lub prawo |
| TLT | Tilt EQ na ścieżkę, `00-FF`; `80` jest neutralne, niskie wartości podkreślają bas, wysokie podkreślają soprany. Niskie wartości powodują lekkie przesterowanie. |
| M | `*` wycisza tę ścieżkę |
| S | `*` solo tej ścieżki |

Mixer jest oparty na ścieżkach, nie na instrumentach. Jeśli ścieżka zmienia instrument, jej poziom, PAN, Tilt i wysyłki pozostają przypisane do tej ścieżki. Instrumenty mają również swoje własne `00-FF` **Vol** i **Pan** w górnym rzędzie ekranu Instrument. Pan instrumentu jest stosowany jako pierwszy, potem PAN ścieżki. Oba zachowują istniejący obraz stereo na środku; stopniowo tłumią przeciwną stronę przy przesunięciach w lewo lub prawo.

### Auto Mix

Mixer zawiera przycisk **AUTO MIX**. Renderuje offline `6 sekund`, wyrównuje głośność ścieżek, następnie porównuje 8 pasm oktawowych z profilem szumu różowego i łagodnie ścisza ścieżki przeciążające te pasma. Proponuje ostrożne zmiany poziomów, zostawia margines szczytowy i uruchamia tymczasowy podgląd. **Apply** zachowuje zaproponowane poziomy, a **Cancel** przywraca poprzednie 8 poziomów. To dobry punkt wyjścia, ale prawdopodobnie zechcesz dopracować wynik.

### Reverb

Użyj **SELECT + [UP]** z Mixera, aby otworzyć ekran ustawień reverbu.

- **Algorithm**: wybór między Clouds a Beads. Każdy algorytm ma własne sterowanie.
- **Return**: poziom mokrego reverbu w miksie master.
- **Clouds**: **Time**, **Damping** plus filtry pre-reverb **LP** i **HP**.
- **Beads**: **Time**, natywny filtr sprzężenia zwrotnego **Tone** oraz filtr pre-reverb **HP**.

To jest sekcja wysyłkowa reverbu Clouds lub Beads, nie procesor granularny.

### Ping-pong Delay

Użyj **SELECT + [DOWN]** z Mixera, aby otworzyć Delay, następnie **SELECT + [UP]** by wrócić. Reverb działa odwrotnie: **SELECT + [UP]** otwiera reverb a **SELECT + [DOWN]** wraca do Mixera.

- **Return**: poziom mokrego delay.
- **To Reverb**: ilość mokrego sygnału delay wysłana do wspólnego reverbu, `0-100%`.
- **Ticks**: czas delay w tickach trackera (`9 ticks` = `1 beat` domyślnie).
- **Feedback**: ilość sprzężenia zwrotnego między kanałami, limit 95%.
- **Filter**: filtr dolnoprzepustowy w torze opóźnienia.

Pierwsze powtórzenie podąża za stereo wejściem. Kolejne sprzężenie zwrotne krzyżuje sygnały między lewym a prawym kanałem.

## 12. Ekran projektu

Ekran Projektu zawiera polecenia **Load**, **Save**, **New**, **Export**, **Manage** i **Scale**, wraz z metadanymi nazwy pliku, tytułu i autora.

### Lokalizacja eksportu

Eksporty, stemy i bouncowane pliki zapisywane są do folderu per-projekt wewnątrz istniejącego katalogu samples aplikacji:

- **Domyślnie**: `<folder aplikacji>/samples/Exports/<nazwa projektu>/`. Gdy projekt nie ma nazwy, folder to `current-project`.
- W wersji web domyślnie to `/user/samples/Exports/<nazwa projektu>/`.
- Na Androidzie folder samples jest w przestrzeni aplikacji: prywatny dla aplikacji w wersji Store, `samples` w folderze wybranym w bezpośredniej wersji APK.

Wiersz **Folder** na ekranie EXPORT pokazuje aktywne miejsce docelowe:

- Naciśnij **EDIT** by wybrać niestandardowy folder przez przeglądarkę folderów. Wybrany folder jest używany dokładnie tak, jak wybrany (nie dodaje się podfolderu projektu) i jest zapamiętywany w ustawieniach.
- **EDIT + OPT** (wyczyść) resetuje do domyślnej lokalizacji.

Gdy nazwany projekt zapisuje się pod nową nazwą, domyślny folder eksportu jest zmieniany odpowiednio, zachowując pliki wcześniej wyeksportowane. Zmiana nazwy nie dotyczy folderu niestandardowego i nie jest wykonywana przez autosave. Jeśli folder docelowy już istnieje, oba foldery są zachowane. Jeśli tracker utraci lokalizację folderu (np. po restarcie), zapis nazwanego projektu zmienia nazwę istniejącego folderu `current-project`. Instrumenty działają po zmianie nazwy: ścieżki próbek wskazujące na zmieniony folder są aktualizowane automatycznie.

**Load** akceptuje pliki `.mid`/`.midi`, importując je jako nowy projekt: nuty grupowane są według kanałów MIDI (po jednym kanale na ścieżkę, do limitu ścieżek), kwantyzowane do 4 wierszy na beat i umieszczane na domyślnym instrumencie AY — numery programów MIDI nie mają ekwiwalentu chiptune, dlatego wybierz prawdziwe instrumenty po imporcie. Używany jest tylko pierwszy tempo pliku (jeden globalny takt, bez zmian tempa na sekcje). Wiersz **MIDI** na ekranie Eksport wykonuje odwrotną operację: zapisuje nuty, głośność i tempo / groove jako Standard MIDI File (po jednym torze MIDI na ścieżkę), instrumenty [MIDI Out](#midi-out) i inne efekty na wiersz poza głośnością i globalnym groove nie mają MIDI odpowiednika i nie są tłumaczone.

**Load** akceptuje piosenki Dirtywave M8 (`.m8s`, firmware 2.x-4.x), importowane jako nowy projekt. Przenoszona jest tylko struktura i nuty (wiersze piosenki, łańcuchy z transpose, frazy z nutą, velocity i numerem instrumentu, note-off, tempo i nazwa piosenki); nuty zachowują prawdziwą wysokość (M8 nazywa oktawy dwa wyżej: jego `C-6` to tutaj `C-4`). Każdy użyty instrument w frazie jest importowany pod nazwą instrumentu M8. Instrumenty **MACROSYN** stają się instrumentami Braids (47 kształtów to modele Braids o tych samych numerach; brzmienie, kolor, typ filtra, cutoff, rezonans i pan są zachowane, przybliżona jest charakterystyka cutoff). Instrumenty **SAMPLER** stają się instrumentami Sample: `.m8s` przechowuje tylko ścieżkę do WAV na karcie M8, więc plik jest szukany w folderze `.m8s` i jego nadrzędnych folderach (kopię układu karty) oraz w aktualnym folderze próbek (`Samples` lub `samples` działa); jeśli nie znaleziono, instrument zachowuje ścieżkę M8 i pozostaje niemy. Importowane sample dostają +24 półtony w ustawieniu Pitch, ponieważ M8 odtwarza sample dwie oktawy niżej niż ta aplikacja. **FMSYNTH** staje się instrumentem Genesis FM (przybliżenie), **WAVSYNTH** i **HYPERSYNTH** stają się aChChid. **MIDI OUT** staje się instrumentem MIDI Out (kanał, bank, program i pierwsze cztery CC). Inne typy (zewnętrzne) stają się domyślnym instrumentem AY — wybierz prawdziwe brzmienia po imporcie. FX i tabele są ignorowane.

**Load** akceptuje piosenki Little Sound Dj (`.lsdsng`, po jednej piosence na plik), importowane jako nowy projekt. Wybranie całego save LSDj (`.sav`, do 32 piosenek) otwiera listę jego piosenek (slot, nazwa, wersja, rozmiar); wybierz jedną do importu (piosenka edytowana przy zapisie kartridża nie jest na liście). Przenoszona jest tylko struktura i nuty (wiersze piosenki, łańcuchy z transpose, frazy z nutą i numerem instrumentu, tempo i nazwa piosenki); każdej nucie przypisana jest pełna głośność, bo LSDj nie ma głośności nut, a cztery kanały LSDj PU1, PU2, WAV i NOI stają się ścieżkami 1-4. Instrumenty typu pulse stają się GB Pulse (duty, obwiednia i panoramowanie zachowane; sweep pulsu pominięty), instrumenty hałasu stają się GB Noise (obwiednia, stabilność i panoramowanie), instrumenty wave stają się presetem GB Pulse Hollow Lead, perkusja domyślnie GB Noise. Groove są importowane, a komendy fraz `C` (akord), `D` (delay), `E` (obwiednia pulsu i szumu, nie na kanale wave), `G` (groove), `K` (kill) i `V` (vibrato) zamieniane są na `ARP`, `DEL`, `CEI`/`CEP`/`CED`, `GGR`, `KIL` i `PVB`. Inne komendy (`A`, `F`, `H`, `L`, `M`, `O`, `P`, `R`, `S`, `T`, `W`, `Z`), tabele, ramki wave, sample perkusji i ustawienia syntezatora są ignorowane, więc piosenka ich wykorzystująca będzie brzmiała inaczej. Tempo jest konwertowane do groove o 6 tickach.

Na desktopie i Androidzie wiersz **LSDJ** na ekranie Export wykonuje odwrotną operację, w ograniczonym zakresie: wybierz go i podaj istniejący plik `.lsdsng` jako **szablon** (zapamiętywany do zamknięcia aplikacji, **EDIT + OPT** usuwa zapamiętanie). Jego wiersze piosenki, łańcuchy, frazy, tempo i tytuł są zastępowane, a reszta (tabele, groove, waves, zestawy, ustawienia syntezatora) jest zachowywana. Ścieżki 1-4 stają się PU1, PU2, WAV i NOI; instrumenty GB Pulse i GB Noise zapisywane są jako LSDj pulse i noise, pozostałe typy jako domyślne pulse. FX, głośności nut i note-offy nie są eksportowane, a piosenek korzystających z łańcucha `80` lub powyżej albo frazy `FF` lub powyżej nie da się wyeksportować (LSDj ma 128 łańcuchów i 255 fraz). Wyeksportowany plik nie był jeszcze przetestowany w LSDj.

Na desktopie wiersz **M8S** ekranu Export wykonuje odwrotną operację, w ograniczonym zakresie: wybierz go i podaj istniejący `.m8s` jako **szablon** (zapamiętywany do zamknięcia aplikacji, **EDIT + OPT** usuwa zapamiętanie). Piosenka, łańcuchy, frazy, tempo i tytuł są zastępowane, a reszta jest zachowana, z wyjątkiem tego, że instrumenty Braids są zapisywane jako MacroSynth, a Sample jako Sampler (ścieżka `/Samples/<nazwa pliku>` - WAV musisz skopiować sam na M8). Pod warunkiem, że szablon ma instrument tego typu do skopiowania pozostałych parametrów. FX nie są eksportowane, a piosenek korzystających z frazy `FF` lub powyżej nie da się wyeksportować (M8 ma 255 fraz). Eksport nie był testowany na prawdziwym M8. Szczegóły w [M8 songs](m8s-format.md).

Na desktopie Key jazz pozwala wpisywać nazwę pliku, tytuł i autora bezpośrednio z klawiatury zamiast korzystać z klawiatury ekranowej — zobacz [Key jazz](#key-jazz-desktop-only).

### Scale / Quantize

Ekran Scale kontroluje globalny silnik skali 12-TET. Wiersz **Mode** wybiera pomiędzy dwoma trybami:- **Quantizer** (domyślnie): wprowadzanie fraz pozostaje chromatyczne; po włączeniu, nuty są zaokrąglane w dół tuż przed wysłaniem do silnika dźwięku, więc zmiana toniki lub skali reharmonizuje utwór bez edycji fraz.
- **Note Lock**: nuty wpisywane do fraz są przypinane do najbliższej nuty skali podczas wprowadzania, więc można pisać tylko nuty skali. Nowe nuty i zmniejszenia przesuwają się w dół; `EDIT + RIGHT` / `EDIT + UP` podnoszą nutę do kolejnej nuty powyżej w skali. Wprowadzanie zawsze uwzględnia skalę projektu na tym ekranie: zmiana toniki lub skali natychmiast wpływa na nowo wprowadzone nuty na aktywnych ścieżkach, podczas gdy już zapisane nuty pozostają bez zmian. FX `SCL` jest niedostępne w tym trybie, a już zapisany `SCL` jest ignorowany. Kwantyzacja odtwarzania zwykłych nut jest pomijana (wprowadzanie jest już zablokowane); kwantyzacja akordów przez `CRD` nadal obowiązuje.

- **Clock source** wybiera między **Internal** (własne tempo projektu), **Midi Clock** (sekwencer podąża za przychodzącym zegarem MIDI) i **Ableton Link** (sekwencer podąża za tempem sesji Link w sieci lokalnej). Przy **Midi Clock** wiersz **Tempo** jest przyciemniony i pokazuje zmierzone tempo przychodzącego zegara; transportem (play/stop) sterują komunikaty Start/Continue/Stop nadawcy, więc kontrolka PLAY aplikacji nie jest potrzebna. Przy **Ableton Link** wiersz staje się wierszem **Link**: zamiast pola tempa zawiera przełącznik połączenia/rozłączenia z sesją (zob. niżej). Źródło zegara jest zapisywane w settings.txt i obowiązuje dla wszystkich projektów.
- **Tempo** ustawia tempo projektu w pełnych BPM (`3-500`). Wewnętrznie tracker nadal przechowuje tick rate w Hz (`tick rate = BPM x 0,4`), więc istniejące projekty wczytują się i zapisują bez zmian; projekt z tempem ułamkowym pokazuje je zaokrąglone i przyciąga do pełnego BPM przy pierwszej edycji tempa. Zgrubna regulacja idzie co 5 BPM, dokładna co 1; usunięcie przywraca domyślne 125 BPM.

ChooChooTracker zapisuje projekty jako `.cct`. To własny format, który jest łatwy do odczytania także przez człowieka.

Używaj **Save** przed zmianą typów instrumentów lub wczytaniem innego projektu.

### Ableton Link

Ableton Link pozwala ChooChooTracker podążać za tempem innych aplikacji obsługujących Link (Ableton Live, LinkHut itd.) w tej samej sieci lokalnej (Wi-Fi lub Ethernet). Jest tylko odbiornikiem: tracker dostosowuje własne tempo do sesji i nigdy nie wysyła tempa ani transportu do innych uczestników.

**Co jest synchronizowane:**

- Tempo sesji - tracker stale podąża za tempem sesji Link (w zakresie 3-500 BPM trackera).
- Faza beatu - naciśnięcie PLAY wyrównuje odtwarzanie do siatki beatów sesji: utwór startuje od następnego taktu (kwant 4 beatów).

**Co nie jest synchronizowane:**

Podczas odtwarzania z Link ChooChooTracker sprawdza wspólną fazę beatu raz na takt 4/4 i w razie potrzeby łagodnie koryguje własny tick rate. Kompensuje to drobne różnice zegarów między urządzeniami bez pomijania ani powtarzania ticków trackera i nigdy nie zmienia sesji Link innego uczestnika.

- Groove oraz wartości `SPD` i `DELAY` nie są dotknięte - pozostają względne wobec śledzonego tempa.
- Pozycje kursora w song/chain/phrase i ustawienia miksera nigdy nie są współdzielone.

**Jak używać:**

1. Otwórz ekran Project i ustaw **Clock source** na **Ableton Link**. Wiersz poniżej staje się wierszem **Link**.
2. Naciśnij EDIT na **[Connect]**, aby dołączyć do sesji. Komunikat statusu po prawej pokazuje postęp: `Wait...` podczas łączenia, `No net` gdy brak sieci, `No sess` gdy nie znaleziono sesji, `Err` przy wewnętrznej awarii. Gdy jesteś połączony, przycisk pokazuje **[Disconnect]**, a status mniej więcej co sekundę przełącza się między `connected` a `Peers: N` (N = liczba peerów).
3. Poczekaj na `connected` i naciśnij PLAY - odtwarzanie startuje wyrównane do następnego taktu sesji.
4. Naciśnij EDIT na **[Disconnect]** (lub ustaw źródło zegara z powrotem na Internal), aby opuścić sesję.

W trybie Link sesja jest właścicielem tempa: wiersz nie pokazuje pola BPM, a tempo sesji jest stosowane do projektu automatycznie (w zakresie 3-500 BPM trackera).

**Dostępność na platformach:** obsługiwane są PortMaster (główny cel), Windows, Linux, macOS i Android. Wersja Web i wersje Miyoo nie zawierają Link.

**Uwaga o PortMaster:** na urządzeniach PortMaster bufor audio jest większy (512 klatek przy 48 kHz), więc wyrównanie beatu jest przybliżone, w granicach około 10-15 ms, a nie co do sampla. Sama synchronizacja tempa nie jest dotknięta.

## 13. Settings

Użyj Settings do ustawień, które pozostają przy aplikacji, nie przy aktualnym projekcie. Kontrole dotyczące całego projektu, takie jak tempo, groove i poziomy miksera, pozostają na swoich ekranach.

### Controls

- **Repeat delay** i **Repeat speed** ustawiają, po jakim czasie i z jaką szybkością powtarza się przytrzymany przycisk nawigacji lub edycji. Zwiększ opóźnienie, jeśli powtarzanie wymyka się spod kontroli; zmniejsz je dla szybkiego wpisywania w trackerze.
- **Stick live mode** wybiera sposób działania przypisanego przycisku **STICK LIVE**: `HOLD` działa tylko podczas trzymania, `TOGGLE` pozostaje aktywny po naciśnięciu, a `FREE` jest zawsze aktywny. Zobacz [Motion recording](#motion-recording).
- **Key mapping** przypisuje sterowanie klawiaturą lub kontrolerem. Wybierz komendę, naciśnij nowy kontroler, a następnie wybierz **Done**, by zapisać przypisanie.

### Audio

**Sample rate** oferuje `32 kHz` lub `48 kHz` (domyślnie). Zmiany stosują się natychmiast i są zapisywane. Zmiana częstotliwości próbkowania zatrzymuje odtwarzanie i podgląd sampli, podczas gdy silniki dźwięku i każde uzbrojone wejście audio są otwierane ponownie przy nowej częstotliwości. Jeśli urządzenie odrzuci nową częstotliwość, poprzednie ustawienie pozostaje aktywne. `32 kHz` zmniejsza obciążenie przetwarzaniem i ogranicza najwyższą odtwarzaną częstotliwość do 16 kHz. Wskaźnik obciążenia CPU pokazuje koszt renderowania audio.

**Buffer size** wymienia responsywność na stabilność. Użyj mniejszej wartości dla bardziej precyzyjnego grania na żywo; zwiększ ją, jeśli dźwięk trzaska lub wskaźnik CPU jest stale wysoki. Dostępne wartości to `32`, `64`, `128`, `256`, `512` i `1024` próbek. Wyświetlacz obciążenia CPU pomaga dobrać stabilne ustawienie.

### MIDI

Wybierz tutaj urządzenia wejściowe i wyjściowe, przypisz kanały MIDI przychodzące do instrumentów i mapuj kontrolery MIDI CC. Zobacz [MIDI](#15-midi) aby połączyć klawiaturę, sterować mikserem lub wysyłać nuty do sprzętu zewnętrznego.

### Synths

- **AY Quality** wybiera poziom jakości dla instrumentów AY/YM. **AY Sample dithering** może wygładzić odtwarzanie próbek AY; wyłącz go dla bardziej surowego, lo-fi brzmienia lub gdy celujesz w oryginalny sprzęt.
- **Braids BITS**, **DRFT** i **SIGN** to globalne ustawienia charakteru Braids, wspólne dla wszystkich instrumentów Braids. Ustawiają ogólną rozdzielczość, dryft i charakter sygnatury przed doprecyzowaniem poszczególnych dźwięków. Zobacz [Braids](#braids).

### Mixer

- **Mix volume** to główny poziom wyjścia aplikacji. Ustaw go niżej, gdy podajesz sygnał do innego miksera lub rejestratora; nie zmienia miksu ścieżek utworu.
- **Tilt pivot** ustawia częstotliwość środkową dla wszystkich kontrolerów Mixer **Tilt** w tym projekcie, od `250 Hz` do `4000 Hz` (domyślnie `1 kHz`).

### Graphics

- **Edit color theme** zmienia kolory interfejsu. **Load font** ładuje czcionkę `.cnfont`.
- **Song waveform** pokazuje lub ukrywa falę miksu na stronie Song; patrz [Song waveform](#song-waveform).
- **Track visuals** wybiera dla każdej ścieżki tryb **Detailed** (fala instrumentu i nakładki) lub **Audio waveform** (końcowe audio ścieżki). Użyj **All detailed** lub **All audio**aby zmienić wszystkie naraz.
- **Waveform FPS** ustawia częstotliwość odświeżania oscyloskopu instrumentu i wizualizacji miksera, od `1` do `60` klatek na sekundę. Obniż jeśli wolisz spokojniejszy obraz lub potrzebujesz więcej zasobów.
- **Renderer** pokazuje, czy aplikacja korzysta z `GPU` czy `Software`.

### Support report and quit

**Support report** tworzy mały plik diagnostyczny do zgłoszenia błędu. Dołącz go wraz z projektem `.cct`, gdy problem zależy od konkretnego utworu. **Quit ChooChooTracker** czysto zamyka aplikację.

### Song waveform

Graphics → Song waveform jest domyślnie wyłączony. Dotknij EDIT, by go przełączyć lub użyj EDIT + kierunek; wybór jest zapisywany jako preferencja aplikacji. Po włączeniu dwurzędowa fala dźwięku nad stroną SONG pokazuje końcowy miks zsumowany do mono. Wszystkie inne strony zachowują swoje pełne wysokości, normalne odstępy i podglądy instrumentów niezależnie od tej opcji. Zapisana preferencja jest kompatybilna z wcześniejszymi wersjami. Elementy dotykowe SONG podążają za aktualnie wyświetlanymi rzędami w obu trybach.

## 14. Troubleshooting

### CPU

Zużycie CPU zależy od aktywnych silników i efektów. Modele fizyczne Plaits i pogłos Clouds są cięższe od podstawowych głosów AY. Sprawdź wydajność na docelowej platformie, szczególnie z 8 głosami Plaits i aktywnymi dwoma sendami. Odczyt CPU bliski `100%` może powodować trzaski lub opóźnienia audio.

Jeśli napotkasz trzaski, awarie lub zwolnienia, użyj **Settings > Support report** i dołącz wygenerowany `support-report.txt` do zgłoszenia błędu. Jeśli problem wywołuje konkretny utwór, dołącz też plik `.cct`.

### Brak dźwięku

Sprawdź numer instrumentu, wyciszenie lub solo ścieżki, poziom LVL ścieżki, Mix volume aplikacji oraz obwiednię instrumentu. Dla próbek upewnij się, że oryginalny WAV nadal znajduje się pod zapisaną ścieżką.

Na ArkOS naciśnij **MENU + L3**, aby przełączyć wyciszenie systemu operacyjnego. Jeśli to nie pomoże, zrestartuj sprzęt i ponownie podłącz interfejs audio. Spróbuj opcji "repair audio" w ArkOS, może pomóc.

### Nieprawidłowe działanie wejścia

Dostosuj Repeat delay i Repeat speed w Settings. Jeśli pojedyncze naciśnięcie powoduje dwukrotny ruch, sprawdź, czy tylko jeden fizyczny kontroler jest przypisany do tego kierunku i zgłoś dokładny ekran oraz skrót.

## 15. MIDI

Settings > **MIDI** otwiera to podmenu: **MIDI In** / **MIDI Out** wybierz podłączone urządzenie, przełączając się przez wykryte porty z `OFF` na obu końcach; w obu kierunkach musi być wybrane urządzenie.

- **Podgląd dźwięku z klawiatury MIDI**: po wybraniu urządzenia MIDI In, granie nut na podłączonej klawiaturze odsłuchuje instrument na dowolnym ekranie, tak jak skrót na ekranie **EDIT + PLAY** na [Instrument](#5-instruments). To tylko podgląd — nuty nie wchodzą do utworu.
- **Mapowanie kanałów**: otwiera listę 16 kanałów MIDI; przypisz każdemu instrument (`OFF` domyślnie), aby nuty z tego kanału odtwarzały dany instrument niezależnie od aktualnie wybranego na ekranie Instrument — np. kanał `01` -> instrument `05`. Kanał ustawiony na `OFF` działa według wybranego aktualnie instrumentu, co jest oryginalnym zachowaniem. Przypisanie jest zapisywane do settings.txt.
- **Mapowanie MIDI CC**: mapuje przychodzący kontroler do parametru instrumentu, w tym **Volume**, lub do globalnych sterowań. Globalne wiersze pokazują `TRK 1`-`TRK 8` zamiast instrumentu i oferują sterowanie `Mute`, `Solo`, `Volume`, `Reverb send` i `Delay send` dla ścieżki; **Song play/stop** nie ma celu. Nowe mapowanie ignoruje CC, które je nauczyło lub skonfigurowało i aktywuje się przy następnym fizycznym ruchu, więc ustawianie mapowań podczas odtwarzania nie zeruje patcha do zera. Wyciszenie i solo stosują `0-63` = wyłączone, `64-127` = włączone.
- **Synchronizacja zegarem MIDI**: przy wybranym urządzeniu MIDI In ustaw **Clock source** na ekranie Project na **Midi Clock**, aby podążać za zewnętrznym sekwencerem. Tracker mierzy tempo przychodzącego zegara (24 impulsy na ćwierćnutę) i pokazuje je w przyciemnionym wierszu **Tempo**; komunikaty **Start**, **Continue** i **Stop** sterują odtwarzaniem. Źródło zegara jest zapamiętywane w settings.txt.
- **Sterowanie zewnętrznym urządzeniem MIDI**: przydziel ścieżce typ instrumentu [MIDI Out](#midi-out) i ustaw jej kanał; wyzwalanie nut na tej ścieżce wysyła prawdziwe MIDI do wybranego urządzenia MIDI Out zamiast generować dźwięk w ChooChooTracker.

## 16. Credits and licensing

ChooChooTracker to fork ChipNomad i zachowuje jego licencję MIT. Kod Braids, Plaits, Plaits-Alt, Clouds DSP, Warps-pochodny MME DSP i stmlib pochodzi z otwartoźródłowych wydań Mutable Instruments na ich warunkach MIT. Plaits-Alt pochodzi z forka lylepmills/eurorack Plaits Lab; zachowano oryginalne noty źródłowe. Silnik aChChid korzysta z Open303 Robina Schmidta, copyright 2009, na licencji MIT. MonkSynth używa natywnego DSP od JonET na licencji MIT; zob. `LICENSE.monksynth.txt`. Bogie i Sintered to oryginalne implementacje natywne. MIDI I/O używa RtMidi Gary’ego P. Scavone’a, na licencji typu MIT. Dokładne przypisania w dołączonych plikach licencyjnych. Ableton Link jest dostarczany na podstawie specjalnej umowy licencyjnej z Ableton, nie jest na licencji MIT i nasza licencja nie obejmuje Twoich forków ani innych projektów niż ChooChooTracker.