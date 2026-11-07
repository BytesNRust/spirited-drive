#!/usr/bin/env python3
"""The reflective masks from the owner's PSDs (tools/track_mask_psd.py): reads each PSD's "mask" layer and writes data/track_masks/<TRACK>/<name>.png, the file track_mask_load reads
(a mod puts its own in <mod>/track_masks/<TRACK>/).

A texel of the mask layer is reflective when it is painted: opaque enough (alpha 128 or more) and not black (its brightest channel 64 or more). The PNG is grey, 255 (white)
for the reflective texels and 0 (black) for the rest; the game reads red 128 or more as reflective. The layer's own opacity is ignored: it is only how the mask shows over the texture.

usage: track_mask_from_psd.py <dir of TRACK/<name>.psd> [<output dir, default data/track_masks next to tools/>]
Needs psd-tools (pip install psd-tools) and Pillow.
"""
import os
import sys

from PIL import Image

ALPHA_FROM = 128   # a texel of the layer less opaque than this is not painted
PAINT_FROM = 64    # a painted texel darker than this (its brightest channel) is black: not reflective


def texel_reflective(r, g, b, a):
    """Whether a texel of the mask layer is painted reflective: opaque enough and not black."""
    return a >= ALPHA_FROM and max(r, g, b) >= PAINT_FROM


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
    layer = find_layer(psd, "mask")
    if layer is None:
        raise ValueError('no layer named "mask"')
    image = layer.topil()   # the layer's own pixels, over its own bounds (the layer may be moved or trimmed)
    width, height = psd.width, psd.height
    mask = Image.new("L", (width, height), 0)
    if image is None:
        return mask
    image = image.convert("RGBA")
    px = image.load()
    out = mask.load()
    for y in range(image.height):
        for x in range(image.width):
            tx, ty = layer.left + x, layer.top + y
            if not (0 <= tx < width and 0 <= ty < height):
                continue
            r, g, b, a = px[x, y]
            if texel_reflective(r, g, b, a):
                out[tx, ty] = 255
    return mask


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
            mask = reflective_mask(PSDImage.open(os.path.join(folder, name)))
            os.makedirs(os.path.join(dest, track), exist_ok=True)
            path = os.path.join(dest, track, name[: -len(".psd")] + ".png")
            mask.save(path, optimize=True)
            count += 1
            print(path)
    print(count, "masks")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
