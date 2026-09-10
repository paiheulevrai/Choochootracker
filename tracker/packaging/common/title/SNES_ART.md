# SNES title art contract

The title scene is authored at NTSC 256x224 and is enlarged to a 4:3 display
with nearest-neighbour sampling. Scrolling planes wrap at their native widths.
The renderer restores cropped planes to their original canvas origins: scene
at y=112, viaduct at y=152, foreground at y=136, and logo at (52,24).

`snes_sky.bmp` includes the far mountains; `snes_scene.bmp` is an independent
intermediate plane between those mountains and the viaduct. It and
`snes_foreground.bmp` scroll independently. The foreground scrolls faster
than the viaduct to preserve depth.
`snes_viaduct.bmp` is drawn in front of the train, placing its fence and arches
in the foreground.
Magenta (`#ff00ff`) is the transparent entry.

Magenta-only padding is removed from the packaged planes; magenta remains the
transparent entry inside their visible bounds. The title deliberately does not
impose total OAM, per-scanline sprite, tile, or pixel budgets. Run
`python scripts/validate_snes_title.py` with the bundled Emscripten Python to
check the exported assets.
