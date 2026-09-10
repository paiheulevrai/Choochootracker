#!/usr/bin/env python3
"""Validate the geometry and BMP format of title-screen assets."""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
TITLE = ROOT / "tracker" / "packaging" / "common" / "title"
ASSETS = {
    "snes_sky.bmp": (512, 224), "snes_scene.bmp": (435, 112),
    "snes_foreground.bmp": (512, 88), "snes_viaduct.bmp": (187, 72),
    "snes_train.bmp": (240, 32), "snes_logo.bmp": (152, 36),
}


def read_bmp(path):
    data = path.read_bytes()
    if data[:2] != b"BM": raise ValueError("not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, planes, bpp = struct.unpack_from("<iiHH", data, 18)
    if planes != 1 or bpp != 24: raise ValueError("must be an opaque 24-bit BMP (no alpha)")
    return width, height


def main():
    errors = []
    for name, (want_w, want_h) in ASSETS.items():
        try: width, height = read_bmp(TITLE / name)
        except Exception as exc: errors.append(f"{name}: {exc}"); continue
        if (width, height) != (want_w, want_h): errors.append(f"{name}: expected {want_w}x{want_h}, got {width}x{height}")
    if errors:
        print("SNES title validation failed:", *errors, sep="\n  ")
        return 1
    print("Title assets valid (cropped geometry, 24-bit BMP).")


if __name__ == "__main__": sys.exit(main())
