# Mesure de niveau des moteurs

Depuis `tracker` dans MSYS2 UCRT64, lancez `make -f Makefile.test measurements -j4`. Cela crée, sans les ajouter à Git, une seconde de WAV pour chaque modèle Braids, moteur Plaits/Plaits-Alt, 30 réglages de macro et deux notes. Plaits et Plaits-Alt sont rendus dans leur routage natif `TRIG/LPG`, avec decay et LPG colour à `255`; les percussions utilisent 250 ms. `pcm/` contient la référence : un échantillon PCM sinusoïdal à 90 %.

Pour diagnostiquer séparément le routage VCA, utilisez `build/tests/render_engine_measurements.exe plaits-vca` ou `plaits-alt-vca`. Ne mélangez jamais ces WAVs à la calibration native.

Puis, à la racine :

```powershell
python scripts/measure_amplitude.py
```

Les résultats sont `results/amplitude_metrics.csv` et `results/compensation_gains.json`. Le gain proposé aligne la médiane RMS (hors 100 ms d'attaque et de fin) sur PCM, puis le plafonne pour préserver 1 dB de marge sur le 95e percentile des crêtes. Les moteurs percussifs sont inclus dans le diagnostic, mais un gain global par famille ne les rendra pas tous identiques : vérifier les WAV avant d'appliquer une constante de production.
