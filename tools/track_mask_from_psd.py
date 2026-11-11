#!/usr/bin/env python3
"""The reflective masks from the owner's PSDs (tools/track_mask_psd.py): reads each PSD's "mask" layer and writes data/track_masks/<TRACK>/<name>.png, the file track_mask_load reads
(a mod puts its own in <mod>/track_masks/<TRACK>/).

A texel of the mask layer is reflective when it is painted: opaque enough (alpha 128 or more) and not black (its brightest channel 64 or more). A yellow texel (red and green 192
or more, blue 64 or less, as car_mask_from_psd.py reads a headlight) is a lamp's mark instead (track_mask.c: the tunnel and street lamps, tools/track_lamp_marks): not reflective,
kept yellow. Without lamps the PNG is grey, 255 (white) for the reflective texels and 0 (black) for the rest; with lamps it is RGBA, white reflective, yellow (255, 255, 0) the
lamps, transparent the rest, as tools/track_lamp_marks writes them. The game reads red 128 or more as reflective and red and green 128 or more with blue under 128 as a lamp. The
layer's own opacity is ignored: it is only how the mask shows over the texture.

usage: track_mask_from_psd.py <dir of TRACK/<name>.psd> [<output dir, default data/track_masks next to tools/>]
Needs psd-tools (pip install psd-tools) and Pillow.
"""
import os
import sys

from PIL import Image

ALPHA_FROM = 128   # a texel of the layer less opaque than this is not painted
PAINT_FROM = 64    # a painted texel darker than this (its brightest channel) is black: not reflective


LAMP_FROM = 192   # a lamp's yellow: red and green this much or more
LAMP_BLUE = 64    # and blue this much or less


def texel_lamp(r, g, b, a):
    """Whether a texel of the mask layer marks a lamp: opaque enough and yellow."""
    return a >= ALPHA_FROM and r >= LAMP_FROM and g >= LAMP_FROM and b <= LAMP_BLUE


def texel_reflective(r, g, b, a):
    """Whether a texel of the mask layer is painted reflective: opaque enough, not black and not a lamp's yellow."""
    return a >= ALPHA_FROM and max(r, g, b) >= PAINT_FROM and not texel_lamp(r, g, b, a)


def find_layer(psd, name):
    stack = list(psd)
    while stack:
        layer = stack.pop(0)
        if layer.is_group():
            stack = list(layer) + stack
        elif layer.name.strip().lower() == name:
            return layer
    return None


def reflective_mask(psd):
    """The mask layer of an opened PSD as an 'L' Image the size of the PSD: 255 reflective, 0 not."""
    return marks(psd)[0]


def mask_png(psd):
    """The PNG to write for an opened PSD: the grey mask (reflective_mask) when it marks no lamp, else RGBA: white reflective, yellow the lamps, transparent the rest."""
    reflective, lamps = marks(psd)
    if lamps.getbbox() is None:
        return reflective
    out = Image.new("RGBA", reflective.size, (0, 0, 0, 0))
    out.paste((255, 255, 255, 255), mask=reflective)
    out.paste((255, 255, 0, 255), mask=lamps)
    return out


def marks(psd):
    """The mask layer of an opened PSD as two 'L' Images the size of the PSD: its reflective texels and its lamps' (255 marked, 0 not)."""
    layer = find_layer(psd, "mask")
    if layer is None:
        raise ValueError('no layer named "mask"')
    image = layer.topil()   # the layer's own pixels, over its own bounds (the layer may be moved or trimmed)
    width, height = psd.width, psd.height
    mask = Image.new("L", (width, height), 0)
    lamps = Image.new("L", (width, height), 0)
    if image is None:
        return mask, lamps
    image = image.convert("RGBA")
    px = image.load()
    out, lamp_out = mask.load(), lamps.load()
    for y in range(image.height):
        for x in range(image.width):
            tx, ty = layer.left + x, layer.top + y
            if not (0 <= tx < width and 0 <= ty < height):
                continue
            r, g, b, a = px[x, y]
            if texel_lamp(r, g, b, a):
                lamp_out[tx, ty] = 255
            elif texel_reflective(r, g, b, a):
                out[tx, ty] = 255
    return mask, lamps


def main(argv):
    if len(argv) not in (2, 3):
        print(__doc__)
        return 2
    from psd_tools import PSDImage
    src = argv[1]
    dest = argv[2] if len(argv) == 3 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "track_masks")
    count = 0
    for track in sorted(os.listdir(src)):
        folder = os.path.join(src, track)
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".psd"):
                continue
            mask = mask_png(PSDImage.open(os.path.join(folder, name)))
            os.makedirs(os.path.join(dest, track), exist_ok=True)
            path = os.path.join(dest, track, name[: -len(".psd")] + ".png")
            mask.save(path, optimize=True)
            count += 1
            print(path)
    print(count, "masks")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
