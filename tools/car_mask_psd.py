#!/usr/bin/env python3
"""Layered PSDs for the hand-made car masks (game/race/car_mask.h).

Each texture becomes one PSD at its native size, 8-bit RGB, with two layers:
  texture  the original texture (locked)
  mask     the texel's class, at layer opacity 50%:
             transparent  PAINT: recoloured by the paint, and shines
             blue (0,0,255)  KEEP its colour, and shines (glass, lamps)
             white        KEEP its colour, no shine (plastics, trim, plate, badges, vents)
tools/car_mask_from_psd.py reads the mask layer back into data/car_masks/<CAR>/<tex>_mask.png (0 paint, 128 shine, 255 matte).

usage: car_mask_psd.py <drafts dir> <output dir>
<drafts dir>/<CAR>/<tex>.rgb.png and <tex>.keep.png (0 / 128 / 255) come from car_mask_draft (the game's pixels: they stay out of the repository).
Writes <output dir>/<CAR>/<tex>.psd. Needs Pillow; writes the PSD by hand (no other dependency).
"""
import os
import struct
import sys

from PIL import Image

LOCK_ALL = 0x80000000   # 'lspf' protection flags: everything locked
MASK_OPACITY = 128      # 50 %
SHINE = 128             # the class of a kept texel that shines (car_mask.h CAR_MASK_SHINE)


def pascal(name, pad):
    raw = name.encode("ascii")[:255]
    out = bytes([len(raw)]) + raw
    return out + b"\0" * ((-len(out)) % pad)


def layer_record(name, width, height, opacity, lock, channels):
    rec = struct.pack(">iiii", 0, 0, height, width) + struct.pack(">H", len(channels))
    for cid in channels:
        rec += struct.pack(">hI", cid, 2 + width * height)   # 2: the compression word; raw rows follow
    rec += b"8BIM" + b"norm" + bytes([opacity, 0, 0, 0])      # opacity, clipping, flags (visible), filler
    extra = struct.pack(">I", 0) + struct.pack(">I", 0) + pascal(name, 4)   # no layer mask, no blending ranges
    if lock:
        extra += b"8BIM" + b"lspf" + struct.pack(">II", 4, LOCK_ALL)
    return rec + struct.pack(">I", len(extra)) + extra


def channel_data(plane):
    return struct.pack(">H", 0) + plane   # raw


def write_psd(path, rgb, keep):
    """rgb: RGB Image; keep: 'L' Image, 0 paint, 128 kept with a shine, 255 kept without."""
    width, height = rgb.size
    if keep.size != rgb.size:
        raise ValueError("mask and texture sizes differ")
    r, g, b = (bytes(band.tobytes()) for band in rgb.split())
    classes = keep.tobytes()
    alpha = bytes(0 if c == 0 else 255 for c in classes)
    red = bytes(0 if c == SHINE else 255 for c in classes)   # blue where it shines, white where it does not
    white = b"\xff" * (width * height)
    texture_channels = (0, 1, 2)
    mask_channels = (-1, 0, 1, 2)   # alpha first, as Photoshop writes it
    records = layer_record("texture", width, height, 255, True, texture_channels)
    records += layer_record("mask", width, height, MASK_OPACITY, False, mask_channels)
    data = b"".join(channel_data(p) for p in (r, g, b))
    data += b"".join(channel_data(p) for p in (alpha, red, red, white))
    layer_info = struct.pack(">h", 2) + records + data
    if len(layer_info) % 2:
        layer_info += b"\0"
    section = struct.pack(">I", len(layer_info)) + layer_info + struct.pack(">I", 0)   # no global layer mask
    # the merged image: the texture under the mask at its opacity
    over = Image.merge("RGB", [Image.frombytes("L", rgb.size, red)] * 2 + [Image.frombytes("L", rgb.size, white)])
    soft = Image.frombytes("L", rgb.size, alpha).point(lambda v: v * MASK_OPACITY // 255)
    merged = Image.composite(over, rgb, soft)
    planes = b"".join(band.tobytes() for band in merged.split())
    out = b"8BPS" + struct.pack(">H", 1) + b"\0" * 6 + struct.pack(">HIIHH", 3, height, width, 8, 3)
    out += struct.pack(">I", 0) + struct.pack(">I", 0)   # no colour mode data, no image resources
    out += struct.pack(">I", len(section)) + section
    out += struct.pack(">H", 0) + planes
    with open(path, "wb") as f:
        f.write(out)


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    drafts, out_dir = argv[1], argv[2]
    count = 0
    for car in sorted(os.listdir(drafts)):
        folder = os.path.join(drafts, car)
        if not os.path.isdir(folder):
            continue
        os.makedirs(os.path.join(out_dir, car), exist_ok=True)
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".rgb.png"):
                continue
            tex = name[: -len(".rgb.png")]
            rgb = Image.open(os.path.join(folder, name)).convert("RGB")
            keep = Image.open(os.path.join(folder, tex + ".keep.png")).convert("L").point(lambda v: 0 if v == 0 else SHINE if v < 192 else 255)
            path = os.path.join(out_dir, car, tex + ".psd")
            write_psd(path, rgb, keep)
            count += 1
            print(path)
    print(count, "files")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
