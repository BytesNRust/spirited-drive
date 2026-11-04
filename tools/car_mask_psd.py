#!/usr/bin/env python3
"""Layered PSDs for the hand-made car masks (game/race/car_mask.h).

Each texture becomes one PSD at its native size, 8-bit RGB, with two layers:
  texture  the original texture (locked)
  mask     the texel's class, at layer opacity 50%:
             transparent  PAINT: recoloured by the paint, and shines
             blue (0,0,255)   KEEP its colour, and shines (glass)
             green (0,255,0)  KEEP, shines as the body (a two-tone car's roof and pillars)
             magenta (255,0,255)  the fixed black
             red (255,0,0)    a brake light, orange (255,128,0) a position light, yellow (255,255,0) a headlight, cyan (0,255,255) a reversing light
             white        KEEP its colour, no shine (plastics, trim, plate, badges, vents)
tools/car_mask_from_psd.py reads the mask layer back into data/car_masks/<CAR>/<tex>_mask.png (car_mask.h's classes; the colours are its own).
The low detail bank's textures are the same with a _lo suffix: <tex>_lo.psd is read into <tex>_lo_mask.png.

usage: car_mask_psd.py <drafts dir> <output dir>
<drafts dir>/<CAR>/<tex>.rgb.png and <tex>.keep.png (0 / 128 / 255) come from car_mask_draft (the game's pixels: they stay out of the repository);
the low detail bank's are <tex>_lo.rgb.png and <tex>_lo.keep.png.
Writes <output dir>/<CAR>/<tex>.psd (<tex>_lo.psd for the low bank). Needs Pillow; writes the PSD by hand (no other dependency).
"""
import os
import struct
import sys

from PIL import Image

LOCK_ALL = 0x80000000   # 'lspf' protection flags: everything locked
MASK_OPACITY = 128      # 50 %
SHINE = 128             # the class of a kept texel that shines (car_mask.h CAR_MASK_SHINE)
# car_mask.h's classes and the colour each has in the mask layer (car_mask_from_psd.py reads them back); any other non-zero class is matte
CLASS_COLOUR = {128: (0, 0, 255), 192: (0, 255, 0), 224: (255, 0, 255), 240: (255, 0, 0), 244: (255, 128, 0), 248: (255, 255, 0), 252: (0, 255, 255)}
MATTE = (255, 255, 255)


def class_of(value):
    """A draft or mask PNG's grey as the class it stands for: the lamps' greys come in fours (240..243 brake, 244..247 position, 248..251 headlight, 252..254
    reversing), the old two-tone drafts' values below 192 shine."""
    if value == 0:
        return 0
    if value < 192:
        return SHINE
    if value < 224:
        return 192
    if value < 240:
        return 224
    if value < 255:
        return 240 + (value - 240) // 4 * 4 if value < 252 else 252
    return 255


def mask_planes(classes):
    """The mask layer's alpha and red, green and blue planes (bytes) for `classes` (a texel's class a byte each)."""
    colours = [CLASS_COLOUR.get(c, MATTE) for c in classes]
    alpha = bytes(0 if c == 0 else 255 for c in classes)
    return (alpha,) + tuple(bytes(colour[k] for colour in colours) for k in range(3))


def pascal(name, pad):
    raw = name.encode("ascii")[:255]
    out = bytes([len(raw)]) + raw
    return out + b"\0" * ((-len(out)) % pad)


def layer_record(name, width, height, opacity, lock, channels, hidden=False):
    rec = struct.pack(">iiii", 0, 0, height, width) + struct.pack(">H", len(channels))
    for cid in channels:
        rec += struct.pack(">hI", cid, 2 + width * height)   # 2: the compression word; raw rows follow
    rec += b"8BIM" + b"norm" + bytes([opacity, 0, 2 if hidden else 0, 0])      # opacity, clipping, flags (bit 1: hidden), filler
    ascii_name = name.encode("ascii", "ignore").decode("ascii") if not name.isascii() else name
    extra = struct.pack(">I", 0) + struct.pack(">I", 0) + pascal(ascii_name, 4)   # no layer mask, no blending ranges
    if not name.isascii():   # 'luni': the Unicode name (the Pascal one above is its ASCII part)
        text = struct.pack(">I", len(name)) + name.encode("utf-16-be")
        text += b"\0" * (len(text) % 2)
        extra += b"8BIM" + b"luni" + struct.pack(">I", len(text)) + text
    if lock:
        extra += b"8BIM" + b"lspf" + struct.pack(">II", 4, LOCK_ALL)
    return rec + struct.pack(">I", len(extra)) + extra


def channel_data(plane):
    return struct.pack(">H", 0) + plane   # raw


def write_psd(path, rgb, keep, base_name="texture", guide=None):
    """rgb: RGB Image; keep: 'L' Image of classes (class_of), 0 paint, 128 kept with a shine, 192 as the body, 224 the fixed black, 240..252 the lamps, 255 kept without.
    guide: None, or (name, RGB Image, 'L' Image alpha): a hidden, locked layer between the texture and the mask (the dash's wheel)."""
    width, height = rgb.size
    if keep.size != rgb.size:
        raise ValueError("mask and texture sizes differ")
    r, g, b = (bytes(band.tobytes()) for band in rgb.split())
    classes = keep.tobytes()
    alpha, red, green, blue = mask_planes(classes)
    texture_channels = (0, 1, 2)
    mask_channels = (-1, 0, 1, 2)   # alpha first, as Photoshop writes it
    records = layer_record(base_name, width, height, 255, True, texture_channels)
    data = b"".join(channel_data(p) for p in (r, g, b))
    count = 2
    if guide is not None:
        gname, grgb, galpha = guide
        if grgb.size != rgb.size or galpha.size != rgb.size:
            raise ValueError("guide and texture sizes differ")
        records += layer_record(gname, width, height, 255, True, mask_channels, hidden=True)
        data += b"".join(channel_data(bytes(p)) for p in (galpha.tobytes(),) + tuple(band.tobytes() for band in grgb.split()))
        count = 3
    records += layer_record("mask", width, height, MASK_OPACITY, False, mask_channels)
    data += b"".join(channel_data(p) for p in (alpha, red, green, blue))
    layer_info = struct.pack(">h", count) + records + data
    if len(layer_info) % 2:
        layer_info += b"\0"
    section = struct.pack(">I", len(layer_info)) + layer_info + struct.pack(">I", 0)   # no global layer mask
    # the merged image: the texture under the mask at its opacity
    over = Image.merge("RGB", [Image.frombytes("L", rgb.size, plane) for plane in (red, green, blue)])
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
            keep = Image.open(os.path.join(folder, tex + ".keep.png")).convert("L").point(class_of)
            path = os.path.join(out_dir, car, tex + ".psd")
            write_psd(path, rgb, keep)
            count += 1
            print(path)
    print(count, "files")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
