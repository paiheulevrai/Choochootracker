# Manuale Utente di ChooChooTracker
> Questa è una traduzione generata automaticamente di `docs/USER_MANUAL.md` dalla versione inglese attuale. Se diverge dall’originale in inglese, prevale il testo inglese.
>
> **Convenzioni.** L’interfaccia dell’applicazione è in inglese. Conservare in inglese nomi di schermo, menu, campi e pulsanti, codici FX, nomi di moduli e motori, e estensioni di file.


ChooChooTracker è un tracker musicale a 8 tracce per console portatili.

Segue il classico flusso di lavoro e sistema di navigazione di LSDj, con diverse opzioni di sound design:

- Emulazione di chip e console vintage
- Soundfonts
- Oscillatori Mutable Instruments Braids, Plaits e Plaits-Alt, con filtro multimodale
- Campioni PCM, forme d’onda a ciclo singolo e oscillatori wavetable, con filtro multimodale
- aChChid, un versatile synth acid basato su Open303
- Bogie, un sintetizzatore di batterie nativo a 12 modelli con filtro multimodale
- MonkSynth, sintetizzatore vocale con unisono a dieci voci e delay stereo
- MME, sintetizzatore multi-modulazione a due oscillatori con filtro multimodale
- Sintered, sintetizzatore digitale sperimentale di batterie nativo a sei modelli

## 1. Installazione e file

### ArkOS / PortMaster

Installa `choochootracker.zip` tramite PortMaster, oppure copia il pacchetto estratto nella cartella `ports` della console. Avvia **ChooChooTracker** dal menu Ports.

### Miyoo Mini / Mini Plus (Onion OS)

Estrai l’archivio MiyooPorts nella radice della SD in modo che le cartelle `Roms/PORTS/` si uniscano a quelle esistenti di Onion. Avvia **ChooChooTracker** dal menu Ports dopo aver aggiornato la collezione. Fai un backup della cartella `projects/` dell’app prima di sostituire un’installazione esistente.

Il tempo di caricamento di un progetto può variare in base ai tipi di strumento usati nel brano. Se l’avvio o il caricamento di un progetto sembra anormalmente lento, indica quanto impiega ciascuno, insieme al modello di console e alla versione di Onion OS.

### Raspberry Pi OS 64 bit

Estrai l’archivio Raspberry Pi AArch64, installa il runtime SDL2 (`sudo apt install libsdl2-2.0-0`) ed esegui `./chipnomad.sh` dalla directory estratta. Tieni gli strumenti inclusi e le altre risorse accanto all’eseguibile.

### Windows

Avvia `choochootracker.exe` con `SDL2.dll` e `libwinpthread-1.dll` nella stessa directory.

L’input su Windows supporta tastiera (vedi la sezione Jazz mode per i dettagli) o Gamepad.

### Web

Puoi usare Choochootracker online su https://choochootracker.vercel.app/.

Puoi controllare questa versione con tastiera, controlli a schermo su dispositivi mobili, o il tuo gamepad.

Impostazioni salvate, sample, temi, progetti, ecc. sono memorizzati nello storage del browser.

Nota: a causa di restrizioni iOS, usare midi con la versione web su iPhone può essere impossibile.

### Configurazione delle cartelle

| Cartella | Contenuto |
|---|---|
| `AY_Wavetables` | wavetable AY `.aywave` |
| `fonts` | font ChipNomad `.cnfont` |
| `instruments` | preset strumenti `.cni` ma anche banche FM e Soundfonts |
| `pitch-tables` | intonazioni alternative `.csv` |
| `projects` | file canzone e progetto `.cct` |
| `samples` | file `.wav` per il motore Sampler |
| `SR_wavetables` | wavetable in formato Serum per il motore BYOWTBL |
| `themes` | temi ChipNomad `.cth` |
| `title` | risorse della schermata titolo |
| `waveforms` | forme d’onda a ciclo singolo `.wav` per il motore 2xSCWF |

## 2. Controlli

ChooChooTracker funziona al meglio con un gamepad che abbia D-pad, 2 stick analogici e 7 pulsanti.

| Controllo logico | Predefinito handheld | Predefinito Windows |
|---|---|---|
| **[SINISTRA/DESSTRA/SU/GIÙ]** | D-pad | Frecce direzionali |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Space |
| **SELECT** | Select | Shift |
| Assi di modulazione | Stick sinistro e destro | Stick del controller |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

Le mappature possono essere cambiate in **Settings > Controls > Key mapping**, anche su Android con un gamepad esterno. Su PortMaster, salva una nuova mappatura con **Done**, quindi rilascia il pulsante prima di continuare nelle impostazioni.

### Navigazione

- Usa **[SU/GIÙ/SINISTRA/DESSTRA]** per muovere il cursore.
- Tieni premuto **SELECT + [DIREZIONE]** per muoverti tra le schermate.
- Tieni premuto **OPT + [DIREZIONE]** per navigazioni specifiche per la schermata.
- Su un valore, usa **EDIT + [SINISTRA/DESSTRA]** per modifiche fini o **EDIT + [SU/GIÙ]** per modifiche grossolane.
- Tocca **EDIT** per inserire un valore o attivare un comando.

### Selezione e clipboard

- **SELECT + OPT** entra in modalità selezione e cicla tra intervalli utili.
- Usa **[DIREZIONE]** per estendere l’intervallo.
- Tocca e rilascia **OPT** per copiare la selezione ed uscire dalla modalità selezione.
- **OPT + EDIT** taglia la selezione.
- Fuori dalla modalità selezione, tieni premuto **SELECT** e premi **EDIT** per incollare.
- **EDIT + [DIREZIONE]** modifica le celle selezionate insieme.

### Riproduzione

- **PLAY** avvia dalla posizione del cursore. Premi di nuovo per fermare.
- Puoi modificare note, strumenti e catene mentre suona; le modifiche si sentono al prossimo tick del sequencer.
- **SELECT + PLAY** avvia tutte le tracce quando usato fuori dalla schermata Song.
- **EDIT + PLAY** ascolta in anteprima uno strumento dal Instrument Pool.

### Global Bounce

Seleziona un intervallo di righe e colonne della Song, Chain o Phrase, poi doppio tap su **EDIT** per aprire la schermata BOUNCE TO SAMPLE. Il bounce include tutte le tracce selezionate: le tracce la cui prima catena inizia più tardi nell’intervallo attendono silenziose e si uniscono alla loro prima catena, così il file contiene tutte le catene delle tracce selezionate.

La schermata offre:

- **File name**: modifica con la tastiera dei caratteri. All’apertura la prossima sequenza libera (`001`, `002`, ...) è proposta qui: il primo numero il cui file `.wav` non esiste ancora nella cartella di esportazione. Modificalo liberamente — il nome è usato così com’è. Se esiste già un file con quel nome, viene aggiunto un suffisso `_001`, `_002`... per non sovrascrivere file esistenti. Il contatore si resetta caricando o creando un progetto.
- **Sample rate** e **Bit depth**: stesse opzioni della schermata di esportazione
- **Include in a sample name**: tre checkbox che antepongono tag di contesto al nome file quando si preme **Start** (spente di default):
  - **[BPM]** — il tempo corrente del progetto, es. `[120]`
  - **[Key]** — nota di base e scala del progetto, es. `[Cmaj]`, `[F#min Pent]`
  - **[Bars:Beats:16ths]** — la lunghezza del bounce: una frase corrisponde a una battuta (16 sedicesimi), un beat equivale a 4 sedicesimi. Le frasi intere mostrano solo le battute, es. `[2]`; le frasi parziali mostrano `[0:2]` (due battiti) o `[0:1:3]` (un battito più tre sedicesimi). Nel bounce multitraccia decide la traccia più lunga. Se la selezione termina a metà battito, conta un sedicesimo in meno: l’ultima riga selezionata è il punto di taglio, quindi il suo note-off cade alla fine del sedicesimo precedente.
  - I tag sono applicati nell’ordine `[BPM][Key][Length]` prima del nome del file.
- **Start**: esegue il rendering della selezione in un file WAV nella cartella di esportazione (vedi *Export location* nella schermata Project)
- **Cancel**: torna indietro senza effettuare il bounce

Mentre il bounce è in esecuzione, **OPT** lo annulla. Quando il bounce termina o viene annullato, viene mostrata la schermata da cui si era aperto.

## 3. Struttura della Song e mappa delle schermate

Tieni premuto **SELECT + [DIREZIONE]** per spostarti tra le schermate.

La riga di navigazione principale è:

<pre>
        I
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

Riga centrale:

- **M**: Mixer (da qui si accede a Reverb, Delay)
- **S**: Song (da qui si accede a Project, Settings)
- **C**: Chain
- **P**: Phrase (da qui si accede a Groove)
- **I**: Instrument (da qui si accede a Modulation e Instrument Pool)
- **T**: Table (da qui si accede all’editor AY Wavetable)

Riga superiore:

- **R**: Reverb
- **P**: Project
- **G**: Groove
- **M**: Modulation
- **I**: Insert FX

Riga inferiore:

- **D**: Delay
- **S**: Settings
- **P**: Instrument Pool
- **W**: AY Wavetable editor

Una Song in ChooChooTracker segue questa gerarchia:

```text
Song -> Chain -> Phrase row -> Note + Instrument + FX
```

Una song ha 8 tracce. Ogni traccia contiene una sequenza di catene, e ogni catena contiene una sequenza di frasi. Le frasi sono sequenze di note a 16 passaggi. Una nota contiene un pitch, uno strumento, una velocity e fino a 3 FX di traccia. Uno strumento è ciò che produce il suono.

## 4. Song, Chain e Phrase

### Song

La schermata Song contiene 8 colonne, una per traccia audio. Ogni cella si riferisce a una chain. Una song può contenere fino a 256 righe (`00-FF`).

Le tracce scorrono da sinistra a destra, mentre le loro catene scorrono verticalmente. Ogni traccia è indipendente, e catene possono contenere da 1 a 16 frasi, quindi le tracce possono andare fuori sincronia. Disegna le tue catene per restare insieme o lascia che si disperdano se è ciò che la traccia richiede.

È pratica comune nella comunità dei tracker riservare una catena per frasi vuote, spesso `00` o `FE`.

Puoi mantenere più sub-song in un progetto creando “isole”: sezioni separate da righe vuote.

#### Live Mode

Doppio tap su **OPT** per entrare o uscire da Live Mode; `LIVE` sostituisce il titolo Song. In Live Mode, **PLAY** avvia la chain selezionata sulla propria traccia quando quella traccia è ferma. Su una traccia in riproduzione, mette in coda la chain selezionata per il prossimo bordo di chain; premi due volte **PLAY** per un cambio urgente al prossimo bordo di frase. Una cella Song vuota selezionata mette in coda uno stop quantizzato. `+` indica chain in coda, `!` chain urgente e `-` stop in coda.

Seleziona una riga Song su più colonne per mettere in coda o fermare quelle tracce insieme. Selezioni di più righe sono volutamente rifiutate. Le code Live si azzerano durante il loop di un intervallo Song. Live è una modalità performance e non si salva nel progetto.

#### Controlli

- **OPT + [SU/GIÙ]**: salta 16 posizioni su o giù
- Seleziona un intervallo, poi usa **SHIFT + EDIT**: clona superficialmente le catene mantenendo le frasi originali
- Seleziona un intervallo, poi doppio tap **SHIFT + EDIT**: clona profondamente sia catene che frasi
- Seleziona un intervallo, poi usa **EDIT + [SU/GIÙ]**: sposta la selezione su o giù
- Tocca **OPT** 3 volte: aggiunge o rimuove un evidenziamento per organizzazione visiva
- **OPT + SHIFT**: muta la traccia corrente o colonne selezionate (rilascia prima **OPT** per mantenere il mute attivo)
- **OPT + PLAY**: solo la traccia corrente o colonne selezionate (rilascia prima **OPT** per mantenere il solo attivo)
- **OPT + [SINISTRA/DESSTRA]**: solo tutte le tracce a sinistra o destra della traccia corrente
- Seleziona un intervallo, poi doppio tap **EDIT**: apre la schermata BOUNCE TO SAMPLE (vedi Bounce sopra)

Fermare la riproduzione e cambiare mute o solo usano una breve correzione di uscita di 3 ms per ridurre i click. Questo vale anche per la riattivazione dell’audio e la rimozione dei solo durante la riproduzione.

### Chain

Una chain è un elenco ordinato di frasi a 16 step con trasposizione opzionale. Può contenere fino a 16 frasi, e la stessa frase può apparire più volte. La 2a colonna imposta la trasposizione in semitoni.

Un asterisco (`*`) appare accanto a chain riutilizzate nel progetto. Puoi clonare chain dalla schermata Song.

#### Controlli

- **OPT + [SINISTRA/DESSTRA]**: spostati tra le tracce
- **OPT + [SU/GIÙ]**: spostati tra le chain nella traccia corrente
- Seleziona un intervallo, poi usa **SHIFT + EDIT**: clona frasi
- Seleziona un intervallo, poi doppio tap **EDIT**: apre la schermata BOUNCE TO SAMPLE per le righe chain selezionate della traccia corrente (vedi Bounce sotto Song)

### Phrase

Una phrase è il pattern di traccia in un tradizionale step sequencer.

Una phrase è lunga 16 step, con 1 riga per step. Ogni riga contiene una nota, uno strumento, un volume da `00` a `7F` e colonne FX. Le note usano la notazione tracker come `C-4` (nota e ottava).

Per fermare una nota, inserisci `NOTE OFF`, usa l’FX kill-note, o suona un’altra nota senza strumento impostato.

Quando una chain contiene più phrase, la schermata Phrase funziona come un editor pattern continuo. Muovendosi sotto la riga `F` o sopra la `0` si apre la phrase successiva o precedente della chain.

Un asterisco (`*`) appare accanto a phrase riutilizzate nel progetto. Puoi clonare le phrase dalla schermata Chain.

Il selettore FX mostra comandi comuni più quelli supportati dallo strumento di quella riga. Questo evita una lunga lista globale di comandi specifici per motore. ChooChooTracker supporta più FX di ChipNomad.

### Controlli- **OPT + EDIT** su una nota vuota: inserisce `NOTE OFF`
- **EDIT + [SU/GIÙ]** su una colonna nome FX: apre la schermata di selezione FX
- **OPT + [SINISTRA/DESSTRA]**: sposta tra le tracce
- **OPT + [SU/GIÙ]**: sposta tra le frasi nella catena corrente
- Seleziona un intervallo nella colonna strumento, poi usa **SHIFT + EDIT**: clona strumenti
- Seleziona un intervallo, poi usa **EDIT + [SU/GIÙ]**: ruota le righe della frase
- Seleziona un intervallo, poi doppio tap su **EDIT**: apre la schermata BOUNCE TO SAMPLE per le righe frase selezionate della traccia corrente (vedi Bounce sotto Song)

Su questa schermata, con una selezione attiva, la colonna note può essere riempita
con pattern ritmici:

- **OPT + SINISTRA** riempie le celle note della selezione con un pattern fisso a 16 step
  e avanza al pattern successivo a ogni pressione. Dopo l’ultimo pattern le note sono
  cancellate e il ciclo ricomincia. I pattern sono allineati a righe frase assolute,
  quindi una selezione parziale riceve solo gli step che vi ricadono dentro.
- **OPT + DESTRA** riempie le celle note della selezione con un pattern casuale di
  3-8 note (7 e 8 meno probabili), ciclando indefinitamente finché **OPT** è premuto.
- **OPT + SU** muta le note già presenti nella selezione: a ogni pressione sceglie 1-3 note
  e le sposta casualmente entro un’ottava (spostamenti piccoli più frequenti di quelli estremi).
  La 8° pressione ripristina le note originali all’inizio del ciclo, poi il ciclo ricomincia.
- **OPT + SU** su selezione che copre solo la colonna volume (nessuna cella note,
  strumento o FX) randomizza le velocità invece di mutare: la prima pressione applica il
  pattern Minimal (ogni volume varia fino a +-20), la seconda Medium (fino a +-50), la terza Random
  (valori completamente casuali) e la quarta ripristina i volumi iniziali. Rilasciando **OPT** il
  ciclo torna a Minimal. Righe senza volume restano invariate.
- **OPT + GIÙ** distribuisce le note di strumenti Sampler con slicing presenti nella selezione
  in una scala cromatica crescente: la prima nota per ogni strumento (più alta nel sequencer)
  mantiene valore, ogni nota successiva sale di un semitono rispetto alla precedente, trasformando
  un gruppo di note identiche in trigger consecutivi di slice. Ogni strumento con slicing si distribuisce
  indipendentemente partendo dalla sua prima nota; le note di strumenti senza slicing restano invariate,
  e il risultato non si allinea mai alla scala del progetto. L’ultima nota scritta diventa l’ultima nota
  usata nell’inserimento, quindi continuare manualmente continua con lo slice successivo. Una selezione
  con solo strumenti con slicing si distribuisce a ogni pressione; se ci sono anche note senza slicing,
  il primo tocco distribuisce le note, dai successivi il random arp agisce su di esse.
- **OPT + GIÙ** su selezione senza note di strumenti sliced è un arpeggiatore casuale per le note già presenti.
  Viene scelto un accordo dalla palette CRD – accordi a 3 note per selezioni di 1-2 note, 3+ note per 3,
  qualsiasi accordo per 4 o più – con la radice su una nota casuale nella scala del progetto entro il registro
  della selezione (qualsiasi nota cromatica se la scala è disattivata o la traccia esclusa), e le sue note
  sono quantizzate alla scala come in riproduzione per CRD. Ogni pressione ri-distribuisce l’accordo sulle note
  posizionate: randomizzato, dal più basso al più alto, dal più alto al più basso, poi su e giù; la 5a pressione
  ripristina le note originali e la successiva sceglie un nuovo accordo. Il nome dell’accordo appare nella barra
  di stato durante l’arpeggio; non vengono aggiunte o rimosse note, rilasciare **OPT** reimposta il processo.
- Ogni pressione di fill cancella le note della selezione prima di scrivere, e le note inserite mantengono lo
  strumento usato nell’ultima azione; rilasciando **OPT** i cicli tornano al primo passo.

### Key jazz (solo desktop)

Premi **Esc** nella schermata Phrase per attivare key jazz, un metodo di inserimento note in stile piano QWERTY
(simile a [m8c](https://github.com/laamaa/m8c)):

- `Z S X D C V G B H N J M` e `Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P` digitano note cromatiche su più di due ottave,
  usando la posizione fisica del tasto in modo che la disposizione sia la stessa su tastiere AZERTY, QWERTY e QWERTZ.
- Ogni pressione scrive la nota sulla riga corrente e si sposta alla riga successiva.
- `[` / `]` spostano l’ottava.
- **Shift + frecce** selezionano un intervallo di righe e colonne; una freccia normale dopo cancella la selezione.
- **Ctrl+C** / **Ctrl+X** copiano o tagliano le colonne selezionate, o la nota/strumento/volume della riga corrente
  se niente è selezionato; **Ctrl+V** incolla al cursore.
- **Delete** rimuove la/e riga/e intera/e (tutte le colonne) e sposta le righe successive in su per colmare il vuoto;
  il cursore resta sulla stessa riga.
- **Backspace** elimina l’elemento sotto il cursore nella colonna corrente (o nelle colonne selezionate), sposta
  verso l’alto quello sotto di esso, e sposta indietro il cursore di una riga, come un editor di testo.
- **Insert** inserisce una riga vuota, spingendo verso il basso il resto della frase.
- **Ctrl+S** salva il progetto. Fuori da key jazz, Ctrl+S non fa nulla.
- Premere di nuovo Esc disattiva key jazz.

Quando key jazz è attivo, assume il controllo completo dei tasti note nella schermata Phrase, inclusi i shortcut
Edit/Opt/Motion che condividono quegli stessi tasti — disattivalo con Esc per riusare quei comandi.

Key jazz funziona, indipendentemente attivato con Esc, anche su:

- **Song** e **Chain**: digita direttamente l’indice esadecimale di catena o frase (`0-9`, `A-F`) invece di incrementare
  con Su/Giù. La prima cifra digitata su una cella sostituisce il suo valore; le cifre successive senza spostare il
  cursore si combinano (digitando "3" poi "F" imposta 3F). Su Song, **Shift+frecce** selezionano un intervallo,
  **Delete** elimina righe intere e sposta tutto in su, **Backspace** elimina la/e colonna/e corrente/i o selezionate
  e sposta su, **Insert** inserisce una riga vuota, **Ctrl+C**/**Ctrl+X**/**Ctrl+V** copiano/tagliano/incollano le righe
  selezionate (o la riga corrente se niente è selezionato) – gli stessi comandi strutturali della schermata Phrase.
  Questo prende il controllo di Shift, quindi Shift+Destra/Su non saltano più a Chain/Project finché key jazz è attivo qui;
  Esc li riporta.
- **Ogni schermata con campo testo**: digita direttamente da tastiera invece di usare la tastiera virtuale su schermo.
  Copre nome file progetto, titolo e autore, nome strumento, nome tema colore, nome tabella pitch, nome file bounce e
  i dialoghi di nome (inserisci nome, crea cartella). Backspace funziona come in un campo testo normale; Shift digita maiuscole.
  Esc attiva/disattiva tutto insieme; la tastiera virtuale si apre normalmente se key jazz è spento.
- **Instrument**: con key jazz acceso, i tasti note suonano lo strumento sulla traccia corrente per l’audizione, senza
  inserirne alcuna (stessa disposizione piano della schermata Phrase; il tasto premuto suona, il suono si rilascia con il rilascio del tasto).
  I tasti ottava spostano l’ottava, partendo da quella della prima nota dello strumento. Nel campo **Name** i tasti digitano testo invece.
  Nulla suona se la traccia è in riproduzione.

## 5. Strumenti

Tutti gli strumenti hanno alcuni parametri comuni:

- Tipo di strumento
- Nome, fino a 15 caratteri
- Velocità tabella predefinita, in tick per riga tabella
- Volume
- Panning
- Trasposizione attiva o no

Ogni strumento ha una tabella predefinita con lo stesso numero nell’intervallo `00-7F`. Puoi usarle come tabelle ausiliarie,
ma mantenere le tabelle ausiliarie nel range `80-FE` evita conflitti e confusione. Le tabelle sono tra i principali strumenti di
sound design in ChooChooTracker.

### Comandi

- **OPT + [SINISTRA/DESSTRA]**: sposta tra strumenti
- **EDIT + PLAY**: anteprima dello strumento
- **SHIFT + OPT**: copia lo strumento
- **SHIFT + EDIT**: incolla lo strumento

### SOUNDFONT (sperimentale)

Copia banche originali SF2/SF3, strumenti SFZ/EXS con i loro campioni o archivi ZIP in
`instruments/USER/soundfont`. Seleziona **SOUNDFONT** su Instrument. La riga scan mostra
**Not scanned**, **Scanning...** o **Scanned**, con l’azione **Rescan**.
La scansione legge i metadati; non carica tutti i campioni di ogni preset in memoria.

La riga **Preset** mostra il preset attivo. Tocca e lascia EDIT per aprire il browser su quel preset:
le collezioni appaiono a sinistra e i relativi preset a destra. Ogni collezione presenta una lista piatta,
includendo i preset dentro file bank e archivi ZIP. File WAV/AIFF di supporto restano con i loro strumenti mappati;
collezioni con solo campioni individuali mostrano quei campioni come preset. ID download e hash degli archivi
sono rimossi dai nomi mostrati; nota musicale, BPM e numeri di take sono mantenuti. EDIT seleziona un preset;
EDIT+PLAY ne fa l’anteprima.

Tieni premuto EDIT e premi Sinistra/Destra su Preset per muoverti sequenzialmente con lo stesso ordine del browser,
continuando nella collezione successiva e ricominciando da un estremo all’altro della libreria. Il ciclo rapido prepara
la selezione richiesta più recente. Il preset corrente resta suonabile mentre si prepara la sostituzione. Se il caricamento
fallisce, il preset richiesto resta selezionato e lo strumento muto. **Error:** e la causa appaiono in fondo a Instrument;
il ciclo continua da quel preset in entrambe le direzioni. Riscan dopo l’importazione o modifica dei file libreria.
Lo stato del preset appare direttamente sotto la riga Preset con un colore fisso. Il caricamento non manda messaggi su SONG
o altre pagine.

**Native keys** mostra la gamma note-on autorizzata del preset selezionato, usando C4 per il MIDI 60. **(gaps)** indica
che alcune note all’interno della gamma non hanno zone mappate. È l’unione delle velocity layer; non tutte le velocity o
articolazioni devono suonare a ogni tasto mostrato. Campioni release-only non estendono la gamma. La sintonia e gli FX pitch
non modificano questa visualizzazione né estendono la mappatura.

**Edit**, a destra di Preset, apre l’editor sperimentale SoundFont:
intono (−2400 a +2400 cent), cutoff (20–20000 Hz), risonanza (0–40 dB), delay/attack/hold/decay/release dell’inviluppo (0–20000 ms),
sostegno (0–100%), volume, pan, e **Bend step** (1–100 cent per unità PBN). I nuovi strumenti usano un bend step di 1 cent.
File più vecchi mostrano **Stock**, mantenendo la scala PBN precedente (25 cent in pitch lineare, 1 in period mode); imposta
Bend step a 1 per cambi più fini. Clear ripristina a 1 cent. Questo influenza solo PBN SoundFont; FPT rimane con sintonia fissa.
Volume e pan sono gli stessi valori strumento mostrati nella pagina home. I campi tono e inviluppo di default sono su **Bank**,
mantenendo i valori originali di ogni layer. Un valore esplicito sovrascrive quel parametro su tutti i layer del preset.
Clear riporta a **Bank**, zero cent per Tune, volume pieno o pan centrale. EDIT+Sinistra/Destra fa modifiche fini;
EDIT+Su/Giù fa modifiche maggiori. OPT torna a Instrument; EDIT+PLAY esegue l’anteprima dello strumento sulla nota canzone o default.
Questo modifica i controlli di riproduzione strumento, non le mappature per campione, zone chiave/velocity o loop.

Sintonia, cutoff e risonanza si applicano in tempo reale. Attack, delay e hold si impostano a note-on. Decay, sustain
e release possono aggiornare una voce in riproduzione; il release inizia con un note OFF, non solo su riga vuota. Il decay
è inaudibile con sustain al 100%. Un release non può estendere un campione oltre il suo audio disponibile. Un override di release
esplicito fa rispondere anche un campione one-shot a note OFF. Con le impostazioni Bank, resta il comportamento one-shot originale.
Le gamme chiave e velocity del preset restano autoritative; note fuori gamma sono silenziose. FPT può pitchare un campione già
selezionato oltre quella gamma senza cambiarne la mappatura.
Le modifiche si salvano in strumento e progetto senza cambiare la banca sorgente; brani più vecchi senza override mantengono
le loro impostazioni banco originali.Un WAV con un tipo di loop specifico del sampler sconosciuto può riprodurre l’audio senza quel loop. L’avviso dell’Instrument nomina il tipo di loop ignorato; il file sorgente rimane invariato. Un loop WAV in avanti che termina esattamente un campione oltre l’audio è accettato come fine esclusiva al confine del campione, con un avviso; le mappature PCM e SFZ/EXS esplicite restano invariate. Altri limiti di loop e root note non validi segnalano errori specifici. Un singolo byte di allineamento RIFF a zero erroneamente contato in un layout frame PCM di dimensione dispari viene ignorato con un avviso; altri frame PCM parziali restano errori. I loop WAV standard alternati o all’indietro non sono ancora supportati.

**SoundFont FX** espone otto controlli nel selettore contestuale Lane FX. Tutti i valori seguenti usano la notazione esadecimale del tracker. Gli FX colpiscono solo la loro traccia e lasciano inalterati i controlli salvati dello strumento. Ogni nuova nota parte dalle impostazioni dello strumento, poi applica i comandi nella sua riga e nelle righe successive.

| FX | Controllo | Valori |
| --- | --- | --- |
| FPT | Pitch | `00` = −24 semitoni, `18` = zero, `30` = +24; passi esatti di semitono |
| FCF | Cutoff | `00`–`FE`: 20 Hz–20 kHz, logaritmico |
| FRS | Risonanza | `00`–`FE`: 0–40 dB |
| FAT | Attack | `00`–`FE`: 0–20 secondi, spaziatura più fine sui tempi brevi |
| FHD | Hold | `00`–`FE`: 0–20 secondi prima del decay, impostato sulla riga della nota |
| FSU | Sustain | `00`–`FE`: 0–100% del livello mantenuto |
| FDC | Decay | `00`–`FE`: 0–20 secondi, spaziatura più fine sui tempi brevi |
| FRL | Release | `00`–`FE`: 0–20 secondi, spaziatura più fine sui tempi brevi |

Per FCF/FRS/FAT/FHD/FDC/FSU/FRL, `FF` ritorna all’impostazione di Edit (incluso **Bank**). Metti Attack/Hold sulla riga nota. Decay/Sustain/Release possono cambiare la nota attiva; usa OFF per sentire il suo rilascio. Volume e pan usano i comuni controlli; i comandi legacy FVL/FPN vengono ancora caricati e riprodotti ma non sono più offerti. Gli FX di Pitch cambiano la tuning senza modificare la zona di key selezionata del bank. I comandi pitch/fine pitch, MOD e effetti traccia del sequencer esistenti restano disponibili. I progetti salvano riferimenti stabili a bank/preset e comandi FX; le librerie di campioni restano esterne.

Tutti i bank sono bank USER. Strumenti grandi possono superare la cache PCM predefinita di 64 MiB, e parametri sorgente non supportati producono avvisi. Consulta la [guida SoundBanks](soundbanks/README.md) per i percorsi piattaforma, importazione browser, configurazione cache, copertura formati e limiti di validazione attuali.

#### Compatibilità SoundFont: bank, preset e modulazione

Un singolo **file bank** SF2 o SF3 può contenere molti **preset**, insieme ai loro campioni. Una **collezione** di libreria è la cartella usata per raggruppare questi file nel browser. Sono livelli diversi: un preset può avere comportamenti non supportati anche se altri preset nello stesso file si traducono bene.

La **modulazione preset** significa legami programmati da input come velocity, key, mod wheel o pressure a un parametro sonoro. Questi legami possono appartenere a un preset o a un layer strumento sottostante condiviso da vari preset. Non sono necessariamente un’impostazione che influisce su ogni suono del bank.

| Funzionalità preset | Traduzione attuale |
| --- | --- |
| Mappatura campione/key/velocity, root pitch e tuning | Implementato; intervalli originali mantenuti. Velocity zone e velocity tracker ordinaria funzionano ancora. |
| Offset campione, loop supportati, gain/pan e gruppi esclusivi | Implementato nel parser e limiti voce. |
| Cutoff/resonance low-pass statici e inviluppo volume | Implementato tramite renderer tracker; non garantisce risposta identica ad altri player SoundFont. |
| Rotte modulazione preset/strumento personalizzate (`pmod` / `imod`) | Non implementato. Mappature controller-parametro, quantità e curve risposta autore ignorate. |
| Modulazioni/vibrato LFO e routing inviluppo modulazione SoundFont | Non implementato; movimenti programmati su pitch, filtro o volume possono mancare. |
| Envelope hold/decay scalato per key, generatori fixed-key/fixed-velocity | Non implementato. |
| Invii chorus/reverb SoundFont | Non implementato. Effetti tracker impostabili manualmente ma non riproducono automaticamente le impostazioni del preset. |

Il vecchio messaggio **“Custom bank modulation ignored”** descrive un controllo a livello di file: il parser ha trovato record di modulazione personalizzati in qualche punto del bank. Non determina che il preset selezionato li usi, né che tutto il bank sia inutilizzabile. Questa limitazione generale è documentata qui invece di ripetere il messaggio sotto ogni preset. Le diagnostiche dettagliate del parser mantengono i record; altri errori di caricamento e avvisi di compatibilità compaiono quando applicabili. Il motore non dichiara nemmeno un comportamento completo dei modulatori di default SoundFont. PBN, vibrato, Edit e Lane FX sono controlli indipendenti, non traduzioni automatiche della programmazione preset omessa.

Per scegliere una libreria, parti da preset basati principalmente su mappature campione, loop e inviluppi statici. Suoni il cui carattere dipende da articolazioni controller, filtri animati o LFO programmati richiedono confronto ravvicinato in un player SoundFont completo. **Carica e suona** non significa **riproduce fedelmente il preset**. Confronta note e velocity utili, oltre a note tenute e note OFF. Cambiare da SF2 a SF3 (o viceversa) non rimuove da solo queste esigenze di modulazione. Una conversione deve tradurre mappature e comportamento in funzioni supportate da questo motore; rendere un effetto in un campione preserva solo quel risultato renderizzato, non la risposta interattiva del controller. Consulta la [copertura formati e limiti](soundbanks/README.md#format-coverage) e le [note di validazione](soundbanks/validation.md) prima di scegliere o convertire un bank. Questi limiti descrivono l’implementazione sperimentale attuale, non restrizioni proprie di tutti i player SoundFont.

#### Esplorare le collezioni SoundFont

In SAMPLE → SOUNDFONT, **Preset** apre le tue collezioni. **Right** o **EDIT/A** entra direttamente in una cartella. In una collezione foglia, Right passa ai suoi preset; un altro Right avanza di 15 voci senza caricare. EDIT/A carica il preset evidenziato. Left torna alle collezioni e poi alla cartella superiore; anche OPT torna indietro. PLAY avvia/ferma il brano durante l’esplorazione.

Icone di cartella e di suono identificano le voci. Le collezioni usano tutta l’altezza del pannello. Il preset selezionato resta sotto le otto anteprime di traccia; i nomi lunghi scorrono dopo un secondo di inattività sulla selezione e ripartono quando ti muovi. EDIT+Left/Right nella pagina strumento segue lo stesso ordine di cartelle annidate, mantenendo insieme i suoni di ogni collezione. EDIT+PLAY ascolta.

La terza riga riporta Reading preset, Finding samples, Unzipping, Checking samples o Loading samples. Gli strumenti EXS con nomi incorporati vuoti o `(null)` usano i nomi dei file. I banchi sorgente e l’audio non cambiano. Usa **Rescan** dopo aver modificato i file della tua libreria.

Consulta [copertura dei formati e limiti](soundbanks/README.md#format-coverage) per il comportamento supportato del sampler e i formati di sample.

### AY Classic, AY Plus e AY Sample

Questi sono i motori originali ChipNomad AY/YM. AY Classic offre controlli hardware-style per tono, rumore e inviluppo. AY Plus aggiunge oscillatori software e modulazione più ricca. AY Sample riproduce un sample tramite livelli di volume stile AY, distinto dal motore Sampler.

#### AY Classic

È un tipo strumento legacy da ChipNomad `v1.0.0`. Il più recente strumento AY Plus offre più funzioni.

Gli strumenti AY Classic hanno questi parametri:

- Mixer: tono acceso/spento, rumore acceso/spento, forma inviluppo (`0-F`)
- Volume: inviluppo ADSR generato software
- Periodo inviluppo automatico: acceso/spento, con rapporto da `1:1` a `F:F`

La **Qualità AY** in Settings influenza solo il rendering AY/YM. Non cambia la qualità di Braids, Plaits o Sampler. **Sample dithering** si applica alla quantizzazione AY Sample.

Consulta la [documentazione ChipNomad AY-3-8910](https://chipnomad.org/chips/ay-3-8910/) per i dettagli del chip.

#### AY Plus

AY Plus applica concetti classici di sintetizzatore al design sonoro AY-3-8910/YM2149F. Puoi pensare a AY come un sintetizzatore con 5 oscillatori:

- 3 oscillatori tono a onda quadra
- 1 oscillatore rumore
- 1 generatore inviluppo ampiezza usabile come oscillatore

Secondo questo modello, ogni strumento ha 3 oscillatori hardware: tono, rumore e inviluppo. AY Plus aggiunge un 4° oscillatore software. Aggiornare i registri AY più velocemente del tick di progetto apre più opzioni di sound design. A causa del funzionamento di AY, gli oscillatori sono miscelati usando modulazione ad anello. Prova offset di pitch diversi, detuning e combinazioni di oscillatori per scoprire nuovi suoni.

Ogni oscillatore tonale può essere accordato indipendentemente con offset di pitch e fine tune.

Tipi di oscillatori software e parametri:

- Pulse – onda quadra con larghezza impulsi controllabile
  - FM Depth – profondità FM semplice
  - Pulse width – range completo `00-FF`, numero passaggi impostato nel Project screen
  - Pulse low – controlla valore basso in coppia ripetuta col valore alto sempre `15`
- Sync Tone – effetto hard-sync per oscillatore tono
  - FM Depth – profondità FM semplice
- Sync Env – effetto hard-sync per oscillatore inviluppo
  - FM Depth – profondità FM semplice
  - Pulse width – rapporto durata tra 2 forme della coppia inviluppo
  - Envelope Pair – 2 forme inviluppo per alternare
- Wavetable – wavetable a 32 step, editabili nel Wavetable screen
  - FM Depth – profondità FM semplice
  - Wavetable index
- Tone FM – effetto FM semplice per oscillatore tono
  - FM Depth – profondità FM tono
- Env FM – effetto FM semplice per oscillatore inviluppo
  - FM Depth – profondità FM inviluppo

Gli oscillatori software Pulse e Wavetable non possono essere usati con l’oscillatore Envelope. L’oscillatore Envelope si spegne automaticamente.

La FM semplice alterna rapidamente tra 2 valori di periodo, 1 più basso e 1 più alto. È simile all’uso di un’onda quadra come modulatore FM. Pulse, Sync Tone, Sync Env e Wavetable usano un rapporto `1:1` tra portante FM e modulatore. Tone FM e Env FM possono imitare altri rapporti tramite offset pitch dell’oscillatore software.

#### AY Sample

Puoi caricare file WAV mono o stereo a `8-bit`, `16-bit`, `24-bit` o `32-bit`. Vengono convertiti a `8-bit` e troncati a `16 KB`. Questo limite corrisponde alla dimensione pagina RAM dello ZX Spectrum.

AY riproduce i campioni con scritture veloci al registro volume. L’uscita è unipolare, quindi i campioni vanno “sollevati a zero” per una riproduzione più pulita. Questo elimina click di fine campione e migliora le code silenziose.

AY ha risoluzione DAC a `4-bit` con scala volume non lineare, quindi i campioni `8-bit` diventano `4-bit` durante la riproduzione. L’opzione Sample dithering in Settings può migliorare la percezione della profondità bit. Il dithering non è pratico su piattaforme retro come ZX Spectrum e Atari ST, quindi disattivalo per hardware reale o se cerchi un suono lo-fi granuloso.

I campioni si comportano come un altro oscillatore software AY Plus, ma i loro parametri extra li rendono più facili da gestire come tipo strumento separato.

Parametri sample:

- Sample rate – riprodurre un campione alla sua frequenza nativa produce un `C-4`
- Sample start
- Sample length
- Sample loop start
- Sample pitch offset
- Sample fine pitch offset

Le sezioni Tone e Noise corrispondono a quelle di AY Plus. Per il funzionamento di AY, tutti gli oscillatori sono miscelati tramite modulazione ad anello.

### Braids

Braids offre 47 modelli di sintesi. I controlli principali sono:

- **Model**: oscillatore o algoritmo di sintesi
- **Timbre** e **Color**: macroparametri dipendenti dal modello
- **Filter**: filtro LP/BP/HP aggiuntivo, pendenza `12` o `24 dB`, cutoff e risonanza
- **ADSR**: attacco, decay, sustain e release

Tocca **Model** per scegliere da liste di modelli categorizzate.

| Categoria | Modelli |
|---|---|
| Analog | `00 CSAW`, `01 MORPH`, `02 SAW-SQUARE`, `03 SINE-TRI`, `04 BUZZ`, `05 SQUARE-SUB`, `06 SAW-SUB`, `07 SQUARE-SYNC`, `08 SAW-SYNC` |
| Multi Osc | `09 TRIPLE-SAW`, `10 TRIPLE-SQR`, `11 TRIPLE-TRI`, `12 TRIPLE-SINE`, `13 TRIPLE-RING`, `14 SAW-SWARM`, `15 SAW-COMB`, `16 TOY` |
| Filter / Voice | `17 FILTER-LP`, `18 FILTER-PEAK`, `19 FILTER-BP`, `20 FILTER-HP`, `21 VOSIM`, `22 VOWEL`, `23 VOWEL-FOF`, `24 HARMONICS` |
| FM / Chaos | `25 FM`, `26 FEEDBACK-FM`, `27 CHAOTIC-FM` |
| Physical | `28 PLUCKED`, `29 BOWED`, `30 BLOWN`, `31 FLUTED`, `32 STRUCK-BELL`, `33 STRUCK-DRUM` |
| Drums | `34 KICK`, `35 CYMBAL`, `36 SNARE` |
| Wavetables | `37 WAVETABLES`, `38 WAVE-MAP`, `39 WAVE-LINE`, `40 WAVE-PARA` |
| Noise / Granular | `41 FILTER-NOISE`, `42 TWIN-PEAKS`, `43 CLOCK-NOISE`, `44 GRAN-CLOUD`, `45 PARTICLE`, `46 DIGI-MOD` |

#### Parametri

Timbre e Color dipendono dal modello selezionato. Consulta il manuale originale Braids di Mutable Instruments per elenco completo e dettagli, o sperimenta ad orecchio.

#### VCF e VCA

Braids alimenta il filtro multimodo standard di ChooChooTracker e l’inviluppo ADSR.

#### Impostazioni

Le impostazioni globali di Braids sono disponibili nelle app settings:

- **BITS**: riduce la risoluzione di uscita
- **DRFT**: aggiunge instabilità pitch oscillatore
- **SIGN**: riproduce l’algoritmo originale Braids di imperfezione d’onda

### aChChid**aChChid** è un motore acid bass monofonico basato su Open303. `Square` e `Saw` usano il suo oscillatore, filtro, inviluppo e comportamento di accent nativi TB-303. `Braids` sostituisce solo l’oscillatore, continuando poi attraverso lo stesso filtro 303 e percorso amplificatore. Espone Model, Timbre, Color e un waveshaper extra. Il waveshaper evolve da saturazione morbida a wavefolding.

Cutoff e Decay usano modifica logaritmica: **EDIT + LEFT/RIGHT** si muove di un passo musicale, mentre **EDIT + UP/DOWN** di sedici passi. Cutoff copre da 200 Hz a 20 kHz; Decay da 200 ms a 2 s.

Una `F` nella colonna note volume attiva un accent. `ASL` scivola verso quella nota dal pitch precedente senza riattivare l’inviluppo 303; mantiene automaticamente la gate precedente, e `ASL 00` dà un glide di `60 ms`. `ATY` mantiene la nota aperta per l’intero step corrente. Le note senza `ASL` riattivano sempre l’inviluppo.

Le destinazioni di modulazione includono Decay e Accent, più Timbre e Color in modalità onda Braids.

### Bogie

**Bogie** è un sintetizzatore di batteria one-shot nativo. Le note del tracker impostano il pitch e il volume tracker/strumento il livello; ogni trigger ha un decay interno, quindi niente pagina ADSR. Ha 12 modelli: `Kick`, `Snare`, `Hat`, `Clap`, `Tom`, `Rim`, `FM`, `Noise`, `Cowbell`, `Cymbal`, `Shaker` e `Clave`.

I suoi sei macro sono **Decay**, **Tone**, **Sweep**, **Noise**, **FM** e **Drive**. Le etichette si adattano dove utile (per esempio, il Noise di Kick è `Click`, l’FM di Cowbell è modulazione incrociata metallica). Ogni macro è attiva su ogni modello, ma il suo ruolo musicale cambia con il modello: Sweep può alterare pitch, spaziatura burst o diffusione metallica; FM aggiunge cross-modulazione, uno strato metallico o un'armonica a tipo anello. La metà inferiore di ogni gamma è pensata per suoni di batteria convenzionali; la metà superiore apre volutamente timbriche più ampie e risultati più sintetici.

Ogni modello può usare il filtro multimodo LP/BP/HP condiviso, con carattere Clean, Classic, Aggro o Acid, pendenza `12`/`24 dB`, cutoff e risonanza. Bogie supporta tutti i suoi macro attivi più Volume, Pitch, Cutoff e Resonance come destinazioni di modulazione e motion recording. Usa decay breve con Cowbell per attacchi phonk affilati; aumenta FM per un campanaccio più duro e metallico.

Nella schermata Modulation, premi **EDIT** su una sorgente per scegliere una famiglia (Envelopes, LFO o Sticks), poi la sorgente stessa. **Left** e **Right** cambiano comunque sorgente direttamente.

### Sintered

**Sintered** è un synth per percussioni digitali one-shot nativo. Le note impostano il pitch fondamentale; i sei modelli sono `Knot`, `Shard`, `Burst`, `Comb`, `Logic` e `Melt`. Ogni modello ha **Decay**, **Mod** e **Motion**, più tre controlli specifici: Knot usa Ratio/Spread/Fold, Shard Ratio/Feedback/Bite, Burst Noise/Color/Feedback, Comb Time/Damping/Regen, Logic Rate/Pattern/Crush e Melt Ratio/Chaos/Drive. **Mod** è sempre la profondità di accoppiamento interna.

Motion è un macro inviluppo interno: valori sotto `80` usano forma AD, `80` lo disabilita e valori sopra `80` usano solo decay. Allontanarsi dal centro rende l’inviluppo più veloce. Le sue destinazioni sono fisse per modello, così Sintered resta prevedibile nei pattern tracker: Knot anima Mod/Fold; Shard Feedback/Bite; Burst Noise/Color/Feedback; Comb Damping/Regen; Logic Rate/Crush; Melt Chaos/Drive. Tutti i modelli resettano il loro stato al trigger della nota e limitano il feedback interno per colpi ripetibili.

### MonkSynth

MonkSynth è un sintetizzatore vocale. Modella vocale, carattere della voce, soffio, glide, vibrato, unisono e delay dalla pagina Instrument. EDIT apre tutti i controlli e il nome della patch.

Scegli un banco e un preset sui controlli VOICE. ALL include FACTORY e USER; EDIT+PLAY ascolta il preset evidenziato, mentre EDIT lo seleziona. Salva i tuoi suoni in `instruments/USER/monk/`. Strumenti e brani salvati mantengono le impostazioni di MonkSynth.

I valori sono esadecimali `00..FF`, tranne Voices (`01..0A`, da una a dieci). Attack, decay e release coprono 0..5 secondi; il detune 0..50 cent e l’apertura delle voci 0..0,5. Volume, Pitch e i sei controlli principali della voce sono disponibili come destinazioni di modulazione e di motion recording.

I sedici controlli compaiono insieme in **MonkSynth FX**: `MWV` (vocale), `MIN` (voce), `MAM` (soffio), `MFL` (glide), `MFB` (profondità vibrato), `MSH` (velocità vibrato), `MUV` (voci), `MDT` (detune), `MVS` (apertura), `MDM` (mix delay), `MDR` (velocità delay), `MLV` (livello) e `EAT`/`EDC`/`ESU`/`ERL` (ADSR). Questi significati valgono quando MONK è lo strumento attivo; i significati esistenti dei comandi MME non cambiano. Il gruppo Trigger FX condiviso mantiene TDC/TCL senza voci di inviluppo duplicate.

### MME

**MME** (Multi Modulation Engine) è un synth a due oscillatori dedicato alle interazioni tra oscillatori. I modelli sono `Ring`, `Fold`, `Cross`, `VPM`, `Sync`, `Logic` e `Vocode`. Entrambi gli oscillatori sono interni e seguono le note tracker; `Interval` ne regola la relazione.

I sei macro sono **Waves**, **Interval**, **Amount**, **Flow**, **Feedback** e **Shaper**. Waves interpola tra coppie di waveform musicali A/B. Amount controlla l’interazione principale del modello, Flow cambia direzione o variante algoritmo, e Feedback è limitato ma diventa volutamente selvaggio al limite superiore. Shaper è uno stadio finale condiviso che va da pulito a saturazione e wavefolding.

MME usa il filtro multimodo LP/BP/HP e ADSR condivisi. Tutti i sei macro, Cutoff e Resonance sono disponibili per modulazione e motion recording. Tieni premuto **EDIT** su Model per aprire il popup modello.

### MIDI Out

**MIDI Out** (desktop, PortMaster e browser Web MIDI) comanda un dispositivo MIDI esterno invece di sintetizzare audio: il trigger di una nota invia un vero MIDI Note On sul **Channel** dello strumento (`1-16`) e, quando la nota termina, il corrispondente Note Off. Il volume diventa velocity, e le voci di una traccia accordo ricevono ciascuna Note On/Off proprie. Non si sente audio da ChooChooTracker stesso - seleziona prima il dispositivo di output MIDI in [Settings](#13-settings). Vedi [MIDI](#15-midi) per la panoramica completa, inclusa l’anteprima sonora da tastiera MIDI.

**Program** e **Bank high/low** (CC0/CC32) sono opzionali - mostrati come `--` se disattivati, `Clear` disattiva un campo e ne ricorda il valore. Se impostati, sono inviati una volta sola, subito prima della nota successiva, ogni volta che differiscono dall’ultimo valore mandato su quel canale (non prima di ogni nota, per evitare di riattivare gli inviluppi/patch del dispositivo ricevente).

**MC1 number**-**MC4 number** selezionano il numero CC (`00-7F`) cui ciascuno dei quattro FX riga MC1-MC4 invia - vedi [MIDI Out FX](#midi-out-fx). Anche `--`/opzionale; uno slot non impostato rende il suo FX inattivo.

### Classici motori architetturali

I motori di questa categoria condividono un'architettura VCO → VCF → VCA.

Sono disponibili diversi tipi di VCO:

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **Sampler**
- **2xSCWF**
- **BYOWTBL**

Ogni strumento manda il proprio VCO in un filtro multimodo.

Il filtro ha pendenza commutabile `12 dB` o `24 dB` e può funzionare come low-pass (LP), band-pass (BP) o high-pass (HP).

Sono disponibili diversi caratteri di filtro:

- **Clean**: sterile e digitale
- **Classic**: ispirato ai classici synth analogici americani
- **Aggro**: ispirato ai synth analogici giapponesi ruggenti
- **Acid**: doppio dell’aggro in aggressività

Classic, Aggro e Acid possono diventare molto risonanti. Parti da valori bassi e alzali gradualmente.

Il filtro alimenta un VCA controllato da un inviluppo ADSR. La forma dell’inviluppo può variare tra curve esponenziali, lineari e logaritmiche.

#### Plaits e Plaits-Alt

**Plaits** offre 24 motori, raggruppati nel popup selezione così:

| Categoria | Motori |
|---|---|
| Analog / Waves | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digital | `07 CHIPTUNE`, `15 SPEECH` |
| Texture / Noise | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Physical | `19 STRING`, `20 MODAL` |
| Drums | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** è una collezione di modelli alternativi per il modulo originale Plaits. La maggior parte è più strana o sperimentale di un tipico VCO.

| Categoria | Motori |
|---|---|
| Granular / Micro | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| Phase / Harmonic | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| Acoustic / Physical | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| Polyphony / Harmony | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| Digital / Weird | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

I parametri seguono il design Mutable:

- **Harmonic** controlla rapporti armonici, bilanciamento o scelta modello all’interno di un motore.
- **Timbre** va generalmente da spettri scuri/scarsi a luminosi/densi.
- **Morph** esplora un’altra dimensione timbrica.
- **Main/Aux** fonde l’uscita principale con quella alternativa del motore.

Il loro significato preciso dipende dal motore. Per esempio, i motori accordo li usano per tipo di accordo, inversione e waveform. I modelli fisici per materiale, eccitazione e decay, mentre i motori per batteria per tono, carattere e decay. Riferirsi al manuale Plaits per dettagli.

Per `CHORD`, `DIATONIC CHORD` e `WT DIATONIC CHORD`, Harmonic mostra il nome accordo selezionato (`MIN`, `MAJ`, `MIN7`, `MAJ7` ecc.) invece del valore grezzo. Braids `WAVE PARA` nomina nello stesso modo le sue voicing Color.

Tocca **Engine** per scegliere da liste di motori categorizzate.

**Env Mode** ha 2 instradamenti:

- `TRIG` riproduce il modulo con TRIG collegato e LEVEL non patchato
- `VCA` mantiene LEVEL aperto e applica l’ADSR tracker dopo la voce

#### Sampler

Questo motore Sample pulito riproduce campioni PCM mono o stereo caricati in RAM.- Tocca **Sample** per caricare un WAV PCM non compresso `8-bit`, `16-bit` o `24-bit`. Premi **PLAY** nel browser per ascoltare il file evidenziato.
- Quando un sample è caricato, appare **EDIT** accanto al nome. Toccalo per aprire la schermata di modifica del Sample. **OPT** o **SELECT + [LEFT]** torna allo strumento.
- La schermata di modifica mostra il nome file (con un marcatore `*` quando il sample in RAM differisce dal file su disco), il formato (frequenza di campionamento, canali), una forma d’onda alta, e i campi **Region**, **Select**, **Slice**, **Process** e **File**.
- Le modifiche esistono solo in RAM finché un salvataggio non le scrive: il progetto memorizza il percorso del file del sample e lo ricarica da disco al prossimo caricamento del progetto. Uscire dal tracker senza **Save**/**Save As** scarta le operazioni di processamento (il marcatore `*` è l’avviso). Caricare un sample diverso nello strumento scarta le modifiche non salvate e resetta la Region sull’intero sample (Start `00`, End `FF`).
- **Region** imposta i marker di inizio/fine riproduzione, disponibili anche nella schermata dello strumento Sampler. Impostano i limiti normalizzati di riproduzione (`00-FF`); se Start è dopo End, il sample suona al contrario. **EDIT + [LEFT/RIGHT]** sposta finemente il marker di uno step (`01`) e zooma la forma d’onda attorno ad esso mentre si tiene premuto **EDIT**; **EDIT + [UP/DOWN]** sposta grossolanamente (step 16) e riporta la vista sull’intero sample. **EDIT + OPT** resetta il marker al valore predefinito (Start `00`, End `FF`).
- La vista della forma d’onda zooma su una finestra fissa di un secondo attorno al marker modificato e pan per mantenerlo visibile; i sample che stanno nella finestra restano alla vista 1:1. Lo zoom è temporaneo: dura solo mentre si tiene premuto **EDIT**, rilasciandolo torna alla vista completa 1:1, come anche gli spostamenti grossolani. Entrando nella schermata la vista si resetta sempre sull’intero sample.
- **Select** imposta una selezione per il processamento in frame assoluti, indipendente dai marker di Start/End. **EDIT + [LEFT/RIGHT]** sposta una maniglia di venti frame e zooma su di essa mentre si tiene premuto **EDIT**; **EDIT + [UP/DOWN]** salta di `frameCount/64` (minimo 16) e torna sulla vista dell’intero sample. **EDIT (tap)** su una maniglia copia la posizione corrente del marker Start o End su quella maniglia. **EDIT + OPT** su una maniglia cancella tutta la selezione. Quando è vuota entrambe le maniglie mostrano `-`; se invertite si scambiano automaticamente. La selezione è uno stato temporaneo dell’editor: non viene salvata con il progetto e entrando nella schermata si inizializza con la region di riproduzione (intero sample con i marker predefiniti).
- **Slice** è una riga singola con quattro celle: **Mode**, **Count**, **Slice** e **Frame** — salvata con lo strumento. Off riproduce la finestra Start/End. Le note di Phrase selezionano slice cromaticamente da **C-0**, e le note oltre l’ultimo slice ritornano all’inizio (la nota N suona lo slice `N % count`; quindi una corsa oltre l’ultimo slice ricomincia dal primo). Le note slice non traspongono il pitch né usano la quantizzazione Scale, ma **CRD** mantiene lo slice selezionato e trasporta le voci come un accordo. Gli start degli slice sono segnati sulla forma d’onda con linee brevi larghe due pixel (arancione scuro; la linea dello slice corrente è più chiara), e lo slice sotto il cursore ha una banda nera di sfondo sulla forma d’onda (il colore di selezione non viene disegnato sopra la banda, così resta nero e distinto chiaramente dal resto).
  - **Mode** cicla `OFF` / `EQUAL` / `AUTO` / `LAZY` (EDIT + left/right o tap; EDIT + OPT spegne lo slicing). Cambiare modalità inizializza i punti di slice: **EQUAL** divide la finestra Start/End in **Count** parti uguali; **AUTO** esegue un rilevamento transienti nella finestra e piazza uno slice ad ogni attacco trovato (fino a **Count**); **LAZY** parte con uno slice che copre tutto. Passare a LAZY salva l’impostazione corrente di EQUAL/AUTO (count e limiti); tornare a EQUAL o AUTO la ripristina esattamente — nessuna conferma e nessuna perdita. Slice e Stretch sono mutuamente esclusivi: abilitarne uno disabilita l’altro, e le celle Slice sono attenuate mentre Stretch controlla la durata.
  - **Count** mostra il numero totale di slice (`01`..`64`). **EDIT + [LEFT/RIGHT]** lo aumenta o diminuisce di uno (minimo 1, massimo 64), **EDIT + [UP/DOWN]** cicla tra i conti potenze di due `2 / 4 / 8 / 16 / 32 / 64` con avvolgimento. Ogni variazione ricalcola la divisione: **EQUAL** ridivide la finestra equamente, **AUTO** rilancia il rilevamento col nuovo valore (la sensibilità scatta dal count — più slice significano soglia più sensibile). La casella mostra `-` in OFF. In **LAZY** rimane luminosa (conteggia gli slice piazzati a mano) ma è solo visuale: il cursore la salta — spostandosi a destra da **Mode** o sinistra da **Slice** si finisce su **Slice**, scendendo dal marker **END** della riga **Select** si arriva lì, toccare la casella non fa nulla (gli slice manuali non hanno divisioni da ricalcolare). **EDIT + OPT** e un semplice tap **EDIT** qui non fanno nulla: spegnere lo slicing è su **Mode**, cancellare uno slice è su **Slice**.
  - **Slice** è il browser: mostra uno slice indicizzato da 1 (`01`..count). **EDIT + [LEFT/RIGHT]** sposta slice uno alla volta, **EDIT + [UP/DOWN]** salta quattro slice (entrambi limitati, senza avvolgimento); lo slice attuale è indicato sulla forma d’onda dal marker più chiaro e dalla banda nera, e la vista zoomata si centra sullo start dello slice. **EDIT + OPT** cancella lo slice corrente (lo unisce al precedente; il primo slice non può essere cancellato, e cancellare l’ultimo rimasto spegne slicing — i limiti sopravvivono a OFF e tornano quando si riprende una modalità). **EDIT (tap)** ascolta lo slice selezionato con playback one-shot (funziona in tutte le modalità slice; la preview si ferma dopo il rilascio del tasto o l’uscita dalla schermata). La casella mostra `-` in OFF.
  - **Frame** mostra lo start frame dello slice corrente in esadecimale. **EDIT + [LEFT/RIGHT]** lo sposta di dieci frame (zoomando sul marker mentre si tiene premuto **EDIT**), **EDIT + [UP/DOWN]** lo sposta di cento e torna sulla vista completa. Un marker resta dentro il proprio slice: non può superare l’inizio dello slice precedente o successivo. **EDIT + OPT** cancella lo slice.
  - **B** esce alla schermata dello strumento dalla riga Slice come altrove; **SHIFT + LEFT** esce da qualsiasi punto.
  - **Workflow LAZY**: con LAZY selezionato, tocca **PLAY** per avviare un playback one-shot dell’intero sample — suona una volta dall’inizio e si ferma automaticamente alla velocità e pitch originali (un marker verde di due pixel segue la posizione sulla forma d’onda); toccare nuovamente **PLAY** ferma prima. Mentre suona, ogni click di **EDIT** piazza uno slice nella posizione di riproduzione, ovunque si trovi il cursore (slice più vicini di 50 ms a uno già esistente vengono rifiutati con `Too close to slice`). Riprodurre il sample aggiunge nuove slice nell’ordine alle posizioni toccate, mantenendo quelli esistenti — la casella **Count** si aggiorna per ogni slice piazzato e il browser **Slice** si muove su quello nuovo. Le stesse regole di browsing e modifica degli altri slice valgono per quelli manuali: **EDIT + direzione** su **Slice** li esplora, **Frame** regola lo start selezionato, **EDIT + OPT** cancella lo slice attuale. La cella Frame si attenua mentre il drop da playback è attivo. **SHIFT + PLAY** non è intercettato in LAZY: avvia il playback di phrase come in qualunque altra schermata. Gli slice LAZY suonano nel sequencer come EQUAL e AUTO: le note Phrase mappano cromaticamente da **C-0** sulle slice manuali, con ritorno all’inizio dopo l’ultimo.
  - **Suggerimenti combo**: mentre il cursore resta su una cella della riga Slice e nessun altro messaggio è attivo, la barra di stato mostra l’indizio di quella cella e lo mantiene finché il cursore resta. Su **Mode** l’indizio è legato alla modalità attiva e mostra solo la descrizione (il nome della modalità è già nella cella): `Divides sample in equal parts`, `Divides sample based on transients`, `Press Play and add slices with EDIT` — cambiare modalità scambia subito il testo, OFF lo cancella. Su **Count** mostra `Adjust the number of slices`, su **Slice** `Browse slices`, su **Frame** `Adjust slice start`. Qualunque messaggio di azione sostituisce l’indizio finché non scade.
- **Process** seleziona un’operazione d’editing distruttiva: **Crop**, **Normalize**, **Delete**, **Silence**, **Fade In**, **Fade Out** o **Reverse** (EDIT + left/right cicla, tap cicla avanti, EDIT + OPT disattiva). **GO** (stessa riga) esegue l’operazione sulla selezione corrente, o sull’intero sample se vuota. **Crop** mantiene solo la selezione; **Normalize** scala la selezione al picco massimo (entrambi i canali condividono lo stesso guadagno per mantenere l’immagine stereo); **Delete** rimuove la selezione e unisce le code (cancellare l’intero sample è rifiutato); **Silence** azzera; **Fade In**/**Fade Out** applicano un fade lineare nella selezione; **Reverse** suona al contrario la selezione (i frame sono invertiti in loco, entrambi i canali di un frame si spostano insieme, la lunghezza e i marker restano invariati). Crop e Delete richiedono una selezione — se vuota mostrano `Select region first`. Ogni operazione pausa leggermente l’audio, conserva un undo a livello unico, e segna il sample come modificato in RAM: il file su disco non è toccato fino ai flussi di salvataggio (vedi sotto), uscire dalla schermata cancella l’undo.
- **UNDO** (vicino a GO) riporta il sample allo stato prima dell’ultima operazione. È attenuato finché non viene fatta un’operazione, alterna tra pre e post operazione a pressioni ripetute, e si azzera entrando nella schermata.
- **File** contiene i flussi di salvataggio. Lo strumento memorizza il percorso completo del WAV da cui è stato caricato; questi flussi scrivono quel file o puntano lo strumento su uno nuovo. Non toccano mai il nome dello strumento.
  - Il campo File cicla tra **Save** e **Save As** (EDIT + left/right o tap); **GO** (stessa riga) esegue l’azione mostrata.
  - **Save** sovrascrive il WAV da cui il sample è stato caricato. Per un sample senza slice chiede prima `Overwrite <name>?`. Un sample con slice apre il dialog **SAVE SLICES** chiedendo dove salvare i punti slice: **SAVE TO SAMPLE** li scrive nel WAV come cue chunks (portabili — i DAW li mostrano come marker) e li mantiene anche nel progetto, **SAVE TO PROJECT** scrive un WAV semplice e mantiene i punti solo nel progetto, **CANCEL** annulla. Una casella `Don't ask again in this project` memorizza la scelta nel progetto e salta il dialog da allora (la scelta è salvata come `- Sample save choice:` nel file .cct e si può modificare solo editando il file). Il dialog è saltato per sample senza slice. **Save** è attenuato (e saltato nella navigazione) finché non è caricato un sample con percorso file. Un errore di scrittura mantiene il marcatore sporco e mostra l’errore.
  - **Save As** scrive il sample corrente su un nuovo file: inserisci un nome file (precompilato col nome attuale senza estensione), poi scegli una cartella nel browser. Il sample è scritto come `<folder>/<name>.wav`, lo strumento punta sul nuovo file, e la cartella è ricordata come cartella sample predefinita. È attenuato (e saltato nella navigazione) finché non ci sono dati sample caricati, così un sample appena caricato può ricevere un percorso. I percorsi più lunghi di 255 caratteri sono rifiutati.
  - Il marcatore `*` prima del nome file indica che il sample in RAM differisce dal file su disco (qualunque operazione di processo lo imposta, Save e Save As lo cancellano). Il marcatore è solo di sessione: non si salva nel progetto e si resetta entrando nella schermata. Se più strumenti usano lo stesso file WAV, salvare uno sovrascrive il file per tutti.
- Nella schermata Sample dello strumento, usa **EDIT + [LEFT/RIGHT]** per caricare il WAV precedente o successivo nella stessa cartella.
- **Pitch** trasporta in semitoni (`-48` a `+48`).
- **Loop** seleziona Off, Loop o Ping-Pong.
- **Speed** controlla il time-stretching granulare da `0%` a `500%` (`100%` è normale).I formati WAV non supportati mostrano un errore. Converti i file insoliti in WAV `PCM8`, `PCM16` o `PCM24` prima di importarli.

##### Ingresso audio

Apri **Settings → Audio → Audio Input** prima di registrare da un microfono o da un’interfaccia audio USB. L’elenco mostra gli ingressi disponibili in quel momento sul dispositivo. Se un’interfaccia armata viene scollegata, la cattura si interrompe e il gestore audio viene riavviato in modo che l’app continui a funzionare. Se stavi registrando, premi **STOP** per terminare la ripresa interrotta. Dopo il ricollegamento, seleziona **[Refresh]** e verifica che l’ingresso desiderato sia armato prima di registrare di nuovo; il solo ricollegamento non lo riarma in modo affidabile. **[List]** rilegge l’elenco degli ingressi corrente senza riavviare l’audio.

Seleziona la casella **ARM** di un ingresso per renderlo disponibile al registratore del Sampler; il suo piccolo indicatore mostra il livello in ingresso. Arma una sola interfaccia alla volta: questa versione apre solo la prima interfaccia armata, anche se più caselle sono selezionate. La tua selezione è ricordata per nome del dispositivo, quindi un’interfaccia non disponibile resta disarmata invece di essere sostituita da un altro ingresso. Premi **OPT** per tornare alle impostazioni Audio.

##### Registrazione dell’ingresso audio

Nella schermata strumento Sample, **REC** (o **EDIT/REC** quando è caricato un sample) apre l’editor di sample, che riunisce registrazione e modifica in una schermata. Arma prima gli ingressi nella schermata **Settings → Audio → Audio Input** (vedi [Ingresso audio](#ingresso-audio)). Le righe superiori dell’editor controllano la registrazione:

- **Input** scorre la sorgente di registrazione: `T1`..`T8` registrano le otto tracce del tracker, seguite dal microfono o dall’interfaccia USB armati. Per registrare una sorgente esterna, continua oltre `T8` finché compare il nome del dispositivo. `-` significa che nessuna sorgente è stata scelta esplicitamente; **REC** usa allora il primo ingresso armato e ne mostra il nome. Tieni premuto **EDIT** e premi **LEFT**/**RIGHT** per andare indietro/avanti, oppure **EDIT+OPT** per tornare a `-`. Toccare Input lo fa avanzare direttamente.
- L’indicatore di livello accanto a **Input** mostra il livello in tempo reale dell’ingresso armato (solo sorgenti esterne). **MON** attiva il monitoraggio dell’ingresso: l’ingresso armato viene inviato all’uscita a -6 dB per ridurre il feedback. L’impostazione è salvata nel file delle impostazioni.
- **REC** avvia la registrazione. Le sorgenti esterne registrano subito; una sorgente di traccia mostra **ARMED** e la registrazione inizia quando parte la riproduzione (premi **PLAY**). **STOP** termina la registrazione: il WAV viene scritto nella cartella di esportazione del progetto (`recNNN.wav`), caricato nello strumento Sampler corrente e la Region torna a essere l’intero sample. Uscire dalla schermata con **OPT** durante una registrazione in corso la termina allo stesso modo.
- **PLAY** (mostrato dopo una registrazione) ascolta la ripresa; diventa **STOP** mentre suona l’anteprima. **CLEAR** (accanto) elimina il file WAV e scarica il sample dallo strumento; dopo l’eliminazione, **REC** riprende il suo posto.
- La forma d’onda mostra l’ingresso in tempo reale durante la registrazione e la forma d’onda completa a registrazione terminata. La riga di stato mostra il nome del file durante la registrazione e il numero di frame dopo; una ripresa esterna mostra inoltre `drop 0` quando non è andato perso alcun frame audio. Un sample registrato è completamente modificabile: regione di loop, selezione, slicing, elaborazione e salvataggio funzionano come con qualsiasi file caricato.

Le registrazioni sono scritte alla frequenza di campionamento audio selezionata, come WAV stereo a 16 bit. Le sorgenti di traccia catturano ciò che gli strumenti della traccia renderizzano (dopo gli FX); le sorgenti esterne catturano direttamente l’ingresso del dispositivo.

#### 2xSCWF

2xSCWF è un oscillatore a doppia forma d’onda single-cycle. Carica 1 WAV mono contenente esattamente 1 ciclo in ciascun oscillatore. Entrambi leggono in avanti e ciclano alla fine del periodo.

Il controllo Mix sfuma in crossfade A e B.

Detune è fine ed esponenziale da unisono fino a `+200 ct`, poi avanza a passi di semitono da `+3 st` a `+24 st`.

2xSCWF considera ogni file come una tabella a 1 ciclo e lo legge esattamente una volta per ciclo di oscillatore. Questo consente di fondere e detunare 2 onde single-cycle.

La cartella factory `waveforms/AKWF/` contiene forme d’onda dalla libreria Adventure Kid. Ce ne sono molte altre online se vuoi ampliare la collezione.

#### BYOWTBL

BYOWTBL è un oscillatore wavetable doppio compatibile con le tabelle di Serum. Ogni oscillatore carica un WAV mono contenente frame single-cycle consecutivi. `Pos A` e `Pos B` scansionano le loro tabelle indipendentemente da `00` a `FF`. Il motore interpola linearmente all’interno di ogni frame e tra frame adiacenti. `Mix` sfuma i due oscillatori, mentre `Detune` sposta l’oscillatore B.

### Famiglia di strumenti FM

#### Strumenti nativi OPLL e VRC7

Il selettore Instrument Type include un gruppo FM con **OPLL / MSX** (YM2413) e **VRC7** (DS1001). Ognuno offre **73 programmi** in liste Bank / Preset scorrevoli: i 15 toni originali, 40 toni distinti aggiuntivi dalle tabelle YM2413/VRC7/YMF281B di emu2413, e 18 programmi a due operatori creati da ChooChoo. I byte dei toni duplicati esatti sono omessi in ogni motore. Le palette aggiuntive usano lo slot tono programmabile del chip; non espandono la ROM fisica. Il programma zero identifica un tono personalizzato. EDIT + freccia sinistra/destra seleziona il preset precedente o successivo nel filtro bank. Fine ct regola la sintonia da -100 a +100 cent.

Nella lista programmi, EDIT + PLAY ascolta il suono evidenziato; rilasciando i tasti si ferma. EDIT conferma, OPT annulla. Scorrere o ascoltare i suoni non cambia lo strumento, la tabella o la canzone. Il gesto normale di audition nella pagina Instrument funziona ancora dopo la selezione. Gli inviluppi chip nativi gestiscono attack/release; l’inviluppo Amp opzionale aggiunge un ADSR software (vedi controlli chip nativi sotto). Non esiste un editor completo per gli operatori.

#### AdLib / OPL2 e OPL3

FM include anche AdLib / OPL2 (YM3812) e OPL3 (YMF262). Bank filtra la lista factory; Preset apre gruppi di categorie e una vista All. EDIT + PLAY ascolta prima della selezione, EDIT conferma, OPT annulla, e EDIT + freccia carica la voce precedente/successiva corrispondente. Fine ct regola la sintonia locale. Mode mostra 2 operatori, 4 operatori, o Dual voice. OPL2 nasconde patch incompatibili OPL3; OPL3 può riprodurre la collezione comune a due operatori.

I titoli popup di bank e preset indicano il motore corrente. Cambiare motore o slot strumento resetta il filtro bank a **All banks**, anche passando tra i motori AdLib e OPL3 compatibili. Tornare da un popup nello stesso slot mantiene il filtro scelto. Cancellare l’importazione di una bank DX7 ripristina il filtro precedente.

La conferma del preset pubblica anche le modifiche fatte al rilascio del tasto al successivo tick UI/audio. Le canzoni in riproduzione non richiedono un altro tasto o un restart del transport per applicare il patch. L’output segue comunque il buffer audio configurato e il comportamento dell’envelope/retrigger del motore.

La collezione factory contiene 697 elementi originali da The Fat Man 2-op, The Fat Man 4-op e DMXOPL3, con 589 patch uniche normalizzate. Gli alias mantengono i nomi originali e l’attribuzione. I livelli operatori nativi, flag di profondità LFO, offset note, intonazioni fisse per la percussione e tuning dual-voice sono mantenuti. Il volume tracker usa ampiezza software; non riproduce le curve di volume MIDI o policy di offset velocity originali. Le stime di durata di release non tronca note sostenute. Alcuni effetti hanno attacchi lenti: tieni premuto Seashore invece di aspettarti un click breve per sentirlo.

Selezionare un preset non modifica la tabella dello slot né gli inserti di traccia.

#### Strumenti nativi Sega PSG e Game Boy

CHIP include Sega PSG, GB Pulse e GB Noise. Su Sega PSG, **Bass range: Extended** mantiene distinte le note basse. Scegli **Chip** per il range originale limitato, dove le note più basse convergono. Il rumore a rate fissa mantiene il pitch originale; il rumore derivato da tono segue il bass range selezionato.
Gli strumenti GB usano i registri pulse/noise DMG; non è esposto il canale onda del Game Boy. Le loro liste preset contengono ora rispettivamente **24, 24 e 28** preset. Le aggiunte coprono rumore Sega periodico/collegato, sweep di pitch GB, percussioni brevi, rumore metallico, drone e crescendi. Sono programmi originali, non estratti da giochi.
La pagina offre controllo nativo di mode/duty/width, envelope/sweep/noise come applicabile, più un ADSR software di ampiezza.

#### DX7 FM

Seleziona **Type → FM → DX7 FM**; DX7 è la settima voce dopo Arcade / YM2151.
DX7 FM usa sei operatori. La pagina mostra Bank, Preset e Fine ct accanto alle impostazioni comuni. Il browsing preset usa liste bank/categoria. Tieni premuto EDIT+PLAY per ascoltare il suono evidenziato; rilascia per fermare. Solo EDIT conferma al rilascio; OPT annulla. Caricare un suono copia il patch completo senza cambiare tabelle tracker o insert FX traccia.

Per una libreria personale persistente, metti i file `.syx` in
**`instruments/banks/dx7/`** accanto alla libreria strumento esistente. Le sottocartelle sono supportate. Apri **DX7 → Bank**: ogni file appare come una bank nominata, pronta per il browsing Preset. Riapri Bank dopo aver aggiunto o rimosso file; non serve conversione o caricamento strumento. Una bank originale DX7/TX7 standard contiene **32 voci**. Un file con quattro messaggi bank appare come quattro bank numerate (128 voci totali). Sono accettati anche file single-voice. Il patch selezionato viene salvato con lo strumento, quindi rimuovere la bank sorgente non cambia la canzone.

**Load Instrument** accetta anche singole voci `.syx` e bank da 32 voci originali DX7/TX7. Seleziona un patch per usarlo, o annulla per mantenere lo strumento attuale. Usa la cartella Bank sopra per una libreria persistente; salva la canzone o un `.cni` per conservare un patch modificato. Sono rifiutati formati Yamaha non supportati.

DX7 ha un limite di **16 note attive contemporaneamente nella canzone**, inclusi gli accordi e le code di rilascio. Al raggiungimento del limite, prima vengono terminati i release tail, poi le note trattenute silenziose, per permettere nuovi attacchi. L’audizione preset non è disponibile durante la riproduzione della canzone.

#### Editor operatori FM

Gli strumenti Genesis FM, Arcade FM, OPL2, OPL3, OPLL e VRC7 hanno un pulsante **EDIT** a destra di Fine ct. Apre una tabella con i parametri del patch:

- Genesis FM / Arcade FM
- OPL2 / OPL3: su OPL3 la modalità (2op, 4op, dual) si cambia qui.
- OPLL / VRC7: modificare un preset ROM lo trasforma in “Custom”.

Freccia sinistra/destra e i tasti abituali modificano i valori; lo strumento continua a suonare per ascoltare i cambiamenti. La barra di stato descrive il parametro sotto il cursore. OPT o Left+Shift tornano alla schermata strumento. DX7 e SID non hanno editor operatori.

Genesis FM (YM2612) e Arcade FM (YM2151) condividono browser Bank/Preset, audition e controllo fine-tune come gli altri motori FM. I loro inviluppi nativi controllano il release; cambiare volume o pitch tracker non li riattiva.

#### Preset FM factory e USER

Gli strumenti chip nativi usano **Bank:** per scegliere ALL, una collezione inclusa o USER. **Preset:** sceglie un suono in quella selezione. Tieni premuto EDIT e premi sinistra o destra su entrambe le righe per ciclare; un breve tap su EDIT apre il selettore.

Le collezioni incluse sono pacchetti ZIP in `instruments/FACTORY/`. I file USER vanno in `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse`, o `gb-noise`.
Sfoglia direttamente le cartelle o metti un ZIP di preset compatibili. Gli ZIP restano intatti su disco. Queste cartelle accettano file `.cni` compatibili; CNI è il formato portable di ChooChoo/ChipNomad per strumenti, non uno standard chip. Il browser USER legge anche i seguenti formati sorgente, inclusi dentro ZIP:

| Cartella USER | Formati aggiuntivi |
| --- | --- |
| `dx7` | Voci singole e bank da 32 voci DX7/TX7 `.syx` Yamaha |
| `genesis` | `.tfi`, DefleMask `.dmp`, Furnace `.fui` |
| `arcade` | bank VOPM `.opm`, DefleMask `.dmp`, Furnace `.fui` |
| `opl2`, `opl3` | `.wopl`, `.opli`, `.woplx`, `.sbi`, Furnace `.fui` |
| `opll`, `vrc7` | patch personalizzate Furnace `.fui` e selezioni ROM fisse |
| `sega`, `gb-pulse`, `gb-noise` | Furnace `.fui`, DefleMask `.dmp` |
| `sid` | GoatTracker 2 `.ins` (GTI5), Furnace `.fui` |

Una bank apre voci compatibili. File o funzionalità non supportati riportano un motivo; voci incompatibili in bank misti sono escluse mantenendo numeri sorgente stabili. Il caricamento è transazionale: un preset rifiutato lascia lo strumento corrente inalterato. I preset sorgente sostituiscono la tabella tracker dello strumento con una vuota, tranne SysEx DX7 che mantiene la tabella esistente. CNI ripristina la sua tabella salvata. I patch caricati appartengono alla canzone e restano funzionanti anche spostando o cancellando il pacchetto sorgente.

Le bank WOPL possono contenere slot inutilizzati senza un flag esplicito di vuoto. Il browser omette slot senza nome i cui operatori attivi sono completamente attenuati senza attack. Voci senza nome ma riproducibili restano disponibili come `Program N [Bbank]` o `Drum N [Bbank]`, usando i numeri program e bank zero-based della sorgente. Uno strumento singolo OPLI senza nome usa il nome file. Patch silenziose nominate vengono mantenute.

I file Furnace, DefleMask e GoatTracker vengono importati come strumenti, non come canzoni. Macro sorgente animate, campioni e parametri di playback specifici possono non trasferirsi. Metti i file Game Boy nella cartella Pulse o Noise corrispondente.

Il ciclo di preset USER continua attraverso file, cartelle, bank e ZIP in entrambe le direzioni, avvolgendosi agli estremi. Riaprire il browser USER segue la selezione corrente. ALL include suoni USER sotto Unsorted per i motori categorizzati; USER rimane una scelta di livello superiore separata. Sega PSG e Game Boy usano liste flat ALL/Factory e un browser USER gerarchico, senza pannello Unsorted. I vecchi file DX7 in `instruments/banks/dx7/` restano accessibili in USER.

Un motore mostra solo le collezioni incluse compatibili. Factory Presets combina i set toni OPLL/VRC7 inclusi per il motore selezionato; DX7 unisce gli originali ChooChoo e YSE. Le collezioni scaricate con nome restano separate. Fat Man 2-op è elencato sotto OPL2 e Fat Man 4-op sotto OPL3. File CNI OPL2 compatibili possono ancora essere usati nelle cartelle USER OPL3.

#### Controlli chip nativi

Le anteprime delle forme d’onda degli strumenti nativi si aggiornano al ridisegno dei controlli.
Gli strumenti FM possono sovrapporre la curva opzionale Amp env; SID mantiene l’anteprima forma d’onda senza questa sovrapposizione FM, anche scorrendo bank e preset.

Tutti i sette motori FM offrono **Bright**, **Feedback** e **Amp env**. Bright va da -63 a +63, con zero che preserva il patch; cambia i livelli degli operatori di modulazione mantenendo i portanti. L’effetto udibile dipende dall’algoritmo. Feedback di default è **Preset**, oppure sovrascrive il feedback nativo con valori 0–7. Questi controlli non riscrivono i byte operatori nativi salvati.Amp env predefinito è **Bypass**. Seleziona **ADSR** per aggiungere attacco, decadimento, sustain, rilascio e modellare il suono nativo. A/D/R usano l’intervallo quadratico condiviso da 0 a 5 secondi; il sustain va da silenzio al livello massimo. Le envelop native continuano a funzionare, quindi questa envelope non può estendere un suono oltre il rilascio nativo. La navigazione dei preset preserva i controlli FM amp e tone dello slot.

I seguenti FX frase appaiono anche come destinazioni supportate per modulazione e motion-recording. I valori nella colonna FX sono esadecimali. Le impostazioni si applicano alla riproduzione senza cambiare lo strumento salvato. I controlli sweep e envelope GB nativi si attivano al trigger della nota successiva; duty/width e frequenza del rumore possono cambiare dal vivo.

| Engines | FX | Control |
|---|---|---|
| Native FM | `OL1`–`OL6` | Livelli assoluti di uscita degli operatori: modulatori OPLL/VRC7 e OPL `00–3F`; carrier OPLL/VRC7 `00–0F`; Genesis/Arcade `00–7F`; DX7 `00–63` (0–99). Valori più alti significano maggiore uscita. Solo operatori supportati appaiono. |
| OPLL/VRC7/OPL2/OPL3 | `OAR`, `ODR`, `ORR`, `OSL` | Attacco, decadimento, rilascio e attenuazione sustain dell’operatore 1, `00–0F`. |
| OPLL/VRC7/OPL2/OPL3/Genesis/Arcade | `OMU 00–0F` | Moltiplicatore di frequenza operatore 1. |
| Genesis/Arcade | `LFR` | Velocità LFO nativo: Genesis `00–07`, Arcade `00–FF`. |
| Arcade | `LAD`, `LPD 00–7F` | Profondità separate di ampiezza e pitch LFO. |
| Genesis/Arcade | `LAS`, `LEN` | Sensibilità ampiezza nativa `00–03` e abilitazione LFO `00–01`. |
| Genesis/Arcade | `LPS 00–07` | Sensibilità pitch nativa. |
| SID | `SAT`, `SDE`, `SSU`, `SRL 00–0F` | Attacco, decadimento, sustain e rilascio nativi. I valori tempo aumentano fino a `0F`; sustain aumenta fino a livello massimo. |
| SID | `SPR 01–10` | Frequenza partner silenzioso da 1× a 16×; influenza ring modulation e hard sync. |
| Tutti i native FM | `FBK 00–07` | Feedback nativo assoluto, inizializzato dallo strumento. |
| Tutti i native FM con Amp env abilitato; Sega/GB | `EAT`, `EDC`, `ESU`, `ERL`, `ESH 00–FF` | Attacco, decadimento, sustain, rilascio, forma. Non abilitano amp FM bypassato. |
| Sega PSG | `CMD 00–02`, `CNR 00–03` | Tonalità / rumore bianco / rumore periodico; tre velocità rumore fisse o derivata dal tono. |
| GB Pulse | `CMD 00–03` | Duty pulse nativo. |
| GB Noise | `CMD 00–01`, `CND 00–07`, `CNS 00–0D` | Larghezza rumore, divisore clock e shift. |
| GB Pulse | `CSP 00–07`, `CSS 00–07`, `CSD 00–01` | Periodo sweep, shift, direzione verso il basso. |
| GB Pulse / Noise | `CEI 00–0F`, `CEP 00–07`, `CED 00–01` | Livello iniziale envelope nativo, periodo, direzione verso l’alto. |

La selezione FX nativi usa lo strumento nella colonna `I` della riga frase, o lo strumento attivo trovato guardando indietro se `I` è vuoto. Il popup FX mostra una breve descrizione del controllo, il valore preset attuale e l’intervallo comando valido in un blocco compatto. Selezionare un effetto diverso parte da quel valore; riaprire lo stesso effetto preserva il valore modificato. Una selezione multi-riga risolve separatamente lo strumento di ogni riga. Le tabelle usano il loro contesto strumento. Le modifiche dei valori nativi si fermano agli estremi legali, incluse duty, rumore, sweep, ADSR, feedback e livelli operatori. I valori sono esadecimali: ad esempio, il massimo `63` su DX7 significa decimale 99.

#### Strumenti SID e Phrase FX

Scegli **CHIP → SID**. Bank offre **ChooChoo SID Originals** e **SIDkit Effects**. Sfoglia, ascolta e salva come gli altri preset native-chip.

La pagina dello strumento espone waveform/pulse width, modalità filtro/cutoff/reso e controlli ADSR nativi. **EDIT**, vicino al nome preset, apre **SID EDIT**. Wave e PW hanno etichette separate; Filter ha una riga modalità, poi Cutoff e Reso; etichette ADSR sono accanto ai valori. Toccare ogni campo seleziona quel controllo preciso. Vengono conservati intervalli parametri esistenti e comportamento di regolazione fine/grossa.

SID EDIT usa le righe compatte dell’editor FM, colori del tema, valori esadecimali e controlli EDIT+direzione. OPT o Left+Shift torna indietro; EDIT+PLAY ascolta lo strumento. La pagina modifica direttamente la patch salvata, quindi i cambiamenti si salvano con strumento e progetto.

SID EDIT raggruppa waveform, ring/sync, ADSR, pulse width, filtro, motion, arpeggio, pitch sweep e vibrato. La barra stato identifica ogni controllo e il suo intervallo. Ring e sync usano ciascuno due delle quattro voci SID disponibili, così un accordo SID può usare meno note quando uno dei due è attivo.

| FX | Range | Azione |
| --- | --- | --- |
| `SCP` | `00–FF` | Base pulse-width, scalata a larghezza nativa a 12 bit; resta attivo il movimento pulse della ricetta. |
| `SCT` | `00–FF` | Base cutoff, scalata a cutoff nativo a 11 bit; resta attivo il movimento filtro della ricetta. |
| `SRN` | `00–0F` | Risonanza nativa. |
| `SWV` | `00–07` | Triangle, saw, tri+saw, pulse, tri+pulse, saw+pulse, tri+saw+pulse, noise. |
| `SFI` | `00–07` | Bit modalità filtro: 1 low-pass, 2 band-pass, 4 high-pass; 0 bypass. |
| `SMR` | `00–FF` | Clock macro ricetta, scalato da 1 a 200 Hz. |
| `SRG` | `00–01` | Ring modulation tri con silent partner. |
| `SSY` | `00–01` | Sync oscillatore con silent partner. |

Pulse width e cutoff supportano `SLE`. Forme d’onda discrete, switch, risonanza e feedback FM mantengono i loro passaggi nativi utili; valori byte più ampi non creerebbero più stati hardware. `FBK` seleziona feedback `00–07` direttamente. I livelli operatore usano gli intervalli specifici per motore sopra elencati. Luminosità e regolazioni master mantengono l’intero intervallo byte.

#### Preset factory e USER per SID e native-chip

Gli strumenti native-chip usano **Bank:** per scegliere ALL, una collezione inclusa o USER. **Preset:** sceglie un suono dentro quella selezione. Tieni premuto EDIT e premi Left o Right su una riga per ciclare; un breve tocco EDIT apre il selettore.

Le collezioni incluse sono ZIP sotto `instruments/FACTORY/`. I file USER sono sotto `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse`, o `gb-noise`. Sfoglia direttamente le cartelle o metti un ZIP di cartelle e preset `.cni` compatibili. DX7 accetta anche singole voci `.syx` supportate e banche da 32 voci, anche dentro ZIP. Gli ZIP restano intatti su disco. Non è supportato il caricamento diretto WOPL/WOPLX; usa preset CNI compatibili o converti offline i formati supportati.

Il ciclo dei preset USER continua attraverso file, cartelle, banche e ZIP in entrambe le direzioni, girando agli estremi. Riaprire il browser USER segue la selezione attuale. ALL include suoni USER sotto Unsorted per motori categorizzati; USER resta una scelta top-level separata. Sega PSG e Game Boy usano liste flat ALL/Factory e un browser USER gerarchico, senza pannello Unsorted. I vecchi file DX7 in `instruments/banks/dx7/` restano accessibili in USER.

Un motore mostra solo le collezioni incluse compatibili. Factory Presets combina i set tono OPLL/VRC7 inclusi per il motore selezionato; DX7 combina originali ChooChoo e YSE. Le collezioni scaricate rimangono separate. Fat Man 2-op è listato sotto OPL2 e Fat Man 4-op sotto OPL3. File CNI OPL2 compatibili possono ancora essere usati in cartelle USER OPL3.

## 6. Modulazione e motion recording

Ogni strumento può avere fino a 4 slot di modulazione. Strumenti audio possono indirizzare **Instrument Pan** o **Track Pan**; entrambi usano `00` per sinistra, `80` per centro e `FF` per destra.

I tipi di modulazione disponibili sono:

- ADSR - classica envelope attacco-decay-sustain-release
- AHD - envelope attack-hold-decay
- LFO - oscillatore a bassa frequenza con varie forme
- syncLFO (ex SLFO) - LFO con un’impostazione di tick per unità e moltiplicatore di periodo
- slowLFO - cicli lunghi con un controllo di periodo proporzionale, che inizia subito oltre il range dell’LFO ordinario
- FLFO - LFO a frequenza audio che può arrivare a kHz; FLFO può essere pesante per la CPU
- SLIN - controllo joystick lineare
- SRAT - controllo joystick basato sulla velocità
- Sequence - legge la sequenza del tracker come segnale di modulazione: **PITCH** è l’intonazione sequenziata finale, **VOL** il volume sequenziato finale e **TRANCE GATE** un modulatore ritmico indipendente. Motif, Rate e Length scelgono il pattern del gate; Trigger sceglie se gira libero, si ritriggera a ogni nota o si riavvia ai confini di frase o catena. I gate chiusi danno zero, quelli aperti mezzo livello, e Accents distribuisce in modo euclideo gate a livello pieno tra i gate attivi. Non usa né modifica alcun inserto di traccia.

La lista delle destinazioni di modulazione dipende dal tipo di strumento.

Quando una envelope ADSR o AHD indirizza il Volume, diventa l’envelope di volume dello strumento. Un LFO che indirizza il Volume invece offsetta il volume di uscita corrente.

La quantità di modulazione può essere positiva o negativa. Il range dipende dalla destinazione. Se il range completo della destinazione è `127` o meno, come per la maggior parte dei parametri, la quantità definisce un range assoluto. Destinazioni più ampie come il pitch usano una quantità scalata.

Le sorgenti Sequence PITCH e VOL hanno un **Mode**. **Normal** mantiene udibile il valore del tracker e lo usa anche come modulazione. **Bypass** lo trasforma in sola sorgente: PITCH ritriggera sull’ultima intonazione (o C-4 alla prima nota) e VOL mantiene l’ultimo livello udibile. Trance Gate è già solo sorgente. Se uno slot Sequence attivo mette PITCH o VOL in bypass, vince il bypass. Per evitare feedback, PITCH e VOL non possono puntare a Pitch o Volume.

Tutti i parametri di durata, incluso attacco, hold, decadimento e periodo, sono misurati in tick.

**syncLFO** è il vecchio SLFO: regola i suoi tick e il suo moltiplicatore per un movimento sincronizzato al tempo. Le impostazioni SLFO esistenti e i brani mantengono lo stesso timing.

Usa **slowLFO** per sweep più lunghi di quanto possa fare l’LFO ordinario. Il suo controllo Period va da 256 a 131.072 tick. Tutti gli LFO basati sui tick seguono il tempo del brano e condividono le stesse forme, la stessa selezione di wavetable e le stesse modalità di trigger. Scegli Free per uno sweep che continua tra le note; Retrig, Phrase e Chain lo riavviano ai rispettivi confini.

Forme LFO:

- Tri - onda triangolare bipolare
- Sin - onda sinusoidale bipolare
- UniTri - onda triangolare unipolare
- UniSin - onda sinusoidale unipolare
- RampDn - rampa lineare discendente (onda a dente di sega)
- RampUp - rampa lineare ascendente (onda a dente di sega)
- ExpDn - rampa esponenziale discendente (onda a dente di sega esponenziale)
- ExpUp - rampa esponenziale ascendente (onda a dente di sega esponenziale)
- Square - onda quadra
- Random - onda casuale sample-and-hold
- Wavetable - legge una delle 32 wavetables AY del progetto. I valori `00` a `0F` mappano da `-100%` a `+100%` senza interpolazione.

Tipi di trigger LFO:

- Free - riavvia l’LFO solo quando cambia lo strumento
- Retrig - riavvia l’LFO a ogni nota
- Phrase - riavvia LFO, syncLFO e slowLFO all’ingresso in una nuova frase
- Chain - riavvia LFO, syncLFO e slowLFO all’ingresso in una nuova catena
- Hold - si ferma dopo 1 ciclo e mantiene l’ultimo valore
- Once - si ferma dopo 1 ciclo e ritorna a zero

### Motion recording

- **STICK LIVE** (`L1` di default) applica modulazione stick senza cambiare la frase. In **Settings > Stick live mode**, `HOLD` (default) lo abilita mentre si tiene premuto; `TOGGLE` lo attiva con una pressione e disattiva alla successiva; `FREE` lo mantiene sempre attivo. Rilasciando in modalità `TOGGLE` lo mantiene attivo.
- Durante la riproduzione, tieni premuto **MOTION RECORD** (`L2`) per applicare la modulazione stick e registrare le destinazioni modificate come valori FX assoluti.
- Tieni premuto **MOTION ERASE** (`R2`) per rimuovere FX destinazione corrispondenti dalla riga corrente.

Il `~` in basso a destra indica Stick live attivo. Record (`*`, o `!` per overflow) ed Erase (`x`) hanno priorità indicatore e sono momentanei in entrambe le modalità. Entrambi abilitano l’elaborazione stick live mentre tenuti; rilasciarli lascia intatto un toggle live abilitato separatamente.

La modalità si salva con le impostazioni dell’applicazione, ma il toggle live parte sempre spento all’avvio. Cambiare modalità resetta il toggle: `HOLD` segue il pulsante Stick live premuto, `TOGGLE` aspetta una nuova pressione, `FREE` è attivo immediatamente.

Il motion recording scrive FX traccia nella frase attualmente riprodotta. Aggiorna prima FX corrispondenti, poi usa slot vuoti da destra a sinistra. Non sovrascrive FX diversi. Se tutti e 3 gli slot sono pieni, quel movimento non si registra nello step. Un `!` in basso a destra indica che più destinazioni sono cambiate di quante le 3 colonne FX potessero contenere.

Il motion recording supporta destinazioni Braids, Plaits, Sampler, 2xSCWF, BYOWTBL e Bogie, inclusi macro Bogie attivi e controlli filtro quando applicabili.

## 7. Effetti insert traccia

Ognuna delle otto tracce ha due slot seriali, **TF1 → TF2**. Da Instrument, usa la consueta gesture Shift+Up per aprire MOD, poi Shift+Up di nuovo per **F: Insert FX**. Shift+Down torna a MOD. OPT+Left/Right seleziona la traccia (1–8), senza cambiare il selettore strumento. L’intestazione modulo apre il selettore esistente; il bypass mantiene modulo, valori configurati e automazione. I parametri sono valori byte: colonna sinistra 1–4, colonna destra 5–8. Il testo di contesto mostra nomi completi e valori decodificati. I progetti vecchi si caricano con entrambi gli slot OFF.

Il display di navigazione mantiene visibile l’intera colonna F–M–I–P in tutte le schermate strumento, inclusi Insert FX e il pool strumenti, e evidenzia la schermata corrente.| Modulo | Parametri in ordine |
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

I controlli continui usano 00–FF, tranne FORMANT VOW (00–7F). I controlli bipolari hanno un neutro esatto a 80. Le modalità di distorsione sono 00 Density, 01 Drive, 02 Spiral, 03 Mojo, 04 Dyno. I rapporti del compressore 00–07 sono 1.5, 2, 3, 4, 6, 8, 16, 20:1; Compressor Detect 00–02 ascolta il proprio ingresso stereo, sinistro o destro; 03–0A ascolta invece le tracce 1–8. Il filtro detector a 80 è neutro, sotto è low-pass, sopra è high-pass. Una sorgente di traccia è presa dopo il suo volume, pan e Tilt EQ ma prima dei suoi inserti; una sorgente muta o assente contribuisce con silenzio. Qualsiasi slot può contenere un modulo qualsiasi; moduli ripetuti sono indipendenti. Chorus e Phaser iniziano con una modulazione udibile ma controllata. Riduci Mix, Depth o Feedback per un effetto quasi trasparente; valori elevati di Depth, Feedback e Mix diventano intenzionalmente estremi. Flanger e Rotary usano default di modulazione dolce. La Speed del Rotary usa l’intero intervallo **00-FF**: **00 = 0.01 Hz** (una rotazione ogni 100 secondi), **FF = 25 Hz** (il massimo Chaos precedente). La spaziatura esponenziale offre un controllo più fine sulle modulazioni LFO lente. I nuovi inserimenti Rotary partono da **8F**, circa 0.8 Hz. Ciò sostituisce le quattro velocità discrete; le impostazioni Rotary esistenti e le automazioni F11/F21 di velocità devono essere riadattate alla nuova scala (le vecchie Slow/Fast/Hyper/Chaos corrispondono approssimativamente a **8F/CB/E7/FF**). Altri parametri restano invariati. La profondità bit del Bitcrusher va da 4 a 16 bit e la riduzione di velocità da 1x a 32x. Le modalità Destruction sono Fold, Clip e Crush. Saturation e distorsione includono controllo di livello in uscita o mix per bilanciare il segnale processato con quello originale.
Trance Gate è un sequencer di ampiezza sincronizzato al tempo per pad, lead e staccati. I valori di Pattern 00–3F selezionano motivi musicali nominati; 00–1F sono groove base e 20–3F variazioni sperimentali. Rate offre valori normali, tripletto e puntati da 1/2 a 1/32. Length tronca il motivo ruotato indipendentemente di 16 passi a 1–16 passi. Ogni nota-on della traccia riavvia la sequenza; il note-off la fa continuare durante il release. Forward, Reverse e Ping-Pong definiscono la direzione; Chaos usa una mutazione ripetibile che mantiene la densità ogni quattro loop. All’inserto master segue il tempo del trasporto senza riattivare note. Gate imposta la durata di ogni step aperto, Smooth arrotonda i bordi mantenendo una dissolvenza minima senza click, e Mix sfuma tra segnale asciutto e gate.

**Beads** usa il DSP granulare originale di Mutable Instruments come inserto stereo. Scegli Beads in uno slot inserto traccia. I suoi otto controlli usano anche gli attuali comandi F11–F18 / F21–F28 e destinazioni MOD inserto; non servono nuovi FX ID o formato progetto. I granuli girano continuamente senza eventi aggiuntivi.

- **Time:** posizione nel buffer di registrazione di quattro secondi.
- **Size:** granuli lunghi al contrario a 00, granuli più corti intorno a 55, poi granuli più lunghi in avanti. FF attiva la modalità delay originale dopo la transizione.
- **Density:** granuli regolari sotto il centro, tempistica casuale sopra; la zona morta al centro ferma nuovi granuli. Allontanarsi dal centro aumenta la frequenza.
- **Pitch:** da −24 a +24 semitoni; 80 è unisono esatto.
- **Scatter:** 00 senza casualità. Aumentandolo applica la casualità uniforme interna di Time di Beads al massimo e di Size a metà, indipendentemente per ogni granulo. Pitch e Shape restano fissi.
- **Feedback:** rimanda i granuli processati nel percorso di registrazione.
- **Shape/Freeze:** 00–7F registra, 80–FF congela. In entrambi i casi i sette bit bassi sweepano lo stesso inviluppo da rettangolare a decadimento, campana e reverse. Aggiungi/sottrai 80 per attivare/disattivare frozen senza cambiare Shape: 40 e C0 hanno lo stesso inviluppo. Il suggerimento mostra la posizione dell’inviluppo e lo stato Recording/Frozen. Tieni premuto EDIT + Su/Giù per cambiare di 10 esadecimali (16); Sinistra/Destra di 1. Questo controllo è discreto quindi il cambio non sfuma sul bit freeze; la modulazione dell’intero byte può attraversare intenzionalmente i due stati.
- **Mix:** 00 è asciutto esatto, FF è completamente processato. Le variazioni sono smussate.

Freeze mantiene l’audio catturato dall’effetto; selezionare un altro modulo o resettare Beads lo elimina. Gli invii riverbero della traccia restano disponibili.

### Far pompare un kick sul mix o su un’altra traccia

Per un master che pompa, metti il kick sulla traccia 1, seleziona Compressor in MASTER FX, imposta Detect su 03 (Track 1), Ratio su 04 (6:1), Attack verso 60 (circa 1,6 ms), Release verso C0 (circa 115 ms), Makeup su 00 e Mix su FF. Abbassa Threshold finché gli altri suoni affondano a ogni kick, poi regola Release perché risalgano prima del successivo. Il suono diretto del kick mantiene i propri inserti di traccia ma evita i due slot di inserto master; i suoi ritorni di riverbero e delay pompano comunque.

Per abbassare solo un basso, un pad o un synth, metti il suo Compressor su quella traccia e seleziona la traccia del kick con Detect. Il kick sorgente resta udibile e il suo volume non è cambiato dal compressore di quella traccia. Un kick e un basso combinati sulla stessa traccia non si possono separare con questo instradamento.

### FORMANT

Seleziona **FORMANT** su TF1 o TF2 per modellare un suono in toni di tipo vocale. Funziona meglio con una sorgente sostenuta e ricca di armoniche, come un’onda a dente di sega.

Imposta MODE su `00` VOW e usa **VOW** per spazzare da A (`00`) a U (`7F`) passando per E, I e O. Imposta MODE su `01` CUSTOM per regolare tu stesso **F1** e **F2**. **RES** restringe o allarga il carattere vocale; **MIX** miscela l’effetto con il segnale originale. Usa MOD → INSERT FX per muovere i controlli attivi. MODE non è una destinazione MOD.

### Lane FX per Insert

I comandi inserto usano **F11–F18** per TF1 e **F21–F28** per TF2. Il roll FX e il popup Phrase/Table mostrano solo parametri usati dagli insert configurati nella traccia selezionata. Il popup li raggruppa come **TF1: nome effetto** e **TF2: nome effetto**; slot OFF sono nascosti. Gli insert bypassati restano modificabili. Nel popup FX di Phrase e Table la descrizione segue lo slot inserto della traccia selezionata: mostra modulo, nome parametro, estremi di range decodificati e valore base salvato. I comandi della song esistenti si mantengono se un cambio modulo li rende inutilizzati; aprire il selettore parte da un controllo disponibile. Durante la modifica di un valore, il suggerimento in basso mostra valore decodificato e unità. Qualsiasi colonna FX di Phrase o Table può indirizzare uno slot; la griglia Phrase mantiene le sue tre colonne. Questi valori immediati persistono tra note, strumenti, phrases, chain e loop normali. Un nuovo playback/stop dura resetta i valori. Modificare un valore base sulla pagina F cancella solo quell’override di parametro. MOD agisce sul valore base/automazione effettivo senza salvare offset. Selezionare un nuovo modulo ripristina i default e cancella gli override di quello slot. Il bypass mantiene l’automazione esistente sospendendo l’elaborazione dopo una corta dissolvenza.

**Gli indirizzi si riferiscono a slot di traccia/posizioni parametro, non a effetti nominati.** Un Phrase riutilizzato su un’altra traccia usa i moduli di quella traccia. Cambiare moduli può cambiare il significato di un comando; i comandi non vengono riscritti. Indirizzi OFF/non usati sono safe no-op e rimangono intatti nei dati salvati di Phrase/Table.

Il selettore MOD strumento aggiunge una categoria Insert FX. Le sorgenti di ogni strumento controllano gli insert su qualsiasi traccia lo riproduca, incluso il segnale combinato della nota. Lo stesso strumento su due tracce influenza catene indipendenti. Si applicano tempi sorgente esistenti, valori firmati, regole di combinazione sorgenti, modalità stick e registrazione movimento. I controlli discreti sono quantizzati e limitati alle scelte valide. Non esiste una nuova banca di modulazione di traccia.

Inizia con pochi insert, controlla sovraccarichi o crepitii, e bypassa gli effetti che non aiutano il brano. Drive e guadagno possono saturare il mix, usa l’indicatore clip del Mixer mentre li regoli.


## 8. Tabelle

Le tabelle sono uno strumento centrale per il sound design nei tracker. In termini Vortex Tracker, combinano strumenti e ornamenti, ma possono fare molto di più. Se conosci LSDj o NerdSEQ, l’idea ti sarà familiare.

La colonna Pitch accetta valori di pitch relativi (`~`) o assoluti (`=`) in semitoni. La colonna Volume va da `00` a `0F`, si applica a ogni motore sopra l’inviluppo e può essere usata come gate: alterna `0F` e `00` e quindi cicla con `HOP 00`. Le 4 lane FX corrispondono principalmente alle lane della phrase, anche se alcune FX si comportano diversamente nelle tabelle.

Mettere un FX `TIC` nell’ultima riga della tabella imposta la velocità di quella colonna e sovrascrive la velocità tabella predefinita dello strumento. Ogni colonna FX può correre a una velocità tick diversa. Le colonne Pitch e Volume seguono la velocità della 1ª colonna FX.

Una tabella da 16 righe può sembrare corta rispetto a uno strumento lungo di Vortex Tracker, ma l’FX `HOP` può creare loop condizionali come "ripeti queste righe 5 volte", oltre a loop annidati. Con loop e velocità colonne FX indipendenti, quelle 16 righe possono andare lontano.

È possibile copiare/incollare lane FX tra phrase e tabelle ;)

### Modalità Retrigger

Il campo `Retrig` in cima allo schermo Table seleziona quando una tabella già attiva si riavvia. `Inst` è la modalità predefinita e compatibile: una riga phrase contenente uno strumento seleziona la tabella di default di quello strumento e la avvia da riga `00`. `Phrase` riavvia la tabella attiva quando parte una phrase, `Chain` quando parte una chain, e `Free` la lascia correre finché la traccia non si ferma o un comando esplicito la sostituisce.

Una modalità strutturale non torna mai indietro nel tempo. Se una traccia non ha una tabella strumento attiva, la prima riga phrase con uno strumento attiva la sua tabella esattamente da quella riga, anche se è la riga `4`, `8` o `F` di una phrase. Numeri di strumento successivi non sostituiscono una tabella `Phrase`, `Chain` o `Free` attiva. Questo rende le tabelle utili come automazione tipo LFO senza simulare silenziosamente righe phrase anteriori.

`TBL`, `TBX` e `RET` sono sempre azioni esplicite: selezionano o riavviano immediatamente, qualunque sia la modalità. Su un confine Phrase o Chain, il riavvio avviene prima della lettura della nuova riga phrase, quindi un `TBL` o `TBX` su quella riga ha l’ultima parola. Gli FX della riga tabella `00` si eseguono normalmente; gli FX cumulativi mantengono l’accumulo usuale.

Le tabelle `00-7F` sono riservate alle tabelle strumento predefinite. Le tabelle `80-FE` sono destinate alle tabelle ausiliarie avviate dall’effetto `TBX`. Il range inferiore funziona pure per tabelle ausiliarie, ma usarlo così può causare conflitti e confusione.

### Controlli

- **EDIT + [SU/GIÙ]** su una colonna nome FX: apre la schermata di selezione FX
- **OPT + [DIREZIONE]**: sposta tra le tabelle

## 9. Instrument Pool e AY Wavetable

### Instrument Pool

L'Instrument Pool mostra tutti gli strumenti del progetto e permette di riordinarli. In riproduzione, mostra anche quali strumenti sono attivi.

#### Controlli

- **EDIT**: modifica lo strumento e salta allo schermo Instrument
- **SHIFT + OPT**: copia lo strumento
- **SHIFT + EDIT**: incolla lo strumento
- **EDIT + [SU/GIÙ]**: riordina gli strumenti
- **EDIT + PLAY**: anteprima dello strumento

### AY Wavetable

Wavetable è un tipo di oscillatore software AY Plus. Un progetto può contenere fino a 256 wavetable, ciascuna con 32 step. Tutti gli strumenti wavetable condividono lo stesso set di onde. Qui le modifichi.

I livelli di volume AY non sono lineari, quindi la vista Standard disegna l’onda per corrispondere ai livelli effettivi in uscita. Doppio tap su **OPT** per passare tra vista Standard e vista **LFO** persistente, usando lo stesso shortcut di Song/Live Mode: la vista LFO disegna i passi grezzi `0`–`15` linearmente, con la linea centrale tra `7` e `8` come punto zero modulazione.

Lo schermo ha 2 righe logiche. La riga superiore contiene i pulsanti **Load** e **Save**, la 2ª contiene l’editor wavetable.

#### Controlli

- **OPT + [SINISTRA/DESSTRA]**: scorre la lista wavetable
- **OPT + [SU/GIÙ]**: scorre la lista di 16 onde a volta
- **SHIFT + OPT**: copia la wavetable
- **SHIFT + EDIT**: incolla la wavetable
- **OPT + PLAY**: alterna tra vista Standard e LFO

## 10. Tracker FX

I Track FX forzano valori del sequencer o del motore sonoro su singoli step. Altri groovebox li chiamano "parameter locks". Ogni riga Phrase ha 3 slot FX.### Sequencer FX

| FX | Valore | Comportamento dettagliato |
|---|---|---|
| `ARP` | `XY` | Arpeggia la nota base, `+X` passi e `+Y` passi. `37` produce un pattern di accordo minore. |
| `ARC` | `XY` | `X` seleziona la direzione dell'arpeggio o la modalità intervallo; `Y` è la sua velocità in ticks. |
| `PVB` | `XY` | Vibrato di pitch: `X` è la velocità e `Y` la profondità. In modalità Lineare, la profondità usa passi da `10-cent`. |
| `PBN` | signed `XX` | Aggiunge `XX` unità di pitch ogni riga di frase/tabella; `FF` significa `-1`. Usa `00` per fermare. |
| `PSL` | `XX` ticks | Scivola dal pitch precedente alla nuova nota in `XX` ticks. |
| `PIT` | signed `XX` | Offset relativo accumulato in passi della tabella pitch. |
| `FIN` | signed `XX` | Offset fine accumulato in centesimi con pitch lineare, unità di periodo altrimenti. |
| `PRD` | signed `XX` | Offset relativo accumulato del periodo oscillatore. |
| `VOL` | signed `XX` | Offset relativo accumulato del volume (`FF` è -1, non volume pieno). |
| `VSL` | signed `XX` | Aggiunge `XX` al volume ad ogni riga di frase/tabella. Usa `00` per fermare. |
| `PAN` | `00-FF` | Panoramica assoluta dello strumento: sinistra, centro (`80`), destra. |
| `TPN` | `00-FF` | Panoramica assoluta della traccia mixer: sinistra, centro (`80`), destra. |
| `RET` | `XY` | Riattiva ogni `Y` ticks; `X` applica un cambiamento di volume. `Y=0` ferma il riattivamento. |
| `DEL` | `XX` ticks | Ritarda l'avvio della nota. Un ritardo più lungo dello step groove corrente salta la nota. |
| `OFF` | `XX` ticks | Invia note-off dopo `XX` ticks ed entra nella fase di rilascio ADSR. |
| `KIL` | `XX` ticks | Termina rigidamente la voce dopo `XX` ticks senza eseguire il rilascio ADSR. |
| `TIC` | `XX` ticks | Imposta i ticks per riga nella tabella. In una tabella modifica la velocità di quella colonna FX. |
| `TBL` | `00-FE`, `FF` off | Sostituisce e riavvia la tabella strumento immediatamente; `FF` la ferma. |
| `TBX` | `00-FE`, `FF` off | Avvia o sostituisce una tabella ausiliaria immediatamente; `FF` la ferma. |
| `THO` | riga `XX` | Salta tutte le colonne della tabella strumento alla riga `XX`. |
| `TXH` | riga `XX` | Salta tutte le colonne della tabella ausiliaria alla riga `XX`; non usato dall’interno di una tabella. |
| `GRV` | groove `XX` | Seleziona un groove per la traccia corrente. |
| `GGR` | groove `XX` | Seleziona un groove per ogni traccia. |
| `HOP` | `XY` | Salta alla riga `Y`, `X` volte; `X=0` loop infinito. HOP in tabella agisce sulla colonna propria. |
| `SNG` | signed `XX` | Sposta la riproduzione di `XX` righe canzone. |
| `PRO` | `00-64` | Valuta una probabilità di trigger assoluta da `0%` a `100%`. |
| `MOD` | `AB` | Trigger alla passata `A` di un ciclo a `B` passate. |
| `RNN` | `MX` | Suona una nota casuale. `M` sceglie la modalità: `0` sempre casuale, `1` casuale a ogni PLAY, `2` sceglie e mantiene finché il valore cambia. `X` sceglie il range: `0` qualsiasi nota, `1` +3 st, `2` +6 st, `3` +12 st, `4` -3 st, `5` -6 st, `6` -12 st, `7` +/-3 st, `8` +/-6 st, `9` +/-12 st. Le scelte casuali seguono la scala del progetto quando il motore di scala è applicato alla traccia. |
| `SPD` | signed `XX` | Seleziona un rapporto clock persistente per traccia; vedi la tabella velocità più sotto. |
| `SLE` | `00-FF` ticks | Imposta il tempo di glide persistente per FX motore continui per traccia. `00` è immediato. Impostazione si resetta a `00` alla fine della riproduzione. |

### Invii Audio FX

| FX | Valore | Comportamento dettagliato |
|---|---|---|
| `RSN` | `00-FF` | Imposta l’invio reverbero della traccia fino al prossimo trigger nota. |
| `DSN` | `00-FF` | Imposta l’invio delay della traccia fino al prossimo trigger nota. |

### ADSR / Trigger FX

| FX | Valore | Comportamento dettagliato |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | Sovrascrive attacco, decay, sustain, release o forma inviluppo. |
| `TDC` / `TCL` | `00-FF` | Sovrascrive Decay o Color in modalità Trigger su Plaits e Plaits-Alt. |

#### Condizioni

Ispirato dalle "trig conditions" svedesi, il tracker supporta probabilità di trigger e condizioni modulo.

| FX | Valore | Significato |
|---|---|---|
| `PRO` | `00-64` | Probabilità di trigger assoluta da `0%` a `100%` |
| `MOD` | `AB` | Trigger all’iterazione `A` di `B`; ad esempio `12`, `22`, `14`, `34` |

I contatori `MOD` sono locali a traccia e frase. Combinazioni non valide, come `A` maggiore di `B`, non triggerano.

`MOD34` triggera alla 3ª passata di ogni ciclo a 4 passate. `MOD1F` triggera alla 1ª passata di ogni ciclo a 16 passate.

#### Velocità di riproduzione

Come NerdSEQ, ChooChooTracker supporta una velocità di riproduzione indipendente per ogni traccia.

`SPD` legge il valore come byte signed. `00` è velocità normale, valori positivi accelerano la traccia, valori negativi la rallentano. L’impostazione resta attiva finché un altro comando `SPD` la modifica.

| Valore | Velocità |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

Progetti più vecchi senza supporto signed `SPD` mantengono la mappatura originale `00-10`, quindi suonano come salvati.

Moltiplicatori molto alti possono superare la risoluzione dello scheduler tick del tracker e non sono stati testati su hardware.

### Scale & accordi FX

#### Automazione della scala

`SCL XY` cambia la scala globale di riproduzione e la tonalità di base senza modificare le note nelle frasi. `X` seleziona una scala (`0` Cromatica, `1` Maggiore, `2` Minore, fino a `C` Personalizzata); `Y` seleziona la tonalità (`0` C fino a `B` B). Il comando si applica solo a FX di frase, non a tabelle. Se più tracce usano `SCL` nello stesso tick, vince la traccia dal numero più basso.

`SCL` fa parte della modalità Quantizer. Con **Note Lock** attivo il comando non appare nella lista FX e ogni `SCL` già scritto nelle frasi viene ignorato.

#### Accordi

`CRD XY` trasforma la nota sulla propria riga di frase in un accordo. `Y` seleziona: `0` Maggiore, `1` Minore, `2` Dim, `3` Aug, `4` Sus2, `5` Sus4, `6` Power, `7` Maj7, `8` Min7, `9` Dom7, `A` Min7b5, `B` Dim7, `C` Add9, `D` MinAdd9, `E` Maj9, o `F` Min9. `X` seleziona una delle 16 voicing: `0-3` sono un ciclo di inversioni chiuse ascendenti; `4-7` aggiungono drop-2; `8-B` aggiungono drop-3; `C-F` combinano entrambi i drop. Il ciclo si estende per un’ottava extra per triadi e power chords, così ogni valore è distinto; valori più alti diventano progressivamente più ampi e meno convenzionali. Per esempio, `CRD 4Y` è posizione root drop-2 e `CRD DY` è prima inversione con entrambi i drop. Ai confini della tabella pitch, le voci sono limitate al range. Il comando non è persistente: va scritto ogni riga da accordare. Ogni nota generata viene quantizzata indipendentemente se Scala attiva, in modalità Quantizer e Note Lock. Disponibile solo per motori software; strumenti AY lo ignorano e restano monofonici.

### Modulation FX

Phrase e table FX possono cambiare una slot modulazione senza modificare lo strumento:

| Pattern FX | Significato |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Offset relativo Amount per slot modulazione 1-4 |
| `M11`, `M12`, `M13`, `M14` | Offset relativi P1-P4 per slot modulazione 1 |
| `M21`, `M22`, `M23`, `M24` | Offset relativi P1-P4 per slot modulazione 2 |
| `M31`, `M32`, `M33`, `M34` | Offset relativi P1-P4 per slot modulazione 3 |
| `M41`, `M42`, `M43`, `M44` | Offset relativi P1-P4 per slot modulazione 4 |

Il valore è interpretato come variazione relativa signed `8-bit` (`01` aggiunge `1`, `FF` sottrae `1`). Comandi ripetuti si accumulano. Valori effettivi sono limitati all’intervallo valido.

`P5` non è esposto come FX di frase o tabella. È disponibile come destinazione modulazione `M1–M4 Wavetable`, così un modulatore può scandire l’indice AYWavetable di un altro.

| Tipo modulazione | P1 | P2 | P3 | P4 | P5 quando Shape è Wavetable |
|---|---|---|---|---|---|
| ADSR | Attack | Decay | Sustain | Release | Non usato |
| AHD | Attack | Hold | Decay | Non usato | Non usato |
| LFO | Shape | Trigger mode | Periodo | Non usato | Indice wavetable AY |
| syncLFO | Shape | Trigger mode | Ticks | Moltiplicatore | Indice wavetable AY |
| slowLFO | Shape | Trigger mode | Passo di periodo (00–FF) | - | Indice wavetable AY |
| FLFO | Shape | Trigger mode | Frequenza (`1 Hz` a `20 kHz`) | Non usato | Indice wavetable AY |

### Braids FX

| FX | Valore | Significato |
|---|---|---|
| `BMD` | `00-2E` | Modello Braids |
| `BTM` | `00-FF` | Timbro normalizzato assoluto |
| `BCL` | `00-FF` | Colore normalizzato assoluto |
| `BCF` | `00-FF` | Cutoff esponenziale, da `20 Hz` a `20 kHz` |
| `BRS` | `00-FF` | Risonanza filtro esponenziale |

### aChChid FX

| FX | Valore | Significato |
|---|---|---|
| `ASL` | `00-FF` | Scivola verso la nota senza riattivazione. `00` è `60 ms`; valori maggiori estendono il glide. |
| `ADC` | `00-FF` | Override decay: `200 ms` a `2 s`. |
| `AAC` | `00-FF` | Quantità di accento: da nessuno a massimo. |
| `ATM` | `00-FF` | Override Timbro Braids; attivo solo in modalità onda Braids. |
| `ACL` | `00-FF` | Override Colore Braids; attivo solo in modalità onda Braids. |
| `ACF` | `00-FF` | Cutoff filtro 303 esponenziale, da `20 Hz` a `20 kHz`. |
| `ARS` | `00-FF` | Risonanza filtro 303, da nulla a massimo. |
| `AEM` | `00-FF` | Modulazione inviluppo filtro 303, da nulla a massimo. |

### Bogie FX

| FX | Valore | Significato |
|---|---|---|
| `DMD` | `00-0B` | Seleziona il modello Bogie fino al prossimo trigger. |
| `DDC`, `DTO`, `DSW`, `DNO`, `DFM`, `DDR` | `00-FF` | Override Decay, Tone, Sweep, Noise, FM o Drive fino al prossimo trigger. Sei macro disponibili su ogni modello. |
| `DCF` | `00-FF` | Cutoff filtro, mappatura logaritmica. |
| `DRS` | `00-FF` | Risonanza filtro. |

### Sintered FX

| FX | Valore | Significato |
|---|---|---|
| `SMD` | `00-05` | Seleziona il modello Sintered fino al prossimo trigger. |
| `SDC`, `SMP`, `SMA`, `SMB`, `SMO`, `SMC` | `00-FF` | Override Decay, Mod, prime due macro modello, Motion o ultima macro modello fino al prossimo trigger. |
| `SCF`, `SRS` | `00-FF` | Cutoff e risonanza filtro. |

### MIDI Out FX

| FX | Valore | Significato |
|---|---|---|
| `MC1`-`MC4` | `00-FF` | Invia un MIDI CC, riconvertito a `0-127`. Il numero CC per ogni slot si imposta per strumento (`MC1 number`-`MC4 number` nella schermata strumento), non dall’FX stesso - uno slot senza numero è inattivo. |

### Plaits FX

| FX | Valore | Significato |
|---|---|---|
| `PMD` | `00-17` | Motore Plaits |
| `PHA` | `00-FF` | Armonici normalizzati assoluti |
| `PTM` | `00-FF` | Timbro normalizzato assoluto |
| `PMO` | `00-FF` | Morfologia normalizzata assoluta |
| `PAX` | `00-FF` | Blend Main/Aux: `00` Main, `FF` Aux |
| `PCF` | `00-FF` | Cutoff esponenziale, da `20 Hz` a `20 kHz` |
| `PRS` | `00-FF` | Risonanza filtro esponenziale |

### Sample FX

| FX | Valore | Significato |
|---|---|---|
| `SPL` | `00-03` | Modalità riproduzione: `00` Forward, `01` Reverse, `02` Loop, `03` Ping-Pong |
| `SLI` | `00-FF` | Riproduce la slice numerata (`01` = prima slice) indipendentemente dalla nota; `00` mantiene mappatura nota naturale. Richiede modalità slice abilitata nello strumento |
| `SPT` | signed `XX` | Trasposizione sample in semitoni |
| `SST` | `00-FF` | Inizio riproduzione normalizzato |
| `STA` | `00-FF` | Alias di `SST` (inizio riproduzione normalizzato) |
| `SEN` | `00-FF` | Fine riproduzione normalizzato |
| `SVL` | `00-FF` | Volume sample assoluto |
| `SCF` | `00-FF` | Cutoff esponenziale, da `20 Hz` a `20 kHz` |
| `SRS` | `00-FF` | Risonanza filtro esponenziale |
| `SSP` | `00-FF` | Velocità sample, mappata da `0%` a `500%` |
| `RNS` | `M0` | Suona uno slice casuale di un sample tagliato invece di quello assegnato alla nota. `M` sceglie la modalità: `0` sempre casuale, `1` casuale a ogni PLAY, `2` sceglie e mantiene finché il valore cambia. Richiede la modalità slice attiva nello strumento; il nibble basso non è usato. |

### AY FX condivisi dagli strumenti AY

| FX | Valore | Comportamento dettagliato |
|---|---|---|
| `AYM` | `XY` | `X` è la forma inviluppo; `Y` seleziona off/tone/noise/tone+noise (`0/1/2/3`). |
| `NOI` | signed `XX` | Offset relativo accumulato del periodo rumore. |
| `NOA` | `00-1F`, `FF` | Periodo rumore assoluto; `FF` dà priorità periodo rumore a tracce precedenti. |
| `ERT` | qualsiasi | Riattiva la forma inviluppo hardware corrente. |
| `EAU` | `XY` | Rapporto inviluppo automatico `X:Y`; `X=0` lo disabilita. |

### AY Classic-only FX

| FX | Valore | Comportamento dettagliato |
|---|---|---|
| `EVB` | `XY` | Vibrato periodo inviluppo: `X` velocità, `Y` profondità. |
| `EBN` | signed `XX` | Aggiunge `XX` al periodo inviluppo ogni riga di frase/tabella. |
| `ESL` | `XX` ticks | Scivola dal periodo inviluppo precedente al nuovo valore. |
| `ENT` | nota `XX` | Imposta periodo inviluppo dalla nota pitch-table mostrata dall’UI. |
| `EPT` | signed `XX` | Offset relativo accumulato del periodo inviluppo. |
| `EPL` | byte `XX` | Imposta il byte basso del periodo inviluppo. |
| `EPH` | byte `XX` | Imposta il byte alto del periodo inviluppo. |

### AY Plus FX| FX | Valore | Comportamento dettagliato |
|---|---|---|
| `TNN` | nota `XX` | Imposta l’oscillatore tone su una nota della tabella delle altezze, ignorando la nota della riga. |
| `TNP` / `TNF` | `XX` con segno | Passo pitch-step / offset fine accumulato per il tone. |
| `TRT` | qualsiasi | Re-triggera la fase dell’oscillatore tone se supportato. |
| `ENN` | nota `XX` | Imposta l’oscillatore envelope su una nota della tabella delle altezze. |
| `ENP` / `ENF` | `XX` con segno | Passo pitch-step / offset fine accumulato per l’envelope. |
| `SFT` | `00-07` | Tipo software: Nessuno, Pulse, Sync Tone, Sync Env, Wavetable, Tone FM, Env FM, Sample. |
| `SFN` | nota `XX` | Imposta l’oscillatore software su una nota della tabella delle altezze. |
| `SFP` / `SFF` | `XX` con segno | Passo pitch-step / offset fine accumulato per l’oscillatore software. |
| `SRT` | qualsiasi | Resetta la fase dell’oscillatore software. |
| `SFM` | `XX` con segno | Offset accumulato della profondità FM. |
| `PWM` | `XX` con segno | Offset accumulato della larghezza dell’impulso. |
| `SPL` | `XX` con segno | Offset accumulato del livello basso dell’impulso. |
| `SWT` | `XX` con segno | Offset accumulato dell’indice wavetable. |

### FX Sample AY

AY Sample accetta i comandi AY condivisi più `TNN`, `TNP`, `TNF`, `TRT`, `SFN`, `SFP` e `SFF` come descritto sopra.

| FX | Valore | Comportamento dettagliato |
|---|---|---|
| `SMS` | `00-FF` | Imposta l’inizio della riproduzione del Sample AY legacy a `XX x 64` campioni sorgente. |

I parametri Engine FX si applicano al tipo di strumento corrispondente e si resettano al prossimo trigger di nota, salvo diversa indicazione. `SPD` è intenzionalmente persistente.

Il selettore FX filtra questa lista ai comandi comuni più il gruppo supportato dallo strumento attivo. Il pannello di aiuto contestuale mostra dettagli del valore durante la modifica.

## 11. Mixer, Riverbero e Delay

Il Mixer è lo schermo principale più a sinistra. Mostra il carico CPU e gli avvisi di clipping. Un `!` rosso nella colonna **CLIP** segnala la traccia il cui segnale dry ha spinto il mix oltre la soglia di sicurezza.

Usalo per bilanciare le 8 tracce, modellare ogni traccia con Tilt EQ e inviare audio a riverbero o delay:

| Campo | Significato |
|---|---|
| LVL | Livello post-engine della traccia, `000-100` |
| REV | Invio al riverbero condiviso, `000-100` |
| DLY | Invio al delay ping-pong condiviso, `000-100` |
| PAN | Sposta l’audio a sinistra o destra |
| TLT | Tilt EQ per traccia, `00-FF`; `80` è neutro, valori bassi favoriscono bassi e valori alti gli acuti. I valori bassi introducono un leggero overdrive morbido. |
| M | `*` silenzia questa traccia |
| S | `*` ascolto solo questa traccia |

Il mixer è basato sulle tracce, non sugli strumenti. Se una traccia cambia strumento, livello, PAN, Tilt e invii restano assegnati alla traccia. Gli strumenti hanno anche il proprio **Vol** e **Pan** `00-FF` nella riga superiore dello schermo Instrument. Il PAN strumento si applica prima, seguito dal PAN traccia. Entrambi preservano un’immagine stereo centrata; attenuano progressivamente il lato opposto spostandosi a sinistra o destra.

### Auto Mix

Il Mixer contiene un pulsante **AUTO MIX**. Renderizza `6 secondi` offline, bilancia il volume delle tracce, confronta poi 8 bande di ottava con un profilo di rumore rosa e abbassa dolcemente le tracce che saturano quelle bande. Propone modifiche di livello prudenti, lascia un po’ di margine di picco e avvia un’anteprima temporanea. **Apply** mantiene i livelli proposti, mentre **Cancel** ripristina gli 8 livelli precedenti. È un buon punto di partenza, ma probabilmente vorrai ritoccare il risultato.

### Riverbero

Usa **SELECT + [SU]** dal Mixer per aprire le impostazioni del riverbero.

- **Algorithm**: seleziona Clouds o Beads. Ogni algoritmo mantiene i suoi controlli specifici.
- **Return**: livello wet del riverbero nel mix master.
- **Clouds**: **Time**, **Damping**, più filtri pre-riverbero **LP** e **HP**.
- **Beads**: **Time**, filtro native feedback-filter **Tone**, e filtro pre-riverbero **HP**.

Questa è solo la sezione send del riverbero Clouds o Beads, non il processore granulare.

### Delay Ping-pong

Usa **SELECT + [GIÙ]** dal Mixer per aprire Delay, poi **SELECT + [SU]** per tornare. Il riverbero funziona in modo inverso: **SELECT + [SU]** apre e **SELECT + [GIÙ]** torna al Mixer.

- **Return**: livello wet del delay.
- **To Reverb**: quantità del segnale delay wet inviato al riverbero condiviso, `0-100%`.
- **Ticks**: durata delay in tick tracker (`9 tick` = `1 battito` di default).
- **Feedback**: quantità di cross-feedback, limitata a `95%`.
- **Filter**: filtro low-pass nel percorso del segnale delay.

Il primo echo segue l’input stereo. I feedback successivi attraversano i canali sinistro e destro.

## 12. Schermata Progetto

La schermata Progetto offre i comandi **Load**, **Save**, **New**, **Export**, **Manage** e **Scale**, insieme a nome file, titolo e metadati autore.

### Posizione Export

Export, stems e bounce sono salvati in una cartella per progetto dentro la directory samples preesistente dell’app:

- **Default**: `<cartella app>/samples/Exports/<nome progetto>/`. Finché il progetto non ha nome, la cartella è `current-project`.
- Nella versione web il default è `/user/samples/Exports/<nome progetto>/`.
- Su Android la cartella samples è all’interno dello spazio di lavoro dell’app: privata nell’installazione Play Store, `samples` nella cartella scelta nella build APK diretta.

La riga **Folder** nella schermata EXPORT mostra la destinazione attiva:

- Tocca **EDIT** per scegliere una cartella personalizzata col browser cartelle. La cartella scelta è usata esattamente come selezionata (non si aggiunge sottocartella progetto) e rimane memorizzata nelle impostazioni.
- **EDIT + OPT** (cancella) reimposta sulla posizione di default.

Quando un progetto nominato viene salvato con un nuovo nome, la cartella export di default viene rinominata di conseguenza, mantenendo i file precedentemente esportati col progetto. Il rinomino non si applica a cartelle personalizzate, né viene fatto dall’autosave. Se esiste già una cartella con il nome destinazione, entrambe sono mantenute. Se il tracker perde traccia della cartella (es. dopo un riavvio), salvare un progetto nominato rinomina la cartella esistente `current-project`. Gli strumenti continuano a funzionare dopo il rinomino: i percorsi dei sample caricati che puntano nella cartella rinominata vengono aggiornati automaticamente.

**Load** importa file `.mid`/`.midi` come nuovo progetto: le note sono raggruppate per canale MIDI (un canale per traccia, fino al numero tracce), quantizzate a 4 righe per battito e assegnate a un unico strumento AY di default — i numeri programma MIDI non hanno equivalenti chiptune, quindi scegli strumenti reali dopo. Viene usato solo il primo tempo del file (un’unica velocità tick globale, nessun cambio tempo parziale). La riga **MIDI** dello schermo Export fa il contrario: esporta note, volume e tempo/groove della traccia corrente come Standard MIDI File (una traccia MIDI per traccia tracker); strumenti [MIDI Out](#midi-out) e FX per riga oltre volume e groove globale non hanno equivalenti MIDI e non sono tradotti.

**Load** importa canzoni Dirtywave M8 (`.m8s`, firmware 2.x-4.x) come nuovo progetto. Vengono importate solo struttura e note (righe song, catene con transpose, phrase con nota, velocity e numero strumento, note-off, tempo e nome song); le note mantengono la loro altezza reale (l’M8 chiama ottave due toni sopra: il suo `C-6` è qui `C-4`). Ogni strumento usato da una phrase è importato col nome dello strumento M8. Gli strumenti **MACROSYN** diventano Braids (le 47 forme sono i modelli Braids, stesse posizioni; timbro, colore, tipo filtro, cutoff, risonanza e pan sono caricati, la curva di cutoff è approssimativa). Gli strumenti **SAMPLER** diventano Sample: il `.m8s` memorizza solo il percorso WAV sulla scheda M8, quindi il file viene cercato nella cartella del `.m8s` e nelle sue cartelle superiori (funziona una copia della struttura scheda) e nella tua cartella campioni corrente (`Samples` o `samples` entrambi funzionano); se non trovato, lo strumento mantiene il percorso M8 e rimane silenzioso. I campioni importati ricevono +24 semitoni nel Pitch, perché l’M8 suona un sample due ottave sotto rispetto a questa app. **FMSYNTH** diventa uno strumento FM Genesis (approssimazione), **WAVSYNTH** e **HYPERSYNTH** diventano aChChid. **MIDI OUT** diventa strumento MIDI Out (canale, bank, program e prime quattro CC). Altri tipi (esterni) diventano strumento AY di default — scegli suoni reali dopo. FX e tabelle sono ignorati.

**Load** importa canzoni Little Sound Dj (`.lsdsng`, una canzone per file) come nuovo progetto. Importando un save LSDj intero (`.sav`, max 32 canzoni) si apre la lista canzoni (slot, nome, versione, dimensione); scegli una canzone per importarla (la canzone aperta al salvataggio della cartuccia non è listata). Vengono importate solo struttura e note (righe song, catene con transpose, phrase con nota e strumento, tempo e nome song); ogni nota ha volume pieno perché LSDj non ha volume note; i 4 canali LSDj PU1, PU2, WAV e NOI diventano tracce 1-4. Strumenti pulse diventano GB Pulse (duty, envelope e panning mantenuti; sweep pulse no), strumenti noise diventano GB Noise (envelope, stabilità e panning), strumenti wave diventano preset GB Pulse Hollow Lead e kit GB Noise di default. I groove sono importati, i comandi phrase `C` (accordo), `D` (delay), `E` (envelope pulse e noise, non su wave), `G` (groove), `K` (kill) e `V` (vibrato) vengono convertiti in `ARP`, `DEL`, `CEI`/`CEP`/`CED`, `GGR`, `KIL` e `PVB`. Gli altri comandi (`A`, `F`, `H`, `L`, `M`, `O`, `P`, `R`, `S`, `T`, `W`, `Z`), tabelle, wave frames, campioni kit e impostazioni synth sono ignorati, quindi la canzone suonerà diversa se dipende da questi. Il tempo è convertito per groove a 6 tick.

Su desktop e Android, la riga **LSDJ** nello schermo Export fa il contrario in modo limitato: toccala e scegli un `.lsdsng` esistente come **template** (rimane attivo fino a uscita, **EDIT + OPT** lo dimentica). Righe song, catene, phrase, tempo e titolo sono sostituiti, tutto il resto (tabelle, groove, waves, kit, synth) è mantenuto così com’è. Le tracce 1-4 diventano PU1, PU2, WAV e NOI; strumenti GB Pulse e GB Noise sono salvati come strumenti pulse e noise LSDj, altri tipi come pulse di default. FX, volume note e note-off non sono esportati; una canzone che usa catena `80` o superiore o phrase `FF` o superiore non può essere esportata (LSDj ha 128 catene e 255 phrase). Il file esportato non è stato testato in LSDj.

Su desktop, la riga **M8S** dello schermo Export fa il contrario in modo limitato: toccala e scegli un `.m8s` esistente come **template** (rimane finché non esci, **EDIT + OPT** cancella). Song, catene, phrase, tempo e titolo sono sostituiti, tutto il resto mantenuto, eccetto che strumenti Braids sono salvati come MacroSynth e Sample come Sampler (percorso `/Samples/<nome file>`: copia WAV lì per l’M8 da solo), se il template ha strumenti di quei tipi per copiare gli altri parametri. FX non sono esportati; canzoni con phrase `FF` o superiori non possono essere esportate (M8 ha 255 phrase). Il file esportato non è stato testato su un M8 reale. Vedi [M8 songs](m8s-format.md) per i dettagli.

Su desktop, key jazz permette di digitare nome file, titolo e autore direttamente da tastiera invece che con quella virtuale — vedi [Key jazz](#key-jazz-desktop-only).

### Scala / Quantize

La schermata Scale controlla il motore globale scala 12-TET. La riga **Mode** seleziona tra due modalità:- **Quantizer** (predefinito): l'inserimento delle frasi resta cromatico; quando abilitato, le note vengono arrotondate verso il basso immediatamente prima di raggiungere il motore sonoro, così cambiare la tonica o la scala riarteggia la canzone senza modificare le frasi.
- **Note Lock**: le note inserite nelle frasi sono agganciate alla nota più vicina della scala durante l'immissione, quindi si possono scrivere solo note della scala. Nuove note e diminuzioni si agganciano verso il basso; `EDIT + RIGHT` / `EDIT + UP` alzano la nota alla scala successiva. L'inserimento segue sempre la scala del progetto in questa schermata: cambiare tonica o scala influisce subito sulle note inserite nei brani abilitati, mentre quelle già memorizzate restano inalterate. L’effetto `SCL` non è disponibile in questa modalità e ogni `SCL` già scritto viene ignorato. La quantizzazione in riproduzione delle note semplici è bypassata (l’inserimento è già bloccato); la quantizzazione degli accordi tramite `CRD` è ancora attiva.

- **Clock source** sceglie tra **Internal** (il tempo del progetto), **Midi Clock** (il sequencer segue un clock MIDI in ingresso) e **Ableton Link** (il sequencer segue il tempo di una sessione Link sulla rete locale). Con **Midi Clock**, la riga **Tempo** appare attenuata e mostra il tempo misurato del clock in ingresso; il trasporto (play/stop) è controllato dai messaggi Start/Continue/Stop del mittente, quindi il controllo PLAY dell’app non serve. Con **Ableton Link**, la riga diventa la riga **Link**: contiene l’interruttore di connessione/disconnessione dalla sessione al posto di un campo tempo (vedi sotto). La sorgente di clock è salvata in settings.txt e vale per tutti i progetti.
- **Tempo** imposta il tempo del progetto in BPM interi (`3-500`). Internamente il tracker salva ancora il tick rate in Hz (`tick rate = BPM x 0,4`), quindi i progetti esistenti si caricano e salvano senza modifiche; un progetto con tempo frazionario lo mostra arrotondato e si aggancia a un BPM intero alla prima modifica del tempo. La regolazione grossa avanza di 5 BPM, quella fine di 1; cancellare ripristina il valore predefinito di 125 BPM.

ChooChooTracker salva i progetti come `.cct`. È un formato proprietario semplice da leggere anche per un umano.

Usa **Save** prima di cambiare tipo di strumento o caricare un altro progetto.

### Ableton Link

Ableton Link permette a ChooChooTracker di seguire il tempo di altre app compatibili con Link (Ableton Live, LinkHut, ecc.) sulla stessa rete locale (Wi-Fi o Ethernet). È solo ricevitore: il tracker adatta il proprio tempo alla sessione e non invia mai tempo né trasporto agli altri partecipanti.

**Cosa viene sincronizzato:**

- Tempo di sessione - il tracker segue continuamente il tempo della sessione Link (nel range 3-500 BPM del tracker).
- Fase del beat - premere PLAY allinea la riproduzione alla griglia di beat della sessione: il brano parte alla battuta successiva (quantum di 4 beat).

**Cosa non viene sincronizzato:**

Durante la riproduzione con Link, ChooChooTracker controlla la fase di beat condivisa una volta per battuta 4/4 e regola dolcemente il proprio tick rate quando serve. Questo compensa piccole differenze di clock tra dispositivi senza saltare né ripetere tick del tracker, e non cambia mai la sessione Link di un altro partecipante.

- Groove e valori `SPD` e `DELAY` non sono influenzati - restano relativi al tempo seguito.
- Le posizioni del cursore in song/chain/phrase e le impostazioni del mixer non vengono mai condivise.

**Come usarlo:**

1. Apri la schermata Project e imposta **Clock source** su **Ableton Link**. La riga sotto diventa la riga **Link**.
2. Premi EDIT su **[Connect]** per unirti alla sessione. Il messaggio di stato a destra mostra l’avanzamento: `Wait...` durante la connessione, `No net` se non c’è rete, `No sess` se non si trova alcuna sessione, `Err` in caso di errore interno. Mentre sei connesso, il pulsante mostra **[Disconnect]** e lo stato alterna circa ogni secondo tra `connected` e `Peers: N` (N = numero di peer).
3. Attendi `connected` e premi PLAY - la riproduzione parte allineata alla prossima battuta della sessione.
4. Premi EDIT su **[Disconnect]** (o rimetti la sorgente di clock su Internal) per lasciare la sessione.

In modalità Link la sessione possiede il tempo: la riga non mostra un campo BPM e il tempo della sessione viene applicato automaticamente al progetto (nel range 3-500 BPM del tracker).

**Disponibilità per piattaforma:** PortMaster (obiettivo principale), Windows, Linux, macOS e Android sono supportati. La versione Web e quelle Miyoo non includono Link.

**Nota PortMaster:** sui dispositivi PortMaster il buffer audio è più grande (512 frame a 48 kHz), quindi l’allineamento del beat è approssimativo, entro circa 10-15 ms, e non esatto al sample. La sincronizzazione del tempo in sé non è influenzata.

## 13. Impostazioni

Usa Settings per le preferenze che restano col programma e non col progetto attuale. Controlli di progetto come tempo, groove e livelli mixer rimangono nelle loro schermate dedicate.

### Controlli

- **Repeat delay** e **Repeat speed** impostano dopo quanto tempo e con che velocità un pulsante di navigazione o modifica tenuto premuto si ripete. Aumenta il delay se la pressione continua va troppo veloce; abbassalo per un inserimento tracker rapido.
- **Stick live mode** sceglie come funziona il pulsante mappato **STICK LIVE**: `HOLD` è attivo solo tenendolo premuto, `TOGGLE` resta attivo dopo la pressione, `FREE` è sempre attivo. Vedi [Motion recording](#motion-recording).
- **Key mapping** assegna i controlli da tastiera o controller. Scegli un comando, premi il nuovo controllo, poi seleziona **Done** per salvare.

### Audio

**Sample rate** offre `32 kHz` o `48 kHz` (predefinito). Le modifiche si applicano subito e vengono salvate. Cambiare la frequenza di campionamento ferma la riproduzione e l’anteprima dei sample mentre i motori sonori e qualsiasi ingresso audio armato vengono riaperti alla nuova frequenza. Se un dispositivo rifiuta la nuova frequenza, l’impostazione precedente resta attiva. `32 kHz` riduce il lavoro di elaborazione e limita la frequenza riprodotta più alta a 16 kHz. L’indicatore di carico CPU mostra il costo del rendering audio.

**Buffer size** scambia reattività con stabilità. Usa un valore più piccolo per suonare live con minor latenza; aumentalo se l’audio scoppietta o il carico CPU è sempre alto. I valori disponibili sono `32`, `64`, `128`, `256`, `512` e `1024` campioni. Il monitoraggio del carico CPU aiuta a trovare un’impostazione stabile.

### MIDI

Scegli i dispositivi di input e output, assegna i canali MIDI in arrivo agli strumenti e mappa i controlli MIDI CC. Vedi [MIDI](#15-midi) per collegare una tastiera, controllare il mixer o inviare note a hardware esterno.

### Synth

- **AY Quality** sceglie la qualità usata dagli strumenti AY/YM. **AY Sample dithering** può levigare la riproduzione dei campioni AY; lascialo spento per un suono più diretto e lo-fi o se punti a hardware originale.
- **Braids BITS**, **DRFT** e **SIGN** sono impostazioni globali dei caratteri di Braids, condivise da ogni strumento Braids. Servono per fissare risoluzione, deriva e carattere generale prima di rifinire i singoli suoni. Vedi [Braids](#braids).

### Mixer

- **Mix volume** è il volume master dell’applicazione. Abbassalo se alimenti un altro mixer o registratore; non modifica il bilanciamento dei singoli canali del brano.
- **Tilt pivot** imposta la frequenza centrale per ogni controllo Mixer **Tilt** in questo progetto, da `250 Hz` a `4000 Hz` (default `1 kHz`).

### Grafica

- **Edit color theme** cambia i colori dell’interfaccia. **Load font** carica un font `.cnfont` per la visualizzazione.
- **Song waveform** mostra o nasconde la forma d’onda del mix nella pagina Song; vedi [Song waveform](#song-waveform).
- **Track visuals** sceglie per ogni traccia o **Detailed** (forma d’onda strumento e sovrapposizioni) o **Audio waveform** (forma d’onda audio risultante). Usa **All detailed** o **All audio** per cambiare tutte le tracce insieme.
- **Waveform FPS** imposta quante volte al secondo strumenti e visualizzatori del mixer si ridisegnano, da `1` a `60` frame per secondo. Abbassalo per una visuale meno rumorosa o più risorse disponibili.
- **Renderer** segnala se l’app usa rendering `GPU` o `Software`.

### Support report e uscita

**Support report** crea un piccolo file diagnostico per bug report. Allegalo con il progetto `.cct` quando il problema dipende da una canzone specifica. **Quit ChooChooTracker** chiude l’app in modo pulito.

### Song waveform

Graphics → Song waveform è OFF di default. Premi EDIT per attivarla o disattivarla, o usa EDIT + direzione; la scelta è salvata come preferenza dell’app. Se attivata, sopra la pagina SONG appare una forma d’onda a due righe con il mix finale sommato in mono. Le altre pagine mantengono layout a tutta altezza, spaziatura normale e anteprime strumenti indipendentemente da questa opzione. La preferenza salvata è compatibile con versioni precedenti. I target touch SONG seguono le righe mostrate in entrambe le modalità.

## 14. Risoluzione problemi

### CPU

Il carico CPU dipende dai motori e dagli effetti attivi. Modelli fisici Plaits e riverbero Clouds pesano più delle voci base AY. Verifica le prestazioni sulla console di destinazione, specialmente con 8 voci Plaits e entrambi i send attivi. Un valore CPU vicino a `100%` può causare scoppiettii o ritardi dell’audio.

Se riscontri pop, crash o rallentamenti, usa **Settings > Support report** e allega il file `support-report.txt` al bug report. Se un brano specifico causa il problema, allega anche quel `.cct`.

### Nessun suono

Controlla il numero dello strumento, lo stato mute o solo della traccia, il livello LVL della traccia, il volume Mix dell’applicazione e l’inviluppo dello strumento. Per i campioni, verifica che il WAV originale esista ancora nel percorso salvato.

Su ArkOS, premi **MENU + L3** per silenziare o riattivare il sistema operativo. Se non funziona, riavvia l’hardware e ricollega l’interfaccia audio. Prova l’opzione “repair audio” su ArkOS, può aiutare.

### Input non corretto

Regola Repeat delay e Repeat speed in Settings. Se una singola pressione si muove due volte, controlla che sia mappato un solo controllo fisico su quella direzione e segnala la schermata e la combinazione precisa.

## 15. MIDI

Settings > **MIDI** apre questo sottomenu: **MIDI In** / **MIDI Out** seleziona un dispositivo collegato, scorrendo le porte rilevate con `OFF` agli estremi; entrambi i sensi richiedono un dispositivo selezionato.

- **Sound preview from a MIDI keyboard**: con un dispositivo MIDI In selezionato, suonare note sulla tastiera collegata fa ascoltare uno strumento su qualsiasi schermata, come il comando `EDIT + PLAY` sulla schermata [Instrument](#5-instruments). È solo anteprima, non inserisce note nel brano.
- **Channel mapping**: apre la lista dei 16 canali MIDI; assegna a ciascuno uno strumento (`OFF` per default) così le note ricevute su quel canale fanno ascoltare quello strumento indipendentemente da quale sia selezionato nella schermata Instrument – ad esempio canale `01` -> strumento `05`. Un canale lasciato `OFF` usa lo strumento selezionato corrente, comportamento originale. Questa mappatura viene salvata in settings.txt.
- **MIDI CC mapping**: mappa un controller in ingresso a un parametro strumento, incluso **Volume**, o a controlli globali. Le righe globali mostrano `TRK 1`-`TRK 8` anziché uno strumento e offrono Mute, Solo, Volume, send Riverbero e send Delay della traccia; **Song play/stop** non ha destinazione. Una nuova mappatura ignora il CC che l'ha imparata/impostata e si attiva al prossimo movimento fisico, quindi impostare la mappatura durante la riproduzione non resetta i parametri a zero. Mute e solo usano `0-63` = spento, `64-127` = acceso.
- **Sincronizzazione con clock MIDI**: con un dispositivo MIDI In selezionato, imposta **Clock source** nella schermata Project su **Midi Clock** per seguire un sequencer esterno. Il tracker misura il tempo del clock in ingresso (24 impulsi per quarto) e lo mostra nella riga **Tempo** attenuata; i messaggi **Start**, **Continue** e **Stop** controllano la riproduzione. La sorgente di clock è ricordata in settings.txt.
- **Guidare un dispositivo MIDI esterno**: assegna a una traccia il tipo strumento [MIDI Out](#midi-out) e imposta il suo Canale; le note su quella traccia inviano vero MIDI al dispositivo MIDI Out selezionato invece di produrre suono in ChooChooTracker.

## 16. Crediti e licenze

ChooChooTracker è un fork di ChipNomad e mantiene il modello di licenza MIT. Braids, Plaits, Plaits-Alt, Clouds DSP, MME DSP derivato da Warps e codice stmlib sono tratti dalle release open-source Mutable Instruments sotto le rispettive licenze MIT. Plaits-Alt proviene dal fork lylepmills/eurorack Plaits Lab; le note di copyright originali sono valide. Il motore aChChid usa Open303 di Robin Schmidt, copyright 2009, sotto licenza MIT. MonkSynth usa il DSP nativo di JonET sotto licenza MIT; vedi `LICENSE.monksynth.txt`. Bogie e Sintered sono implementazioni native originali. MIDI I/O usa RtMidi di Gary P. Scavone, con licenza in stile MIT. Per dettagli esatti sugli attribuzioni consulta i file di licenza inclusi. Ableton Link è fornito con accordo di licenza speciale da Ableton, non è sotto licenza MIT e la nostra licenza non si estende ai tuoi fork o altri progetti oltre ChooChooTracker.