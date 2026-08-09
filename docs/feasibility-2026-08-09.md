# Étude de faisabilité — 9 août 2026

Ce document évalue des idées. Il ne les transforme pas en fonctionnalités promises.

> Mise à jour : Plaits, les sends Clouds Reverb/Delay, `PRO`, `MOD` et `SPD` ont été implémentés après cette étude. Les verdicts ci-dessous restent l'analyse initiale ; les benchmarks et tests musicaux sur RG353V restent nécessaires.

## Plaits

Faisable sous forme d'un nouvel instrument `Plaits`. Le snapshot Mutable présent dans `inspirations/` est sous licence MIT et contient 24 engines, avec les sorties principale et auxiliaire.

Le point technique important est la fréquence d'échantillonnage : Plaits travaille nativement à 48 kHz alors que ChooChooTracker mélange à 96 kHz pour Braids. La solution la moins risquée consiste à laisser Plaits à 48 kHz puis à suréchantillonner sa sortie par deux. Modifier les constantes internes de tous les engines serait plus difficile à valider.

Chaque voix demande un bloc de travail d'environ 16 Kio, en plus de l'objet `Voice` et de ses tables. La mémoire n'est pas inquiétante sur RG353V. La charge CPU doit en revanche être mesurée avec un prototype d'une voix, puis avec huit voix et les engines les plus coûteux, avant d'intégrer l'interface ou le format projet.

Verdict : faisable, risque moyen, prototype DSP et benchmark obligatoires.

## Sends Reverb et Delay

Le mixer peut recevoir deux niveaux de send par piste. Le moteur doit accumuler deux bus stéréo pendant le mixage, traiter chaque effet une seule fois par callback, puis ajouter les retours au master. Il ne faut surtout pas instancier une reverb ou un delay par piste.

La classe `clouds::Reverb` est isolable du reste de Clouds et sous licence MIT. Elle est conçue autour de 32 kHz et d'un buffer de 16 384 mots. Deux options sont raisonnables : la faire tourner à 32 kHz avec conversion 96/32 kHz, ou adapter ses délais et tripler sa mémoire pour 96 kHz. La première option est plus petite mais doit être écoutée attentivement.

Le delay synchronisé est simple côté DSP, mais le mot « BPM » doit être défini avec les grooves. Le réglage le plus robuste serait une division musicale fondée sur la durée nominale de quatre rows, tandis que le swing resterait dans le séquenceur. Le filtre de chaque retour peut réutiliser le filtre stéréo du moteur moderne.

Deux colonnes `REV` et `DLY` peuvent être ajoutées au mixer. `Select+Up` et `Select+Down` sont disponibles pour ouvrir les écrans de réglage des deux effets. Les sends et paramètres globaux devront être sauvegardés dans `.cct`.

Verdict : faisable, risque moyen. Valider d'abord une reverb globale sans interface, puis mesurer Reverb + Delay + huit voix.

## FX de lecture et conditions

### Tables comme automation

C'est déjà possible : les quatre colonnes FX d'une table acceptent les FX Braids et Sample et les rejouent à la vitesse propre de chaque colonne. Une table peut donc séquencer cutoff, résonance, timbre, color, start, end ou volume. Le reset des FX au trig a été placé avant l'initialisation de la table afin que sa row 0 soit appliquée immédiatement.

### Déjà présent

`PSL` réalise déjà un glide vers la nouvelle note avec une durée en ticks. Ajouter un second FX Glide créerait deux commandes pour le même comportement. Il faut d'abord valider et éventuellement renommer l'aide de `PSL`.

### Vitesse de lecture par piste

Un FX `SPD` peut multiplier la vitesse du séquenceur sur sa piste uniquement, sans modifier le BPM global, le pitch ni la vitesse de lecture audio des samples. Une codification simple serait `01=/4`, `02=/2`, `03=x1`, `04=x2`, `05=x4`. La valeur resterait active jusqu'au prochain `SPD` et serait remise à `x1` au démarrage de Play.

Chaque piste possède déjà son compteur de groove et son playhead : les pistes peuvent donc se désynchroniser sans nouvelle architecture. Le multiplicateur doit s'appliquer au temps du groove avec un accumulateur fractionnaire afin de conserver le swing et d'éviter les arrondis. Les changements de vitesse doivent aussi passer par le chemin normal des rows pour que notes, conditions, `HOP` et fins de chaîne restent cohérents.

La limite est le tick du moteur. Avec le groove standard à 6 ticks, `x2` donne 3 ticks par row et `x4` alterne correctement autour de 1,5 tick. Si une row ne dure déjà qu'un tick, `x2` ou `x4` demanderait plusieurs trigs dans le même tick ; le moteur actuel ne peut pas leur donner une durée audio distincte. La première version devrait donc plafonner à une row par tick. Un fonctionnement exact dans tous les grooves exigerait de segmenter le rendu audio à l'intérieur du tick.

Verdict : faisable et peu coûteux en CPU, risque faible avec le plafond d'une row par tick, risque moyen si `x2/x4` doivent rester exacts sur les grooves très rapides.

### Conditions simples

Probability, Modulo, `1ST`, `!1ST`, `PRE` et `!PRE` peuvent être évalués juste avant la lecture d'un trig. Lorsqu'une condition est fausse, la note, l'instrument, le volume et les FX de cette row doivent être ignorés ensemble.

État minimal nécessaire par piste : compteur de passages de phrase, résultat du trig conditionnel précédent et générateur aléatoire reproductible. Ces états doivent être remis à zéro au démarrage de Play. `Every 3rd/4th/5th`, rows paires et rows impaires sont déjà des cas particuliers de Modulo ; il ne faut pas créer de commandes séparées.

### Dépendance à la piste voisine

`NEI` et `!NEI` sont possibles, mais il faut définir leur sens lorsque les pistes utilisent des grooves différents. Le moteur traite les pistes de gauche à droite, donc la piste N-1 est disponible, mais elle n'est pas forcément sur la même row. La définition recommandée est « dernier trig conditionnel évalué sur N-1 pendant le tick courant » ; Track 1 reçoit toujours faux. Sans cette règle, le résultat dépendrait visuellement du désalignement des pistes.

### Modes de parcours

Forward, backward, ping-pong et random touchent le déplacement du playhead, les boucles, `HOP`, les chaînes et la fin de morceau. Ils sont nettement plus risqués que les conditions de trig et ne doivent pas être mélangés au premier lot. Random exige aussi une définition de la fin d'un cycle.

### Fill

`FILL` n'a pas encore de source : bouton maintenu, latch global ou état sauvegardé. Il faut choisir son contrôle avant de définir le FX.

Verdict : commencer, si cette famille est retenue, par Probability + Modulo + `1ST` + `PRE`. Garder `NEI`, Fill et les modes de parcours pour une seconde étape.

## Mesures CPU du 9 août

- Projet vide : 37 %
- Une voix Braids : 41 %
- Huit voix Braids : 47 %

Le profil indiquait que les huit émulateurs AY étaient rendus même pour des pistes vides, Braids ou Sample. Ils sont désormais ignorés lorsqu'aucun instrument AY ne les utilise. Il faut refaire les trois mesures sur RG353V avant d'estimer le budget disponible pour Plaits et les sends.

## Correctifs issus des tests

- Nouveau projet : la table linéaire est maintenant réellement initialisée en cents.
- Braids : correction de l'octave et du fine tuning lorsque Linear Pitch est désactivé.
- Sample : acceptation des WAV PCM 8 ou 16 bits, mono ou stéréo.
- Sample : les erreurs de chargement restent affichées trois fois plus longtemps.
- Mixer : ajout de gardes contre les index de cellule invalides. Le crash signalé n'est pas considéré comme résolu sans reproduction ou log.

## Idée ultérieure : hôte de firmwares Versio

Un fichier `.bin` Versio est une application complète compilée pour le STM32H7/Daisy Seed. Il ne peut pas être chargé comme une bibliothèque ARM64 ou Windows : il attend le démarrage du microcontrôleur, ses interruptions, DMA, codec, ADC, GPIO et mémoire.

Deux approches sont distinctes :

- **Couche matérielle hôte** : réimplémenter l'API Versio (audio stéréo, sept potentiomètres, deux switches, bouton, CV/gates et mémoire), puis recompiler chaque firmware open source nativement. C'est le chemin raisonnable, sous réserve d'auditer la licence de chaque firmware.
- **Lecture directe des `.bin`** : émuler le Cortex-M7 et ses périphériques avec un moteur de type QEMU/Unicorn. C'est une piste de recherche lourde, fragile pour le temps réel à 96 kHz et inadaptée au socle produit actuel.

Si ChooChooTracker reçoit un système de plugins, préférer un petit ABI DSP (`init`, `setParameter`, `process`) avec plugins natifs ou WebAssembly. Le catalogue Versio mélange sources, binaires et licences différentes ; la licence MIT de l'index ne couvre pas automatiquement les firmwares listés.
