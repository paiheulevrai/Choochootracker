#!/usr/bin/env python3
"""Make 256x224 SNES-shaped title assets from the approved railway art."""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
TITLE = ROOT / "tracker" / "packaging" / "common" / "title"
KEY = (255, 0, 255)


def snes_color(value):
    return (round(value * 31 / 255) * 255 + 15) // 31


def transparent(pixel):
    r, g, b = pixel
    return r > 100 and b > 100 and r > g * 1.6 and b > g * 1.6


def read_bmp(path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"{path}: not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, planes, bpp = struct.unpack_from("<iiHH", data, 18)
    if planes != 1 or bpp not in (24, 32):
        raise ValueError(f"{path}: expected 24/32-bit BMP")
    height_abs, stride = abs(height), ((width * (bpp // 8) + 3) // 4) * 4
    pixels = []
    for y in range(height_abs):
        row = []
        src_y = y if height < 0 else height_abs - 1 - y
        base = offset + src_y * stride
        for x in range(width):
            b, g, r = data[base + x * (bpp // 8):base + x * (bpp // 8) + 3]
            pixel = (r, g, b)
            row.append(KEY if transparent(pixel) else pixel)
        pixels.append(row)
    return pixels


def sample(image, out_w, out_h, source_w=None, source_h=None):
    source_h = source_h or len(image)
    source_w = source_w or len(image[0])
    return [[image[min(source_h - 1, y * source_h // out_h)][min(source_w - 1, x * source_w // out_w)]
             for x in range(out_w)] for y in range(out_h)]


def blend(base, overlay):
    return [[overlay[y][x] if overlay[y][x] != KEY else base[y][x]
             for x in range(len(base[0]))] for y in range(len(base))]


def quantize(image, limit, local=False):
    # The source is already pixel art. Bucketed RGB keeps its edges and avoids a new dependency.
    def reduce(pixels, count):
        colors = [p for p in pixels if p != KEY]
        if len(set(colors)) <= count:
            return {color: tuple(snes_color(component) for component in color) for color in set(colors)}
        step = 32
        while step > 1:
            buckets = {}
            for r, g, b in colors:
                key = (r // step, g // step, b // step)
                sums = buckets.setdefault(key, [0, 0, 0, 0])
                sums[0] += r; sums[1] += g; sums[2] += b; sums[3] += 1
            if len(buckets) <= count:
                return {key: tuple(snes_color(v[i] // v[3]) for i in range(3)) for key, v in buckets.items()}
            step *= 2
        return {(0, 0, 0): (0, 0, 0)}

    def apply(pixels, palette):
        keys = list(palette)
        result = []
        for p in pixels:
            if p == KEY:
                result.append(KEY); continue
            key = min(keys, key=lambda q: (p[0] - palette[q][0]) ** 2 + (p[1] - palette[q][1]) ** 2 + (p[2] - palette[q][2]) ** 2)
            result.append(palette[key])
        return result

    if not local:
        palette = reduce([p for row in image for p in row], limit)
        return [apply(row, palette) for row in image]
    result = [row[:] for row in image]
    for top in range(0, len(image), 8):
        for left in range(0, len(image[0]), 8):
            coords = [(y, x) for y in range(top, top + 8) for x in range(left, left + 8)]
            palette = reduce([image[y][x] for y, x in coords], limit)
            values = apply([image[y][x] for y, x in coords], palette)
            for (y, x), value in zip(coords, values): result[y][x] = value
    return result


def write_bmp(path, image):
    height, width = len(image), len(image[0])
    stride = (width * 3 + 3) & ~3
    size = 54 + stride * height
    header = b"BM" + struct.pack("<IHHI", size, 0, 0, 54) + struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, stride * height, 3780, 3780, 0, 0)
    rows = bytearray()
    for row in reversed(image):
        raw = bytearray()
        for r, g, b in row: raw.extend((b, g, r))
        rows.extend(raw + b"\0" * (stride - len(raw)))
    path.write_bytes(header + rows)


def export(name, image, colors, tile_colors=16):
    image = quantize(image, colors)
    image = quantize(image, tile_colors, local=True)
    image = quantize(image, colors)
    write_bmp(TITLE / name, image)


def main():
    layers = {name: read_bmp(TITLE / f"{name}.bmp") for name in ("sky", "far", "middle", "main", "viaduct", "foreground")}
    # Mode-1 tilemaps can be 64x32 tiles: keep two screen widths before wrapping.
    sky = sample(layers["sky"], 512, 224, 1280, 480)
    sky = blend(sky, sample(layers["far"], 512, 224, 1280, 480))
    scene = [[KEY] * 512 for _ in range(224)]
    for name in ("middle", "main"):
        scene = blend(scene, sample(layers[name], 512, 224, 1280, 480))
    foreground = sample(layers["foreground"], 512, 224, 1280, 480)
    viaduct = sample(layers["viaduct"], 512, 224, 1280, 480)
    stars = [[p if max(p) > 180 and min(p) > 70 else KEY for p in row] for row in sky]
    export("snes_sky.bmp", sky, 128)
    export("snes_scene.bmp", scene, 128)
    export("snes_foreground.bmp", foreground, 128)
    export("snes_viaduct.bmp", viaduct, 128)
    export("snes_detail.bmp", stars, 4, 4)
    train = read_bmp(TITLE / "train.bmp")[14:]
    export("snes_train.bmp", sample(train, 240, 32), 16)
    export("snes_logo.bmp", sample(read_bmp(TITLE / "logo.bmp"), 160, 48), 16)


if __name__ == "__main__":
    main()
