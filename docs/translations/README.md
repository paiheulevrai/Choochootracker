# User manual translations

Translations of [`../USER_MANUAL.md`](../USER_MANUAL.md). The English manual is
the reference: when a translation and the English text disagree, the English
text is right.

| Language | File | Translated from |
| --- | --- | --- |
| Français | [`USER_MANUAL.fr.md`](USER_MANUAL.fr.md) | Current English manual (machine-generated) |
| Русский | [`USER_MANUAL.ru.md`](USER_MANUAL.ru.md) | Current English manual (machine-generated) |
| Español | [`USER_MANUAL.es.md`](USER_MANUAL.es.md) | Current English manual (machine-generated) |
| Deutsch | [`USER_MANUAL.de.md`](USER_MANUAL.de.md) | Current English manual (machine-generated) |
| Italiano | [`USER_MANUAL.it.md`](USER_MANUAL.it.md) | Current English manual (machine-generated) |
| 日本語 | [`USER_MANUAL.ja.md`](USER_MANUAL.ja.md) | Current English manual (machine-generated) |
| 中文（简体） | [`USER_MANUAL.zh.md`](USER_MANUAL.zh.md) | Current English manual (machine-generated) |
| Nederlands | [`USER_MANUAL.nl.md`](USER_MANUAL.nl.md) | Current English manual (machine-generated) |
| Polski | [`USER_MANUAL.pl.md`](USER_MANUAL.pl.md) | Current English manual (machine-generated) |
| Português (Brasil) | [`USER_MANUAL.pt.md`](USER_MANUAL.pt.md) | Current English manual (machine-generated) |

## Conventions

- The app interface is in English, so screen, menu, field and button names
  (Song, Chain, Phrase, Instrument, Table, Mixer, **Settings**, **EDIT**,
  **OPT**, **SELECT**...), FX codes (`HOP`, `TBX`, `M1A`...), module and engine
  names (ADSR, LFO, Braids, Plaits...) and file extensions are not translated.
- The translations follow the English manual's Markdown structure. The exact
  wording may be adapted for the target language.
- No em dash: use a plain `-`.

## Keeping a translation current

The files are generated from the current English manual. After changing
`docs/USER_MANUAL.md`, regenerate them with:

```sh
python tools/translate_manuals.py
```

The script requires `OPENAI_API_KEY` and checkpoints each translated fragment
in `.tmp/manual-translation/` so an interrupted run can resume without
translating completed fragments again.
