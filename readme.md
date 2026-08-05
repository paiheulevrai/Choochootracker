# Mobile Groove

Mobile Groove est le nom de travail d'un fork de [ChipNomad](https://github.com/Megus/chipnomad-tracker). L'idée est de garder son tracker inspiré de LSDJ et d'élargir sa palette sonore avec des moteurs de synthèse modernes.

Le projet vise d'abord l'Anbernic RG353V via PortMaster. Une version Windows doit rester facile à compiler pour le développement et le débogage.

Ce n'est pas un DAW et cela ne cherche pas à le devenir. C'est un petit instrument autonome pour composer en mobilité, avec un côté gadget sonore assumé.

## État actuel

La base Windows compile et démarre. Le moteur Braids est intégré au tracker avec une voix monophonique par piste, les 47 modèles accessibles, le filtre, l'ADSR, le mélange avec AY, les modulations et la sauvegarde des paramètres. La suite prioritaire est le build ARM64 et le benchmark réel sur RG353V, avant le sampler PCM 16 bits.

## Principes

- Conserver le tracker, le séquenceur et le workflow actuels de ChipNomad.
- Faire cohabiter les instruments AY/YM et les nouveaux instruments dans un même morceau.
- Enrichir la synthèse sans réécrire ce qui fonctionne déjà.
- Garder une interface adaptée à une petite console et à peu de boutons.
- Préférer une architecture simple, prévisible et facile à porter.

## Sources de référence

Les sources étudiées sont placées dans `inspirations/` :

- `chipnomad-tracker-main/` contient la base ChipNomad.
- `mutable-eurorack/` contient les sources officielles de Mutable Instruments Braids.
- Le sous-module `stmlib` requis par Braids est également présent.

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

La cible est de tenir huit voix Braids simultanées à 96 kHz sur une Anbernic RG353V. Les tests mesureront également trois, six et douze voix afin de connaître la marge réelle. La cible de huit voix ne sera considérée comme validée qu'après un test sur la console.

Les voix silencieuses ne doivent pas consommer de temps DSP inutilement.

## Samples modernes

L'instrument `AYSample` actuel reste disponible pour les sons volontairement chiptune et pour la compatibilité avec les anciens projets.

Il ne convient pas à la philosophie de Mobile Groove : l'import actuel convertit les WAV en mono unsigned 8 bits, limite les données à 16 384 échantillons, puis les joue à travers le DAC 4 bits de l'AY.

Un instrument `Sample` séparé assurera une lecture propre :

- fichiers WAV externes stockés dans le dossier `samples/` du projet ;
- PCM signé 16 bits ;
- mono ou stéréo, avec conservation de la stéréo d'origine ;
- fréquence d'échantillonnage source conservée ;
- lecture directe dans le mixeur flottant, sans passer par l'AY ;
- one-shot et boucle avec points de début et de fin ;
- transposition utile sur environ une à deux octaves dans chaque direction ;
- interpolation linéaire simple ;
- filtre et enveloppe du moteur moderne lorsque cela est pertinent.

Lors d'un import, l'application copie le WAV dans `samples/` et enregistre un chemin relatif. Les échantillons sont chargés en RAM à l'ouverture du projet. Nous ne prévoyons pas de streaming depuis la carte SD, car le moteur vise les drums, les one-shots et les boucles courtes.

Si un fichier manque, le projet reste chargeable. La piste concernée reste silencieuse et l'interface affiche un avertissement.

Les WAV ne seront pas encodés à l'intérieur du fichier projet. Cela garde les projets lisibles et évite de gonfler le format `.cnm` avec de grosses données audio.

## Format audio

- Moteur audio visé : 96 kHz.
- Mixage interne : flottant stéréo.
- Samples : PCM 16 bits mono ou stéréo.
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

Le packaging PortMaster existant dans ChipNomad fournit déjà une bonne base, mais son Makefile se décrit encore comme un placeholder. Il faudra le fiabiliser et vérifier que le package respecte la structure PortMaster actuelle.

## Plan de travail

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
- Ajouter la sauvegarde et le chargement sans casser les anciens projets.
- Ajouter l'écran instrument Braids avec tous les modèles.
- Exposer les destinations de modulation utiles : volume, pitch, timbre, color, cutoff et résonance.

### 5. Porter et mesurer sur RG353V

- Produire le binaire ARM64 sous WSL2.
- Préparer le package PortMaster.
- Tester les contrôles, la sortie audio et la stabilité sur la console.
- Mesurer trois, six, huit et douze voix à 96 kHz.
- Optimiser seulement ce que le benchmark identifie comme coûteux.

### 6. Ajouter le sampler moderne

- Lire les WAV PCM 16 bits sans conversion destructive.
- Conserver le mono ou la stéréo du fichier.
- Copier les fichiers dans `samples/` et stocker un chemin relatif.
- Précharger les samples courts en RAM.
- Ajouter one-shot, boucle, start, end et transposition.
- Gérer proprement les fichiers absents.

### 7. Stabiliser

- Vérifier l'export WAV 16 bits.
- Tester les projets hybrides AY, Braids et Sample.
- Ajouter les licences des sources et bibliothèques compilées au package PortMaster.
- Produire un premier package installable sur RG353V.

## Critères de réussite de la première version

- Le tracker ChipNomad reste utilisable sans régression majeure.
- AY et Braids jouent ensemble dans le même morceau.
- Tous les modèles Braids sont accessibles.
- Le filtre et l'ADSR fonctionnent sans clics audibles.
- Huit voix Braids tiennent à 96 kHz sur RG353V.
- Le projet compile nativement sous Windows.
- Un package PortMaster peut être installé et lancé sur la console.
- Les anciens projets ChipNomad restent chargeables.

Le sampler PCM 16 bits fait partie de l'étape suivante. Son architecture est déjà décidée afin de ne pas enfermer le format projet dans le pipeline 8 bits de `AYSample`.

## Hors périmètre

- pistes audio longues ;
- streaming depuis la carte SD ;
- enregistrement multipiste ;
- effets de mastering ;
- plugins ;
- automation de type DAW ;
- compatibilité avec des microcontrôleurs, DSP dédiés ou modules Eurorack.

Mobile Groove reste un tracker portable. La synthèse est plus riche et les samples sont propres, mais l'application doit rester immédiate et amusante.
