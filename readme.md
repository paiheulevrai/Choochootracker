#TODO

Ajouter le support single waveform (un sample très court égal à un cycle d'onde, qui loope pour faire un pitch suivant la hauteur de note donnée). Cela peut être un nouveau moteur ou bien un ajout au moteur Sample, suivant ce qui est le plus simple.

# ChooChooTracker

ChooChooTracker est un fork de [ChipNomad](https://github.com/Megus/chipnomad-tracker). L'idée est de garder son tracker inspiré de LSDJ et d'élargir sa palette sonore avec des moteurs de synthèse modernes. Son nom rappelle que le premier proof of concept a été réalisé dans le train entre Cahors et Montauban.

Le projet vise d'abord l'Anbernic RG353V via PortMaster. Une version Windows doit rester facile à compiler pour le développement et le débogage.

Ce n'est pas un DAW et cela ne cherche pas à le devenir. C'est un petit instrument autonome pour composer en mobilité, avec un côté gadget sonore assumé.

## État actuel

La base Windows compile et démarre. Braids (47 modèles), Plaits (24 engines) et le sampler PCM cohabitent avec AY sur huit pistes fixes. Le mixer possède volume, mute, solo et sends par piste vers une Clouds Reverb et un delay ping-pong synchronisé en ticks. Les FX `PRO`, `MOD` et `SPD` ajoutent conditions et vitesse par piste. Le binaire ARM64 et le package PortMaster sont produits sous WSL2. Le manuel utilisateur anglais se trouve dans [docs/USER_MANUAL.md](docs/USER_MANUAL.md).

## Principes

- Conserver le tracker, le séquenceur et le workflow actuels de ChipNomad.
- Faire cohabiter les instruments AY/YM et les nouveaux instruments dans un même morceau.
- Enrichir la synthèse sans réécrire ce qui fonctionne déjà.
- Garder une interface adaptée à une petite console et à peu de boutons.
- Préférer une architecture simple, prévisible et facile à porter.

## Sources de référence

Les sources étudiées sont placées dans `inspirations/` :

- `chipnomad-tracker-main/` contient la base ChipNomad.
- `mutable-eurorack/` contient les sources officielles de Mutable Instruments Braids, Plaits, Clouds et stmlib.
- `mutable-instruments-documentation-main/` contient les manuels Mutable utilisés comme référence pour le manuel ChooChooTracker.

Braids est actuellement épinglé au commit `08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4` et `stmlib` au commit `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.

Les dossiers d'inspiration servent de référence. Le fork de travail devra vivre séparément afin que les modifications du projet ne se mélangent pas aux sources amont.

## Ce que ChipNomad fournit déjà

ChipNomad sépare correctement le séquenceur de la génération audio. Le séquenceur avance à chaque tick, puis `chipnomadRender()` demande aux moteurs de produire des blocs audio qui sont mélangés dans un buffer stéréo flottant.

La version étudiée possède déjà :

- des instruments AY1, AY2 et AYSample ;
- quatre slots de modulation génériques par instrument ;
- des modulations ADSR, AHD et LFO ;
- l'import de fichiers WAV ;
- l'export WAV ;
- des cibles Windows, Linux et PortMaster basées sur SDL2.

On réutilisera le système de modulation existant. L'amplitude des moteurs modernes devra néanmoins être calculée ou lissée à la fréquence audio pour éviter les clics et les valeurs en escalier.

## Moteur Braids

Braids est ajouté comme un seul type d'instrument. Le paramètre `MODEL` choisit l'un des modèles du `MacroOscillator`. Nous ne créons pas un type d'instrument différent pour chaque modèle.

Les 47 modèles accessibles de Braids, numérotés de 0 à 46, sont exposés. Ils partagent la même structure de paramètres :

- `MODEL`
- `TIMBRE`
- `COLOR`
- hauteur de note
- déclenchement `STRIKE` lorsque le modèle l'utilise

Une piste est monophonique et possède sa propre instance de Braids, comme une voix matérielle du module original. AY et Braids peuvent être utilisés sur des pistes différentes dans le même projet.

### Chaîne sonore

Pour un modèle tonal :

```text
Braids -> filtre -> amplificateur ADSR -> mixeur
```

Pour un modèle percussif :

```text
Braids avec son comportement STRIKE d'origine -> filtre -> mixeur
```

Les modèles percussifs gardent leur enveloppe et leur décroissance internes. L'interface ne leur impose pas d'ADSR supplémentaire.

### Filtre

Chaque voix Braids possède un filtre numérique avec :

- pente 12 ou 24 dB/octave ;
- mode low-pass, band-pass ou high-pass ;
- cutoff réglable ;
- résonance réglable.

L'implémentation utilise un filtre state-variable 12 dB. Deux étages en cascade fournissent le mode 24 dB.

### Fréquence audio et polyphonie

Braids et ses tables d'origine sont prévus pour fonctionner à 96 kHz. La première version utilisera donc 96 kHz pour éviter de modifier les tables ou d'ajouter un rééchantillonnage interne complexe.

La cible est de tenir huit voix Braids simultanées à 96 kHz sur une Anbernic RG353V. Les tests mesureront également trois et six voix afin de connaître la marge réelle. La cible de huit voix ne sera considérée comme validée qu'après un test sur la console.

Les voix silencieuses ne doivent pas consommer de temps DSP inutilement.

## Samples modernes

L'instrument `AYSample` actuel reste disponible pour les sons volontairement chiptune.

Il ne convient pas à la philosophie de ChooChooTracker : l'import actuel convertit les WAV en mono unsigned 8 bits, limite les données à 16 384 échantillons, puis les joue à travers le DAC 4 bits de l'AY.

Un instrument `Sample` séparé assure une lecture propre :

- fichiers WAV externes stockés dans le dossier `samples/` du projet ;
- WAV PCM 8 ou 16 bits, convertis en PCM signé 16 bits en mémoire ;
- mono ou stéréo, avec conservation de la stéréo d'origine ;
- fréquence d'échantillonnage source conservée ;
- lecture directe dans le mixeur flottant, sans passer par l'AY ;
- one-shot avec points de début et de fin ;
- transposition utile sur environ une à deux octaves dans chaque direction ;
- interpolation linéaire simple ;
- filtre et enveloppe du moteur moderne lorsque cela est pertinent.

La première implémentation charge le WAV en RAM et conserve son chemin dans le projet. La copie dans `samples/` avec un chemin relatif reste à faire avant de considérer le format portable. Nous ne prévoyons pas de streaming depuis la carte SD, car le moteur vise les drums et les one-shots courts.

Si un fichier manque, le projet reste chargeable. La piste concernée reste silencieuse et l'interface affiche un avertissement.

Les WAV ne seront pas encodés à l'intérieur du fichier projet. Cela garde les projets lisibles et évite de gonfler le format `.cct` avec de grosses données audio.

## Format audio

- Moteur audio visé : 96 kHz.
- Mixage interne : flottant stéréo.
- Samples importés : PCM 8 ou 16 bits, mono ou stéréo.
- Export final : WAV stéréo 16 bits.

L'objectif est un son propre et moderne sur une console portable, pas une chaîne de mastering professionnelle.

## Compilation

### Développement sous Windows

Le développement quotidien doit fonctionner nativement sous Windows avec :

- MSYS2 ;
- MinGW-w64 ;
- SDL2 ;
- les Makefiles existants tant qu'ils suffisent.

Cette voie fournit rapidement un exécutable Windows pour tester l'interface, le séquenceur et l'audio. Il n'est pas prévu de remplacer immédiatement les Makefiles par CMake.

### PortMaster

La RG353V exécute un binaire Linux ARM64. La compilation PortMaster passera par WSL2 avec Ubuntu, une toolchain AArch64 et les bibliothèques SDL2 adaptées.

Le workflow visé est :

```text
édition sous Windows
        |
build et tests Windows natifs
        |
cross-compilation ARM64 sous WSL2
        |
copie du package par SSH ou carte SD
        |
test et benchmark sur RG353V
```

Docker n'est pas nécessaire pour travailler au quotidien. Il pourra être ajouté plus tard si nous avons besoin de builds de release reproductibles ou d'une CI identique sur plusieurs machines.

Le packaging PortMaster a été adapté au nom ChooChooTracker, au binaire ARM64 et au format `.cct`. Le ZIP final est généré par `make -f Makefile.portmaster PortMaster-deploy` puis vérifié avant les tests ArkOS.

## Plan de travail

L'[étude de faisabilité du 9 août 2026](docs/feasibility-2026-08-09.md) reste l'analyse initiale. Plaits, les sends et le premier lot de conditions ont depuis été implémentés ; leur validation musicale et CPU sur RG353V reste à faire.

### 1. Établir la base

- Créer le vrai fork de travail de ChipNomad.
- Compiler et lancer ChipNomad sous Windows sans modification fonctionnelle.
- Lancer les tests existants.
- Documenter les versions de SDL2 et de la toolchain utilisées.

### 2. Valider Braids seul

- Compiler le programme de test desktop fourni avec Braids.
- Produire un WAV à 96 kHz.
- Créer une petite classe `BraidsVoice` indépendante du tracker.
- Vérifier la hauteur, `TIMBRE`, `COLOR`, `MODEL` et `STRIKE`.
- Parcourir tous les modèles et vérifier qu'ils produisent du son sans plantage.

### 3. Ajouter la chaîne moderne

- Ajouter le filtre 12 et 24 dB.
- Ajouter l'ADSR à fréquence audio pour les modèles tonals.
- Ajouter un gain de sécurité et protéger le mixeur contre les dépassements.
- Tester les changements de paramètres en temps réel sans clics.

### 4. Intégrer Braids à ChipNomad

- Ajouter `InstrumentType::Braids` et ses données.
- Ajouter une voix par piste.
- Brancher les événements note-on, note-off et retrigger.
- Mélanger les voix modernes avec les puces AY existantes.
- Ajouter la sauvegarde et le chargement du format ChooChooTracker `.cct` à huit pistes.
- Ajouter l'écran instrument Braids avec tous les modèles.
- Exposer les destinations de modulation utiles : volume, pitch, timbre, color, cutoff et résonance.

### 5. Porter et mesurer sur RG353V

- [x] Produire le binaire ARM64 sous WSL2.
- [x] Préparer le package PortMaster.
- Tester les contrôles, la sortie audio et la stabilité sur la console.
- Mesurer trois, six et huit voix à 96 kHz.
- Optimiser seulement ce que le benchmark identifie comme coûteux.

### 6. Qualité de vie et architecture huit pistes

- [x] Utiliser huit pistes fixes.
- [x] Donner une instance AY indépendante à chaque piste.
- [x] Ajouter un mixer avec volume, mute et solo par piste.
- [x] Sauvegarder le volume de chaque piste dans le projet.
- [x] Ignorer le repeat SDL et utiliser un repeat interne déterministe.
- [x] Conserver les directions maintenues pendant les combinaisons de touches.
- [x] Valider sur RG353V qu'aucun appui rapide n'est perdu.
- [x] Valider le délai et la vitesse du repeat sur le matériel.
- [x] Corriger le crash à l'ouverture du mixer.
- [x] Ajouter un CPU meter mesurant la charge du callback audio.

### 7. Revoir la navigation et les FX

- [x] Sortir le mixer du menu Project.
- [x] Ajouter le mixer comme écran principal tout à gauche : `MSCPIT`.
- [x] Permettre le passage direct entre Mixer et Song.
- [x] Séparer les FX universels des FX propres à AY, Braids et Sample.
- [x] Ne proposer que les FX compatibles avec l'instrument actif.
- [x] Ajouter des FX Braids par step pour model, timbre, color, cutoff et résonance.
- [x] Ajouter des FX Sample par step pour pitch, start, end, volume, cutoff et résonance.
- [x] Réutiliser les trois colonnes FX existantes.
- [x] Recharger les valeurs de base de l'instrument au trig suivant ; un FX instrument reste actif jusqu'au prochain trig.

### 8. Ajouter le sampler moderne

- [x] Lire les WAV PCM 8 ou 16 bits ; convertir le 8 bits en PCM16 en mémoire.
- [x] Conserver le mono ou la stéréo du fichier.
- Copier les fichiers dans `samples/` et stocker un chemin relatif.
- [x] Précharger les samples courts en RAM.
- [x] Ajouter one-shot, start, end, volume et transposition.
- [x] Ajouter ADSR et filtre LP/BP/HP 12/24 dB.
- Ajouter le bouclage dans une étape ultérieure si les tests musicaux le justifient.
- [x] Laisser le projet chargeable lorsqu'un fichier est absent.

### 9. Stabiliser

- Vérifier l'export WAV 16 bits.
- Tester les projets hybrides AY, Braids et Sample.
- Ajouter les licences des sources et bibliothèques compilées au package PortMaster.
- Produire un premier package installable sur RG353V.

### 10. Plaits, sends et conditions

- [x] Intégrer les 24 engines Plaits avec sortie Main/Aux, filtre, ADSR et sauvegarde.
- [x] Ajouter les FX Plaits `PMD`, `PHA`, `PTM`, `PMO`, `PAX`, `PCF` et `PRS`.
- [x] Ajouter les sends Reverb et Delay par piste au mixer.
- [x] Intégrer la reverb de Clouds comme effet send global.
- [x] Ajouter un delay ping-pong global synchronisé en ticks avec feedback et filtre.
- [x] Ajouter `PRO 00-64`, `MOD AB` et `SPD 00-10`.
- [x] Rendre `SPD` persistant jusqu'au prochain `SPD`.
- [x] Afficher les sous-écrans Reverb et Delay autour du `M` dans la carte.
- [x] Rédiger le manuel utilisateur anglais.
- [ ] Mesurer huit Plaits, Reverb et Delay sur RG353V.
- [ ] Valider à l'oreille les 24 engines, les sends et tous les ratios `SPD` sur ArkOS.

## Critères de réussite de la première version

- Le tracker ChooChooTracker reste utilisable sans régression majeure.
- AY et Braids jouent ensemble dans le même morceau.
- Tous les modèles Braids sont accessibles.
- Le filtre et l'ADSR fonctionnent sans clics audibles.
- Huit voix Braids tiennent à 96 kHz sur RG353V.
- Le projet compile nativement sous Windows.
- Un package PortMaster peut être installé et lancé sur la console.
- Le format de projet ChooChooTracker `.cct` sauvegarde les niveaux des huit pistes.
- Le mixer est un écran principal accessible sans passer par Project.
- Les instruments Braids et Sample acceptent des FX spécifiques par step.

Le sampler PCM 8/16 bits est intégré. Il reste à rendre ses chemins portables en copiant les WAV dans le dossier `samples/` du projet, puis à le valider sur la console.

## Hors périmètre

- pistes audio longues ;
- streaming depuis la carte SD ;
- enregistrement multipiste ;
- effets de mastering ;
- plugins ;
- automation de type DAW ;
- compatibilité avec des microcontrôleurs, DSP dédiés ou modules Eurorack.

ChooChooTracker reste un tracker portable. La synthèse est plus riche et les samples sont propres, mais l'application doit rester immédiate et amusante. L'écran titre et l'identité visuelle seront intégrés après la stabilisation fonctionnelle.
