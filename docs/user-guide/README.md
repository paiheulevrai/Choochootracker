# Visual user guide

[Read the guide online](https://choochootracker.vercel.app/user-guide/).

The guide covers 26 screens with 63 screenshots, 155 looping control examples,
and 92 expandable reference topics. Its contents list follows the section being
read and expands that section's topics. Screenshots open at a larger size;
underlined controls open animated examples. It supports desktop and phone
layouts, keyboard navigation, printing, and eight bundled font choices.

## Read offline

Open `index.html` in a browser with `assets/` beside it. No server, installation
or network connection is needed. GitHub's file viewer shows HTML source; use
the online link above or download the repository to read the rendered guide.

To produce a single HTML file containing the images, GIFs and fonts:

```sh
python3 docs/user-guide/build_guide.py
```

Open the generated `docs/user-guide/Choo-Choo-User-Guide.html`. This file and
any distribution ZIPs are build outputs, so they are not checked into Git.

## Edit and build

All guide-specific files live in this directory:

| File or folder | Purpose |
| --- | --- |
| `index.html` | Checked-in browser entry point |
| `build_guide.py` | Screen summaries, controls, layout, CSS and interactions |
| `reference_content.py` | Reference rendering, media placement and topic navigation |
| `reference/sections.json` | Detailed topics, with Markdown and rendered HTML |
| `reference/new-controls.json` | Additional controls and animated examples |
| `examples.json` | GIF metadata and control associations |
| `cloning.html`, `cloning.css` | Three-tab cloning explanation |
| `assets/` | Screenshots, GIFs, fonts and their licence notices |
| `tools/prepare_reference.py` | Editorial mapping from the pinned manual into topics |
| `tools/check-*.cjs` | Optional browser checks |

Run the build command after editing, then commit the source changes and
regenerated `index.html`. Building uses Python 3.9 or later and its standard
library; it does not download anything or require the tracker to be built.

For detailed content changes, edit the source snapshot or editorial mappings in
`tools/prepare_reference.py`, regenerate the topics, then build. Regenerating
topics replaces `reference/sections.json`, including direct edits to that file.
The optional reference-generation dependencies can be installed in a virtual
environment:

```sh
python3 -m venv docs/user-guide/.venv
docs/user-guide/.venv/bin/python -m pip install -r docs/user-guide/tools/requirements.txt
docs/user-guide/.venv/bin/python docs/user-guide/tools/prepare_reference.py
python3 docs/user-guide/build_guide.py
```

## Sources and capture settings

This edition describes upstream main at
[`902f96a`](https://github.com/paiheulevrai/Choochootracker/commit/902f96aa8750ea36acd69d94642a8df37b00ffdd),
version 1.0.16-alpha, and was compared with the web manual on 15 October 2026.
The source snapshots, chapter mapping and MIT notice are in `reference/`.
When refreshing the guide, reconcile the text and examples with the chosen
upstream revision together.

Captures use the production tracker UI at 640 × 480, the NostromoAmberDa2 theme,
and `Pixel16x24/02_DepartureMono.cnfont`. The Song waveform is off except in the
example demonstrating that setting. GIFs add an 80-pixel instruction strip,
making each frame 640 × 560, and loop indefinitely. Screenshots use a synthetic
First Journey project and generated drum sample; they contain no personal
songs or sideloaded libraries. Font licences are retained in `assets/fonts/`
and embedded in the generated HTML editions.

## Publish with the web app

From the repository root:

```sh
sh docs/user-guide/publish.sh
```

This copies `index.html` and its assets into the ignored
`web/dist/user-guide/` directory. Both Vercel configurations and the tracker's
`web-deploy` target run this copy step. It requires no WebAssembly rebuild or
extra JavaScript dependencies. The guide is served at `/user-guide/` alongside
the existing manual. To preview the result, serve `web/dist` with
`python3 -m http.server --directory web/dist 8080`.

## Verify

The optional browser checks require Node.js and Playwright with Chromium.
Install Playwright in your development environment; set `CHROME_EXECUTABLE`
to use an already installed Chrome instead of Playwright's browser.
First build the standalone edition, then run from the repository root:

```sh
node docs/user-guide/tools/check-reference.cjs
node docs/user-guide/tools/check-examples.cjs
node docs/user-guide/tools/check-cloning.cjs
node docs/user-guide/tools/check-navigation.cjs
```

These check local assets, internal links, reference expansion, screenshot and
GIF dialogs, keyboard/focus behaviour, cloning tabs, sidebar navigation, and
desktop/phone layout. Screenshots and reports go into ignored `.validation/`.
