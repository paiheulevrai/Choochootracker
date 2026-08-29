# SNES title art contract

The title scene is authored at NTSC 256x224 and is enlarged to a 4:3 display
with nearest-neighbour sampling. The three scrolling planes are 512x224
(64x28 tiles), allowing two screen widths before wrapping. Every source region
aligns to 8x8 tiles; the logo and train bounds align to 16x16 metasprite pieces.

`snes_sky.bmp` includes the far mountains; it, `snes_scene.bmp`, and
`snes_foreground.bmp` each use at most 128 colours (eight 16-colour background
subpalettes). The foreground scrolls faster than the viaduct scene to preserve
depth. `snes_detail.bmp` uses four colours.
`snes_viaduct.bmp` is drawn in front of the train, placing its fence and arches
in the foreground.
`snes_train.bmp` and `snes_logo.bmp` use one 16-colour sprite palette each.
Magenta (`#ff00ff`) is the transparent entry. All other colours are rounded
to the SNES 15-bit BGR precision.

The title deliberately does not impose total OAM, per-scanline sprite, tile,
or pixel budgets. Run `python scripts/validate_snes_title.py` with the bundled
Emscripten Python to check the exported assets.
