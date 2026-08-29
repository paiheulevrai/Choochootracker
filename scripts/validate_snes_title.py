#!/usr/bin/env python3
"""Validate the palette and geometry contract for title-screen BMPs."""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
TITLE = ROOT / "tracker" / "packaging" / "common" / "title"
KEY = (255, 0, 255)
ASSETS = {
    "snes_sky.bmp": (512, 224, 128), "snes_scene.bmp": (512, 224, 128),
    "snes_foreground.bmp": (512, 224, 128),
    "snes_viaduct.bmp": (512, 224, 128),
    "snes_detail.bmp": (512, 224, 4), "snes_train.bmp": (240, 32, 16),
    "snes_logo.bmp": (160, 48, 16),
}


def read_bmp(path):
    data = path.read_bytes()
    if data[:2] != b"BM": raise ValueError("not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, planes, bpp = struct.unpack_from("<iiHH", data, 18)
    if planes != 1 or bpp != 24: raise ValueError("must be an opaque 24-bit BMP (no alpha)")
    stride = (width * 3 + 3) & ~3
    pixels = []
    for y in range(height):
        start = offset + (height - 1 - y) * stride
        pixels.append([tuple(reversed(data[start + x * 3:start + x * 3 + 3])) for x in range(width)])
    return width, height, pixels


def main():
    errors = []
    for name, (want_w, want_h, global_limit) in ASSETS.items():
        try: width, height, image = read_bmp(TITLE / name)
        except Exception as exc: errors.append(f"{name}: {exc}"); continue
        if (width, height) != (want_w, want_h): errors.append(f"{name}: expected {want_w}x{want_h}, got {width}x{height}")
        if width % 8 or height % 8: errors.append(f"{name}: dimensions must align to 8x8 tiles")
        if name in ("snes_train.bmp", "snes_logo.bmp") and (width % 16 or height % 16):
            errors.append(f"{name}: metasprite bounds must align to 16x16 pieces")
        colors = {p for row in image for p in row if p != KEY}
        if len(colors) > global_limit: errors.append(f"{name}: {len(colors)} colors, limit is {global_limit}")
        if any(tuple((round(component * 31 / 255) * 255 + 15) // 31 for component in color) != color for color in colors):
            errors.append(f"{name}: colors are not quantized to SNES 15-bit BGR precision")
        for y in range(0, height, 8):
            for x in range(0, width, 8):
                count = len({image[yy][xx] for yy in range(y, y + 8) for xx in range(x, x + 8) if image[yy][xx] != KEY})
                if count > 16: errors.append(f"{name}: tile {x // 8},{y // 8} has {count} colors"); break
    if errors:
        print("SNES title validation failed:", *errors, sep="\n  ")
        return 1
    print("SNES title assets valid (256x224, 8x8 tiles, no alpha, palette limits respected).")


if __name__ == "__main__": sys.exit(main())
