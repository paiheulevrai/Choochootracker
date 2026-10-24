# Manuel d’utilisation de ChooChooTracker
> Ceci est une traduction générée automatiquement de `docs/USER_MANUAL.md` du manuel anglais actuel. En cas de différence avec la version anglaise, le texte anglais prévaut.
>
> **Conventions.** L’interface de l’application est en anglais. Gardez les noms d’écran, de menu, de champ et de bouton, les codes FX, les noms de modules et moteurs, ainsi que les extensions de fichiers en anglais.


ChooChooTracker est un tracker musical 8 pistes pour consoles portables.

Il suit le workflow et le système de navigation classique de LSDj, avec plusieurs options de conception sonore :

- Émulation de puces et consoles vintage
- Soundfonts
- Oscillateurs Mutable Instruments Braids, Plaits et Plaits-Alt, avec un filtre multimode
- Échantillons PCM, formes d’onde à cycle unique et oscillateurs wavetable, avec un filtre multimode
- aChChid, un synthé acid versatile basé sur Open303
- Bogie, un synthétiseur de batterie natif à 12 modèles avec filtre multimode
- MonkSynth, un synthétiseur vocal avec unisson à dix voix et delay stéréo
- MME, un synthétiseur multi-modulation à deux oscillateurs avec filtre multimode
- Sintered, un synthétiseur natif expérimental pour batterie numérique à six modèles

## 1. Installation et fichiers

### ArkOS / PortMaster

Installez `choochootracker.zip` via PortMaster ou copiez le package décompressé dans le dossier `ports` de la console. Lancez **ChooChooTracker** depuis le menu Ports.

### Miyoo Mini / Mini Plus (Onion OS)

Extrayez l’archive MiyooPorts à la racine de la carte SD pour fusionner ses dossiers `Roms/PORTS/` avec ceux déjà présents dans Onion. Lancez **ChooChooTracker** depuis le menu Ports après avoir rafraîchi la collection. Sauvegardez le dossier `projects/` avant de remplacer une installation existante.

Le temps de chargement d’un projet peut varier selon les types d’instruments utilisés dans la chanson. Si le démarrage ou le chargement d’un projet semble anormalement lent, indiquez la durée de chacun, avec le modèle de console et la version d’Onion OS.

### Raspberry Pi OS 64 bits

Extrayez l’archive Raspberry Pi AArch64, installez le runtime SDL2 (`sudo apt install libsdl2-2.0-0`), puis lancez `./chipnomad.sh` depuis le dossier extrait. Gardez les instruments et autres ressources fournis à côté de l’exécutable.

### Windows

Lancez `choochootracker.exe` avec `SDL2.dll` et `libwinpthread-1.dll` dans le même dossier.

Sur Windows, les entrées supportent le clavier (voir la section Jazz mode pour les détails) ou une manette.

### Web

Vous pouvez utiliser Choochootracker en ligne sur https://choochootracker.vercel.app/ .

Contrôlez cette version au clavier, avec les contrôles à l’écran sur mobile, ou avec une manette.

Les réglages, échantillons, thèmes, projets, etc. sont stockés dans le stockage du navigateur.

Note : à cause des restrictions iOS, l’utilisation de midi sur la version web via un iPhone peut être impossible.

### Configuration des dossiers

| Dossier | Contenu |
|---|---|
| `AY_Wavetables` | Tables d’onde `.aywave` pour puce AY |
| `fonts` | Fichiers de police `.cnfont` ChipNomad |
| `instruments` | Presets instrument `.cni`, mais aussi banques FM et Soundfonts |
| `pitch-tables` | Accordages alternatifs `.csv` |
| `projects` | Fichiers chanson et projet `.cct` |
| `samples` | Fichiers `.wav` pour le moteur Sampler |
| `SR_wavetables` | Tables d’onde format Serum pour le moteur BYOWTBL |
| `themes` | Thèmes ChipNomad `.cth` |
| `title` | Ressources de l’écran titre |
| `waveforms` | Formes d’onde à cycle unique `.wav` pour le moteur 2xSCWF |

## 2. Contrôles

ChooChooTracker fonctionne idéalement avec une manette disposant d’une croix directionnelle, 2 sticks analogiques et 7 boutons.

| Contrôle logique | Par défaut sur console | Par défaut sous Windows |
|---|---|---|
| **[GAUCHE/DROITE/HAUT/BAS]** | Croix directionnelle | Flèches du clavier |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Space |
| **SELECT** | Select | Shift |
| Axes modulation | Sticks gauche et droit | Sticks de la manette |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

Les mappings peuvent être modifiés dans **Settings > Controls > Key mapping**, y compris sur Android avec une manette externe. Sur PortMaster, sauvegardez un nouveau mapping avec **Done**, puis relâchez le bouton avant de continuer dans Settings.

### Navigation

- Utilisez **[HAUT/BAS/GAUCHE/DROITE]** pour déplacer le curseur.
- Maintenez **SELECT + [DIRECTION]** pour changer d’écran.
- Maintenez **OPT + [DIRECTION]** pour une navigation spécifique à l’écran.
- Sur une valeur, utilisez **EDIT + [GAUCHE/DROITE]** pour des changements fins ou **EDIT + [HAUT/BAS]** pour des changements larges.
- Tapez sur **EDIT** pour saisir une valeur ou activer une commande.

### Sélection et presse-papiers

- **SELECT + OPT** active le mode sélection et fait défiler des plages utiles.
- Utilisez **[DIRECTION]** pour étendre la sélection.
- Tapez puis relâchez **OPT** pour copier la sélection et quitter le mode.
- **OPT + EDIT** coupe la sélection.
- Hors mode sélection, maintenez **SELECT** et appuyez sur **EDIT** pour coller.
- **EDIT + [DIRECTION]** modifie simultanément les cellules sélectionnées.

### Lecture

- **PLAY** lance la lecture à partir du curseur. Appuyez à nouveau pour arrêter.
- Vous pouvez éditer notes, instruments et chaînes pendant la lecture ; les changements s’entendent au tick suivant du séquenceur.
- **SELECT + PLAY** lance tous les tracks depuis un écran autre que Song.
- **EDIT + PLAY** préécoute un instrument depuis l’Instrument Pool.

### Global Bounce

Sélectionnez une plage de lignes et colonnes dans Song, Chain ou Phrase, puis tapez deux fois sur **EDIT** pour ouvrir l’écran BOUNCE TO SAMPLE. Le bounce inclut toutes les pistes sélectionnées : les pistes dont la première chaîne commence plus tard dans la sélection attendent en silence et rejoignent à leur première chaîne. Le fichier contient ainsi toutes les chaînes des pistes.

L’écran propose :

- **File name** : éditez avec le clavier caractère. À l’ouverture, le prochain numéro de séquence libre (`001`, `002`, ...) est proposé : le premier numéro pour lequel aucun fichier `.wav` n’existe dans le dossier d’export. Modifiez-le librement — le nom est utilisé tel quel. Si un fichier portant ce nom existe déjà, un suffixe `_001`, `_002`, ... est ajouté pour éviter d’écraser un fichier existant. Le compteur se réinitialise lors du chargement ou de la création d’un projet.
- **Sample rate** et **Bit depth** : les mêmes options que dans l’écran d’export.
- **Include in a sample name** : trois cases à cocher qui préfixent le nom du fichier avec des tags contextuels lors du pressage sur **Start** (toutes décochées par défaut) :
  - **[BPM]** — tempo courant du projet, ex. `[120]`
  - **[Key]** — tonique et gamme du projet, ex. `[Cmaj]`, `[F#min Pent]`
  - **[Bars:Beats:16ths]** — durée du bounce rendue : une phrase = une mesure (16 seizièmes), un temps = 4 seizièmes. Une phrase entière affiche juste les mesures, ex. `[2]` ; une phrase partielle affiche `[0:2]` (2 temps) ou `[0:1:3]` (1 temps + 3 seizièmes). Pour un bounce multi-pistes, c’est la longueur de la piste la plus longue qui compte. Une sélection qui finit en milieu de temps enlève un seizième : la dernière ligne sélectionnée est le point de coupure, donc sa note-off arrive à la fin du seizième précédent.
  - Les tags sont appliqués dans l’ordre `[BPM][Key][Length]` avant le nom du fichier.
- **Start** : rend la sélection dans un fichier WAV dans le dossier d’export (voir *Emplacement d’export* dans l’écran Project)
- **Cancel** : annule et revient en arrière

Pendant le rendu, **OPT** annule. À la fin ou à l’annulation, l’écran d’origine revient.

## 3. Structure de la chanson et carte des écrans

Maintenez **SELECT + [DIRECTION]** pour changer d’écran.

La ligne centrale de navigation :

<pre>
        I
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

Ligne centrale :

- **M** : Mixer (branche vers Reverb, Delay)
- **S** : Song (branche vers Project, Settings)
- **C** : Chain
- **P** : Phrase (branche vers Groove)
- **I** : Instrument (branche vers Modulation et Instrument Pool)
- **T** : Table (branche vers l’éditeur AY Wavetable)

Ligne du haut :

- **R** : Reverb
- **P** : Project
- **G** : Groove
- **M** : Modulation
- **I** : Insert FX

Ligne du bas :

- **D** : Delay
- **S** : Settings
- **P** : Instrument Pool
- **W** : Éditeur AY Wavetable

Une chanson ChooChooTracker suit cette hiérarchie :

```text
Song -> Chain -> Phrase row -> Note + Instrument + FX
```

Une chanson a 8 pistes. Chaque piste contient une séquence de chaînes, et chaque chaîne contient une séquence de phrases. Les phrases sont des séquences de 16 pas contenant des notes. Une note contient une hauteur, un instrument, une vélocité et jusqu’à 3 FX de piste. Un instrument est ce qui génère le son.


## 4. Song, Chain et Phrase

### Song

L’écran Song contient 8 colonnes, 1 par piste audio. Chaque cellule fait référence à une chaîne. Une chanson peut contenir jusqu’à 256 lignes (`00-FF`).

Les pistes vont de gauche à droite, tandis que leurs chaînes s’organisent verticalement. Chaque piste est indépendante, et les chaînes peuvent contenir de 1 à 16 phrases, donc les pistes peuvent se désynchroniser. Concevez vos chaînes pour qu’elles restent synchrones ou laissez-les dériver selon ce que la piste demande.

Il est courant dans la communauté des trackers de réserver une chaîne pour des phrases vides, souvent `00` ou `FE`.

Vous pouvez garder plusieurs sous-chansons dans un projet en créant des « îles » : sections séparées par des lignes vides.

#### Mode Live

Double-tapez **OPT** pour entrer ou quitter le mode Live ; `LIVE` remplace le titre de la chanson. En mode Live, **PLAY** lance la chaîne sélectionnée sur sa piste si la piste est arrêtée. Sur une piste en lecture, elle place en file d’attente la chaîne sélectionnée pour le prochain changement de chaîne ; appuyez deux fois sur **PLAY** pour un changement urgent à la prochaine phrase. Une cellule sélectionnée vide sur Song place un arrêt quantifié en file d’attente. `+` marque une chaîne en file d’attente, `!` une chaîne urgente, `-` un arrêt en file.

Sélectionnez une ligne Song sur plusieurs colonnes pour mettre en file ou arrêter ces pistes ensemble. Les sélections multi-lignes sont interdites volontairement. Les files Live se vident pendant que la boucle Song est active. Live est un mode de performance, non sauvegardé dans le projet.

#### Contrôles

- **OPT + [HAUT/BAS]** : saute de 16 positions vers le haut ou le bas
- Sélectionnez une plage, puis **SHIFT + EDIT** : clone superficiellement les chaînes sans changer leurs phrases
- Sélectionnez une plage, puis double-tapez **SHIFT + EDIT** : clone en profondeur chaînes et phrases
- Sélectionnez une plage, puis utilisez **EDIT + [HAUT/BAS]** : déplace la sélection vers le haut ou le bas
- Tapez 3 fois sur **OPT** : ajoute ou retire une mise en surbrillance pour organiser visuellement
- **OPT + SHIFT** : mute la piste courante ou colonnes sélectionnées (relâchez **OPT** pour garder le mute actif)
- **OPT + PLAY** : solo la piste courante ou colonnes sélectionnées (relâchez **OPT** pour garder le solo actif)
- **OPT + [GAUCHE/DROITE]** : solo toutes les pistes à gauche ou droite de la piste courante
- Sélectionnez une plage, puis double-tapez **EDIT** : ouvre l’écran BOUNCE TO SAMPLE (voir Bounce ci-dessus)

L’arrêt de la lecture et les changements de mute ou de solo utilisent une courte correction de sortie de 3 ms pour réduire les clics. Cela couvre aussi la réactivation du son et l’annulation des solos pendant la lecture.

### Chain

Une chaîne est une liste ordonnée de phrases 16 pas avec transposition optionnelle. Elle peut contenir jusqu’à 16 phrases, et la même phrase peut apparaître plusieurs fois. La 2e colonne définit la transposition en demi-tons.

Un astérisque (`*`) signale une chaîne réutilisée dans le projet. Vous pouvez cloner les chaînes depuis l’écran Song.

#### Contrôles

- **OPT + [GAUCHE/DROITE]** : passe d’une piste à l’autre
- **OPT + [HAUT/BAS]** : passe d’une chaîne à l’autre sur la piste courante
- Sélectionnez une plage, puis **SHIFT + EDIT** : clone les phrases
- Sélectionnez une plage, puis double-tapez **EDIT** : ouvre l’écran BOUNCE TO SAMPLE pour les lignes sélectionnées de la piste (voir Bounce sous Song)

### Phrase

Une phrase est le pattern d’une piste dans un séquenceur pas à pas traditionnel.

Une phrase dure 16 pas, 1 ligne par pas. Chaque ligne contient une note, un instrument, un volume de `00` à `7F`, et colonnes FX. Les notes utilisent la notation tracker comme `C-4` (note et octave).

Pour arrêter une note, insérez `NOTE OFF`, utilisez le FX kill-note, ou jouez une autre note sans instrument.

Quand une chaîne contient plusieurs phrases, l’écran Phrase fonctionne comme un éditeur de pattern continu. Descendre en dessous de la ligne `F` ou au-dessus de la ligne `0` ouvre la phrase suivante ou précédente dans la chaîne.

Un astérisque (`*`) signale une phrase réutilisée dans le projet. Vous pouvez cloner des phrases depuis l’écran Chain.

Le sélecteur FX montre les commandes communes plus celles supportées par l’instrument de cette ligne. Cela évite une longue liste globale de commandes spécifiques à un moteur. ChooChooTracker supporte plus de FX que ChipNomad.

### Contrôles- **OPT + EDIT** sur une note vide : insérer `NOTE OFF`
- **EDIT + [HAUT/BAS]** sur une colonne de nom FX : ouvrir l’écran de sélection FX
- **OPT + [GAUCHE/DROITE]** : naviguer entre les pistes
- **OPT + [HAUT/BAS]** : naviguer entre les phrases dans la chaîne courante
- Sélectionnez une plage dans la colonne instrument, puis utilisez **SHIFT + EDIT** : cloner les instruments
- Sélectionnez une plage, puis utilisez **EDIT + [HAUT/BAS]** : faire tourner les lignes de la phrase
- Sélectionnez une plage, puis double-appuyez sur **EDIT** : ouvrir l’écran BOUNCE TO SAMPLE pour les lignes de phrase sélectionnées de la piste courante (voir Bounce sous Song)

Sur cet écran, avec une sélection active, la colonne note peut être remplie
avec des motifs rythmiques :

- **OPT + GAUCHE** remplit les cellules notes de la sélection avec un motif fixe de 16 pas
  et passe au motif suivant à chaque pression. Après le dernier motif, les
  notes sont effacées et le cycle recommence. Les motifs sont alignés sur les lignes absolues
  de la phrase, donc une sélection partielle ne reçoit que les pas qui y tombent.
- **OPT + DROITE** remplit les cellules notes de la sélection avec un motif aléatoire
  de 3 à 8 notes (7 et 8 sont moins probables), en boucle tant que **OPT** est maintenu.
- **OPT + HAUT** modifie les notes déjà placées dans la sélection : à chaque pression,
  1 à 3 notes sont choisies au hasard et déplacées d’une valeur aléatoire dans une octave
  (les petits décalages sont plus probables que les extrêmes). La 8e pression restaure
  les notes initiales, puis le cycle recommence.
- **OPT + HAUT** sur une sélection couvrant uniquement la colonne volume (sans notes,
  instruments ou FX) randomise les vélocités au lieu de muter : la première pression
  applique le motif Minimal (chaque volume varie de ±20 max), la deuxième Medium (±50 max),
  la troisième Random (valeurs complètement aléatoires), puis la quatrième restaure
  les volumes initiaux. Relâcher **OPT** réinitialise le cycle à Minimal. Les lignes
  sans volume restent inchangées.
- **OPT + BAS** répartit les notes placées d’instruments Sampler découpés dans la sélection
  en une montée chromatique : la première note de chaque instrument (la plus haute dans le séquenceur)
  garde sa valeur, et chaque note suivante de cet instrument monte d’un demi-ton par rapport à la précédente,
  transformant une pile de notes identiques en déclenchements consécutifs de tranches. Plusieurs instruments
  découpés se répartissent indépendamment depuis leur première note, les notes sur instruments non découpés
  restent inchangées, et le résultat n’est jamais calé sur la gamme du projet. La dernière note placée devient
  la dernière note utilisée en saisie, donc continuer manuellement choisit la tranche suivante. Une sélection
  ne contenant que des instruments découpés se répartit à chaque pression ; en présence de notes non découpées,
  la répartition se fait à la première pression et un arpège aléatoire prend le relais à partir de la deuxième pression.
- **OPT + BAS** sur une sélection sans notes d’instruments découpés est un arpégiateur aléatoire
  basé sur les notes déjà placées. Un accord est choisi dans la palette CRD — accords 3 notes
  pour sélections de 1-2 notes, 3+ notes pour 3, et tout accord pour 4 ou plus — avec sa fondamentale
  sur une note aléatoire de la gamme du projet dans la tessiture de la sélection (n’importe quelle note chromatique si la gamme est désactivée ou la piste exclue),
  et ses notes sont quantifiées dans la gamme comme la lecture pour CRD. Chaque pression redistribue
  l’accord sur les notes placées : aléatoire, plus bas au plus haut, plus haut au plus bas, puis montée-descente ;
  la 5e pression restaure les notes originales et la suivante choisit un nouvel accord. Le nom de l’accord s’affiche
  dans la barre d’état pendant l’arpège actif, aucune note n’est ajoutée ou supprimée, et relâcher **OPT**
  réinitialise tout le processus.
- Chaque pression de remplissage efface les notes de la sélection avant d’écrire, et les notes placées utilisent le dernier instrument utilisé ; relâcher **OPT** réinitialise les cycles à leur première étape.

### Clavier jazz (bureau seulement)

Appuyez sur **Esc** dans l’écran Phrase pour activer/désactiver le clavier jazz, un mode
de saisie de notes style piano QWERTY (comme [m8c](https://github.com/laamaa/m8c)) :

- `Z S X D C V G B H N J M` et `Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P` tapent des notes
  chromatiquement sur plus de deux octaves, suivant la position physique de chaque touche
  pour que la disposition soit la même sur AZERTY, QWERTY et QWERTZ.
- Chaque touche écrit la note sur la ligne courante et passe à la ligne suivante.
- `[` / `]` changent l’octave.
- **Shift + flèches** sélectionnent une plage de lignes et colonnes ; une flèche simple
  efface ensuite la sélection.
- **Ctrl+C** / **Ctrl+X** copient ou coupent les colonnes sélectionnées, ou la note/instrument/volume
  de la ligne courante si rien n’est sélectionné ; **Ctrl+V** colle à la position du curseur.
- **Delete** supprime toute(s) ligne(s) (toutes colonnes) et décale le reste de la phrase vers le haut ;
  le curseur reste sur la même ligne.
- **Backspace** supprime l’élément sous le curseur dans la colonne courante uniquement (ou celles sélectionnées),
  décale vers le haut ce qui suit, et recule le curseur d’une ligne, comme un éditeur de texte.
- **Insert** insère une ligne vide, poussant le reste de la phrase vers le bas.
- **Ctrl+S** sauvegarde le projet. Hors clavier jazz, Ctrl+S est inactif.
- Appuyez à nouveau sur Esc pour désactiver le clavier jazz.

Lorsqu’il est activé, le clavier jazz prend totalement le contrôle des touches notes dans l’écran Phrase,
y compris les raccourcis Edit/Opt/Motion qui partagent ces touches — désactivez-le avec Esc pour réutiliser ces raccourcis.

Le clavier jazz fonctionne aussi, activé/désactivé indépendamment avec Esc, sur :

- **Song** et **Chain** : saisissez directement l’index hexadécimal d’une chaîne ou phrase (`0-9`, `A-F`)
  au lieu d’incrémenter avec Haut/Bas. Le premier chiffre tapé remplace la valeur dans la cellule ; les chiffres suivants,
  tapés sans déplacer le curseur, se placent dans le chiffre de droite (taper « 3 » puis « F » donne 3F).
  Sur Song, **Shift+flèches** sélectionnent une plage, **Delete** supprime des lignes entières et décale vers le haut,
  **Backspace** supprime la/les colonne(s) courante(s) et décale vers le haut, **Insert** ajoute une ligne vide,
  et **Ctrl+C**/**Ctrl+X**/**Ctrl+V** copient/coupent/collent les lignes sélectionnées (ou la ligne courante si rien) —
  les mêmes raccourcis d’édition de structure que dans Phrase. Cela prend le contrôle de Shift, donc Shift+Droite/Haut
  ne navigue plus vers Chain/Project tant que le clavier jazz est actif ici — Esc pour récupérer ces raccourcis.
- **Tous les écrans avec un champ texte** : saisie directe au clavier au lieu du clavier virtuel à l’écran.
  Cela inclut le nom du fichier Projet, le titre et l’auteur, le nom de l’instrument, le thème couleur,
  le nom de la table de hauteur, le nom du fichier de bounce, et les boîtes de dialogue de nom (saisie de nom, création de dossier).
  Backspace fonctionne comme un champ texte classique ; Shift pour les majuscules. Esc active/désactive simultanément
  ce mode sur tous ces écrans, et le clavier virtuel s’ouvre normalement lorsque le clavier jazz est désactivé.
- **Instrument** : avec clavier jazz activé, les touches notes jouent l’instrument de la piste courante pour audition,
  sans entrer de note (même disposition piano que dans Phrase ; la dernière touche pressée sonne, et le son est libéré à la relâche).
  Les touches d’octave changent l’octave, depuis celle de la première note de l’instrument. Dans le champ **Name**,
  les touches tapent du texte. Rien ne se joue pendant la lecture de la piste.

## 5. Instruments

Tous les instruments partagent quelques paramètres communs :

- Type d’instrument
- Nom, jusqu’à 15 caractères
- Vitesse table par défaut, en ticks par ligne de table
- Volume
- Panoramique
- Transposition activée ou non

Chaque instrument a une table par défaut avec le même numéro dans la plage `00-7F`. Vous pouvez les utiliser comme tables auxiliaires,
mais garder les tables auxiliaires dans la plage `80-FE` évite les conflits et la confusion. Les tables sont parmi les principaux outils
de design sonore dans ChooChooTracker.

### Contrôles

- **OPT + [GAUCHE/DROITE]** : naviguer entre les instruments
- **EDIT + PLAY** : préécouter l’instrument
- **SHIFT + OPT** : copier l’instrument
- **SHIFT + EDIT** : coller l’instrument

### SOUNDFONT (expérimental)

Copiez vos banques SF2/SF3 originales, instruments SFZ/EXS avec leurs samples, ou archives ZIP
dans `instruments/USER/soundfont`. Sélectionnez **SOUNDFONT** dans Instrument. La ligne de scan affiche **Not scanned**, **Scanning...** ou **Scanned**, avec une action **Rescan**.
Le scan analyse les métadonnées ; il ne charge pas tous les samples de chaque preset en mémoire.

La ligne **Preset** montre le preset actif. Tapez et relâchez EDIT pour ouvrir le navigateur
sur ce preset : les collections s’affichent à gauche et leurs presets à droite. Chaque collection
présente une liste plate, incluant les presets dans les banques et archives ZIP.
Les fichiers WAV/AIFF associés restent avec leurs instruments mappés ; les collections ne contenant
que des samples individuels listent ces samples comme presets. Les IDs de téléchargement et hashes
d’archive sont retirés des étiquettes d’affichage ; la note musicale, BPM et numéro de prise sont conservés.
EDIT sélectionne un preset ; EDIT+PLAY préécoute la sélection chargée.

Maintenez EDIT et pressez Gauche/Droite sur Preset pour parcourir séquentiellement dans l’ordre du navigateur,
en continuant vers la collection suivante et en bouclant à chaque extrémité de la bibliothèque.
La navigation rapide prépare la dernière sélection demandée. Le preset courant reste jouable pendant que le remplacement se prépare.
Si le chargement échoue, le preset demandé reste sélectionné et l’instrument devient silencieux. **Error :**
et la raison apparaissent en bas d’Instrument ; la navigation continue à partir de ce preset dans les deux sens.
Rescan après import ou modification des fichiers de la bibliothèque. Le statut du preset apparaît
juste sous Preset avec une couleur fixe. Le chargement ne poste pas de messages dans SONG ou d’autres pages.

**Native keys** montre la plage de notes-on définie par l’auteur du preset, utilisant C4 pour la note MIDI 60.
**(gaps)** signifie que certaines notes dans cette plage n’ont pas de zone mappée. C’est l’union sur les couches de vélocité ;
toutes les vélocités ou articulations ne doivent pas forcément sonner sur toutes les touches affichées.
Les samples release-only ne prolongent pas la plage. Les réglages de tuning et les FX de pitch n’affectent pas cet affichage ni la cartographie.

**Edit**, à droite de Preset, ouvre l’éditeur SoundFont expérimental :
réglage de l’accordage (−2400 à +2400 cents), coupure (20–20000 Hz), résonance (0–40 dB), enveloppe delay/attack/hold/decay/release (0–20000 ms), sustain (0–100%), volume, panoramique, et
**Bend step** (1–100 cents par unité PBN). Les nouveaux instruments utilisent un pas de bend à 1 cent.
Les anciens fichiers affichent **Stock**, conservant leur ancienne échelle PBN (25 cents en mode pitch linéaire, 1 en mode période) ;
réglez Bend step à 1 pour des bends plus fins. Clear restaure un pas de bend à 1 cent.
Cela concerne uniquement le PBN SoundFont ; FPT reste à l’accordage fixe.
Volume et panoramique sont les mêmes valeurs d’instrument affichées sur la page d’accueil.
Les champs tone et envelope par défaut à **Bank** conservent la valeur de chaque couche.
Une valeur explicite remplace ce paramètre sur toutes les couches du preset.
Clear remet à **Bank**, zéro cent pour Tune, volume maximal, et panoramique centré.
EDIT+Gauche/Droite fait des réglages fins ; EDIT+Haut/Bas fait des changements plus larges.
OPT revient à Instrument ; EDIT+PLAY auditionne l’instrument sur sa note de song ou par défaut.
Ce réglage modifie la lecture de l’instrument, pas la cartographie banque par sample, zones touche/vélocité, ni les points de boucle.

Le tuning, cutoff et résonance s’appliquent en temps réel. Attack, delay et hold se règlent à note-on.
Decay, sustain et release peuvent être mis à jour sur un son en cours ; la release commence à la note OFF, pas seulement sur ligne vide.
Le decay est inaudible à 100% sustain. Un temps de release ne peut pas prolonger un sample au-delà de l’audio disponible.
Un override explicit de release rend aussi un sample one-shot sensible au note OFF.
Avec les réglages Bank, le comportement one-shot d’origine est conservé.
Les plages de touches et vélocités du preset restent autoritaires ; les notes hors plage sont silencieuses.
FPT peut décaler un sample déjà sélectionné au-delà de sa plage sans changer la cartographie.
Les modifications sont sauvegardées dans les fichiers instrument et projet sans modifier la banque source ;
les anciens morceaux sans override conservent leurs réglages banque d’origine.Un WAV avec un type de boucle spécifique au sampleur inconnu peut lire son audio sans cette boucle. L’avertissement de l’Instrument indique le type de boucle ignoré ; le fichier source reste inchangé. Une boucle WAV avant se terminant exactement un échantillon au-delà de l’audio est acceptée comme une fin exclusive à la frontière de l’échantillon, avec un avertissement ; les correspondances PCM et SFZ/EXS explicites restent inchangées. D’autres bornes de boucle invalides et notes de base génèrent des erreurs spécifiques. Un seul octet d’alignement RIFF nul compté par erreur dans une trame PCM de taille impair est ignoré avec un avertissement ; d’autres trames PCM partielles restent des erreurs. Les boucles WAV standards alternées/arrière restent non supportées.

**SoundFont FX** expose huit contrôles dans le sélecteur contextuel Lane FX. Toutes les valeurs ci-dessous utilisent la notation hexadécimale tracker. Les FX affectent uniquement leur piste et ne modifient pas les contrôles enregistrés de l’instrument. Chaque nouvelle note part des réglages de l’instrument, puis applique les commandes sur sa ligne et les suivantes.

| FX  | Contrôle | Valeurs |
| --- | --- | --- |
| FPT | Pitch | `00` = −24 demi-tons, `18` = zéro, `30` = +24 ; pas exact en demi-tons |
| FCF | Cutoff | `00`–`FE` : 20 Hz–20 kHz, échelle logarithmique |
| FRS | Résonance | `00`–`FE` : 0–40 dB |
| FAT | Attaque | `00`–`FE` : 0–20 secondes, résolution plus fine sur les temps courts |
| FHD | Maintien | `00`–`FE` : 0–20 secondes avant déclin, réglé sur la ligne de note |
| FSU | Sustain | `00`–`FE` : 0–100% niveau maintenu |
| FDC | Décroissance | `00`–`FE` : 0–20 secondes, résolution plus fine sur les temps courts |
| FRL | Relâchement | `00`–`FE` : 0–20 secondes, résolution plus fine sur les temps courts |

Pour FCF/FRS/FAT/FHD/FDC/FSU/FRL, `FF` revient au réglage Edit (incluant **Bank**). Mettez Attaque/Maintien sur la ligne de note. Décroissance/Sustain/Relâchement peuvent modifier la note active ; utilisez OFF pour entendre son relâchement. Volume et panoramique utilisent les contrôles communs ; les commandes héritées FVL/FPN se chargent toujours et jouent mais ne sont plus proposées. Les FX Pitch modifient l’accord sans changer la zone clé sélectionnée de la bank. Les commandes de pitch/fine-pitch du séquenceur, MODs et effets de piste restent disponibles. Les projets sauvegardent des références stables bank/preset et commandes FX ; les bibliothèques samples restent externes.

Toutes les banks sont des USER banks. De gros instruments peuvent dépasser le cache PCM par défaut de 64 Mio, et des paramètres source non supportés génèrent des avertissements. Consultez le [guide SoundBanks](soundbanks/README.md) pour les chemins plateformes, import dans le navigateur, configuration du cache, couverture de format et limites actuelles de validation.

#### Compatibilité SoundFont : banks, presets et modulation

Un fichier **bank SF2 ou SF3** peut contenir de nombreux **presets**, avec leurs samples. Une **collection** de bibliothèque est le dossier utilisé pour regrouper ces fichiers dans le navigateur. Ce sont des niveaux différents : un preset peut avoir un comportement non supporté même si d’autres presets du même fichier sont bien traduits.

La **modulation de preset** signifie des liens programmés depuis des entrées comme vélocité, touche, molette mod ou pression vers un paramètre sonore. Ces liens peuvent appartenir à un preset ou à une couche d’instrument sous-jacente partagée par plusieurs presets. Ce n’est pas forcément un réglage affectant tout le son de la bank.

| Fonctionnalité preset | Traduction actuelle |
| --- | --- |
| Cartographie sample/touche/vélocité, note de base et accord | Implémenté ; plages d’écriture conservées. Les zones vélocité et vélocité tracker ordinaires fonctionnent toujours. |
| Décalages de sample, boucles supportées, gain/pan et groupes exclusifs | Implémentés dans l’analyseur et limites de voix. |
| Filtre passe-bas statique, résonance et enveloppe de volume | Implémenté via le moteur tracker ; pas de garantie de reproduction identique avec un autre lecteur SoundFont. |
| Routes de modulation preset/instrument personnalisées (`pmod` / `imod`) | Non implémentées. Mappings contrôleur-paramètre, quantités et courbes de réponse ignorées. |
| LFO modulation/vibrato SoundFont et routage d’enveloppe de modulation | Non implémentés ; mouvements programmés de pitch, filtre ou volume peuvent manquer. |
| Maintien/décroissance d’enveloppe volume à échelle de note, générateurs fixe-touche/fixe-vélocité | Non implémentés. |
| Envois chorus/reverb SoundFont | Non implémentés. Les effets tracker peuvent être réglés manuellement mais ne reproduisent pas automatiquement les réglages du preset. |

L’ancien message **« Custom bank modulation ignored »** décrit un contrôle au niveau fichier : l’analyseur a trouvé des enregistrements de modulation personnalisée quelque part dans la bank. Il ne confirme **pas** que le preset sélectionné les utilise, ni que la bank soit inutilisable. Cette limitation générale est documentée ici au lieu de répéter ce message sous chaque preset. Le diagnostic détaillé de l’analyseur conserve les enregistrements ; d’autres erreurs de chargement et avertissements de compatibilité apparaissent encore si applicable. Le moteur ne prétend pas non plus un comportement complet des modulateurs par défaut SoundFont. Les PBN tracker, vibrato, Edit et Lane FX sont des contrôles indépendants, pas des traductions automatiques de la programmation de preset omise.

Pour choisir une bibliothèque, commencez par des presets basés principalement sur la cartographie de samples, boucles et enveloppes statiques. Les sons dont le caractère dépend d’articulations contrôleur, filtres animés ou LFOs programmés nécessitent une comparaison plus poussée dans un lecteur SoundFont complet. **Charge et joue** ne veut pas dire **reproduit intégralement le preset**. Comparez notes utiles et vélocités, ainsi que notes maintenues et note OFF. Passer de SF2 à SF3 (ou vice versa) ne supprime pas ces besoins de modulation. Une conversion doit traduire cartographies et comportement en fonctionnalités que ce moteur supporte ; rendre un effet dans un sample ne conserve que ce rendu, pas sa réponse interactive aux contrôleurs. Voir la [couverture format et limites](soundbanks/README.md#format-coverage) et les [notes de validation](soundbanks/validation.md) avant de choisir ou convertir une bank. Ces limites décrivent l’implémentation expérimentale actuelle, pas les restrictions inhérentes à tous les lecteurs SoundFont.

#### Parcourir les collections SoundFont

Dans SAMPLE → SOUNDFONT, **Preset** ouvre vos collections. **Right** ou **EDIT/A** entre directement dans un dossier. Dans une collection feuille, Right passe à ses presets ; un autre Right avance de 15 entrées sans charger. EDIT/A charge le preset surligné. Left revient aux collections, puis au dossier parent ; OPT revient aussi en arrière. PLAY démarre/arrête la chanson pendant la navigation.

Des icônes de dossier et de son identifient les entrées. Les collections utilisent toute la hauteur du panneau. Le preset sélectionné reste sous les huit aperçus de pistes ; les noms longs défilent après une seconde d’inactivité de la sélection et repartent quand vous bougez. EDIT+Left/Right sur la page instrument suit le même ordre de dossiers imbriqués, en gardant ensemble les sons de chaque collection. EDIT+PLAY auditionne.

La troisième ligne indique Reading preset, Finding samples, Unzipping, Checking samples ou Loading samples. Les instruments EXS dont les noms intégrés sont vides ou `(null)` utilisent leurs noms de fichier. Les banques sources et l’audio restent inchangés. Utilisez **Rescan** après avoir modifié les fichiers de votre bibliothèque.

Voir [couverture des formats et limites](soundbanks/README.md#format-coverage) pour les comportements pris en charge du sampler et les formats d’échantillons.

### AY Classic, AY Plus et AY Sample

Ce sont les moteurs originaux ChipNomad AY/YM. AY Classic expose des contrôles hardware-style ton, bruit et enveloppe. AY Plus ajoute des oscillateurs software et plus de modulation. AY Sample reproduit un sample via des niveaux de volume style AY, distinct du moteur Sampler.

#### AY Classic

Type d’instrument hérité de ChipNomad `v1.0.0`. Le plus récent instrument AY Plus offre plus de fonctions.

Les instruments AY Classic ont ces paramètres :

- Mixer : ton activé ou désactivé, bruit activé ou désactivé, forme d’enveloppe (`0-F`)
- Volume : enveloppe ADSR générée software
- Période enveloppe automatique : activée ou désactivée, avec un ratio de `1:1` à `F:F`

La **qualité AY** dans Settings affecte uniquement le rendu AY/YM. Elle ne modifie pas la qualité de Braids, Plaits ou Sampler. Le **dithering sample** s’applique à la quantification AY Sample.

Voir la [documentation ChipNomad AY-3-8910](https://chipnomad.org/chips/ay-3-8910/) pour plus de détails sur la puce.

#### AY Plus

AY Plus applique des concepts synthé classiques au design sonore AY-3-8910/YM2149F. On peut voir AY comme un synthé à 5 oscillateurs :

- 3 oscillateurs tonal carré
- 1 oscillateur bruit
- 1 générateur d’enveloppe d’amplitude pouvant aussi servir d’oscillateur

Dans ce modèle, chaque instrument dispose de 3 oscillateurs hardware : ton, bruit et enveloppe. AY Plus ajoute un 4ᵉ oscillateur software. Mettre à jour les registres AY plus vite que la cadence du projet ouvre plus d’options sonores. Du fait du fonctionnement d’AY, les oscillateurs sont mixés en modulation en anneau. Testez différents offsets de pitch, désaccords et combinaisons d’oscillateurs.

Chaque oscillateur tonal peut être accordé indépendamment avec offset pitch et fin.

Types d’oscillateurs software et leurs paramètres :

- Pulse – onde carrée à largeur d’impulsion contrôlable
  - FM Depth – profondeur FM simple
  - Largeur d’impulsion – plage complète `00-FF`, nombre d’étapes réglé dans Project
  - Pulse low – contrôle la valeur basse dans une paire répétée dont la haute est toujours `15`
- Sync Tone – effet hard-sync pour l’oscillateur ton
  - FM Depth – profondeur FM simple
- Sync Env – effet hard-sync pour l’oscillateur enveloppe
  - FM Depth – profondeur FM simple
  - Largeur d’impulsion – ratio de durée entre 2 formes de la paire d’enveloppe
  - Paire enveloppe – 2 formes d’enveloppe entre lesquelles basculer
- Wavetable – tables d’ondes 32 pas, éditées sur l’écran Wavetable
  - FM Depth – profondeur FM simple
  - Index wavetable
- Tone FM – effet FM simple pour l’oscillateur ton
  - FM Depth – profondeur FM tonale
- Env FM – effet FM simple pour l’oscillateur enveloppe
  - FM Depth – profondeur FM enveloppe

Les oscillateurs software Pulse et Wavetable ne peuvent pas être utilisés avec l’oscillateur Envelope. Ce dernier s’éteint automatiquement.

La FM simple alterne rapidement entre 2 valeurs de période, une basse et une haute, similaire à une onde carrée comme modulateur FM. Pulse, Sync Tone, Sync Env et Wavetable utilisent un ratio `1:1` entre porteur FM et modulateur. Tone FM et Env FM peuvent imiter d’autres ratios via offset pitch des oscillateurs software.

#### AY Sample

Vous pouvez charger des fichiers WAV mono ou stéréo en `8-bit`, `16-bit`, `24-bit` ou `32-bit`. Ils sont convertis en `8-bit` et tronqués à `16 KB`. Cette limite correspond à la taille d’une page RAM du ZX Spectrum.

AY joue les samples par écritures rapides sur le registre volume. Sa sortie est unipolaire, donc les samples doivent être « remontés à zéro » pour un rendu plus propre. Cela supprime les clics en fin d’échantillon et améliore les queues silencieuses.

AY a une résolution DAC de `4-bit` avec une échelle de volume non linéaire, donc les samples `8-bit` sont convertis en `4-bit` à la lecture. L’option Dithering sample dans Settings peut améliorer la profondeur perçue. Le dithering n’est pas pratique sur des plateformes rétro comme ZX Spectrum ou Atari ST, donc désactivez-le pour du hardware réel ou un son lo-fi crunchy.

Les samples se comportent comme un autre oscillateur software AY Plus, mais leurs paramètres supplémentaires facilitent leur gestion comme type d’instrument séparé.

Paramètres du sample :

- Taux d’échantillonnage – jouer un sample à son taux natif produit un `C-4`
- Début du sample
- Longueur du sample
- Début de boucle sample
- Offset pitch sample
- Offset fine pitch sample

Les sections Ton et Noise des oscillateurs correspondent à AY Plus. Du fait du fonctionnement AY, tous les oscillateurs sont mixés en modulation en anneau.

### Braids

Braids propose 47 modèles de synthèse. Ses contrôles principaux sont :

- **Model** : oscillateur ou algorithme de synthèse
- **Timbre** et **Color** : paramètres macro dépendant du modèle
- **Filter** : filtre supplémentaire LP/BP/HP, pente `12` ou `24 dB`, cutoff et résonance
- **ADSR** : attaque, décroissance, sustain et relâchement

Tapez sur **Model** pour choisir depuis des listes de modèles catégorisées.

| Catégorie | Modèles |
| --- | --- |
| Analogique | `00 CSAW`, `01 MORPH`, `02 SAW-SQUARE`, `03 SINE-TRI`, `04 BUZZ`, `05 SQUARE-SUB`, `06 SAW-SUB`, `07 SQUARE-SYNC`, `08 SAW-SYNC` |
| Multi Osc | `09 TRIPLE-SAW`, `10 TRIPLE-SQR`, `11 TRIPLE-TRI`, `12 TRIPLE-SINE`, `13 TRIPLE-RING`, `14 SAW-SWARM`, `15 SAW-COMB`, `16 TOY` |
| Filtre / Voix | `17 FILTER-LP`, `18 FILTER-PEAK`, `19 FILTER-BP`, `20 FILTER-HP`, `21 VOSIM`, `22 VOWEL`, `23 VOWEL-FOF`, `24 HARMONICS` |
| FM / Chaos | `25 FM`, `26 FEEDBACK-FM`, `27 CHAOTIC-FM` |
| Physique | `28 PLUCKED`, `29 BOWED`, `30 BLOWN`, `31 FLUTED`, `32 STRUCK-BELL`, `33 STRUCK-DRUM` |
| Batterie | `34 KICK`, `35 CYMBAL`, `36 SNARE` |
| Wavetables | `37 WAVETABLES`, `38 WAVE-MAP`, `39 WAVE-LINE`, `40 WAVE-PARA` |
| Bruit / Granular | `41 FILTER-NOISE`, `42 TWIN-PEAKS`, `43 CLOCK-NOISE`, `44 GRAN-CLOUD`, `45 PARTICLE`, `46 DIGI-MOD` |

#### Paramètres

Timbre et Color dépendent du modèle sélectionné. Reportez-vous au manuel original Braids de Mutable Instruments pour la liste complète et détails, ou improvisez à l’oreille.

#### VCF et VCA

Braids alimente le filtre multimode ChooChooTracker standard et l’enveloppe ADSR.

#### Paramètres globaux

Les réglages globaux de Braids sont accessibles dans les settings de l’app :

- **BITS** : réduit la résolution en sortie
- **DRFT** : ajoute une instabilité de pitch d’oscillateur
- **SIGN** : reproduit l’algorithme d’imperfection de forme d’onde d’origine Braids

### aChChid**aChChid** est un moteur de basse acid monophonique basé sur Open303. `Square` et `Saw` utilisent son oscillateur TB-303 natif, filtre, enveloppe et comportement d'accent. `Braids` remplace uniquement l'oscillateur, puis continue dans le même chemin filtre 303 et amplificateur. Il expose Model, Timbre, Color et un waveshaper supplémentaire. Le waveshaper évolue de la saturation douce au wavefolding.

Cutoff et Decay utilisent une édition logarithmique : **EDIT + GAUCHE/DROITE** déplace d’un pas musical, tandis que **EDIT + HAUT/BAS** bouge de seize pas. Cutoff couvre 200 Hz à 20 kHz ; Decay couvre 200 ms à 2 s.

Un `F` dans la colonne volume de note déclenche un accent. `ASL` glisse vers cette note depuis la note précédente sans retrigger l’enveloppe 303 ; il maintient automatiquement la gate précédente, et `ASL 00` donne un glide de `60 ms`. `ATY` garde la note ouverte toute la durée du pas courant. Les notes sans `ASL` retrigger toujours.

Les destinations de modulation incluent Decay et Accent, plus Timbre et Color en mode onde Braids.

### Bogie

**Bogie** est un synthétiseur de batterie one-shot natif. Les notes du tracker fixent la hauteur, et le volume du tracker/instrument règle le niveau ; chaque déclenchement a son propre decay interne, donc il n’y a pas de page ADSR. Il dispose de 12 modèles : `Kick`, `Snare`, `Hat`, `Clap`, `Tom`, `Rim`, `FM`, `Noise`, `Cowbell`, `Cymbal`, `Shaker` et `Clave`.

Ses six macros sont **Decay**, **Tone**, **Sweep**, **Noise**, **FM** et **Drive**. Leurs étiquettes s’adaptent selon le modèle (par exemple, le Noise du Kick est `Click` et le FM de Cowbell est une modulation croisée métallique). Chaque macro est active sur tous les modèles, mais son rôle musical change selon le modèle : Sweep peut modifier la hauteur, l’espacement des rafales ou la diffusion métallique ; FM peut ajouter une modulation croisée, une couche métallique ou un son d’harmoniques en anneau. La moitié basse de chaque plage correspond aux sons de batterie conventionnels ; la moitié haute ouvre volontairement des morphings timbraux plus larges et des résultats plus synthétiques.

Tous les modèles peuvent utiliser le filtre multimode LP/BP/HP partagé, avec les caractères Clean, Classic, Aggro ou Acid, pente `12`/`24 dB`, cutoff et résonance. Bogie supporte toutes ses macros actives plus Volume, Pitch, Cutoff et Resonance comme destinations de modulation et d’enregistrement de mouvement. Utilisez un Decay court sur Cowbell pour des attaques phonk très nettes ; augmentez FM pour une cloche plus dure et métallique.

Sur l’écran Modulation, appuyez sur **EDIT** sur une source pour choisir une famille (Envelopes, LFO ou Sticks), puis sa source. **Gauche** et **Droite** parcourent encore les sources directement.

### Sintered

**Sintered** est un synthétiseur natif de percussion digitale one-shot. Les notes fixent sa hauteur fondamentale ; ses six modèles sont `Knot`, `Shard`, `Burst`, `Comb`, `Logic` et `Melt`. Chaque modèle a **Decay**, **Mod** et **Motion**, plus trois contrôles spécifiques : Knot utilise Ratio/Spread/Fold, Shard Ratio/Feedback/Bite, Burst Noise/Color/Feedback, Comb Time/Damping/Regen, Logic Rate/Pattern/Crush et Melt Ratio/Chaos/Drive. **Mod** est toujours la profondeur de couplage interne.

Motion est une enveloppe macro interne : valeurs sous `80` utilisent une forme AD, `80` la désactive, et au-dessus de `80` la forme est decay-only. Plus on s’éloigne du centre, plus l’enveloppe est rapide. Ses destinations sont fixes par modèle, pour garder Sintered prévisible dans les patterns tracker : Knot anime Mod/Fold ; Shard Feedback/Bite ; Burst Noise/Color/Feedback ; Comb Damping/Regen ; Logic Rate/Crush ; Melt Chaos/Drive. Tous les modèles réinitialisent leur état à chaque note et limitent leur feedback interne pour des frappes répétables.

### MonkSynth

MonkSynth est un synthétiseur vocal. Façonnez sa voyelle, le caractère de la voix, le souffle, le glide, le vibrato, l’unisson et le delay depuis la page Instrument. EDIT ouvre tous les contrôles et le nom du patch.

Choisissez une banque et un preset au-dessus des contrôles VOICE. ALL inclut FACTORY et USER ; EDIT+PLAY auditionne le preset surligné, tandis que EDIT le sélectionne. Sauvegardez vos propres sons sous `instruments/USER/monk/`. Les instruments et chansons sauvegardés conservent leurs réglages MonkSynth.

Les valeurs sont en hexadécimal `00..FF`, sauf Voices (`01..0A`, une à dix). Attack, decay et release couvrent 0..5 secondes ; le detune couvre 0..50 cents et l’écartement des voix 0..0,5. Volume, Pitch et les six contrôles principaux de la voix sont disponibles comme destinations de modulation et d’enregistrement de mouvement.

Les seize contrôles apparaissent ensemble sous **MonkSynth FX** : `MWV` (voyelle), `MIN` (voix), `MAM` (souffle), `MFL` (glide), `MFB` (profondeur du vibrato), `MSH` (vitesse du vibrato), `MUV` (voix), `MDT` (detune), `MVS` (écartement), `MDM` (mix du delay), `MDR` (vitesse du delay), `MLV` (niveau) et `EAT`/`EDC`/`ESU`/`ERL` (ADSR). Ces significations s’appliquent quand MONK est l’instrument actif ; les significations existantes des commandes MME restent inchangées. Le groupe Trigger FX partagé conserve TDC/TCL sans entrées d’enveloppe en double.

### MME

Choisissez **SYNTH → MME** dans le menu Type d’instrument.

**MME** (Multi Modulation Engine) est un synthétiseur à deux oscillateurs dédié aux interactions entre oscillateurs. Ses modèles sont `Ring`, `Fold`, `Cross`, `VPM`, `Sync`, `Logic` et `Vocode`. Les deux oscillateurs sont internes et suivent les notes du tracker ; `Interval` règle leur relation.

Les six macros sont **Waves**, **Interval**, **Amount**, **Flow**, **Feedback** et **Shaper**. Waves interpole entre paires d’ondes musicales A/B. Amount contrôle l’interaction de base du modèle, Flow change la direction ou la variante algorithme, et Feedback est limité mais devient très sauvage en fin de plage. Shaper est une étape finale partagée allant du propre à la saturation puis au wavefolding.

MME utilise le filtre multimode LP/BP/HP partagé et un ADSR. Les six macros, Cutoff et Resonance sont disponibles à la modulation et à l’enregistrement de mouvement. Maintenez **EDIT** sur Model pour ouvrir la popup modèle.

### MIDI Out

**MIDI Out** (desktop, PortMaster et navigateurs Web MIDI) pilote un appareil MIDI externe au lieu de synthétiser du son : déclencher une note envoie un vrai Note On MIDI sur le **Channel** (`1-16`) de l’instrument et, à la fin de la note, le Note Off correspondant. Le volume devient la vélocité, et les voix d’une piste d’accords reçoivent chacune leur propre Note On/Off. Aucun son n’est produit par ChooChooTracker lui-même – sélectionnez d’abord le périphérique de sortie MIDI dans [Settings](#13-settings). Voir [MIDI](#15-midi) pour tous les détails, y compris l’aperçu sonore via un clavier MIDI.

**Program** et **Bank high/low** (CC0/CC32) sont optionnels – affichés `--` quand désactivés, `Clear` bascule un champ off tout en gardant sa valeur. Si définis, ils sont envoyés une fois juste avant la prochaine note, chaque fois qu’ils diffèrent de la dernière valeur envoyée sur ce canal (pas avant chaque note pour éviter de retrigger les enveloppes/patchs du dispositif récepteur).

Les numéros **MC1** à **MC4** choisissent le numéro CC (`00-7F`) envoyé par chacune des quatre lignes MC1-MC4 – voir [MIDI Out FX](#midi-out-fx). Aussi `--`/optionnel ; une case non définie rend son FX inerte.

### Moteurs d’architecture classique

Les moteurs de cette catégorie partagent une architecture VCO > VCF > VCA.

Plusieurs types de VCO sont disponibles :

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **Sampler**
- **2xSCWF**
- **BYOWTBL**

Chaque instrument alimente son VCO dans un filtre multimode.

Le filtre a une pente commutable `12 dB` ou `24 dB` et peut fonctionner en passe-bas (LP), passe-bande (BP) ou passe-haut (HP).

Plusieurs caractères de filtre sont disponibles :

- **Clean** : stérile et digital
- **Classic** : inspiré des synthés analogiques américains classiques
- **Aggro** : inspiré des synthés analogiques japonais rugissants
- **Acid** : double dose d’agressivité d’Aggro

Classic, Aggro et Acid peuvent être très résonants. Commencez bas et montez doucement.

Le filtre alimente un VCA contrôlé par une enveloppe ADSR. Sa forme peut varier entre exponentielle, linéaire et logarithmique.

#### Plaits et Plaits-Alt

**Plaits** propose 24 moteurs, groupés dans la popup de sélection comme suit :

| Catégorie | Moteurs |
|---|---|
| Analogique / Ondes | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digital | `07 CHIPTUNE`, `15 SPEECH` |
| Texture / Bruit | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Physique | `19 STRING`, `20 MODAL` |
| Batteries | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** est une collection de modèles alternatifs pour le module Plaits original. La plupart sont plus étranges ou expérimentaux qu’un VCO standard.

| Catégorie | Moteurs |
|---|---|
| Granulaire / Micro | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| Phase / Harmonique | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| Acoustique / Physique | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| Polyphonie / Harmonie | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| Digital / Étrange | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

Les paramètres suivent la conception Mutable :

- **Harmonic** contrôle les relations harmoniques, l’équilibre ou le choix du modèle dans un moteur.
- **Timbre** évolue généralement d’un spectre sombre/clairsemé à un spectre brillant/dense.
- **Morph** explore une autre dimension timbrale.
- **Main/Aux** mélange la sortie principale avec la sortie alternative du moteur.

Leur sens précis dépend du moteur. Par exemple, les moteurs d’accords les utilisent pour le type, l’inversion et la forme d’onde. Les modèles physiques pour le matériau, l’excitation et le decay, et les batteries pour le ton, le caractère et le decay. Consultez le manuel Plaits pour plus de détails.

Pour `CHORD`, `DIATONIC CHORD` et `WT DIATONIC CHORD`, Harmonic affiche le nom de l’accord sélectionné (`MIN`, `MAJ`, `MIN7`, `MAJ7`, etc.) plutôt que sa valeur brute. Braids `WAVE PARA` nomme aussi ses voicings Color.

Tapez **Engine** pour choisir dans les listes de moteurs catégorisées.

**Env Mode** a 2 routages :

- `TRIG` reproduit le module avec TRIG connecté et LEVEL non patché
- `VCA` maintient LEVEL ouvert et applique l’ADSR tracker après la voix

#### Sampler

Ce moteur Sample propre lit des échantillons PCM mono ou stéréo chargés en RAM.- Touchez **Sample** pour charger un WAV PCM non compressé en `8-bit`, `16-bit` ou `24-bit`. Appuyez sur **PLAY** dans le navigateur pour écouter le fichier sélectionné.
- Lorsqu'un échantillon est chargé, **EDIT** apparaît à côté de son nom. Touchez-le pour ouvrir l’écran d’édition du Sample. **OPT** ou **SELECT + [LEFT]** permet de revenir à l’instrument.
- L’écran d’édition du Sample affiche le nom de fichier (avec un marqueur `*` si l'échantillon en RAM diffère du fichier sur disque), les informations de format (fréquence d’échantillonnage, canaux), une forme d’onde verticale, et les champs **Region**, **Select**, **Slice**, **Process** et **File**.
- Les modifications vivent uniquement en RAM jusqu’à ce qu’un enregistrement les écrive : le projet stocke le chemin du fichier de l’échantillon et recharge le WAV depuis le disque au prochain chargement du projet, donc quitter le tracker sans **Save**/**Save As** annule toutes les opérations de traitement (le marqueur `*` sert d’avertissement). Charger un autre échantillon dans l’instrument annule aussi les modifications non sauvegardées et réinitialise la Region sur l’échantillon entier (Start `00`, End `FF`).
- **Region** définit les marqueurs de lecture Start/End, aussi disponibles sur l’écran de l’instrument Sampler. Ils fixent les limites de lecture normalisées (`00-FF`) ; si Start est après End, l’échantillon joue à l’envers. **EDIT + [LEFT/RIGHT]** ajuste finement le marqueur d’un pas (`01`) et zoom la forme d’onde autour tant que **EDIT** est maintenu ; **EDIT + [UP/DOWN]** ajuste grossièrement (pas 16) et réaffiche la forme d’onde complète. **EDIT + OPT** réinitialise le marqueur à sa valeur par défaut (Start `00`, End `FF`).
- La vue forme d’onde zoome sur une fenêtre fixe d’une seconde d’audio autour du marqueur modifié et fait un panoramique pour le garder visible ; les échantillons plus courts que la fenêtre restent affichés en 1:1. Le zoom est temporaire : il dure tant que **EDIT** est maintenu, et relâcher **EDIT** revient à la vue 1:1 complète, tout comme les ajustements grossiers. Entrer dans cet écran réinitialise toujours la vue à l’échantillon complet.
- **Select** définit une sélection pour le traitement en trames absolues, indépendante des marqueurs de lecture Start/End. **EDIT + [LEFT/RIGHT]** déplace une poignée de vingt trames et zoom dessus tant que **EDIT** est maintenu ; **EDIT + [UP/DOWN]** saute par `frameCount/64` (minimum 16) et revient à la vue complète. **EDIT (tap)** sur une poignée copie la position actuelle du marqueur Start ou End dessus. **EDIT + OPT** vide la sélection complète. Quand la sélection est vide, les deux poignées affichent `-` ; si elles sont inversées, elles s’échangent automatiquement. La sélection est un état temporaire de l’éditeur pour la session : elle n’est pas sauvegardée avec le projet et est initialisée à la portée de la Region de lecture à l’entrée de l’écran (l’échantillon complet avec les marqueurs par défaut).
- **Slice** est une seule ligne avec quatre cellules de valeur — **Mode**, **Count**, **Slice** et **Frame** — et est sauvegardée avec l’instrument. Off joue la fenêtre Start/End. Les notes de Phrase sélectionnent les slices chromatiquement depuis **C-0**, et les notes après le dernier slice bouclent au début (la note N joue le slice `N % count`, donc une course au-delà du dernier slice repart au premier). Les notes slicées ne transposent pas la hauteur ni n’utilisent la quantification Scale, mais **CRD** conserve le slice sélectionné et transpose ses voix comme un accord. Les départs de slices sont marqués sur la forme d’onde par de courtes lignes de deux pixels de large (orange foncé ; la ligne du slice courant est plus claire), et le slice sous le curseur reçoit une bande de fond noire sur la forme d’onde (la teinte de sélection ne recouvre pas la bande, qui reste entièrement noire et bien distincte du reste de l’aperçu).
  - **Mode** fait défiler `OFF` / `EQUAL` / `AUTO` / `LAZY` (EDIT + gauche/droite ou tap ; EDIT + OPT désactive le slicing). Changer de mode initialise les points de slice : **EQUAL** divise la fenêtre Start/End en parts égales selon **Count** ; **AUTO** fait une détection de transitoires dans la fenêtre et place un slice sur chaque attaque détectée (jusqu’à **Count**) ; **LAZY** commence avec un seul slice couvrant toute la fenêtre. Passer à LAZY mémorise la configuration EQUAL/AUTO courante (count et bornes), et revenir à EQUAL ou AUTO la restaure intégralement — sans confirmation et sans perte. Slice et Stretch sont exclusifs : activer l’un désactive l’autre, et les cellules Slice sont atténuées quand Stretch contrôle la durée.
  - **Count** affiche le nombre total de slices (`01`..`64`). **EDIT + [LEFT/RIGHT]** l’ajuste par pas de un (minimum 1, maximum 64), **EDIT + [UP/DOWN]** fait défiler les puissances de deux `2 / 4 / 8 / 16 / 32 / 64` en boucle. Chaque changement recalcule la division : **EQUAL** redivise la fenêtre uniformément, **AUTO** relance la détection avec la nouvelle cible (la sensibilité dépend du count — plus de slices abaissent le seuil). La boîte affiche `-` en mode OFF. En **LAZY** elle reste lumineuse (elle compte les slices ajoutés à la main) mais est affichée seulement : le curseur l’ignore — en allant à droite depuis **Mode** ou à gauche depuis **Slice**, on arrive sur **Slice**, descendre depuis le marqueur **END** de la ligne **Select** atterrit aussi dessus, et toucher la boîte ne fait rien (les slices manuels ne reposent pas sur une division à recalculer). **EDIT + OPT** et un tap simple sur cette cellule ne font rien ici : désactiver le slicing se fait sur **Mode**, supprimer un slice sur **Slice**.
  - **Slice** est le navigateur : il montre le slice courant indexé à partir de 1 (`01`..count). **EDIT + [LEFT/RIGHT]** se déplace d’un slice à la fois, **EDIT + [UP/DOWN]** saute 4 slices (limité, sans boucle) ; le slice parcouru est indiqué sur la forme d’onde par son marqueur plus lumineux et la bande noire, et la vue zoomée se recentre sur le départ du slice. **EDIT + OPT** supprime le slice courant (il fusionne avec le précédent ; le premier slice ne peut pas être supprimé, et supprimer le dernier slice restant désactive le slicing — les bornes survivent à la désactivation et sont restaurées au retour en mode). **EDIT (tap)** joue un aperçu du slice sélectionné en playback one-shot (fonctionne en tous modes de slices ; l’aperçu s’arrête en relâchant la touche ou en quittant l’écran). La boîte affiche `-` en mode OFF.
  - **Frame** affiche la trame de départ du slice courant en hexadécimal. **EDIT + [LEFT/RIGHT]** la déplace de dix trames (zoom sur le marqueur tant que **EDIT** est maintenu), **EDIT + [UP/DOWN]** déplace de cent trames et revient à la vue complète. Un marqueur reste dans son propre slice : il ne peut pas dépasser le départ du slice précédent ou suivant. **EDIT + OPT** supprime le slice.
  - **B** quitte vers l’écran de l’instrument depuis la ligne Slice comme partout ailleurs sur cet écran ; **SHIFT + LEFT** quitte depuis n'importe où.
  - **Workflow LAZY** : avec LAZY sélectionné, touchez **PLAY** pour lancer une lecture one-shot complète de l’échantillon — il joue une fois depuis le début et s’arrête seul, à la hauteur et vitesse originales (un marqueur vert de deux pixels suit la position sur la forme d’onde) ; toucher **PLAY** à nouveau l’arrête avant la fin. Pendant la lecture, chaque clic sur **EDIT** ajoute un slice à la position de lecture, peu importe où se trouve le curseur (les slices situés à moins de 50 ms d’un slice existant sont refusés avec le message `Too close to slice`). Relancer la lecture conserve les découpages existants et ajoute des nouveaux à chaque position tapée, dans l’ordre — la boîte **Count** est mise à jour après chaque slice ajouté et le navigateur **Slice** suit le nouveau. Les mêmes règles de navigation et modification que dans les autres modes s’appliquent aux slices manuels : **EDIT + direction** sur **Slice** les parcourt, **Frame** ajuste le départ sélectionné, **EDIT + OPT** supprime le slice courant. La cellule Frame est atténuée pendant que la suppression par lecture est armée. **SHIFT + PLAY** n’est pas intercepté en LAZY : il lance la lecture de phrase comme partout ailleurs. Les slices LAZY jouent dans le séquenceur comme EQUAL et AUTO : les notes de phrase se répartissent chromatiquement à partir de **C-0** sur les slices manuels, en bouclant après le dernier.
  - **Astuces de combo** : pendant que le curseur est sur une cellule de la ligne Slice et qu’aucun message n’est affiché, la barre d’état montre l’info-bulle de cette cellule et la maintient tant que le curseur y reste. Sur **Mode**, l’info-bulle dépend du mode actif et n’affiche que la description (le nom du mode est déjà dans la cellule) : `Divides sample in equal parts`, `Divides sample based on transients`, `Press Play and add slices with EDIT` — changer de mode change instantanément le texte, et OFF efface la barre. Sur **Count** : `Adjust the number of slices`, sur **Slice** : `Browse slices`, sur **Frame** : `Adjust slice start`. Toute action remplace l’info-bulle jusqu’à expiration.
- **Process** sélectionne une opération destructive d’édition : **Crop**, **Normalize**, **Delete**, **Silence**, **Fade In**, **Fade Out** ou **Reverse** (EDIT + gauche/droite change, tap avance dans la liste, EDIT + OPT désactive). **GO** (même ligne) applique l’opération à la sélection courante, ou à l’échantillon entier si la sélection est vide. **Crop** garde uniquement la sélection ; **Normalize** ajuste le gain pour que le pic atteigne le niveau maximal (les deux canaux partagent un même gain pour préserver l’image stéréo) ; **Delete** supprime la sélection et relie les extrémités (supprimer l’échantillon complet est refusé) ; **Silence** la remplace par du silence ; **Fade In**/**Fade Out** applique un fondu linéaire d’entrée/sortie sur la sélection ; **Reverse** inverse la sélection (les trames sont échangées sur place, les deux canaux d’une trame bougent ensemble, la longueur et les marqueurs ne changent pas). Crop et Delete nécessitent une sélection — sans sélection ils affichent `Select region first`. Chaque opération met l’audio en pause brièvement, conserve un niveau d’annulation, et marque l’échantillon comme modifié en RAM : le fichier sur disque n’est pas touché jusqu’aux procédures Save (voir ci-dessous), et quitter l’écran supprime l’annulation.
- **UNDO** (à côté de GO) restaure l’échantillon à l’état avant la dernière opération. Le bouton est atténué tant qu’aucune opération n’a été lancée, alterne entre états pré- et post-opération à chaque pression répétée, et se réinitialise à l’entrée dans l’écran.
- **File** contient les procédures de sauvegarde. L’instrument garde le chemin complet du WAV d’origine ; ces procédures écrivent ce fichier ou pointent l’instrument vers un nouveau. Le nom de l’instrument n’est jamais modifié.
  - Le champ File alterne entre **Save** et **Save As** (EDIT + gauche/droite ou tap) ; **GO** (même ligne) lance l’action affichée.
  - **Save** écrase le WAV d’origine. Pour un échantillon sans slices, il demande d’abord `Overwrite <name>?`. Pour un sample slicé, le dialogue **SAVE SLICES** s’ouvre pour choisir où enregistrer les points de slice : **SAVE TO SAMPLE** les écrit dans le WAV en chunks cue (portable — les DAWs les affichent comme marqueurs) et les conserve aussi dans le projet, **SAVE TO PROJECT** écrit un WAV simple et garde les points seulement dans le projet, **CANCEL** annule. Une case `Don't ask again in this project` mémorise la destination choisie dans le projet et évite ce dialogue par la suite (le choix est sauvegardé en `- Sample save choice:` dans le .cct et ne peut être modifié qu’en éditant le fichier projet). Le dialogue est sauté pour les samples sans slices. **Save** est atténué (et ignoré dans la navigation) tant qu’aucun échantillon avec chemin de fichier n’est chargé. Un échec d’écriture conserve le marqueur de modification et affiche l’erreur.
  - **Save As** écrit l’échantillon courant dans un nouveau fichier : entrez un nom (pré-rempli sans extension), puis choisissez un dossier dans le navigateur. Le fichier est écrit en `<folder>/<name>.wav`, l’instrument pointe vers ce nouveau fichier, et le dossier est mémorisé par défaut pour les samples. Le bouton est atténué (et ignoré en navigation) tant que les données d’échantillon ne sont pas chargées, permettant ainsi d’attribuer un chemin à un sample fraîchement chargé. Les chemins de plus de 255 caractères sont refusés.
  - Le marqueur `*` avant le nom de fichier signifie que l’échantillon en RAM diffère du fichier sur disque (toute opération de traitement l’active, Save et Save As le réinitialisent). Le marqueur est temporaire : il n’est pas enregistré avec le projet et se réinitialise à chaque entrée dans l’écran. Si plusieurs instruments utilisent le même fichier WAV, sauvegarder l’un écrase le fichier pour tous.
- Sur l’écran Sample de l’instrument, utilisez **EDIT + [LEFT/RIGHT]** pour charger le WAV précédent ou suivant dans le même dossier.
- **Pitch** transpose par demi-tons (`-48` à `+48`).
- **Loop** sélectionne Off, Loop ou Ping-Pong.
- **Speed** contrôle le time-stretch granulaire de `0%` à `500%` (`100%` normal).Les formats WAV non supportés affichent une erreur. Convertissez les fichiers inhabituels en WAV `PCM8`, `PCM16` ou `PCM24` avant de les importer.

##### Entrée audio

Ouvrez **Settings → Audio → Audio Input** avant d’enregistrer depuis un micro ou une interface audio USB. La liste montre les entrées actuellement disponibles sur l’appareil. Si une interface armée est débranchée, la capture est interrompue et le gestionnaire audio redémarre pour que l’application continue de fonctionner. Si vous enregistriez, appuyez sur **STOP** pour terminer la prise interrompue. Après le rebranchement, sélectionnez **[Refresh]** et vérifiez que l’entrée voulue est armée avant d’enregistrer de nouveau ; le rebranchement seul ne la réarme pas de façon fiable. **[List]** relit la liste des entrées actuelle sans redémarrer l’audio.

Sélectionnez la case **ARM** d’une entrée pour la rendre disponible à l’enregistreur Sampler ; son petit vumètre montre le niveau entrant. Armez une seule interface à la fois : cette version n’ouvre que la première interface armée, même si plusieurs cases sont cochées. Votre sélection est mémorisée par nom d’appareil, donc une interface indisponible reste désarmée au lieu d’être remplacée par une autre entrée. Appuyez sur **OPT** pour revenir aux réglages Audio.

##### Enregistrement d’une entrée audio

Sur l’écran Sample de l’instrument, **REC** (ou **EDIT/REC** quand un sample est chargé) ouvre l’éditeur de sample, qui regroupe enregistrement et édition sur un seul écran. Armez d’abord les entrées dans l’écran **Settings → Audio → Audio Input** (voir [Entrée audio](#entrée-audio)). Les premières lignes de l’éditeur contrôlent l’enregistrement :

- **Input** fait défiler la source d’enregistrement : `T1`..`T8` enregistrent les huit pistes du tracker, suivies du micro ou de l’interface USB armés. Pour un enregistrement externe, continuez après `T8` jusqu’à ce que le nom de l’appareil apparaisse. `-` signifie qu’aucune source n’a été choisie explicitement ; **REC** utilise alors la première entrée armée et affiche son nom. Maintenez **EDIT** et appuyez sur **LEFT**/**RIGHT** pour reculer/avancer, ou **EDIT+OPT** pour revenir à `-`. Taper sur Input le fait défiler directement.
- Le vumètre à côté d’**Input** montre le niveau en direct de l’entrée armée (sources externes uniquement). **MON** active le monitoring de l’entrée : l’entrée armée est envoyée à la sortie à -6 dB pour réduire le larsen. Le réglage est sauvegardé dans le fichier de réglages.
- **REC** démarre l’enregistrement. Les sources externes enregistrent immédiatement ; une source de piste affiche **ARMED** et l’enregistrement démarre quand la lecture démarre (appuyez sur **PLAY**). **STOP** termine l’enregistrement : le WAV est écrit dans le dossier d’export du projet (`recNNN.wav`), chargé dans l’instrument Sampler courant, et la Region revient à l’échantillon entier. Quitter l’écran avec **OPT** pendant un enregistrement le finalise de la même manière.
- **PLAY** (affiché après un enregistrement) auditionne la prise ; il devient **STOP** pendant la préécoute. **CLEAR** (à côté) supprime le fichier WAV et décharge le sample de l’instrument ; après l’effacement, **REC** reprend sa place.
- La forme d’onde montre l’entrée en direct pendant l’enregistrement et la forme d’onde complète une fois l’enregistrement terminé. La ligne d’état affiche le nom du fichier pendant l’enregistrement et le nombre de trames ensuite ; une prise externe affiche aussi `drop 0` quand aucune trame audio n’a été perdue. Un sample enregistré est entièrement éditable : région de boucle, sélection, slicing, traitement et sauvegarde fonctionnent dessus comme sur n’importe quel fichier chargé.

Les enregistrements sont écrits à la fréquence d’échantillonnage audio choisie, en WAV stéréo 16 bits. Les sources de piste capturent ce que rendent les instruments de la piste (après FX) ; les sources externes capturent directement l’entrée de l’appareil.

#### 2xSCWF

2xSCWF est un oscillateur à double forme d’onde à cycle unique. Chargez 1 WAV mono contenant exactement 1 période dans chaque oscillateur. Les deux oscillateurs lisent en avant et bouclent à la fin de leur cycle.

Mix crossfade entre A et B.

Le désaccord est fin et exponentiel du unisson jusqu’à `+200 ct`, puis progresse par demi-tons de `+3 st` à `+24 st`.

2xSCWF traite chaque fichier comme une table à 1 cycle et le lit exactement une fois par cycle d’oscillateur. Cela permet de mixer et désaccorder 2 ondes à cycle unique.

Le dossier d’usine `waveforms/AKWF/` contient des formes d’onde de la bibliothèque Adventure Kid. Beaucoup d’autres sont disponibles en ligne si vous souhaitez étendre la collection.

#### BYOWTBL

BYOWTBL est un oscillateur wavetable double compatible avec les tables Serum. Chaque oscillateur charge un WAV mono contenant des trames consécutives à cycle unique. `Pos A` et `Pos B` parcourent leurs tables indépendamment de `00` à `FF`. Le moteur interpole linéairement à l’intérieur de chaque trame et entre les trames adjacentes. `Mix` crossfade les oscillateurs, et `Detune` décale l’oscillateur B.

### Famille d’instruments FM

#### Instruments natifs OPLL et VRC7

Le sélecteur de type d'instrument propose un groupe FM avec **OPLL / MSX** (YM2413) et **VRC7** (DS1001). Chacun offre désormais **73 programmes** dans des listes défilantes Bank / Preset : les 15 sons originaux, 40 sons distincts supplémentaires issus des tables YM2413/VRC7/YMF281B d’emu2413, et 18 programmes deux-opérateurs créés par ChooChoo. Les doublons exacts sont exclus dans chaque moteur. Les palettes additionnelles utilisent l’emplacement tonal programmable de la puce ; elles n’élargissent pas la ROM physique. Le programme zéro identifie un son personnalisé. EDIT + gauche/droite sélectionne le preset précédent ou suivant dans le filtre de banque. Fine ct ajuste la justesse de -100 à +100 cents.

Dans la liste des programmes, EDIT + PLAY auditionne le son sélectionné ; relâcher arrête. EDIT confirme et OPT annule. La navigation/audition ne modifie pas l’instrument, la table ni la chanson. Le geste habituel d'audition sur la page Instrument reste actif après sélection. Les enveloppes natives de la puce gèrent attaque/libération ; l’enveloppe Amp optionnelle ajoute un ADSR logiciel (voir Contrôles chip natif ci-dessous). Il n’y a pas d’éditeur complet d’opérateur.

#### AdLib / OPL2 et OPL3

FM inclut aussi AdLib / OPL2 (YM3812) et OPL3 (YMF262). La banque filtre la liste d’usine ; Preset ouvre des groupes par catégorie et une vue Tous. EDIT + PLAY auditionne avant sélection, EDIT sélectionne, OPT annule, et EDIT + gauche/droite sur Preset charge l’entrée correspondante précédente/suivante. Fine ct ajuste la justesse locale. Le mode affiche 2 opérateurs, 4 opérateurs, ou voix duale. OPL2 masque les patches OPL3 incompatibles ; OPL3 peut jouer la collection partagée à deux opérateurs.

Les titres des popup Bank et Preset identifient le moteur actif. Changer de moteur ou d’emplacement d’instrument réinitialise le filtre de banque à **Toutes les banques**, y compris en passant entre les moteurs compatibles AdLib et OPL3. Revenir d’un popup dans le même emplacement conserve le filtre choisi. Annuler une importation de banque DX7 restaure le filtre précédent.

La confirmation de preset applique aussi les edits faits au relâchement du bouton au tick UI/audio suivant. Les chansons en cours n’ont plus besoin d’une pression supplémentaire ou d’un redémarrage du transport pour recevoir ce patch. La sortie suit toujours le tampon audio configuré et le comportement d’enveloppe/retrigger du moteur.

La collection d’usine contient 697 entrées sources de The Fat Man 2-op, The Fat Man 4-op et DMXOPL3, avec 589 identités uniques de patch normalisées. Les alias gardent noms et attributions d’origine. Niveaux d’opérateurs natifs sources, flags de profondeur LFO, décalages de notes, tons percussion fixes et accord dual-voix sont conservés. Le volume tracker utilise une amplitude logicielle ; il ne reproduit pas les courbes de volume originales des lecteurs MIDI ni les politiques d’offset de vélocité. Les estimations de durée de release source ne coupent pas les notes soutenues. Certains effets ont des attaques lentes : maintenez Seashore au lieu d’attendre un court clic pour le dévoiler.

Sélectionner un preset ne modifie pas la table du slot ni les inserts appartenant aux pistes.

#### Instruments natifs Sega PSG et Game Boy

CHIP inclut Sega PSG, GB Pulse et GB Noise. Sur Sega PSG, **Plage basse : étendue** conserve la distinction des notes graves. Choisissez **Chip** pour la plage limitée originale où les notes les plus basses convergent. Le bruit à fréquence fixe garde sa hauteur d’origine ; le bruit dérivé du ton suit la plage basse sélectionnée.
Les instruments GB utilisent les registres DMG de pulse/bruit ; aucun canal d’onde Game Boy n’est exposé. Leurs listes de presets contiennent maintenant respectivement **24, 24 et 28** presets. Les ajouts couvrent bruit Sega lié/périodique, balayages de hauteur GB, percussions courtes, bruit métallique, drones et montées. Ce sont des programmes originaux, non des sons extraits de jeux.
La page offre le mode natif/duty/width, enveloppe/sweep/bruit selon le cas, plus un ADSR amplitude logiciel.

#### DX7 FM

Sélectionnez **Type → FM → DX7 FM** ; DX7 est la septième option après Arcade / YM2151.
DX7 FM utilise six opérateurs. Sa page propose Bank, Preset et Fine ct avec les réglages communs d’instrument. La navigation des presets utilise des listes banque/catégorie. Maintenez EDIT+PLAY pour entendre le son sélectionné ; relâchez pour arrêter l’audition. EDIT seul confirme à la relâche ; OPT annule. Charger un son copie son patch complet sans changer les tables tracker ni les inserts de piste FX.

Pour une bibliothèque personnelle persistante, placez les fichiers `.syx` dans
**`instruments/banks/dx7/`** à côté de la bibliothèque d’instruments existante. Les sous-dossiers sont supportés. Ouvrez **DX7 → Bank** : chaque fichier apparaît comme une banque nommée, prête à la navigation Preset. Rouvrez Bank après ajout ou suppression de fichiers ; aucune conversion ni chargement Instrument n’est nécessaire. Une banque originale DX7/TX7 standard contient **32 voix**. Un fichier avec quatre messages de banque apparaît comme quatre banques numérotées (128 voix au total). Les fichiers mono-voix sont aussi acceptés. Le patch sélectionné est sauvegardé avec l’instrument, donc retirer sa banque source ne modifie pas la chanson.

**Load Instrument** accepte aussi les voix uniques et banques 32 voix `.syx` DX7/TX7 originales. Sélectionnez un patch pour l’utiliser, ou annulez pour garder l’instrument actuel. Utilisez le dossier Bank ci-dessus pour une bibliothèque persistante ; sauvegardez la chanson ou un `.cni` pour garder un patch édité. Les formats Yamaha non supportés sont refusés.

DX7 est limité à **16 notes actives dans la chanson**, incluant voix d’accord et relâche de notes. Quand la limite est atteinte, les relâches sont diminuées en priorité, puis les notes tenues silencieuses, pour garder les nouvelles attaques jouables. L’audition de preset est désactivée pendant la lecture de la chanson.

#### Éditeur d’opérateurs FM

Les instruments Genesis FM, Arcade FM, OPL2, OPL3, OPLL et VRC7 ont un bouton **EDIT** à droite de Fine ct. Il ouvre un tableau des paramètres du patch :

- Genesis FM / Arcade FM
- OPL2 / OPL3 : sur OPL3, le mode (2op, 4op, dual) peut être changé ici.
- OPLL / VRC7 : éditer un preset ROM le transforme en « Custom ».

Gauche/Droite et les touches d’édition classiques modifient les valeurs ; l’instrument continue de jouer normalement pour entendre les changements. La barre d’état décrit le paramètre sous le curseur. OPT ou Left+Shift revient à l’écran instrument. DX7 et SID n’ont pas d’éditeur d’opérateurs.

Genesis FM (YM2612) et Arcade FM (YM2151) utilisent le même navigateur Bank/Preset, audition et réglages de fine-tune que les autres moteurs FM. Leurs enveloppes natives contrôlent la release ; changer le volume tracker ou la hauteur ne les retrigger pas.

#### Presets FM d’usine et UTILISATEUR

Les instruments chips natifs utilisent **Bank :** pour choisir ALL, une collection incluse ou USER. **Preset :** choisit un son dans cette sélection. Maintenez EDIT et appuyez sur Gauche ou Droite sur n’importe quelle ligne pour parcourir ; un court tap EDIT ouvre son sélecteur.

Les collections incluses sont des packs ZIP sous `instruments/FACTORY/`. Les fichiers USER vont sous `instruments/USER/<engine>/` : `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse` ou `gb-noise`.
Parcourez les dossiers directement ou mettez-y un ZIP de presets compatibles. Les ZIP restent intacts sur disque. Tous ces dossiers acceptent les fichiers `.cni` compatibles ; CNI est le format portable d’instrument ChooChoo/ChipNomad, pas une norme fabricant chip.
Le navigateur USER lit aussi ces formats sources, y compris en ZIP :

| Dossier USER | Formats additionnels |
| --- | --- |
| `dx7` | Voix uniques et banques 32 voix Yamaha DX7/TX7 `.syx` |
| `genesis` | `.tfi`, DefleMask `.dmp`, Furnace `.fui` |
| `arcade` | Banques VOPM `.opm`, DefleMask `.dmp`, Furnace `.fui` |
| `opl2`, `opl3` | `.wopl`, `.opli`, `.woplx`, `.sbi`, Furnace `.fui` |
| `opll`, `vrc7` | Patches custom Furnace `.fui` et sélections ROM fixes |
| `sega`, `gb-pulse`, `gb-noise` | Furnace `.fui`, DefleMask `.dmp` |
| `sid` | GoatTracker 2 `.ins` (GTI5), Furnace `.fui` |

Une banque s’ouvre en voix compatibles. Les fichiers ou fonctions non supportés donnent un message d’erreur ; les voix incompatibles dans une banque mixte sont exclues tandis que leurs numéros sources restent stables. Le chargement est transactionnel : un preset rejeté ne modifie pas l’instrument courant. Les presets sources remplacent la table tracker de l’instrument par une table vide, excepté pour les SysEx DX7 qui conservent leur table existante. CNI restaure sa table sauvegardée. Les patches chargés appartiennent à la chanson et continuent de fonctionner après déplacement ou suppression du pack source.

Les banques WOPL peuvent contenir des emplacements non utilisés sans flag explicite de vide. Le navigateur omet les emplacements sans nom dont les opérateurs actifs sont complètement atténués et sans attaque. Les voix jouables sans nom restent disponibles sous `Program N [Bbank]` ou `Drum N [Bbank]`, en utilisant les numéros de programme et banque zéro-based sources. Un instrument OPLI mono sans nom utilise son nom de fichier. Les patches silencieux nommés sont conservés.

Les fichiers Furnace, DefleMask et GoatTracker sont importés comme instruments, pas chansons complètes. Les macros sources animées, samples et réglages de lecture spécifiques à une chanson peuvent ne pas se transférer. Placez les fichiers Game Boy dans les dossiers Pulse ou Noise appropriés.

Le cycle de presets USER continue à travers fichiers, dossiers, banques et packs ZIP dans les deux directions, en boucle aux extrémités. Réouvrir le navigateur USER reprend la sélection courante. ALL inclut les sons USER sous Non trié pour les moteurs catégorisés ; USER reste un choix distinct de premier niveau. Sega PSG et Game Boy utilisent des listes ALL/Factory plates et un navigateur USER hiérarchique, sans panneau Non trié. Les anciens fichiers DX7 dans `instruments/banks/dx7/` restent accessibles en USER.

Un moteur affiche seulement ses collections incluses compatibles. Factory Presets combine les séries tones OPLL/VRC7 incluses pour le moteur sélectionné ; DX7 combine les originaux ChooChoo et YSE. Les collections téléchargées nommées restent séparées. Fat Man 2-op est listé sous OPL2 et Fat Man 4-op sous OPL3. Les fichiers CNI OPL2 compatibles peuvent toujours être utilisés dans les dossiers USER OPL3.

#### Contrôles chip natifs

Les aperçus de forme d’onde des instruments natifs se rafraîchissent quand leurs contrôles sont redessinés.
Les instruments FM peuvent superposer leur courbe Amp env optionnelle ; SID conserve son aperçu de forme d’onde sans cette superposition spécifique FM, y compris lors de la navigation dans banques et presets.

Les sept moteurs FM offrent **Bright**, **Feedback** et **Amp env**. Bright va de -63 à +63, zéro conservant le patch ; il modifie les niveaux de l’opérateur de modulation tout en gardant ceux du porteur. Son effet audible dépend de l’algorithme. Feedback par défaut est **Preset**, ou peut remplacer le feedback natif de 0 à 7. Ces contrôles ne modifient pas les bytes opérateurs natifs sauvegardés.L’enveloppe Amp par défaut est **Bypass**. Sélectionnez **ADSR** pour ajouter attaque, déclin, sustain, relâchement et façonner le son natif. A/D/R utilisent une plage quadratique partagée de 0 à 5 secondes ; le sustain va de silence à niveau maximal. Les enveloppes natives continuent de fonctionner, cette enveloppe ne peut donc pas prolonger un son au-delà de son relâchement natif. La navigation dans les presets conserve les réglages FM amp et tone du slot.

Les FX de phrase suivants apparaissent aussi comme destinations prises en charge pour la modulation et l’enregistrement de mouvement. Les valeurs dans la colonne FX sont en hexadécimal. Les réglages s’appliquent à la lecture sans modifier l’instrument enregistré. Les contrôles de balayage et d’enveloppe GB natifs restent enclenchés au déclenchement de la note suivante ; le duty/width et la fréquence du bruit peuvent changer en temps réel.

| Engines | FX | Contrôle |
|---|---|---|
| Native FM | `OL1`–`OL6` | Niveaux absolus de sortie des opérateurs : modulateurs OPLL/VRC7 et OPL `00–3F` ; porteur OPLL/VRC7 `00–0F` ; Genesis/Arcade `00–7F` ; DX7 `00–63` (0–99). Plus haut = plus fort. Seuls les opérateurs supportés apparaissent. |
| OPLL/VRC7/OPL2/OPL3 | `OAR`, `ODR`, `ORR`, `OSL` | Attaque, déclin, relâchement et atténuation de sustain opérateur 1, `00–0F`. |
| OPLL/VRC7/OPL2/OPL3/Genesis/Arcade | `OMU 00–0F` | Multiplicateur de fréquence opérateur 1. |
| Genesis/Arcade | `LFR` | Vitesse LFO native : Genesis `00–07`, Arcade `00–FF`. |
| Arcade | `LAD`, `LPD 00–7F` | Profondeurs d’amplitude et de pitch séparées du LFO. |
| Genesis/Arcade | `LAS`, `LEN` | Sensibilité amplitude native `00–03` et activation LFO `00–01`. |
| Genesis/Arcade | `LPS 00–07` | Sensibilité pitch native. |
| SID | `SAT`, `SDE`, `SSU`, `SRL 00–0F` | Attaque, déclin, sustain et relâchement natifs. Les valeurs de temps augmentent vers `0F` ; le sustain monte vers le niveau maximal. |
| SID | `SPR 01–10` | Fréquence du partenaire silencieux de 1× à 16× ; affecte modulation en anneau et hard sync. |
| Tous FM natifs | `FBK 00–07` | Feedback absolu natif, initialisé depuis l’instrument. |
| Tous FM natifs avec Amp env activé ; Sega/GB | `EAT`, `EDC`, `ESU`, `ERL`, `ESH 00–FF` | Attaque, déclin, sustain, relâchement, forme. Ne réactivent pas un FM amp bypassé. |
| Sega PSG | `CMD 00–02`, `CNR 00–03` | Tone / bruit blanc / bruit périodique ; trois vitesses fixes de bruit ou vitesse dérivée du tone. |
| GB Pulse | `CMD 00–03` | Duty pulse natif. |
| GB Noise | `CMD 00–01`, `CND 00–07`, `CNS 00–0D` | Largeur de bruit, diviseur d’horloge, décalage. |
| GB Pulse | `CSP 00–07`, `CSS 00–07`, `CSD 00–01` | Période de balayage, décalage, direction vers le bas. |
| GB Pulse / Noise | `CEI 00–0F`, `CEP 00–07`, `CED 00–01` | Niveau initial, période, direction montante de l’enveloppe native. |

La sélection des FX natifs utilise l’instrument de la colonne `I` de la ligne de phrase, ou l’instrument actif trouvé en remontant la liste si `I` est vide. Le popup FX montre une courte description du contrôle, sa valeur preset actuelle et la plage de commandes valide dans un bloc compact. Sélectionner un effet natif différent démarre à cette valeur ; rouvrir le même effet conserve sa valeur modifiée. Une sélection multi-lignes résout l’instrument de chaque ligne séparément. Les tableaux utilisent leur contexte d’instrument. Les modifications de valeur natives s’arrêtent aux bornes légales, y compris duty, bruit, balayage, ADSR, feedback et niveaux opérateur. Les valeurs sont en hexadécimal : par exemple, le max `63` du DX7 signifie décimal 99.

#### Instruments SID et FX de phrase

Choisissez **CHIP → SID**. Bank propose **ChooChoo SID Originals** et **SIDkit Effects**. Parcourez, auditionnez et sauvegardez-les comme les autres presets native-chip.

La page d’instrument expose forme d’onde/largeur d’impulsion, mode filtre/cutoff/résonance et contrôles ADSR natifs. **EDIT**, à côté du nom du preset, ouvre **SID EDIT**. Wave et PW ont des labels distincts ; Filter a une ligne mode, puis Cutoff et Reso ; les étiquettes ADSR sont à côté des valeurs. Toucher chaque champ sélectionne ce contrôle précis. Les plages et comportements de réglage fin/grossier existants sont conservés.

SID EDIT utilise les rangées compactes, couleurs de thème, valeurs hexadécimales et contrôles EDIT+direction du FM editor. OPT ou Left+Shift revient ; EDIT+PLAY auditionne l’instrument. La page édite directement le patch stocké existant, donc les changements sont sauvegardés avec l’instrument et le projet.

SID EDIT regroupe forme d’onde, ring/sync, ADSR, largeur d’impulsion, filtre, mouvement, arpège, sweep pitch et vibrato. La barre d’état identifie chaque contrôle et sa plage. Ring et sync utilisent chacun deux des quatre voix SID disponibles, un accord SID peut donc avoir besoin de moins de notes quand l’un ou l’autre est actif.

| FX | Plage | Action |
| --- | --- | --- |
| `SCP` | `00–FF` | Base largeur d’impulsion, mise à l’échelle sur un 12-bit natif ; le mouvement de la pulse recette reste actif. |
| `SCT` | `00–FF` | Base cutoff, mise à l’échelle sur un cutoff 11-bit natif ; le mouvement filtre recette reste actif. |
| `SRN` | `00–0F` | Résonance native. |
| `SWV` | `00–07` | Triangle, dent de scie, tri+dent de scie, pulse, tri+pulse, dent de scie+pulse, tri+dent de scie+pulse, bruit. |
| `SFI` | `00–07` | Bits mode filtre : 1 passe-bas, 2 passe-bande, 4 passe-haut ; 0 bypass. |
| `SMR` | `00–FF` | Horloge macro recette, mise à l’échelle de 1 à 200 Hz. |
| `SRG` | `00–01` | Modulation en anneau triangle avec partenaire silencieux. |
| `SSY` | `00–01` | Synchronisation oscillateur avec partenaire silencieux. |

La largeur d’impulsion et cutoff supportent `SLE`. Les formes d’onde discrètes, interrupteurs, résonance et feedback FM gardent leurs pas natifs utiles ; des valeurs d’octet plus larges ne créeraient pas plus d’états hardware. `FBK` sélectionne directement le feedback `00–07`. Les niveaux opérateurs utilisent les plages propres aux engines listées plus haut. La brillance et les ajustements master conservent la plage complète d’octet.

#### Presets factory et USER SID et native-chip

Les instruments native chip utilisent **Bank :** pour choisir ALL, une collection incluse ou USER. **Preset :** choisit un son dans cette sélection. Maintenez EDIT et pressez Gauche ou Droite sur une ligne pour faire défiler ; un court appui EDIT ouvre le sélecteur.

Les collections incluses sont des packs ZIP sous `instruments/FACTORY/`. Les fichiers USER vont dans `instruments/USER/<engine>/` : `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse`, ou `gb-noise`. Parcourez vos dossiers directement, ou mettez-y un ZIP de dossiers et presets `.cni` compatibles. Le DX7 accepte aussi les `.syx` compatibles de voix simples et banques 32 voix, y compris dans les ZIP. Les ZIP restent intacts sur disque. Le chargement direct WOPL/WOPLX n’est pas supporté ; utilisez des presets CNI compatibles ou convertissez les sources compatibles hors-ligne.

Le défilement USER continue à travers fichiers, dossiers, banks et packs ZIP dans les deux sens, en bouclant aux extrémités. Rouvrir le navigateur USER suit la sélection courante. ALL inclut sous Unsorted les sons USER pour les engines catégorisés ; USER reste un choix séparé en haut de liste. Sega PSG et Game Boy utilisent des listes ALL/Factory plates et un navigateur USER hiérarchique, sans panneau Unsorted. Les anciens fichiers DX7 dans `instruments/banks/dx7/` restent accessibles dans USER.

Un engine montre seulement ses collections incluses compatibles. Factory Presets combine les sets tone OPLL/VRC7 inclus pour l’engine sélectionné ; DX7 combine ChooChoo et YSE originals. Les collections nommées téléchargées restent séparées. Fat Man 2-op est listé sous OPL2 et Fat Man 4-op sous OPL3. Les fichiers CNI OPL2 compatibles peuvent toujours être utilisés dans des dossiers USER OPL3.

## 6. Modulation et enregistrement de mouvement

Chaque instrument peut avoir jusqu’à 4 emplacements de modulation. Les instruments audio peuvent cibler **Instrument Pan** ou **Track Pan** ; les deux utilisent `00` pour gauche, `80` pour centre et `FF` pour droite.

Les types de modulation disponibles sont :

- ADSR – enveloppe classique attaque-déclin-sustain-relâchement
- AHD – enveloppe attaque-maintien-déclin
- LFO – oscillateur basse fréquence avec plusieurs formes
- syncLFO (anciennement SLFO) – LFO avec un réglage de ticks par unité et un multiplicateur de période
- slowLFO – cycles longs avec un contrôle de période proportionnel, commençant juste au-delà de la plage du LFO ordinaire
- FLFO – LFO audio avec fréquences pouvant atteindre le kHz ; FLFO peut être lourd en CPU
- SLIN – contrôle joystick linéaire
- SRAT – contrôle joystick basé sur vitesse
- Sequence – lit la séquence du tracker comme signal de modulation : **PITCH** est le pitch séquencé final, **VOL** le volume séquencé final, et **TRANCE GATE** un modulateur rythmique indépendant. Ses réglages Motif, Rate et Length choisissent le motif de gate ; Trigger indique s’il tourne librement, est redéclenché à chaque note, ou redémarré aux limites de phrase ou de chaîne. Les gates fermés sortent zéro, les gates ouverts sortent un demi-niveau, et Accents répartit des gates à plein niveau de façon euclidienne parmi les gates actifs. Il n’utilise ni ne modifie aucun insert de piste.

La liste des destinations modulation dépend du type d’instrument.

Quand une enveloppe ADSR ou AHD cible Volume, elle devient l’enveloppe de volume de l’instrument. Un LFO ciblant Volume compense le volume de sortie actuel à la place.

La quantité de modulation peut être positive ou négative. Sa plage dépend de la destination. Si la plage maximale de la destination est `127` ou moins, comme pour la plupart des paramètres, la quantité définit une plage absolue. Les destinations plus vastes comme le pitch utilisent une quantité redimensionnée.

Les sources Sequence PITCH et VOL ont un **Mode**. **Normal** garde la valeur du tracker audible tout en l’utilisant comme modulation. **Bypass** en fait une source seulement : PITCH redéclenche au dernier pitch (ou C-4 pour la première note), et VOL garde le dernier niveau audible. Trance Gate est déjà source seulement. Si un emplacement Sequence actif met PITCH ou VOL en bypass, le bypass l’emporte. Pour éviter le feedback, PITCH et VOL ne peuvent pas cibler Pitch ou Volume.

Tous les paramètres de durée, y compris attaque, maintien, déclin et période, sont mesurés en ticks.

**syncLFO** est l’ancien SLFO : réglez ses ticks et son multiplicateur pour un mouvement synchronisé au tempo. Les réglages SLFO existants et les chansons gardent le même timing.

Utilisez **slowLFO** pour des balayages plus longs que ce que le LFO ordinaire peut faire. Son contrôle Period va de 256 à 131 072 ticks. Tous les LFO en ticks suivent le tempo de la chanson et partagent les mêmes formes, la même sélection de wavetable et les mêmes modes de déclenchement. Choisissez Free pour un balayage qui continue entre les notes ; Retrig, Phrase et Chain le redémarrent à leurs limites respectives.

Formes du LFO :

- Tri – onde triangulaire bipolaire
- Sin – onde sinusoïdale bipolaire
- UniTri – onde triangulaire unipolaire
- UniSin – onde sinusoïdale unipolaire
- RampDn – rampe linéaire descendante (dent de scie)
- RampUp – rampe linéaire montante (dent de scie)
- ExpDn – rampe exponentielle descendante (dent de scie exponentielle)
- ExpUp – rampe exponentielle montante (dent de scie exponentielle)
- Square – onde carrée
- Random – onde aléatoire sample-and-hold
- Wavetable – lit une des wavetables AY 32 pas du projet. Valeurs `00` à `0F` mappent de `-100%` à `+100%` sans interpolation.

Types de déclenchement LFO :

- Free – redémarre le LFO uniquement quand l’instrument change
- Retrig – redémarre à chaque note
- Phrase – redémarre LFO, syncLFO et slowLFO quand la lecture entre dans une nouvelle phrase
- Chain – redémarre LFO, syncLFO et slowLFO quand la lecture entre dans une nouvelle chaîne
- Hold – s’arrête après 1 cycle et maintient la dernière valeur
- Once – s’arrête après 1 cycle et revient à zéro

### Enregistrement de mouvement

- **STICK LIVE** (`L1` par défaut) applique la modulation stick sans changer la phrase. Dans **Settings > Stick live mode**, `HOLD` (par défaut) l’active pendant que le bouton est maintenu ; `TOGGLE` l’active à la première pression et le désactive à la suivante ; `FREE` le laisse en permanence activé. Relâcher le bouton en mode `TOGGLE` laisse l’activation active.
- Pendant la lecture, maintenez **MOTION RECORD** (`L2`) pour appliquer la modulation stick et enregistrer les destinations modifiées comme valeurs FX absolues.
- Maintenez **MOTION ERASE** (`R2`) pour retirer les FX destination correspondants de la ligne en cours.

Le `~` en bas à droite indique que Stick live est actif. Record (`*`, ou `!` en cas de débordement) et Erase (`x`) ont la priorité d’affichage et restent momentanés dans les deux modes. Ces deux actions activent le traitement live du stick pendant qu’elles sont maintenues ; les relâcher laisse un toggle live séparément activé intact.

Le mode est sauvegardé dans les réglages de l’application, mais le toggle live démarre toujours désactivé au lancement. Changer de mode réinitialise le toggle : `HOLD` suit tout bouton Stick live actuellement maintenu, `TOGGLE` attend une nouvelle pression, et `FREE` est actif immédiatement.

L’enregistrement de mouvement écrit les FX de piste dans la phrase en cours de lecture. Il met à jour les FX correspondants en premier, puis utilise les slots vides de droite à gauche. Il n’écrase jamais un FX différent. Si les 3 slots sont pleins, ce mouvement n’est pas enregistré sur la mesure. Un `!` en bas à droite signifie que plus de destinations ont changé que les 3 colonnes FX ne peuvent contenir.

L’enregistrement de mouvement supporte les destinations Braids, Plaits, Sampler, 2xSCWF, BYOWTBL et Bogie, y compris macros Bogie et contrôles filtre actifs quand applicable.

## 7. Effets insert de piste

Chacune des huit pistes possède deux slots en série, **TF1 → TF2**. Depuis Instrument, utilisez le geste habituel Shift+Up pour ouvrir MOD, puis Shift+Up encore pour **F : Insert FX**. Shift+Down revient à MOD. OPT+Left/Right sélectionne la piste (1–8), sans changer le sélecteur d’instrument. L’en-tête du module ouvre le sélecteur existant ; bypass conserve le module, ses valeurs configurées et son automation. Les paramètres sont des valeurs octet : colonne gauche 1–4, colonne droite 5–8. Le texte de contexte affiche noms complets et valeurs décodées. Les anciens projets s’ouvrent avec les deux slots OFF.

L’affichage de navigation garde les colonnes F–M–I–P visibles dans toutes les pages d’instrument, y compris Insert FX et pool d’instruments, et surligne l’écran actuel.| Module | Paramètres dans l’ordre |
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

Les contrôles continus utilisent 00–FF, sauf FORMANT VOW (00–7F). Les contrôles bipolaires ont un neutre exact à 80. Les modes de distortion sont 00 Density, 01 Drive, 02 Spiral, 03 Mojo, 04 Dyno. Les ratios du compresseur 00–07 sont 1.5, 2, 3, 4, 6, 8, 16, 20:1. Detect du compresseur 00–02 écoute sa propre entrée stéréo, gauche ou droite ; 03–0A écoute plutôt les pistes 1–8. Le filtre du détecteur à 80 est neutre, en dessous low-pass, au-dessus high-pass. Une source de piste est prélevée après son volume, son pan et son Tilt EQ mais avant ses inserts ; une source muette ou absente fournit du silence. N’importe quel slot peut contenir n’importe quel module ; les modules répétés sont indépendants. Chorus et Phaser démarrent avec une modulation audible mais contrôlée. Réduisez Mix, Depth ou Feedback pour un effet quasi-transparent ; des valeurs élevées de Depth, Feedback et Mix deviennent volontairement extrêmes. Flanger et Rotary utilisent des modulations douces par défaut. La vitesse Rotary utilise toute la plage **00-FF** : **00 = 0,01 Hz** (une rotation toutes les 100 secondes), **FF = 25 Hz** (le maximum ancien Chaos). L’espacement exponentiel donne un contrôle plus fin sur les mouvements lents du LFO. Les nouveaux inserts Rotary démarrent à **8F**, environ 0,8 Hz. Cela remplace les quatre vitesses discrètes ; les réglages Rotary existants et l’automation vitesse F11/F21 doivent être recalibrés sur la nouvelle échelle (Ancien Slow/Fast/Hyper/Chaos correspondent à peu près à **8F/CB/E7/FF**). Les autres paramètres restent inchangés. La profondeur Bitcrusher va de 4 à 16 bits et la réduction de fréquence de 1x à 32x. Les modes Destruction sont Fold, Clip et Crush. Saturation et distortion incluent un contrôle de niveau de sortie ou de mix pour équilibrer avec le signal non traité.

Le Trance Gate est un séquenceur d’amplitude synchronisé au tempo pour pads, leads et stabs. Les valeurs Pattern 00–3F sélectionnent des motifs musicaux nommés ; 00–1F sont des grooves de base, 20–3F des variations expérimentales. Rate propose valeurs droites, triolets et pointées de 1/2 à 1/32. Length tronque le motif 16 pas indépendant tourné à 1–16 pas. Chaque note-on dans la piste relance la séquence ; le note-off la laisse continuer pendant la release. Forward, Reverse et Ping-Pong définissent la direction ; Chaos applique une mutation répétable conservant la densité tous les quatre cycles. En insert master, il suit le temps de transport sans relances de notes. Gate fixe la largeur de chaque pas ouvert, Smooth lisse ses bords avec un fondu minimum sans clic, et Mix fait un crossfade entre sec et gate.

**Beads** utilise la DSP granulaire Mutable Instruments originale comme insert stéréo. Choisissez Beads dans un slot de piste. Ses huit commandes utilisent aussi les commandes F11–F18 / F21–F28 existantes et insèrent des destinations MOD ; aucun nouvel ID FX ni format projet n’est nécessaire. Les grains tournent continûment sans événements additionnels.

- **Time :** position dans la mémoire tampon d’enregistrement de 4 secondes.
- **Size :** grains inversés longs à 00, grains les plus courts vers 55, puis grains longs allant vers l’avant. FF active le mode delay original après transition.
- **Density :** grains réguliers sous le centre, timing aléatoire au-dessus ; la zone morte centrale stoppe les grains. S’éloigner du centre augmente le taux.
- **Pitch :** −24 à +24 demi-tons ; 80 est unisson exact.
- **Scatter :** 00 est aucune variation. Augmenter applique la variation uniforme interne de Time à plein effet et Size à moitié, indépendamment par grain. Pitch et Shape restent fixes.
- **Feedback :** renvoie les grains traités dans la voie d’enregistrement.
- **Shape/Freeze :** 00–7F enregistre, 80–FF fige. Dans ces plages, les 7 bits bas balayent la même enveloppe : rectangle, décroissance, cloche, inversé. Ajouter/soustraire 80 bascule Freeze sans changer Shape : 40 et C0 ont la même enveloppe. L’indication valeur montre position d’enveloppe et état Recording/Frozen. Maintenir EDIT + Haut/Bas change par 10 hex (16) ; Gauche/Droite par 1. Ce contrôle est discret donc la transition n’interpole pas le bit freeze ; la modulation du byte entier peut traverser intentionnellement les deux états.
- **Mix :** 00 sec exact, FF complètement traité. Les changements sont lissés.

Freeze maintient l’audio capturé par l’effet ; changer de module ou reset Beads le vide. Les send reverb track restent disponibles.

### Faire pomper le mix ou une autre piste avec un kick

Pour un master qui pompe, mettez le kick sur la piste 1, sélectionnez Compressor dans MASTER FX, réglez Detect sur 03 (Track 1), Ratio sur 04 (6:1), Attack vers 60 (environ 1,6 ms), Release vers C0 (environ 115 ms), Makeup sur 00 et Mix sur FF. Baissez Threshold jusqu’à ce que les autres sons s’abaissent à chaque kick, puis ajustez Release pour qu’ils remontent avant le suivant. Le son direct du kick garde ses propres inserts de piste mais contourne les deux slots d’insert master ; ses retours de réverb et de delay pompent quand même.

Pour ne baisser qu’une basse, un pad ou un synthé, mettez son Compressor sur cette piste et sélectionnez la piste du kick avec Detect. Le kick source reste audible et son volume n’est pas modifié par le compresseur de cette piste. Un kick et une basse combinés sur la même piste ne peuvent pas être séparés par ce routage.

### FORMANT

Sélectionnez **FORMANT** dans TF1 ou TF2 pour façonner un son en tons de type voyelle. Il fonctionne mieux sur une source soutenue et riche en harmoniques, comme une onde en dent de scie.

Réglez **MODE** sur `00` VOW et utilisez **VOW** pour balayer de A (`00`) à U (`7F`) en passant par E, I et O. Réglez MODE sur `01` CUSTOM pour accorder vous-même **F1** et **F2**. **RES** resserre ou élargit le caractère de la voyelle ; **MIX** mélange l’effet avec le signal d’origine. Utilisez MOD → INSERT FX pour déplacer les contrôles actifs. MODE n’est pas une destination MOD.

### FX Lane pour Inserts

Les commandes d’insert utilisent **F11–F18** pour TF1 et **F21–F28** pour TF2. La sélection FX et le popup Phrase/Table affichent seulement les paramètres utilisés par les inserts configurés de la piste sélectionnée. Le popup les regroupe en **TF1 : nom de l’effet** et **TF2 : nom de l’effet** ; les slots OFF sont cachés. Les inserts en bypass restent modifiables. Dans le popup FX Phrase et Table, la description suit le slot insert de la piste sélectionnée : elle montre le module, le nom du paramètre, les bornes décodées et la valeur de base sauvegardée. Les commandes chanson existantes sont conservées si un changement de module les rend inutilisées ; ouvrir leur sélecteur commence à un contrôle disponible. En éditant une valeur, l’indication en bas affiche la valeur décodée et ses unités. Chaque colonne FX Phrase ou Table peut adresser n’importe quel slot ; la grille Phrase garde ses trois colonnes. Ces valeurs absolues en temps réel persistent à travers notes, instruments, phrases, chaines et boucles ordinaires. Un arrêt net / nouvelle lecture réinitialise. Éditer une valeur de base sur la page F efface seulement ce paramètre en override. MOD agit sur la valeur base/automation effective sans enregistrer ses offsets. Sélectionner un nouveau module restaure les valeurs par défaut et efface les overrides du slot. Le bypass conserve l’automation existante tout en suspendant le traitement après un court fondu.

**Les adresses concernent les slots de piste/positions des paramètres, pas les effets nommés.** Une Phrase réutilisée sur une autre piste utilise les modules de cette piste. Changer les modules peut modifier la signification des commandes ; elles ne sont pas réécrites. Les adresses OFF/inutilisées sont des no-ops sûrs et restent intactes dans les données sauvegardées Phrase/Table.

Le choix MOD instrument ajoute une catégorie Insert FX. Les sources de chaque instrument contrôlent les inserts sur la piste qui le joue, y compris le signal harmonique combiné. Un même instrument sur deux pistes affecte des chaînes indépendantes. Le timing source, montants signés, règles de combinaison source, modes stick et enregistrement mouvements s’appliquent. Les contrôles discrets sont quantifiés et clampés à des choix valides. Il n’y a pas de nouvelle banque de modulation appartenant à une piste.

Commencez avec quelques inserts, surveillez la saturation ou les craquements, et bypassez les effets qui n’aident pas le morceau. Drive et gain peuvent saturer le mix, utilisez l’indicateur de clipping du Mixer en ajustant.

## 8. Tables

Les tables sont un outil central de design sonore dans les trackers. En termes Vortex Tracker, elles combinent instruments et ornements, mais font bien plus. Si vous connaissez LSDj ou NerdSEQ, l’idée vous est probablement familière.

La colonne Pitch accepte des valeurs relatives (`~`) ou absolues (`=`) pitch en demi-tons. La colonne Volume est 00–0F, s’applique à tous les moteurs en plus de leur enveloppe, et peut servir de gate : alternez 0F et 00 puis bouclez avec HOP 00. Les 4 lanes FX correspondent majoritairement aux lanes d’une phrase, bien que certains FX diffèrent en table.

Placer un FX TIC sur la dernière ligne de la table fixe la vitesse de cette colonne et remplace la vitesse table par défaut de l’instrument. Chaque colonne FX peut tourner à une vitesse de tick différente. Les colonnes Pitch et Volume suivent la vitesse de la 1re colonne FX.

Une table de 16 lignes peut sembler courte face à un instrument Vortex Tracker long, mais FX HOP crée des boucles conditionnelles comme « répéter ces lignes 5 fois » et des boucles imbriquées. Avec boucles et vitesses de colonne FX indépendantes, ces 16 lignes vont loin.

Il est possible de copier-coller les FX lanes entre phrases et tables ;)

### Mode Retrigger

Le champ `Retrig` en haut de l’écran Table sélectionne quand une table active redémarre. `Inst` est le mode par défaut et compatible : une ligne phrase contenant un instrument sélectionne sa table par défaut et la démarre à la ligne 00. `Phrase` redémarre la table active à l’entrée d’une phrase, `Chain` à l’entrée d’une chaîne, et `Free` la laisse tourner jusqu’à l’arrêt de la piste ou un remplacement explicite.

Un mode structurel ne remonte jamais dans le temps. Si une piste n’a pas de table instrument active, la première ligne phrase qui introduit un instrument active sa table à cette ligne exacte, même si c’est ligne 4, 8 ou F d’une phrase. Les numéros d’instruments suivants ne remplacent pas une table Phrase, Chain ou Free active. Cela rend les tables utiles comme automate LFO sans simuler silencieusement les étapes phrase précédentes.

TBL, TBX et RET sont toujours des actions explicites : ils sélectionnent ou redémarrent immédiatement, quel que soit le mode. À la frontière Phrase ou Chain, ce redémarrage se fait avant la lecture de la nouvelle ligne, donc un TBL ou TBX sur cette ligne est décisif. Les FX sur la ligne table 00 tournent donc de nouveau normalement ; les FX cumulatifs conservent leur accumulation habituelle.

Les tables 00-7F sont réservées aux tables d’instruments par défaut. Les tables 80-FE sont prévues pour les tables auxiliaires lancées par l’effet TBX. La plage inférieure fonctionne aussi pour les tables auxiliaires, mais peut causer conflits et confusions.

### Contrôles

- **EDIT + [HAUT/BAS]** sur une colonne nom FX : ouvre la sélection FX
- **OPT + [DIRECTION]** : navigue entre les tables

## 9. Instrument Pool et Wavetable AY

### Instrument Pool

L’Instrument Pool affiche tous les instruments du projet et permet de les réordonner. Pendant la lecture, il indique aussi les instruments actifs.

#### Contrôles

- **EDIT** : édite l’instrument et va à l’écran Instrument
- **SHIFT + OPT** : copie l’instrument
- **SHIFT + EDIT** : colle l’instrument
- **EDIT + [HAUT/BAS]** : réordonne les instruments
- **EDIT + PLAY** : pré-écoute l’instrument

### Wavetable AY

Le wavetable est un type d’oscillateur logiciel AY Plus. Un projet peut contenir jusqu’à 256 wavetables de 32 pas chacune. Tous les instruments wavetable partagent le même set d’ondes. Cet écran sert à les éditer.

Les niveaux de volume AY sont non linéaires, donc la vue Standard affiche une forme d’onde correspondant aux niveaux de sortie réels. Double-tapez **OPT** pour basculer entre la vue Standard et la vue **LFO** persistante, utilisant le même raccourci que Song/Live Mode : la vue LFO affiche linéairement les pas `0`–`15` bruts, avec la ligne centrale entre 7 et 8 comme zéro de modulation.

L’écran contient 2 rangées logiques. La rangée supérieure affiche les boutons **Load** et **Save**, la 2e la zone d’édition wavetable.

#### Contrôles

- **OPT + [GAUCHE/DROITE]** : navigue dans la liste wavetable
- **OPT + [HAUT/BAS]** : navigue de 16 en 16 wavetables
- **SHIFT + OPT** : copie la wavetable
- **SHIFT + EDIT** : colle la wavetable
- **OPT + PLAY** : bascule entre vue Standard et LFO

## 10. Tracker FX

Les Tracker FX forçent les valeurs du séquenceur ou du moteur sonore sur les pas individuels. D’autres grooveboxes appellent ça « verrous de paramètres ». Chaque ligne Phrase dispose de 3 slots FX.### Sequencer FX

| FX | Valeur | Comportement détaillé |
|---|---|---|
| `ARP` | `XY` | Arpège la note de base, `+X` pas et `+Y` pas. `37` produit un motif d’accord mineur. |
| `ARC` | `XY` | `X` sélectionne la direction ou le mode de portée de l’arpeggio ; `Y` est sa vitesse en ticks. |
| `PVB` | `XY` | Vibrato de pitch : `X` est la vitesse et `Y` la profondeur. En mode Linéaire, la profondeur utilise des pas de `10 cents`. |
| `PBN` | signé `XX` | Ajoute `XX` unités de pitch à chaque ligne de phrase/table ; `FF` signifie `-1`. Utiliser `00` pour arrêter. |
| `PSL` | `XX` ticks | Glisse du pitch précédent à la nouvelle note sur `XX` ticks. |
| `PIT` | signé `XX` | Offset relatif accumulé en pas du tableau de pitch. |
| `FIN` | signé `XX` | Offset fin accumulé en cents avec pitch Linéaire, en unités de période sinon. |
| `PRD` | signé `XX` | Offset relatif accumulé de période d’oscillateur. |
| `VOL` | signé `XX` | Offset relatif accumulé du volume (`FF` = -1, pas volume max). |
| `VSL` | signé `XX` | Ajoute `XX` au volume à chaque ligne de phrase/table. Utiliser `00` pour arrêter. |
| `PAN` | `00-FF` | Panoramique absolu de l’instrument : gauche, centre (`80`), droite. |
| `TPN` | `00-FF` | Panorama absolu de la piste du mixer : gauche, centre (`80`), droite. |
| `RET` | `XY` | Redéclenche toutes les `Y` ticks ; `X` applique un changement de volume. `Y=0` arrête le redéclenchement. |
| `DEL` | `XX` ticks | Retarde la note-on. Un délai plus long que la résolution du groove saute la note. |
| `OFF` | `XX` ticks | Envoie note-off après `XX` ticks et entre dans la phase release ADSR. |
| `KIL` | `XX` ticks | Coupe la voix brutalement après `XX` ticks sans jouer la release ADSR. |
| `TIC` | `XX` ticks | Définit les ticks par ligne du tableau. Dans un tableau, modifie la vitesse de cette colonne FX. |
| `TBL` | `00-FE`, `FF` off | Remplace et relance immédiatement la table d’instrument ; `FF` l’arrête. |
| `TBX` | `00-FE`, `FF` off | Lance ou remplace une table auxiliaire immédiatement ; `FF` l’arrête. |
| `THO` | ligne `XX` | Saute toutes les colonnes de la table d’instrument à la ligne `XX`. |
| `TXH` | ligne `XX` | Saute toutes les colonnes de la table auxiliaire à la ligne `XX` ; non utilisé depuis l’intérieur d’un tableau. |
| `GRV` | groove `XX` | Sélectionne un groove pour la piste en cours. |
| `GGR` | groove `XX` | Sélectionne un groove pour toutes les pistes. |
| `HOP` | `XY` | Saute à la ligne `Y`, `X` fois ; `X=0` boucle indéfiniment. HOP en table affecte sa propre colonne. |
| `SNG` | signé `XX` | Avance la lecture de `XX` lignes de chanson. |
| `PRO` | `00-64` | Définit la probabilité absolue d’un trigger de `0%` à `100%`. |
| `MOD` | `AB` | Déclenche au passage `A` d’un cycle à `B` passages. |
| `RNN` | `MX` | Joue une note aléatoire. `M` choisit le mode : `0` toujours aléatoire, `1` aléatoire à chaque PLAY, `2` choisit et garde jusqu’à ce que la valeur change. `X` choisit la plage : `0` n’importe quelle note, `1` +3 st, `2` +6 st, `3` +12 st, `4` -3 st, `5` -6 st, `6` -12 st, `7` +/-3 st, `8` +/-6 st, `9` +/-12 st. Les tirages suivent la gamme du projet quand le moteur de gamme s’applique à la piste. |
| `SPD` | signé `XX` | Sélectionne un ratio horloge persistant par piste ; voir le tableau de vitesse ci-dessous. |
| `SLE` | `00-FF` ticks | Définit un glide persistant par piste pour les FX moteur continus. `00` est immédiat. Valeur remise à `00` à l’arrêt de la lecture. |

### Envoyés d’Audio FX

| FX | Valeur | Comportement détaillé |
|---|---|---|
| `RSN` | `00-FF` | Définit l’envoi de reverb de cette piste jusqu’au prochain déclenchement de note. |
| `DSN` | `00-FF` | Définit l’envoi de delay de cette piste jusqu’au prochain déclenchement de note. |

### ADSR / Trigger FX

| FX | Valeur | Comportement détaillé |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | Remplace l’attaque, decay, sustain, release ou la forme de l’enveloppe. |
| `TDC` / `TCL` | `00-FF` | Remplace le Decay ou Color du mode Trigger sur Plaits et Plaits-Alt. |

#### Conditions

Inspiré des « trig conditions » suédoises, le tracker supporte probabilités de trigger et conditions modulo.

| FX | Valeur | Signification |
|---|---|---|
| `PRO` | `00-64` | Probabilité absolue de trigger de `0%` à `100%` |
| `MOD` | `AB` | Trigger au passage `A` de `B`; par ex. `12`, `22`, `14`, `34` |

Les compteurs `MOD` sont locaux à la piste et à la phrase. Les combinaisons invalides, comme `A > B`, ne déclenchent pas.

`MOD34` déclenche au 3e passage d’un cycle de 4 passages. `MOD1F` déclenche au 1er passage d’un cycle de 16 passages.

#### Vitesse de lecture

Comme NerdSEQ, ChooChooTracker supporte une vitesse de lecture indépendante par piste.

`SPD` lit sa valeur comme un octet signé. `00` est vitesse normale, valeurs positives accélèrent la piste, négatives la ralentissent. La valeur reste active jusqu’à un autre `SPD`.

| Valeur | Vitesse |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

Les anciens projets sans `SPD` signé gardent leur mappage `00-10` d’origine, donc ils jouent pareil.

Les multiplicateurs très élevés peuvent dépasser la résolution de l’ordonnanceur de ticks et n’ont pas été testés matériellement.

### Scale & chords FX

#### Automatisation de la gamme

`SCL XY` change la gamme et la tonique globales sans modifier les notes stockées dans les phrases. `X` sélectionne une gamme (`0` Chromatique, `1` Majeure, `2` Mineure, jusqu’à `C` Personnalisée) ; `Y` sélectionne la tonique (`0` C jusqu’à `B` B). La commande s’applique aux FX de phrase seulement, pas aux tables. Si plusieurs pistes envoient `SCL` à la même tick, c’est la piste au numéro le plus bas qui l’emporte.

`SCL` appartient au mode Quantizer. Avec **Note Lock** actif, la commande n’est pas proposée dans la liste FX et tout `SCL` déjà écrit dans les phrases est ignoré pendant ce mode.

#### Accords

`CRD XY` transforme la note sur la ligne de Phrase en un accord. `Y` sélectionne : `0` Majeur, `1` Mineur, `2` Diminué, `3` Augmenté, `4` Sus2, `5` Sus4, `6` Power, `7` Maj7, `8` Min7, `9` Dom7, `A` Min7b5, `B` Dim7, `C` Add9, `D` MinAdd9, `E` Maj9, ou `F` Min9. `X` sélectionne une des 16 voix : `0-3` un cycle inversé fermé montant ; `4-7` ajoutent un drop-2 ; `8-B` ajoutent un drop-3 ; `C-F` combinent les deux drops. Le cycle continue sur une octave supplémentaire pour triades et power chords, chaque valeur est distincte ; les plus grandes deviennent plus larges et moins classiques. Par ex. `CRD 4Y` est drop-2 en position fondamentale et `CRD DY` est 1ère inversion avec double drop. Au bas ou haut du tableau pitch, les voix sont clampées à sa plage. La commande n’est pas persistante : à écrire sur chaque ligne accordée. Chaque note générée est quantifiée indépendamment quand la Scale est active, dans les modes Quantizer et Note Lock. Disponible uniquement pour moteurs logiciels ; les instruments AY l’ignorent et restent monophoniques.

### Modulation FX

Phrase et table FX peuvent changer une modulation sans modifier l’instrument :

| Modèle FX | Signification |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Décalage relatif d’Amplitude pour les slots de modulation 1-4 |
| `M11`, `M12`, `M13`, `M14` | Décalages relatifs P1-P4 pour slot 1 |
| `M21`, `M22`, `M23`, `M24` | Décalages relatifs P1-P4 pour slot 2 |
| `M31`, `M32`, `M33`, `M34` | Décalages relatifs P1-P4 pour slot 3 |
| `M41`, `M42`, `M43`, `M44` | Décalages relatifs P1-P4 pour slot 4 |

La valeur est interprétée comme un changement relatif signé de 8 bits (`01` ajoute `1`, `FF` soustrait `1`). Les commandes répétées s’accumulent. Les valeurs effectives sont clampées dans leur plage valide.

`P5` n’est pas exposé en phrase ou table FX. Il est disponible en destination modulation `M1–M4 Wavetable`, un modulateur peut donc scanner l’index AYWavetable d’un autre.

| Type de modulation | P1 | P2 | P3 | P4 | P5 si Forme est Wavetable |
|---|---|---|---|---|---|
| ADSR | Attack | Decay | Sustain | Release | Inutilisé |
| AHD | Attack | Hold | Decay | Inutilisé | Inutilisé |
| LFO | Shape | Mode Trigger | Période | Inutilisé | Index AY wavetable |
| syncLFO | Shape | Mode Trigger | Ticks | Multiplicateur | Index AY wavetable |
| slowLFO | Shape | Mode Trigger | Pas de période (00–FF) | - | Index AY wavetable |
| FLFO | Shape | Mode Trigger | Fréquence (`1 Hz` à `20 kHz`) | Inutilisé | Index AY wavetable |

### Braids FX

| FX | Valeur | Signification |
|---|---|---|
| `BMD` | `00-2E` | Modèle Braids |
| `BTM` | `00-FF` | Timbre normalisé absolu |
| `BCL` | `00-FF` | Couleur normalisée absolue |
| `BCF` | `00-FF` | Coupure exponentielle, `20 Hz` à `20 kHz` |
| `BRS` | `00-FF` | Résonance du filtre exponentielle |

### aChChid FX

| FX | Valeur | Signification |
|---|---|---|
| `ASL` | `00-FF` | Glisse vers la note sans redéclenchement. `00` = `60 ms` ; valeurs plus élevées augmentent le glide. |
| `ADC` | `00-FF` | Override decay : de `200 ms` à `2 s`. |
| `AAC` | `00-FF` | Quantité d’accent : de aucune à maximale. |
| `ATM` | `00-FF` | Override Timbre Braids ; actif seulement en mode vague Braids. |
| `ACL` | `00-FF` | Override Couleur Braids ; actif seulement en mode vague Braids. |
| `ACF` | `00-FF` | Coupure exponentielle filtre 303, `20 Hz` à `20 kHz`. |
| `ARS` | `00-FF` | Résonance filtre 303, de nulle à maximale. |
| `AEM` | `00-FF` | Modulation enveloppe filtre 303, de nulle à maximale. |

### Bogie FX

| FX | Valeur | Signification |
|---|---|---|
| `DMD` | `00-0B` | Sélectionne le modèle Bogie jusqu’au prochain trigger. |
| `DDC`, `DTO`, `DSW`, `DNO`, `DFM`, `DDR` | `00-FF` | Override Decay, Tone, Sweep, Noise, FM ou Drive jusqu’au prochain trigger. Tous les 6 macros sont disponibles sur chaque modèle. |
| `DCF` | `00-FF` | Coupure filtre, mappée logarithmiquement. |
| `DRS` | `00-FF` | Résonance filtre. |

### Sintered FX

| FX | Valeur | Signification |
|---|---|---|
| `SMD` | `00-05` | Sélectionne le modèle Sintered jusqu’au prochain trigger. |
| `SDC`, `SMP`, `SMA`, `SMB`, `SMO`, `SMC` | `00-FF` | Override Decay, Mod, les deux premiers macros modèle, Motion ou macro finale jusqu’au prochain trigger. |
| `SCF`, `SRS` | `00-FF` | Coupure et résonance filtre. |

### MIDI Out FX

| FX | Valeur | Signification |
|---|---|---|
| `MC1`-`MC4` | `00-FF` | Envoie un MIDI CC, remappé sur `0-127`. Le numéro CC de chaque slot est défini par instrument (`MC1 number`-`MC4 number` dans l’écran instrument), pas par le FX. Un slot sans numéro est inerte. |

### Plaits FX

| FX | Valeur | Signification |
|---|---|---|
| `PMD` | `00-17` | Moteur Plaits |
| `PHA` | `00-FF` | Harmoniques normalisées absolues |
| `PTM` | `00-FF` | Timbre normalisé absolu |
| `PMO` | `00-FF` | Morph normalisé absolu |
| `PAX` | `00-FF` | Blend Principal/Aux: `00` Principal, `FF` Aux |
| `PCF` | `00-FF` | Coupure exponentielle, `20 Hz` à `20 kHz` |
| `PRS` | `00-FF` | Résonance du filtre exponentielle |

### Sample FX

| FX | Valeur | Signification |
|---|---|---|
| `SPL` | `00-03` | Mode lecture : `00` Avant, `01` Arrière, `02` Loop, `03` Ping-Pong |
| `SLI` | `00-FF` | Joue la tranche numérotée (`01` = première tranche), indépendamment de la note ; `00` garde le mappage normal. Requiert le mode tranche activé sur l’instrument. |
| `SPT` | signé `XX` | Transposition sample en demi-tons |
| `SST` | `00-FF` | Début de lecture normalisé |
| `STA` | `00-FF` | Alias de `SST` (début de lecture normalisé) |
| `SEN` | `00-FF` | Fin de lecture normalisé |
| `SVL` | `00-FF` | Volume de sample absolu |
| `SCF` | `00-FF` | Coupure exponentielle, `20 Hz` à `20 kHz` |
| `SRS` | `00-FF` | Résonance du filtre exponentielle |
| `SSP` | `00-FF` | Vitesse du sample, de `0%` à `500%` |
| `RNS` | `M0` | Joue un slice aléatoire d’un sample découpé au lieu de celui associé à la note. `M` choisit le mode : `0` toujours aléatoire, `1` aléatoire à chaque PLAY, `2` choisit et garde jusqu’à ce que la valeur change. Nécessite le mode slice activé sur l’instrument ; le quartet bas est inutilisé. |

### AY FX partagés par les instruments AY

| FX | Valeur | Comportement détaillé |
|---|---|---|
| `AYM` | `XY` | `X` est la forme d’enveloppe ; `Y` sélectionne off/tone/noise/tone+noise (`0/1/2/3`). |
| `NOI` | signé `XX` | Offset relatif accumulé de période de noise. |
| `NOA` | `00-1F`, `FF` | Période de noise absolue ; `FF` donne priorité à la période de noise des pistes plus anciennes. |
| `ERT` | n’importe | Redéclenche la forme d’enveloppe hardware actuelle. |
| `EAU` | `XY` | Ratio enveloppe automatique `X:Y` ; `X=0` désactive. |

### AY Classic uniquement FX

| FX | Valeur | Comportement détaillé |
|---|---|---|
| `EVB` | `XY` | Vibrato période enveloppe : `X` vitesse, `Y` profondeur. |
| `EBN` | signé `XX` | Ajoute `XX` à la période d’enveloppe à chaque ligne phrase/table. |
| `ESL` | `XX` ticks | Glisse de la période enveloppe précédente à la nouvelle. |
| `ENT` | note `XX` | Définit la période enveloppe selon la note du tableau pitch affichée dans l’UI. |
| `EPT` | signé `XX` | Offset relatif accumulé de période enveloppe. |
| `EPL` | octet `XX` | Définit l’octet bas de la période enveloppe. |
| `EPH` | octet `XX` | Définit l’octet haut de la période enveloppe. |

### AY Plus FX| FX | Valeur | Comportement détaillé |
|---|---|---|
| `TNN` | note `XX` | Définit l’oscillateur de tonalité sur une note de la table de pitch, en ignorant la note de la ligne. |
| `TNP` / `TNF` | signé `XX` | Décalage cumulé de pas / précision du pitch de ton. |
| `TRT` | quelconque | Retrigger la phase de l’oscillateur de tonalité là où c’est supporté. |
| `ENN` | note `XX` | Définit l’oscillateur d’enveloppe sur une note de la table de pitch. |
| `ENP` / `ENF` | signé `XX` | Décalage cumulé de pas / précision du pitch d’enveloppe. |
| `SFT` | `00-07` | Type de logiciel : Aucun, Pulse, Sync Tone, Sync Env, Wavetable, Tone FM, Env FM, Sample. |
| `SFN` | note `XX` | Définit l’oscillateur logiciel sur une note de la table de pitch. |
| `SFP` / `SFF` | signé `XX` | Décalage cumulé de pas / précision du pitch de l’oscillateur logiciel. |
| `SRT` | quelconque | Réinitialise la phase de l’oscillateur logiciel. |
| `SFM` | signé `XX` | Décalage cumulé de profondeur FM. |
| `PWM` | signé `XX` | Décalage cumulé de largeur d’impulsion. |
| `SPL` | signé `XX` | Décalage cumulé du niveau bas de l’impulsion. |
| `SWT` | signé `XX` | Décalage cumulé de l’index de la wavetable. |

### FX Sample AY

Le Sample AY accepte les commandes AY communes plus `TNN`, `TNP`, `TNF`, `TRT`, `SFN`, `SFP` et `SFF` comme décrit ci-dessus.

| FX | Valeur | Comportement détaillé |
|---|---|---|
| `SMS` | `00-FF` | Définit le point de départ de lecture legacy Sample AY à `XX x 64` échantillons source. |

Les FX des paramètres de moteur s’appliquent au type d’instrument correspondant et sont réinitialisés au déclenchement de la note suivante, sauf indication contraire. `SPD` est intentionnellement persistant.

Le sélecteur FX filtre cette référence vers les commandes communes plus le groupe supporté par l’instrument actif. Le panneau d’aide contextuelle affiche les détails des valeurs pendant l’édition.

## 11. Mixer, Réverbération et Delay

Le Mixer est l’écran principal le plus à gauche. Il affiche la charge CPU et les alertes de saturation. Un `!` rouge dans la colonne **CLIP** signale la piste dont le signal brut a dépassé la plage sûre.

Utilisez-le pour équilibrer les 8 pistes, façonner chaque piste avec l’EQ Tilt, et envoyer de l’audio vers la réverbération ou le delay :

| Champ | Signification |
|---|---|
| LVL | Niveau post-moteur de la piste, `000-100` |
| REV | Envoi vers la réverbération partagée, `000-100` |
| DLY | Envoi vers le delay ping-pong partagé, `000-100` |
| PAN | Positionne l'audio à gauche ou à droite |
| TLT | EQ Tilt par piste, `00-FF`; `80` est neutre, les valeurs basses favorisent les graves, les valeurs hautes favorisent les aiguës. Les valeurs basses introduisent une saturation douce. |
| M | `*` met la piste en sourdine |
| S | `*` solo de la piste |

Le mixer est basé sur les pistes, pas sur les instruments. Si une piste change d’instrument, son niveau, PAN, Tilt et envois restent attachés à la piste. Les instruments ont aussi leur propre **Vol** et **Pan** `00-FF` dans la première ligne de l’écran Instrument. Le PAN instrument est appliqué d’abord, puis le PAN de piste. Les deux préservent une image stéréo centrée ; ils atténuent progressivement le côté opposé lorsqu’ils sont déplacés à gauche ou à droite.

### Auto Mix

Le Mixer contient un bouton **AUTO MIX**. Il rend `6 secondes` hors ligne, équilibre le volume des pistes, puis compare 8 bandes d’octave à un profil de bruit rose et abaisse doucement les pistes qui encombrent ces bandes. Il propose des changements de niveau prudents, laisse un peu de marge avant l’écrêtage et lance un aperçu temporaire. **Apply** garde les niveaux proposés, tandis que **Cancel** restaure les 8 niveaux précédents. C’est un bon point de départ, mais vous voudrez probablement encore retoucher le résultat.

### Réverbération

Utilisez **SELECT + [UP]** depuis Mixer pour ouvrir l’écran des réglages de réverbération.

- **Algorithm** : sélectionne Clouds ou Beads. Chaque algorithme conserve ses propres contrôles.
- **Return** : niveau de réverb humide dans le mix master.
- **Clouds** : **Time**, **Damping**, plus filtres pré-réverb **LP** et **HP**.
- **Beads** : **Time**, filtre de feedback natif **Tone**, et filtre pré-réverb **HP**.

Ceci est uniquement la section de réverbération en envoi de Clouds ou Beads, pas le processeur granulaire.

### Delay Ping-pong

Utilisez **SELECT + [DOWN]** depuis Mixer pour ouvrir Delay, puis **SELECT + [UP]** pour revenir. La réverbération fonctionne dans l’autre sens : **SELECT + [UP]** ouvre la réverb, et **SELECT + [DOWN]** revient au Mixer.

- **Return** : niveau du delay humide.
- **To Reverb** : part du signal delay envoyé vers la réverbération partagée, `0-100%`.
- **Ticks** : temps de delay en ticks du tracker (`9 ticks` = `1 temps` par défaut).
- **Feedback** : quantité de feedback croisé, limitée à `95%`.
- **Filter** : filtre passe-bas dans le chemin du signal retardé.

La première répétition suit l’entrée stéréo. Les feedbacks suivants croisent entre les canaux gauche et droit.

## 12. Écran Projet

L’écran Projet offre les commandes **Load**, **Save**, **New**, **Export**, **Manage** et **Scale**, ainsi que les métadonnées nom de fichier, titre et auteur.

### Emplacement d’exportation

Les exports, stems et bounces sont écrits dans un dossier par projet à l’intérieur du répertoire samples préexistant de l’app :

- **Par défaut** : `<dossier app>/samples/Exports/<nom du projet>/`. Tant que le projet n’est pas nommé, le dossier est `current-project`.
- Sur la version web, le défaut est `/user/samples/Exports/<nom du projet>/`.
- Sur Android, le dossier samples est dans l’espace de travail de l’app : privé à l’app dans la version Play Store, `samples` dans le dossier choisi dans la version APK directe.

La ligne **Folder** de l’écran EXPORT affiche la destination active :

- Tapez **EDIT** pour choisir un dossier personnalisé avec le navigateur de dossiers. Le dossier choisi est utilisé tel quel (pas de sous-dossier projet ajouté) et est mémorisé dans les réglages.
- **EDIT + OPT** (effacer) remet à la location par défaut.

Quand un projet nommé est sauvegardé sous un nouveau nom, le dossier d’export par défaut est renommé en conséquence, conservant les fichiers exportés précédemment avec le projet. Le renommage ne s’applique pas à un dossier personnalisé, et le dossier n’est pas renommé par l’autosave. Si un dossier du nom cible existe déjà, les deux dossiers sont conservés. Si le tracker perd la trace du dossier (par exemple après un redémarrage), sauvegarder un projet nommé renomme plutôt le dossier `current-project` existant. Les instruments fonctionnent toujours après un renommage : les chemins d’échantillons chargés pointant dans le dossier renommé sont mis à jour automatiquement.

**Load** accepte les fichiers `.mid`/`.midi`, importés comme un nouveau projet : les notes sont groupées par canal MIDI (un canal par piste, jusqu’au nombre de pistes), quantifiées à 4 lignes par temps, et placées sur un instrument AY par défaut — les numéros de programme MIDI n’ont pas d’équivalent chiptune, choisissez donc ensuite de vrais instruments. Seul le premier tempo du fichier est utilisé (un taux de tick global, sans changement de tempo par section). La ligne **MIDI** de l’écran Export fait l’inverse : écrit les notes, volumes et tempo/groove de l’arrangement courant au format Standard MIDI File (une piste MIDI par piste du tracker) ; les instruments [MIDI Out](#midi-out) et autres FX par ligne au-delà du volume et du groove global n’ont pas d’équivalent MIDI et ne sont pas traduits.

**Load** accepte les chansons Dirtywave M8 (`.m8s`, firmware 2.x-4.x), importées comme un nouveau projet : structure (lignes de chanson, chaînes avec transpose, phrases), notes, vélocités, numéros d’instrument, note-offs, tempo et nom. Les notes gardent leur hauteur réelle (le `C-6` du M8 est `C-4` ici). Chaque instrument utilisé est importé sous son nom M8 : **MACROSYN** devient Braids (mêmes 47 modèles ; timbre, couleur, filtre, cutoff, résonance et pan repris, courbe du cutoff approximative), **SAMPLER** devient Sample (le `.m8s` ne stocke que le chemin du WAV sur la carte M8 : le fichier est cherché dans le dossier du `.m8s`, ses parents et votre dossier samples ; sinon l’instrument garde le chemin M8 et reste silencieux ; +24 demi-tons de Pitch, le M8 jouant deux octaves plus bas), **FMSYNTH** devient un FM Genesis (approximation grossière), **WAVSYNTH** et **HYPERSYNTH** deviennent aChChid, **MIDI OUT** devient MIDI Out (canal, bank, programme, 4 premiers CC). Les autres types deviennent un instrument AY par défaut. FX et tables sont ignorés.

**Load** accepte les chansons Little Sound Dj (`.lsdsng`, une chanson par fichier), importées comme un nouveau projet. Choisir une sauvegarde complète LSDj (`.sav`, jusqu’à 32 chansons) ouvre une liste de ses chansons (slot, nom, version, taille) ; sélectionnez-en une à importer (la chanson en cours d’édition à la sauvegarde n’est pas listée). Seule la structure et les notes sont conservées (lignes de chanson, chaînes avec transpose, phrases avec note et instrument, tempo et nom de la chanson) ; chaque note est en volume maximal car LSDj n’a pas de volume note, et les canaux LSDj PU1, PU2, WAV et NOI deviennent pistes 1-4. Les instruments pulse deviennent GB Pulse (degré, enveloppe et panoramique transférés ; le sweeping de pulse ne l’est pas), les instruments bruit deviennent GB Noise (enveloppe, stabilité et panoramique), les instruments wave deviennent le preset GB Pulse Hollow Lead, et les kits un GB Noise par défaut. Les grooves sont importés, et les commandes phrase `C` (chord), `D` (delay), `E` (enveloppe pulse et bruit, pas sur le canal wave), `G` (groove), `K` (kill) et `V` (vibrato) deviennent respectivement `ARP`, `DEL`, `CEI`/`CEP`/`CED`, `GGR`, `KIL` et `PVB`. Les autres commandes (`A`, `F`, `H`, `L`, `M`, `O`, `P`, `R`, `S`, `T`, `W`, `Z`), tables, frames wave, samples de kits et réglages synth sont ignorés, donc une chanson reposant dessus sonnera différemment. Le tempo est converti pour un groove à 6 ticks.

Sur desktop et Android, la ligne **LSDJ** de l’écran Export fait l’inverse, de façon limitée : tapez dessus et choisissez un `.lsdsng` existant comme **template** (mémorisé jusqu’à quitter, **EDIT + OPT** l’oublie). Ses lignes de chanson, chaînes, phrases, tempo et titre sont remplacés, tout le reste (tables, grooves, waves, kits, réglages synth) est conservé. Les pistes 1-4 redeviennent PU1, PU2, WAV et NOI ; les instruments GB Pulse et GB Noise sont remis comme instruments pulse et bruit LSDj, les autres types comme instrument pulse par défaut. FX, volumes de notes et note-offs ne sont pas exportés, et une chanson utilisant la chaîne `80` ou plus, ou la phrase `FF` ou plus ne peut pas être exportée (LSDj limite à 128 chaînes et 255 phrases). Le fichier exporté n’a pas encore été testé dans LSDj.

Sur desktop, la ligne **M8S** de l’écran Export fait l’inverse, de façon limitée : tapez dessus et choisissez un `.m8s` existant comme **template** (mémorisé jusqu’à quitter, **EDIT + OPT** l’oublie). Sa chanson, chaînes, phrases, tempo et titre sont remplacés, le reste est conservé, sauf que les instruments Braids sont réécrits en MacroSynth et les Sample en Sampler (chemin `/Samples/<nom du fichier>` : copiez le WAV vous-même sur le M8), si le template contient un instrument de ce type à copier. Les FX ne sont pas exportés, et une chanson utilisant la phrase `FF` ou plus ne peut pas être exportée (le M8 limite à 255 phrases). Le fichier exporté n’a pas encore été testé sur un vrai M8. Voir [M8 songs](m8s-format.md) pour les détails.

Sur desktop, key jazz vous permet de taper le nom de fichier, le titre et l’auteur directement au clavier physique plutôt que d’utiliser le clavier virtuel à l’écran — voir [Key jazz](#key-jazz-desktop-only).

### Scale / Quantize

L’écran Scale contrôle le moteur global d’échelle 12-TET. La ligne **Mode** sélectionne entre deux modes :- **Quantizer** (par défaut) : l’entrée de phrase reste chromatique ; quand activé, les notes sont arrondies vers le bas juste avant d’atteindre le moteur sonore, ce qui permet de réharmoniser une chanson en changeant la tonique ou la gamme sans éditer ses phrases.
- **Note Lock** : les notes tapées dans les phrases sont ajustées à la note la plus proche de la gamme au moment de l’entrée, seules les notes de la gamme peuvent être écrites. Les nouvelles entrées et diminutions s’alignent vers le bas ; `EDIT + RIGHT` / `EDIT + UP` élèvent la note à la suivante dans la gamme. L’entrée suit toujours la gamme du projet sur cet écran : changer la tonique ou la gamme affecte immédiatement les notes nouvellement entrées sur les pistes activées, tandis que les notes déjà stockées restent inchangées. L’effet `SCL` est indisponible en ce mode et tout `SCL` déjà écrit est ignoré. La quantification à la lecture des notes simples est contournée (l’entrée est déjà verrouillée) ; la quantification des accords via `CRD` s’applique toujours.

- **Clock source** choisit entre **Internal** (le tempo propre au projet), **Midi Clock** (le séquenceur suit une horloge MIDI entrante) et **Ableton Link** (le séquenceur suit le tempo d’une session Link sur le réseau local). Avec **Midi Clock**, la ligne **Tempo** est grisée et affiche le tempo mesuré sur l’horloge entrante ; le transport (play/stop) est piloté par les messages Start/Continue/Stop de l’émetteur, donc la commande PLAY de l’application n’est pas nécessaire. Avec **Ableton Link**, la ligne devient la ligne **Link** : elle contient le bouton de connexion/déconnexion à la session au lieu d’un champ de tempo (voir ci-dessous). La source d’horloge est sauvegardée dans settings.txt et s’applique à tous les projets.
- **Tempo** règle le tempo du projet en BPM entiers (`3-500`). En interne le tracker stocke toujours son taux de tick en Hz (`tick rate = BPM x 0,4`), donc les projets existants se chargent et se sauvegardent sans changement ; un projet à tempo fractionnaire l’affiche arrondi et se cale sur un BPM entier à la première modification. Le réglage grossier avance de 5 BPM, le réglage fin de 1 BPM ; effacer remet le défaut de 125 BPM.

ChooChooTracker sauvegarde les projets au format `.cct`. C’est un format propriétaire facile à lire même par un humain.

Utilisez **Save** avant de changer les types d’instruments ou charger un autre projet.

### Ableton Link

Ableton Link permet à ChooChooTracker de suivre le tempo d’autres applications compatibles Link (Ableton Live, LinkHut, etc.) sur le même réseau local (Wi-Fi ou Ethernet). C’est un récepteur seulement : le tracker ajuste son propre tempo à la session, et ne pousse jamais tempo ni transport vers les autres participants.

**Ce qui est synchronisé :**

- Tempo de session - le tracker suit en continu le tempo de la session Link (dans la plage de 3 à 500 BPM du tracker).
- Phase du temps - appuyer sur PLAY aligne la lecture sur la grille de temps de la session : la chanson démarre à la prochaine mesure (quantum de 4 temps).

**Ce qui n’est pas synchronisé :**

Pendant la lecture Link, ChooChooTracker vérifie la phase de temps partagée une fois par mesure 4/4 et ajuste doucement son propre taux de tick si nécessaire. Cela compense les petites différences d’horloge entre appareils sans sauter ni répéter de ticks du tracker, et ne modifie jamais la session Link d’un autre participant.

- Les grooves, les valeurs `SPD` et `DELAY` ne sont pas affectés - ils restent relatifs au tempo suivi.
- Les positions de curseur song/chain/phrase et les réglages du mixer ne sont jamais partagés.

**Utilisation :**

1. Ouvrez l’écran Project et réglez **Clock source** sur **Ableton Link**. La ligne en dessous devient la ligne **Link**.
2. Appuyez sur EDIT sur **[Connect]** pour rejoindre la session. Le message d’état à droite montre la progression : `Wait...` pendant la connexion, `No net` si aucun réseau n’est disponible, `No sess` si aucune session n’est trouvée, `Err` en cas d’erreur interne. Une fois connecté, le bouton affiche **[Disconnect]** et l’état alterne environ chaque seconde entre `connected` et `Peers: N` (N = nombre de pairs).
3. Attendez `connected`, puis appuyez sur PLAY - la lecture démarre alignée sur la prochaine mesure de la session.
4. Appuyez sur EDIT sur **[Disconnect]** (ou remettez la source d’horloge sur Internal) pour quitter la session.

En mode Link, la session possède le tempo : la ligne n’affiche aucun champ BPM, et le tempo de la session est appliqué automatiquement au projet (dans la plage de 3 à 500 BPM du tracker).

**Disponibilité selon les plateformes :** PortMaster (cible principale), Windows, Linux, macOS et Android sont pris en charge. La version Web et les versions Miyoo n’incluent pas Link.

**Note PortMaster :** sur les appareils PortMaster le buffer audio est plus grand (512 trames à 48 kHz), donc l’alignement sur le temps est approximatif, à environ 10-15 ms près, et non précis à l’échantillon. La synchronisation du tempo elle-même n’est pas affectée.

## 13. Paramètres

Utilisez Paramètres pour les préférences qui restent dans l’application, plutôt que dans le projet courant. Les contrôles globaux du projet comme tempo, groove et niveaux du mixer restent sur leurs propres écrans.

### Contrôles

- **Repeat delay** et **Repeat speed** définissent le délai avant répétition et la vitesse de répétition lorsqu’un bouton de navigation ou d’édition est maintenu. Augmentez le délai si une pression longue provoque des répétitions trop rapides ; diminuez-le pour une saisie rapide de tracker.
- **Stick live mode** choisit le comportement du bouton **STICK LIVE** mappé : `HOLD` est actif tant qu’on le maintient, `TOGGLE` reste actif après une pression, et `FREE` est toujours actif. Voir [Enregistrement de mouvement](#motion-recording).
- **Key mapping** assigne les commandes du clavier ou contrôleur. Choisissez une commande, appuyez sur le nouveau contrôle, puis sélectionnez **Done** pour enregistrer le mapping.

### Audio

**Sample rate** propose `32 kHz` ou `48 kHz` (défaut). Les changements prennent effet immédiatement et sont sauvegardés. Changer la fréquence d’échantillonnage arrête la lecture et la préécoute des samples pendant que les moteurs sonores et toute entrée audio armée se rouvrent à la nouvelle fréquence. Si un appareil refuse la nouvelle fréquence, le réglage précédent reste actif. `32 kHz` réduit le travail de traitement et limite la fréquence reproduite la plus haute à 16 kHz. L’indicateur de charge CPU montre le coût du rendu audio.

**Buffer size** échange réactivité contre stabilité. Choisissez une taille plus petite pour un jeu live plus serré ; augmentez-la si l’audio craque ou si la charge CPU est constamment élevée. Les valeurs disponibles sont `32`, `64`, `128`, `256`, `512` et `1024` échantillons. L’affichage de charge CPU aide à choisir un réglage stable.

### MIDI

Choisissez les appareils d’entrée et de sortie, assignez les canaux MIDI entrants aux instruments, et mappez les contrôleurs MIDI CC ici. Voir [MIDI](#15-midi) pour connecter un clavier, contrôler le mixer, ou envoyer des notes vers du matériel externe.

### Synthés

- **AY Quality** choisit la qualité utilisée par les instruments AY/YM. **AY Sample dithering** peut lisser la lecture des samples AY ; laissez-le désactivé pour un rendu plus direct et lo-fi, ou si vous ciblez le matériel d’origine.
- **Braids BITS**, **DRFT** et **SIGN** sont des réglages globaux du caractère Braids, partagés par chaque instrument Braids. Servez-vous-en pour définir la résolution, la dérive et le profil global avant d’affiner les sons individuels. Voir [Braids](#braids).

### Mixer

- **Mix volume** est le niveau de sortie maître de l’application. Baissez-le si vous alimentez un autre mixer ou un enregistreur ; cela ne modifie pas le mix des pistes dans la chanson.
- **Tilt pivot** règle la fréquence centrale pour chaque contrôle **Tilt** du Mixer dans ce projet, de `250 Hz` à `4000 Hz` (défaut `1 kHz`).

### Graphismes

- **Edit color theme** modifie les couleurs de l’interface. **Load font** charge une police d’affichage `.cnfont`.
- **Song waveform** affiche ou masque la forme d’onde du mixage sur la page Song ; voir [Song waveform](#song-waveform).
- **Track visuals** choisit, pour chaque piste, soit **Detailed** (forme d’onde de l’instrument et superpositions), soit **Audio waveform** (forme d’onde audio résultante de la piste). Utilisez **All detailed** ou **All audio** pour changer toutes les pistes ensemble.
- **Waveform FPS** règle la fréquence de rafraîchissement des scopes d’instrument et des visualiseurs du mixer, de `1` à `60` images par seconde. Baissez cette valeur si vous préférez un affichage plus calme ou avez besoin plus de ressources.
- **Renderer** indique si l’application utilise le rendu `GPU` ou `Software`.

### Rapport de support et quitter

**Support report** crée un fichier diagnostic léger pour un rapport de bug. Joignez-le avec le projet `.cct` lorsque le problème concerne une chanson en particulier. **Quit ChooChooTracker** ferme proprement l’application.

### Song waveform

Graphismes → Song waveform est désactivé par défaut. Appuyez sur EDIT pour basculer, ou utilisez EDIT + direction ; le choix est sauvegardé comme préférence de l’application. Quand activé, une forme d’onde sur deux rangées au-dessus de la page SONG montre le mixage final sommée en mono. Les autres pages gardent leur mise en page pleine hauteur, espacement normal et prévisualisations d’instruments, peu importe cette option. La préférence sauvegardée reste compatible avec les versions antérieures. Les touches tactiles SONG suivent les rangées affichées dans les deux modes.

## 14. Dépannage

### CPU

Le coût CPU dépend des moteurs et effets actifs. Les modèles physiques Plaits et Clouds Reverb demandent plus que des voix AY standards. Testez la performance sur la console cible, surtout avec 8 voix Plaits et les deux sends activés. Une lecture CPU proche de `100%` peut provoquer des craquements ou des délais audio manqués.

En cas de pops, plantages ou ralentissements, utilisez **Settings > Support report** et joignez le fichier `support-report.txt` au rapport de bug. Si une chanson spécifique crée le problème, joignez aussi ce `.cct`.

### Pas de son

Vérifiez le numéro d’instrument, silence ou solo de la piste, niveau de piste LVL, Mix volume de l’application et enveloppe de l’instrument. Pour les samples, vérifiez que le WAV d’origine existe toujours à son chemin enregistré.

Sur ArkOS, appuyez sur **MENU + L3** pour basculer le silence du système. Si cela ne suffit pas, redémarrez le matériel et reconnectez votre interface audio. Essayez l’option "repair audio" dans ArkOS, elle peut aider.

### Entrée incorrecte

Ajustez Repeat delay et Repeat speed dans Paramètres. Si une seule pression produit deux mouvements, vérifiez qu’un seul contrôle physique est mappé à cette direction et signalez l’écran exact ainsi que le raccourci utilisé.

## 15. MIDI

Paramètres > **MIDI** ouvre ce sous-menu : **MIDI In** / **MIDI Out** choisit un appareil connecté, en faisant défiler les ports détectés avec `OFF` aux extrémités ; un appareil doit être sélectionné dans chaque sens.

- **Préécoute sonore d’un clavier MIDI** : avec un appareil MIDI In sélectionné, jouer des notes sur le clavier connecté auditionne un instrument sur n’importe quel écran, comme le raccourci à l’écran **EDIT + PLAY** sur la page [Instrument](#5-instruments). C’est une préécoute seulement — cela ne saisit pas les notes dans la chanson.
- **Affectation des canaux** : ouvre la liste des 16 canaux MIDI; assignez à chacun un instrument (`OFF` par défaut) pour que les notes reçues sur ce canal préécoutent cet instrument quelle que soit la sélection sur la page Instrument — par ex. canal `01` → instrument `05`. Un canal laissé `OFF` revient à l’instrument actuellement sélectionné, comportement original. Cette affectation est sauvegardée dans settings.txt.
- **Mapping MIDI CC** : mappe un contrôleur entrant à un paramètre d’instrument, y compris son **Volume**, ou à des contrôles globaux. Les lignes globales affichent `TRK 1`-`TRK 8` au lieu d’un instrument et fournissent les commandes piste **Mute**, **Solo**, **Volume**, **Send Reverb**, et **Send Delay** ; la commande **Song play/stop** n’a pas de cible. Un nouveau mapping ignore le CC qui l’a appris/configuré et prend effet au prochain mouvement physique, donc configurer pendant la lecture ne remet pas un patch à zéro. Silence et solo utilisent `0-63` = off, `64-127` = on.
- **Synchronisation par horloge MIDI** : avec un appareil MIDI In sélectionné, réglez **Clock source** de l’écran Project sur **Midi Clock** pour suivre un séquenceur externe. Le tracker mesure le tempo sur l’horloge entrante (24 impulsions par noire) et l’affiche sur la ligne **Tempo** grisée ; les messages **Start**, **Continue** et **Stop** contrôlent la lecture. La source d’horloge est mémorisée dans settings.txt.
- **Pilotage d’un appareil MIDI externe** : donnez à une piste le type d’instrument [MIDI Out](#midi-out) et réglez son canal ; déclencher des notes sur cette piste envoie du MIDI réel vers l’appareil MIDI Out sélectionné au lieu de produire du son dans ChooChooTracker.

## 16. Crédits et licence

ChooChooTracker est un fork de ChipNomad et conserve sa licence MIT. Les codes Braids, Plaits, Plaits-Alt, Clouds DSP, Warps dérivé de MME DSP et stmlib sont issus des sources open-source de Mutable Instruments sous leurs mentions MIT respectives. Plaits-Alt vient du fork lylepmills/eurorack Plaits Lab ; ses mentions sources sont conservées. Le moteur aChChid utilise Open303 de Robin Schmidt, copyright 2009, sous licence MIT. MonkSynth utilise le DSP natif de JonET sous licence MIT ; voir `LICENSE.monksynth.txt`. Bogie et Sintered sont des implémentations natives originales. Le MIDI I/O utilise RtMidi de Gary P. Scavone, sous licence MIT. Consultez les fichiers de licence inclus pour les attributions exactes. Ableton Link est fourni avec un accord de licence spécial d’Ableton, n’est pas sous licence MIT, et notre licence ne s’étend pas à vos forks ni à d’autres projets que ChooChooTracker.