# Designbook — 12 août 2026

Ce dossier rassemble les maquettes ASCII avant toute modification de l'interface.
La cible est l'écran tracker de 40 colonnes × 20 lignes. Les crochets indiquent
un champ éditable ; `>` le focus. Les libellés et valeurs sont illustratifs.

## État actuel relevé dans le code

### En-tête commun des instruments

Toutes les fiches instrument partagent aujourd'hui ce haut d'écran. `Tbl. Tic`
et `Vol` sont assez éloignés du bloc de gauche et la ligne ne forme pas une
répartition régulière.

```text
INSTRUMENT 00

Type    [Plaits  ]       Load  Save
Name    [BASSLINE       ]
Transp. [On ]      Tbl. Tic [01]  Vol [FF]
```

### AY classique (référence de mise en page)

AY répartit les contrôles en deux colonnes, avec des titres de sections et des
lignes blanches qui donnent une lecture par blocs.

```text
INSTRUMENT 00

Type    [AY1     ]       Load  Save
Name    [LEAD           ]
Transp. [On ]      Tbl. Tic [01]  Vol [FF]

Tone    [On ]
Noise   [Off]            Auto env period [On ]
Env     [A /\\]          Enabled         [1:1]

Volume envelope
Atk     [10]
Dec     [20]
Sus     [0F]
Rel     [40]
```

AY2 et AY Sample emploient aussi deux colonnes et des sections (« Tone »,
« Envelope », « Noise », « Soft osc », « Sample »). C'est le langage visuel à
reprendre pour les moteurs modernes.

### Braids, Plaits et PCM Sample aujourd'hui

Ces trois fiches utilisent actuellement une liste verticale : libellé à gauche,
valeur à la colonne 12, puis une ligne ADSR en bas. La liste est lisible mais
n'exprime pas les familles de paramètres.

```text
BRAIDS                         PLAITS                 PCM SAMPLE

Model    [07 SAW-SYNC       ]  Engine  [07 CHIPTUNE ]  Sample [Load kick.wav]
Timbre   [0512]               Harmonic[0512]          Pitch  [+0 st]
Color    [0256]               Timbre  [0512]          Start  [00]
Filter   [On  ]               Morph   [0512]          End    [FF]
Mode     [LP  ]               Main/Aux[80]            Filter [On]
Slope    [24 dB]              Env Mode[TRIG]          Mode   [LP]
Cutoff   [8000 Hz]            Filter  [On]            Slope  [24 dB]
Reso     [80]                 Mode    [LP]            Cutoff [8000 Hz]
ADSR     A [10] D [20]        Slope   [24 dB]         Reso   [80]
         S [C0] R [40]        Cutoff  [8000 Hz]        ADSR A[10] D[20] S[C0] R[40]
                              Reso    [80]
                              ADSR D [20] C [40]  (TRIG)
```

### Popup actuel de modèle

Le popup dispose déjà de catégories, mais les montre en premier écran puis
ouvre un second écran pour les entrées. On ne voit jamais la catégorie et son
contenu simultanément.

```text
BRAIDS MODEL

> ANALOG >
  MULTI OSC >
  FILTER / VOICE >
  FM / CHAOS >
  PHYSICAL >
  DRUMS >

              EDIT/RIGHT : entrer
              OPT/LEFT   : retour
```

### Mixer et aide de navigation

Le mixer n'affiche pas d'aide de navigation sur la page principale. Les pages
Reverb et Delay ajoutent en revanche une instruction dans le titre, ce qui est
incohérent avec le reste de l'application.

```text
MIXER                              CLOUDS REVERB  SEL+UP: BACK

   LVL  REV  DLY  MUTE  SOLO        Return      [100%]
T1 080  010  000  OFF   OFF         Time        [40]
T2 080  020  000  OFF   OFF         Damping     [80]
...                                 Filter      [8000 Hz]
```

## Proposition A — fiches moteurs modernes en sections

Conserver l'en-tête commun, puis systématiser la grille AY : deux blocs de
largeur égale, titres de sections, une ligne blanche seulement entre familles.
Les paramètres propres au moteur restent à gauche ; l'enveloppe et le filtre
commun sont à droite. Cela rend Braids, Plaits, PCM Sample et Plaits-Alt
prévisibles sans ajouter de nouvelles commandes.

### Gabarit commun

```text
INSTRUMENT 00

Type    [Plaits  ]       Load  Save
Name    [BASSLINE       ]
Transp. [On ]   Tbl.Tic [01]   Vol [FF]

SOUND                    SHAPER
<paramètre moteur>       <enveloppe / sortie moteur>
<paramètre moteur>       <enveloppe / sortie moteur>
<paramètre moteur>       <enveloppe / sortie moteur>

FILTER                   AMPLIFIER
Enabled [On ]            Mode    [VCA ]
Mode    [LP ]            A [10]  D [20]
Slope   [24 dB]          S [C0]  R [40]
Cutoff  [8000 Hz]
Reso    [80]
```

### Braids

```text
INSTRUMENT 00

Type    [Braids  ]       Load  Save
Name    [DIGI LEAD      ]
Transp. [On ]   Tbl.Tic [01]   Vol [FF]

OSCILLATOR               FILTER
Model   [07 SAW-SYNC   ] Enabled [On ]
Timbre  [0512]          Mode    [DIGILP ]
Color   [0256]          Slope   [24 dB]
                        Cutoff  [8000 Hz]
                        Reso    [80]
AMPLIFIER              
A [10]  D [20] S [C0]  R [40]
```

### Plaits et Plaits-Alt

Même placement pour les deux banques ; seuls le type et le catalogue changent.
Le projet reste ainsi stable lorsque Plaits-Alt sera ajouté.

```text
INSTRUMENT 00

Type    [Plaits-Alt]     Load  Save
Name    [METAL HIT      ]
Transp. [On ]   Tbl.Tic [01]   Vol [FF]

ENGINE                   FILTER
Engine  [12 GLISSON    ] Enabled [On ]
Harmonic[0512]          Mode    [DIGILP ]
Timbre  [0256]          Slope   [24 dB]
Morph   [0768]          Cutoff  [8000 Hz]
Main/Aux[80]            Reso    [80]

AMPLIFIER
Mode    [TRIG]
Decay   [20]  Color [40]
```

En mode VCA, le bloc `ENVELOPE` devient `A [10] D [20] / S [C0] R [40]`.

### PCM Sample

```text
INSTRUMENT 00

Type    [PCM Sample]     Load  Save
Name    [KICK 909       ]
Transp. [On ]   Tbl.Tic [01]   Vol [FF]

SAMPLE                   FILTER
File    [Load kick.wav ] Enabled [On ]
Pitch   [+0 st]         Mode    [DIGILP ]
Start   [00]            Slope   [24 dB]
End     [FF]            Cutoff  [8000 Hz]
                        Reso    [80]
AMPLIFIER
A [10]  D [20] S [C0]  R [40]
```

Décision recommandée : garder des coordonnées fixes, même lorsqu'un champ est
conditionnel. Un champ indisponible est masqué, sans remonter les autres ; la
mémoire musculaire reste intacte.

## Proposition B — ligne commune mieux répartie

Décaler `Tbl. Tic` vers la gauche et réserver trois groupes réguliers sur la
ligne. `Vol` reste à droite, mais sans l'espace mort actuel.

```text
ACTUEL
Transp. [On ]      Tbl. Tic [01]  Vol [FF]

PROPOSÉ
Transp. [On ]   Tbl.Tic [01]   Vol [FF]
0........10........20........30.......39
```

Ce changement n'affecte ni les valeurs, ni la navigation : uniquement les
coordonnées de dessin et du curseur.

## Proposition C — sélecteur à deux panneaux

Remplacer le popup à deux étapes par une « vue inventaire » unique. Le panneau
gauche sélectionne la catégorie ; le panneau droit liste ses entrées. Les
flèches haut/bas restent dans le panneau actif, gauche/droite changent de
panneau, `EDIT` confirme l'entrée et `OPT` annule. Le modèle courant est
marqué `*`. Les listes longues défilent dans leur panneau.

```text
BRAIDS MODEL
┌────────────────┬───────────────────────┐
│> ANALOG         │  CSAW                 │
│  MULTI OSC      │* SAW-SYNC             │
│  FILTER / VOICE │  SINE-TRI             │
│  FM / CHAOS     │  SQUARE-SUB           │
│  PHYSICAL       │  ...                  │
│  DRUMS          │                       │
│  WAVETABLES     │                       │
│  NOISE / GRAN.  │                       │
└────────────────┴───────────────────────┘
 LEFT/RIGHT panel · UP/DOWN move · EDIT select · OPT cancel
```

Même composant, mêmes touches, pour :

- modèle Braids ;
- moteur Plaits et Plaits-Alt ;
- type d'instrument (« engine »).

Pour le type d'instrument, le panneau gauche a deux catégories sobres et le
panneau droit est alphabétique. Le défilement gauche/droite existant reste
possible sur le champ `Type`; `EDIT` ouvre le popup.

```text
INSTRUMENT TYPE
┌────────────────┬───────────────────────┐
│> CHIP           │* AY Classic           │
│  SYNTH          │  AY Plus              │
│                │  AY Sample            │
│                │                       │
│                │                       │
└────────────────┴───────────────────────┘

INSTRUMENT TYPE
┌────────────────┬───────────────────────┐
│  CHIP           │  Braids               │
│> SYNTH          │  PCM Sample           │
│                │* Plaits               │
│                │  Plaits-Alt           │
└────────────────┴───────────────────────┘
```

Règle de compatibilité : l'ordre alphabétique est uniquement un ordre
d'affichage. Les valeurs sérialisées `InstrumentType`, IDs de modèles et IDs
d'engines ne changent pas ; sinon les anciens projets changeraient de son.

## Propositions complémentaires à valider

## État d'implémentation — 12 août

- Le thème Wood est le thème de démarrage ; la page Web reprend sa palette et
  donne accès au manuel et au dépôt GitHub.
- Les sélecteurs de type, de modèle et d'engine sont maintenant des popups à
  deux panneaux. Le défilement direct avec `EDIT + gauche/droite` est conservé.
- Plaits-Alt est un type séparé : ses 24 engines supplémentaires sont groupés
  dans les cinq catégories ci-dessous et ne reprennent aucun engine Plaits.
- Le mixer affiche `!` dans sa colonne CLIP pour la ou les pistes ayant poussé
  le mix sec au-delà du seuil. Les titres Reverb et Delay ne portent plus
  d'aide de navigation.
- `Tbl.Tic` a été déplacé vers la gauche dans l'entête commun. Les réglages
  ADSR restent sur une seule ligne, conformément à la correction effectuée
  pendant cette revue.

### Alertes de clipping : localisation immédiate

Quand une piste dépasse le seuil, le mixer doit désigner la ou les sources au
lieu d'une alerte générale. Une marque `CLIP` reste visible brièvement après le
pic, puis est effaçable avec `EDIT` sur la cellule.

```text
MIXER                                      MASTER CLIP!

   LVL  REV  DLY  MUTE  SOLO
T1 080  010  000  OFF   OFF
T2 100  080  050  OFF   OFF   ! CLIP
T3 090  000  000  OFF   OFF
T4 100  100  100  OFF   OFF   ! CLIP

! = clip détecté depuis le dernier acquittement
```

Le témoin est une information de diagnostic, pas un nouveau limiteur ni un
changement de mixage.

### Navigation : supprimer l'aide contextuelle

Harmoniser Reverb et Delay avec les autres écrans : titres courts, sans
`SEL+UP: BACK` ou `SEL+DOWN: BACK`. Les combinaisons restent dans le manuel et
la mémoire de l'utilisateur est soutenue par la navigation uniforme.

```text
AVANT : CLOUDS REVERB  SEL+UP: BACK
APRÈS : CLOUDS REVERB
```

### Web, thème Wood et liens

Utiliser la palette Wood déjà présente dans l'application (brun profond,
bois clair, texte crème), sans reproduire artificiellement le grain dans la
page. Ajouter une petite barre de liens sous le statut : manuel local servi
avec la web build et dépôt GitHub.

```text
┌────────────────────────────────────────┐
│  CHOOCHOO TRACKER WEB PREVIEW          │
│  PREALPHA - DON'T MISS YOUR STOP       │
│  [ Start tracker ]  Choo choo!         │
│  ┌──────────────────────────────────┐  │
│  │             TRACKER              │  │
│  └──────────────────────────────────┘  │
│        Manual · GitHub                 │
└────────────────────────────────────────┘
fond #24170f · cadre #8a663d · texte #ead8b8 · accent #b88a4b
```

## Hors périmètre de cette passe de maquette

La charge CPU est considérée acceptable selon les tests du 12 août. Aucun
indicateur de performance ou changement DSP n'est proposé ici. L'ajout de
Plaits-Alt doit être chiffré séparément (catalogue, IDs stables, données de
projet, voix et tests), puis il héritera du gabarit et du popup ci-dessus.

## Écran instrument — gabarit implémenté

Le gabarit des moteurs de voix (Braids, Sample, Plaits et Plaits-Alt) utilise
l'espace vertical libre sans déplacer le séquenceur. Le moteur a sa propre
ligne pleine largeur ; les paramètres sont ensuite organisés en deux sections
alignées. Cette structure évite que les noms de modèles longs empiètent sur le
filtre.

```text
INSTRUMENT 00

Type    Plaits          Load  Save             1 ---
Name                                            2 ---
Transp. On      Tbl.Tic 01   Vol FF            3 ---

Engine       00 VA VCF                         4 ---
SOURCE                  FILTER                  5 ---
Harmonic     0000        Status   On           6 ---
Timbre       0003        Mode     LP           7 ---
Morph        0000        Slope    12 dB        8 ---
Main/Aux     00          Cutoff   20000 Hz
Env Mode     VCA         Reso     00

ADSR  AFF  D90  S80  R00  Shape 00
      [graphique ADSR A → R, 17 colonnes × 3 lignes]
```

Règles de présentation :

- `Engine` (ou `Model` pour Braids, `Sample` pour PCM), `SOURCE`, `FILTER`,
  `ADSR` et `LPG` sont en `textTitles`, la couleur d'accent des thèmes AY.
- Les libellés de paramètres et les valeurs non focalisées restent en
  `textDefault`; la valeur focalisée utilise `textValue`.
- La ligne de statut du filtre s'appelle `Status`, pas `Filter`, car le titre
  de section indique déjà le contexte.
- Les valeurs commencent en colonne 26 : `20000 Hz` et `24 dB` doivent rester
  entièrement visibles avant le séquenceur.
- La preview ADSR est rasterisée, commune aux trois moteurs de voix, placée
  sous A/D/S/R (colonnes 6 à 22) et haute de trois lignes. Elle reflète Attack,
  Decay, Sustain, Release et Shape (`00` logarithmique, `80` linéaire, `FF`
  exponentielle). En Plaits TRIG, la ligne devient `LPG` et aucune preview ADSR
  n'est dessinée.

## Pistes différées

### Glyphes contextuels dans les fiches instrument

Les futures fiches instrument pourront associer un glyphe compact à certains
paramètres musicaux. Le texte reste toujours visible : le glyphe donne un repère
immédiat et autorise des libellés courts, sans devenir une iconographie à
apprendre.

```text
ENG  ⚡   HARMO ∥   TIMB ≈   MORPH ◇   MIX ⊙
STAT ●   FILT [LP]   SLOPE /   CUT ─●─   RES ···
```

- Les formes sont dessinées dans la bitmap font existante, avec l'épaisseur et
  les couleurs du thème ; aucune police ou bibliothèque supplémentaire.
- Elles accompagnent seulement les paramètres à lecture rapide : moteur,
  Harmo/Timb, Morph, Main/Aux, filtre, statut, volume, sends et ADSR.
- Les glyphes de valeur utilisent peu d'états discrets (par exemple 4, 8 ou 16)
  et sont redessinés lors d'une modification ; pas d'animation permanente.
- `ADSR`, `VOL`, `LP/BP/HP`, `Hz` et `dB` restent des termes explicites.

### Écran titre

Avant le tracker, ajouter un écran titre d'inspiration SNES : logo
`CHOOCHOOTRACKER`, fond et cadre cohérents avec le thème actif, puis un menu
vertical volontairement court.

Le visuel de référence est
`inspirations/titlescreendraft.png`. Son illustration pixel art 256 couleurs
(train, pont, montagnes et logo) est à reprendre comme base, en conservant sa
composition générale. Le cadrage sera ajusté au 640 × 480 de la RG353V ; le
menu existant de la maquette est remplacé par celui ci-dessous.

```text
             CHOOCHOOTRACKER

                 > CONTINUE
                   OPEN
                   NEW
```

- `CONTINUE` recharge le dernier projet ouvert ou sauvegardé ; il est indisponible
  lorsqu'aucun projet récent n'est connu.
- `OPEN` ouvre le navigateur de projets existant.
- `NEW` crée un projet vierge avec le flux actuel.
- Les trois actions réemploient les flux de chargement/création existants :
  l'écran titre n'ajoute ni format de projet, ni sauvegarde automatique.

### Langage visuel partagé avec le tracker

Le tracker ne doit pas devenir une illustration pleine page : sa densité et sa
lecture immédiate restent prioritaires. Il peut toutefois reprendre les codes
du titre pour que les deux écrans appartiennent au même jeu :

- fond bleu nuit ou bleu ardoise, avec panneaux presque noirs ;
- cadres fins brun/métal et angles légèrement renforcés, réservés aux panneaux,
  popups et focus ;
- texte crème clair, titres dorés et valeur/focus jaune chaud ;
- touches bleu acier ou vert paysage pour les valeurs secondaires et états
  actifs ;
- bitmap font nette et glyphes à 1 px, sans texture sous les données du tracker.

Le séquenceur conserve des cellules sobres et planes. Les textures, le décor et
les gros logos restent propres à l'écran titre ; ils ne doivent pas concurrencer
notes, FX, curseur de lecture et valeurs éditables.

**Règle de densité :** cette direction ne modifie ni le nombre de champs
visibles, ni les coordonnées, ni les commandes des écrans tracker existants.
Les maquettes illustrées sont des références de palette, de cadres, de typo et
de glyphes ; elles ne sont pas des propositions d'ajouter des panneaux, des
contrôles ou de l'information. L'habillage 16 bits ferroviaire doit se poser
autour de l'UI actuelle, jamais la densifier.

### Plan d'intégration — après stabilisation

Ce chantier est esthétique. Il attend la fin des tests et débugs fonctionnels,
afin de ne pas mélanger les régressions d'interface avec celles du moteur ou du
format de projet.

1. Créer un thème optionnel `Railway 16-bit` : palette bleu nuit, crème, or,
   acier et états de focus. Les thèmes et les neuf couleurs principales sont
   déjà configurables ; cette étape ne change aucune navigation.
2. Poser un habillage léger autour des coordonnées existantes : titre, cadre
   externe, séparateurs et panneaux sobres. La grille tracker reste plane.
3. Ajouter les glyphes contextuels et leurs quelques états discrets aux fiches
   instrument, sans créer de nouvelles cellules éditables.
4. Ajouter l'écran titre illustré et ses trois actions `CONTINUE`, `OPEN`,
   `NEW`, en réemployant les flux de projet existants.
5. Vérifier le rendu et la lisibilité sur Windows, Web et RG353V.

La cible est une semaine environ pour une finition complète, découpée en
étapes utilisables et réversibles. Ce n'est pas une réécriture : ni le moteur
audio, ni les contrôles tracker, ni le format `.cct` ne doivent être modifiés.
