# Maintenir le fork PatchNomad

PatchNomad part de ChipNomad, mais les deux projets ne poursuivent plus exactement le même objectif. ChipNomad va migrer une grande partie de son code vers des classes C++ et refaire son moteur d'interface. PatchNomad ajoute de son côté Braids, puis probablement un sampler PCM, avec une cible matérielle précise sous PortMaster.

Il ne faut donc pas chercher à garder les deux dépôts identiques. Cette stratégie produirait des conflits fréquents et laisserait l'architecture de PatchNomad dépendre des décisions prises pour un autre projet. ChipNomad reste une source utile de correctifs et d'idées. PatchNomad possède désormais son code, son format de projet et son calendrier.

## Ce que le fork conserve

Les limites suivantes permettent de savoir qui possède quoi.

| Zone | Politique |
| --- | --- |
| `chipnomad_lib/external/mutable/` | Snapshot figé aux commits documentés. Pas de mise à jour périodique. |
| `chipnomad_lib/synth/BraidsVoice` | Code PatchNomad. C'est la frontière entre Braids et le reste du moteur. |
| Moteur audio, instruments et format `.cnm` | Code PatchNomad, même si une partie vient historiquement de ChipNomad. |
| Interface et séquenceur | Examiner les changements de ChipNomad, puis reprendre seulement ceux qui sont utiles. |
| SDL2, entrées, fichiers et cibles de compilation | Bons candidats aux correctifs venus de ChipNomad, à condition de les tester sur Windows et PortMaster. |
| Packaging PortMaster | Code PatchNomad. Les changements du standard PortMaster priment sur les habitudes historiques de ChipNomad. |

Cette séparation évite de mélanger le DSP Mutable, qui est stable, avec les parties encore mouvantes du tracker.

## Relier le dépôt amont

Le dépôt PatchNomad doit avoir deux remotes : `origin` pour le fork public et `upstream` pour ChipNomad.

```sh
git remote add upstream https://github.com/Megus/chipnomad-tracker.git
git fetch upstream
git remote -v
```

Si `upstream` existe déjà, il suffit de lancer `git fetch upstream`. Ne jamais fusionner `upstream/main` directement dans `main` par habitude.

Le commit ChipNomad utilisé au départ doit être inscrit dans le registre de synchronisation situé à la fin de ce document. S'il existe un commit local qui correspond exactement à cette base, on peut aussi le marquer avec un tag :

```sh
git tag chipnomad-base-YYYYMMDD <commit-local>
```

Si l'import initial contient déjà des modifications PatchNomad, ne pas fabriquer un faux tag de base. Le SHA amont dans le registre suffit.

## Examiner les changements de ChipNomad

Une revue mensuelle ou avant une release est suffisante. Suivre chaque commit en temps réel n'apporte rien au projet.

```sh
git fetch upstream
git log --oneline --decorate <dernier-sha-amont>..upstream/main
git diff --stat <dernier-sha-amont>..upstream/main
```

Classer les changements avant d'écrire du code :

1. Les corrections de crash, de corruption de projet, d'audio ou de portabilité méritent une revue immédiate.
2. Les changements du format `.cnm` doivent être étudiés, même s'ils ne sont pas repris, afin de connaître les incompatibilités futures.
3. Les améliorations d'interface peuvent être réimplémentées si elles conviennent au petit écran et aux contrôles de PatchNomad.
4. Les refontes C++ ne sont pas reprises uniquement pour rester proche de l'amont. Elles doivent résoudre un problème rencontré dans PatchNomad.
5. Les renommages, déplacements de fichiers et nettoyages sans effet visible sont généralement ignorés.

Un commit ignoré n'est pas une dette. Il répond souvent à une contrainte propre à ChipNomad.

## Importer un correctif

Chaque lot de synchronisation vit dans une branche courte créée depuis un `main` propre et fonctionnel.

```sh
git switch main
git switch -c sync/chipnomad-YYYYMMDD
git cherry-pick -x <sha-amont>
```

L'option `-x` conserve l'origine du commit dans le message. Pour un petit correctif qui s'applique proprement, le cherry-pick est préférable.

Quand le code amont a été réécrit autour de nouvelles classes et que le cherry-pick traîne toute cette architecture avec lui, reprendre seulement le comportement corrigé. Le message du commit local doit alors citer le SHA amont et expliquer en une phrase pourquoi le correctif a été adapté.

Exemple :

```text
fix: preserve empty project titles when loading

Adapted from ChipNomad <sha>. The upstream implementation depends on the new
Project class, so this keeps the fix in PatchNomad's current parser.
```

Une synchronisation ne doit pas mélanger un correctif amont et une nouvelle fonctionnalité PatchNomad. Deux changements séparés sont plus simples à tester et à annuler.

## Résoudre les conflits

La résolution doit préserver le comportement de PatchNomad, pas reproduire la forme du nouveau code ChipNomad.

Quelques zones demandent une revue manuelle :

- allocation et destruction de `ChipNomadState` et des objets `BraidsVoice` ;
- thread audio SDL et données partagées avec l'interface ;
- routage entre instruments AY, Braids et futurs samples ;
- chargement et sauvegarde des projets ;
- conversion des événements clavier et manette ;
- chemins de données sous Windows, Linux et PortMaster.

Ne pas accepter automatiquement un côté entier du conflit dans ces fichiers. Lire le flux complet, vérifier les appelants, puis faire le plus petit changement qui conserve les deux comportements nécessaires.

Si un correctif amont exige plusieurs jours de migration C++, fermer la branche de synchronisation et ouvrir une tâche distincte. Une urgence de synchronisation ne doit pas décider de l'architecture du fork.

## Protéger le format de projet

Le format `.cnm` est le contrat le plus important. Une refonte interne ne justifie pas de casser les morceaux existants.

Les règles sont les suivantes :

- les anciens projets ChipNomad doivent continuer à se charger ;
- un champ absent reçoit une valeur par défaut sûre ;
- un nouveau champ PatchNomad doit être ignoré proprement par les anciennes versions lorsque le format texte le permet ;
- une modification incompatible exige une nouvelle version de format et un chemin de migration ;
- les fichiers de test des anciennes versions restent dans le dépôt ;
- charger puis sauvegarder un projet ne doit pas supprimer des données inconnues sans avertissement.

Si ChipNomad change son format, ajouter au moins un exemple amont récent aux tests avant de prétendre à la compatibilité. PatchNomad n'a pas besoin d'écrire exactement le même format que toutes les futures versions de ChipNomad, mais il doit annoncer clairement la limite.

## Garder Mutable Instruments figé

Braids et les fichiers stmlib utilisés par PatchNomad sont des snapshots, pas des dépendances vivantes. Leurs commits sont déjà épinglés dans le README.

On ne modifie pas directement `external/mutable` pour adapter le tracker. Les adaptations vivent dans `BraidsVoice` ou dans le mixeur PatchNomad. Cette règle garde le DSP d'origine reconnaissable et simplifie les comparaisons avec la source.

Une modification du snapshot ne se justifie que pour :

- corriger un bug DSP reproduit par un test ;
- réparer un problème de compilation sur une cible prise en charge ;
- retirer du code inutilisé après avoir vérifié les licences et les symboles liés.

Dans ce cas, isoler le patch dans un commit dédié, noter le fichier d'origine et ajouter un test. Il n'y a pas de raison de surveiller Mutable Instruments à chaque synchronisation ChipNomad.

## Validation minimale

Avant une synchronisation, les tests doivent passer sur `main`. Après l'import, exécuter au minimum depuis `tracker` :

```sh
make test
make windows
```

Un changement limité au packaging PortMaster ne demande pas de refaire tous les tests DSP, mais le ZIP final doit être construit et vérifié :

```sh
make -f Makefile.portmaster PortMaster-deploy
unzip -t ../releases/patchnomad.zip
```

Sur RG353V, vérifier les contrôles, la lecture, l'arrêt, la sauvegarde et la sortie du programme. Un changement audio doit aussi être testé avec un projet AY, un projet Braids et un projet hybride. Les mesures de charge se font sur la console, pas seulement sur le PC.

Tout bug amont importé doit laisser derrière lui un petit test qui échoue sans le correctif. Les changements triviaux de documentation ou de packaging n'en ont pas besoin.

## Publier la synchronisation

Une fois les tests terminés :

1. mettre à jour le registre ci-dessous ;
2. relire le diff sans tenir compte de l'intention du commit amont ;
3. fusionner la branche dans `main` sans réécrire l'historique public ;
4. supprimer la branche de synchronisation ;
5. garder le SHA amont dans les commits adaptés ou cherry-pickés.

Ne jamais rebaser le `main` public de PatchNomad sur ChipNomad. Après plusieurs releases, les deux historiques auront trop divergé et le rebase rendrait les contributions et les rapports de bugs difficiles à suivre.

## Registre de synchronisation

Ajouter une ligne à chaque revue, y compris lorsqu'aucun commit n'est repris. Cela évite de relire plusieurs fois le même intervalle.

| Date | SHA ChipNomad examiné | Commits repris | Décision |
| --- | --- | --- | --- |
| YYYY-MM-DD | `<sha>` | aucun ou liste des SHA | Base initiale, correctifs repris, ou changements ignorés avec motif court |

Le dernier SHA examiné devient le point de départ de la prochaine revue. Une ligne peut simplement dire que la refonte C++ n'apporte pas encore de correctif nécessaire à PatchNomad.

## Quand cesser de synchroniser

À mesure que les architectures divergent, la revue commit par commit finira par coûter plus cher qu'elle ne rapporte. À ce stade, consulter les releases et les rapports de bugs de ChipNomad suffit. Une bonne idée peut toujours être réimplémentée localement sans tenter de rapprocher les arborescences.

Le fork reste en bonne santé si ses projets sont lisibles, ses tests passent et ses utilisateurs peuvent migrer depuis la base ChipNomad annoncée. Le nombre de commits partagés avec l'amont n'est pas un indicateur utile.
