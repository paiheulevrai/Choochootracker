# Audit de dépôt — Rapport complet

Date: 2026-08-12T02:50:57+02:00
Auteur: Assistant IA (Copilot CLI runtime in VS Code)

Résumé exécutif
---------------
Objectif : fournir un audit complet et multi-axes du dépôt (performance temps réel, robustesse mémoire, concurrence, maintenabilité, tests/CI, licences et portabilité). Le rapport identifie risques, priorités et actions recommandées, avec des références vers les fichiers concernés.

Contexte et portée
------------------
- Cible : code C/C++ du moteur audio (chipnomad_lib), gestion des voices (Braids, Plaits, Sample), la couche AY, les managers audio de l'UI et le code d'import/export. Le dépôt contient aussi du code web généré (`web/dist`) et des outils externes vendorisés (Mutable, stmlib).
- Méthode : recherches automatiques (grep) pour patterns, puis revue ciblée des fichiers identifiés.

Principales conclusions (résumé)
--------------------------------
- Le principal risque runtime identifié est l'allocation/réallocation dans les chemins audio ou proches (mix buffers, audio manager). Une correction atomique a été appliquée pour un cas dans `chipnomad_lib.cpp` pendant l'audit.
- Le filtre multimode est correctement centralisé — duplication antérieure corrigée.
- Peu d'usage de mutex/locks dans le runtime (bon) ; on utilise au moins une std::atomic pour le CPU meter. Risque de contention bas, mais attention aux allocations/IO en threads audio.
- Plusieurs appels à fonctions C-style potentiellement dangereux (strcpy, memcpy, sprintf) apparaissent dans le code I/O et parsers — la plupart sont encadrés par snprintf/length checks, mais quelques strcpy subsistent et méritent revue.
- I/O sur disque (fopen/fread/fwrite) et opérations lourdes d'import/export existent mais semblent confinées hors du chemin audio principal ; vérifier toutefois qu'aucune n'est invoquée depuis le callback.
- Bon niveau de tests unitaires (docs mentionnent 161 cas et le repo contient un dossier `tracker/tests`), mais manque un benchmark/CI spécifique temps-réel (audio smoke test) sur hardware cible.

Détails par axe
---------------
1) Performance et sécurité temps réel
- Observations
  - Réallocs détectés dans le rendu audio : [chipnomad_lib.cpp](C:/Users/surga/Desktop/mobilegroove/chipnomad_lib/chipnomad_lib.cpp) (mix/reverb/delay buffers), [tracker/src/audio_manager.cpp] (floatBuffer/samplePreviewBuffer).
  - `chip->render()` est appelé seulement pour les chips actifs (filtre conditionnel présent), ce qui a déjà permis de réduire la charge CPU notablement.
  - Plaits est exécuté à 48 kHz et upsamplé ; il est coûteux par voix.
  - Master effects (Reverb/Delay) sont traités une fois par callback (bonne pratique).
- Risques
  - allocations/IO/locks dans ou proches du callback audio provoquent glitchs et instabilité.
- Recos
  - Pré-allouer les buffers dimensionnés pour le pire cas (ou dimensionner au démarrage selon la configuration) pour éviter realloc en callback.
  - Si realloc inévitable, faire la réallocation hors thread audio et échanger les buffers atomiquement (double-buffering + drapeaux atomiques).
  - Ajouter profils micro (per-engine) et runner de benchmark sur RG353V.

2) Robustesse mémoire et correctness
- Observations
  - Mélange d'allocations C (malloc/free) et d'allocations C++ (new/delete). Le `ChipNomadState` est `malloc`-ed (documenté) et les voix sont allouées via `new` et stockées par pointeur — la pratique est cohérente mais fragile si un futur contributeur embedra objets C++ directement dans le struct alloué par malloc.
  - Quelques usages de `strcpy` existent (ex. `project.cpp`), la plupart des écritures utilisateur utilisent `snprintf`/`strncpy` correctement.
  - `memcpy`/`memmove` sont présents dans des sections DSP (filtres, buffers) — usage normal et performant.
- Risques
  - Risque faible-moyen : mismatch free/delete si code modifié de façon incorrecte. Risque faible : buffer overflow via strcpy si input non validée.
- Recos
  - Remplacer `strcpy` par `strncpy`/`strlcpy` ou vérifier explicitement la taille avant copie.
  - Ajouter des assertions/guarantees sur la propriété mémoire (comments, wrappers) pour éviter futur mélange unsafe malloc/new.
  - Ajouter sanitisers (ASAN) comme job CI pour builds desktop/dev.

3) Concurrence et thread-safety
- Observations
  - Très peu de primitives de synchronisation : un `std::atomic<int>` pour le CPU load dans `tracker/src/audio_manager.cpp`. Doctest vendored code déclare mutex pour le runner de tests mais hors runtime.
  - Pas d'usage massif de `std::mutex`/`pthread_mutex` dans le runtime audio.
- Risques
  - Faible risque de deadlock; risque principal : opérations non-atomiques (alloc/IO) exécutées dans callback audio multitâche.
- Recos
  - Éviter allocations et I/O dans callbacks ; documenter clairement quelles fonctions sont sûres en temps réel.
  - Si partage d'état entre UI et audio, utiliser mécanismes lock-free (atomics, ring buffers) et documenter invariants.

4) I/O, parsing et sécurité des entrées
- Observations
  - Nombreuses utilisations de `fopen`/`fread`/`fwrite` dans `project_io`, `export_wav`, `import_wav`, etc. Ces opérations sont attendues hors temps réel.
  - Certains parsers utilisent `strcpy` et manquent d'oversize guards visibles à la lecture superficielle.
- Risques
  - Exposition modérée aux corruptions de fichiers d'entrée malformés et, potentiellement, to-do security issues (path traversal, format fuzzing).
- Recos
  - S'assurer que l'I/O bloquante n'est jamais invoquée depuis le thread audio.
  - Ajouter validation defensive pour les parsers (length checks, limites, error handling). Mettre en place fuzz-tests pour importers (WAV, project files).

5) Maintenabilité, duplications et architecture
- Observations
  - MultimodeFilter est centralisé (bon). Les voices sont séparées (Braids/Sample/Plaits) et la boucle de rendu passe par plusieurs loops (chips, braids, sample, plaits). C'est simple et clair mais fait plusieurs passes sur mixBuffer.
- Recos
  - Considérer une architecture par-track (faire tout pour chaque track dans une seule passe) pour réduire le nombre de passes mémoire si le profiling le justifie.
  - Documenter le contrat de `ChipNomadState` (qui alloue quoi) dans un commentaire central.

6) Tests & qualité
- Observations
  - Présence de tests unitaires sous `tracker/tests` et mention dans docs d'une suite de 161 tests.
  - Pas (ou pas trouvé) de bench audio automatisé ciblant RG353V dans le CI.
- Recos
  - Ajouter job CI: "audio-smoke" — build + run headless 30s callback with dummy SDL audio and check no crashes/underruns.
  - Ajouter benchmarks qui mesurent CPU usage pour 0..8 voices Plaits + sends.

7) Licences & dépendances
- Observations
  - Mutable Instruments code (Braids/Plaits/stmlib) vendored under MIT as documented. docs/fork-maintenance mention explicit snapshots.
- Recos
  - Vérifier la présence des fichiers LICENSE pour chaque vendor snapshot et ajouter une short README in `chipnomad_lib/external/` listant attributions et exact commits.

8) Portabilité & builds
- Observations
  - Windows/MSYS2 UCRT64, WSL2, PortMaster and WebAssembly targets described in docs. Build scripts exist (Makefile.portmaster, Makefile.web).
- Recos
  - Automatiser cross-builds in CI for x86_64 and aarch64 (PortMaster) to catch early regressions.

9) Observability & debugging
- Observations
  - `audioOverload` detection present; CPU meter atomic present. Logging stderr used in CLI players.
- Recos
  - Ajouter counters: realloc_count_in_audio, realloc_attempts_failed, per-engine CPU counters (histogram). Expose via debug UI or metrics endpoint.

Actions recommandées (priorisées)
---------------------------------
Priorité haute (1–2 jours)
- Auditer et corriger tous les cas de `ptr = realloc(ptr, ...)` sur les chemins audio : appliquer le pattern temp->realloc->check->swap (patches à faible risque).
- Pré-allocation des buffers audio globaux au démarrage et élimination des reallocs pendant les callbacks.
- Ajouter un job CI "audio-smoke" qui exécute le binaire headless 30s (ou plus) et vérifie l'absence de crash.

Priorité moyenne (1–2 semaines)
- Ajouter benchmarks automatisés pour combinaisons d'engines (Plaits engines les plus coûteux) et sends.
- Revoir les usages `strcpy`/`strncpy` et remplacer les usages non protégés.
- Documenter le modèle d'ownership de `ChipNomadState` et interdire l'embedding de C++ objects sans adaptation.

Priorité basse (2–4 semaines)
- Étudier lazy-init des SoundChip et mesurer gains mémoire/CPU.
- Consolider la boucle de mix pour réduire les passes mémoire si le profiling montre un bénéfice.
- Ajouter ASAN/UBSAN runs sur builds desktop pour attraper bugs mémoire tôt.

Pièces spécifiques à vérifier manuellement (liste de fichiers)
-----------------------------------------------------------
- C:/Users/surga/Desktop/mobilegroove/chipnomad_lib/chipnomad_lib.cpp — mix/reallocs, chip init
- C:/Users/surga/Desktop/mobilegroove/chipnomad_lib/playback_ay.cpp — timerFunction* (performant, micro-optimizations)
- C:/Users/surga/Desktop/mobilegroove/chipnomad_lib/synth/multimode_filter.cpp — vérifié (bon)
- C:/Users/surga/Desktop/mobilegroove/tracker/src/audio_manager.cpp — reallocs floatBuffer/preview
- C:/Users/surga/Desktop/mobilegroove/tracker/src/sample_utils.cpp — conversions memory
- C:/Users/surga/Desktop/mobilegroove/chipnomad_lib/project_io.cpp — parsers, file I/O
- C:/Users/surga/Desktop/mobilegroove/chipnomad_lib/chips/chip_ay.cpp — AY render implementation

Exemples de petites corrections à appliquer (illustrations)
----------------------------------------------------------
1) Pattern sûr pour realloc (existant maintenant dans chipnomad_lib.cpp) :
   float* newBuf = (float*)realloc(oldBuf, newSize);
   if (!newBuf) { handle_error(); } else { oldBuf = newBuf; }
   -> Pour éviter fuite partielle, allouer tout d'abord temporaires pour chaque buffer et vérifier.

2) Remplacer strcpy par strncpy ou safe wrapper :
   strncpy(dest, src, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0';

Checklist de validation après corrections
----------------------------------------
- [ ] Tous les reallocs dans le chemin audio ont été remplacés ou sécurisés.
- [ ] CI audio-smoke ajouté et passe 30s sans crash.
- [ ] Bench Plaits 1..8 voices ajouté et comparé avant/après modifications.
- [ ] ASAN/UBSAN runs sur builds de développement (optionnel hors release).

Notes finales et pièges connus
-----------------------------
- Le projet est un fork de ChipNomad et contient des snapshots vendorisés. La divergence historique et la politique de fork sont documentées dans `docs/fork-maintenance.md` — suivre ces règles pour éviter rebase/confusion.
- Le mix entre code C (malloc/free) et C++ (new/delete) est un design intentionnel (documenté), mais il doit rester cohérent. Tout changement sur `ChipNomadState` nécessite attention particulière.

Livrable
--------
- Ce rapport est enregistré dans `/docs/audit-report.md` (ce fichier). Si tu veux, je peux :
  - Générer une checklist GitHub Issues/PRs pour les actions prioritaires (sans appliquer les patches).
  - Produire les patches sûrs (réalloc/strcpy -> patch) dans un diff prêt à PR.

Prochaine étape ?
-----------------
- Préfères-tu que je génère les patches sûrs pour tous les reallocs trouvés dans des chemins audio (je les laisse en draft, pas de PR), ou que je produise uniquement une checklist/issue list que tu pourras assigner ?

Fin du rapport.
