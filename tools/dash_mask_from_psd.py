#!/usr/bin/env python3
"""The dash masks from the owner's PSDs: reads each PSD's "mask" layer and writes data/dash_masks/<CAR>/dash_mask.png (game/race/dash_mask.h).

The PSD is a dash's 640 x 480 2D layer (tools/dash_mask_psd.py makes the drafts) with a layer named "mask" painted over it. A pixel of the mask layer is classed by its alpha and colour:
  alpha < 128, or black (brightest channel < 64)   0    AUTO: not marked, the cockpit's own gauge ellipses decide
  white or light grey (every channel >= 192)       255  LIT: lit whatever its brightness (a warning lamp)
  blue-dominant (blue > red, green)                64   DASH: the dash's art, never lit (the speedo's rim)
  green-dominant (green > red, blue)               128  DIAL: inside a dial, its bright marks (numerals, ticks) are lit, its dark face is not
  anything else                                    0    AUTO
The layer's own opacity is ignored: it is only how the mask shows over the dash. The PNG is indexed (black, blue, green, white), the colours dash_mask.h reads.
A PSD of another size is written at its own size (the game scales a mask to 640 x 480 by nearest neighbour).

usage: dash_mask_from_psd.py <dir of CAR/dash.psd> [<output dir, default data/dash_masks next to tools/>]
Needs psd-tools (pip install psd-tools) and Pillow.
"""
import os
import sys

from PIL import Image

AUTO, DASH, DIAL, LIT = 0, 64, 128, 255   # game/race/dash_mask.h's classes
PALETTE = {AUTO: (0, 0, 0), DASH: (0, 0, 255), DIAL: (0, 255, 0), LIT: (255, 255, 255)}
PSD_NAME = "dash.psd"
PNG_NAME = "dash_mask.png"


def mask_class(r, g, b, a=255):
    """The class of a mask layer's pixel (colour and alpha), as dash_mask.c's dash_mask_class reads a painted PNG."""
    if a < 128 or max(r, g, b) < 64:
        return AUTO
    if min(r, g, b) >= 192:
        return LIT
    if b > r and b > g:
        return DASH
    if g > r and g > b:
        return DIAL
    return AUTO


def find_layer(psd, name):
    stack = list(psd)
    while stack:
        layer = stack.pop(0)
        if layer.is_group():
            stack = list(layer) + stack
        elif layer.name.strip().lower() == name:
            return layer
    return None


def mask_image(width, height, layer_image, left, top):
    """The class image ('L', width x height) of a mask layer's RGBA pixels placed at (left, top); unpainted pixels are AUTO."""
    out = Image.new("L", (width, height), AUTO)
    if layer_image is None:
        return out
    px = layer_image.convert("RGBA").load()
    dst = out.load()
    for y in range(layer_image.height):
        for x in range(layer_image.width):
            tx, ty = left + x, top + y
            if 0 <= tx < width and 0 <= ty < height:
                dst[tx, ty] = mask_class(*px[x, y])
    return out


def save_mask(classes, path):
    """The class image as the indexed PNG dash_mask.h reads."""
    index = {AUTO: 0, DASH: 1, DIAL: 2, LIT: 3}
    img = Image.new("P", classes.size)
    img.putpalette([c for v in (AUTO, DASH, DIAL, LIT) for c in PALETTE[v]])
    img.putdata([index.get(v, 0) for v in classes.getdata()])
    img.save(path, optimize=True)


def psd_mask(psd):
    layer = find_layer(psd, "mask")
    if layer is None:
        raise ValueError('no layer named "mask"')
    return mask_image(psd.width, psd.height, layer.topil(), layer.left, layer.top)


def main(argv):
    if len(argv) not in (2, 3):
        print(__doc__)
        return 2
    from psd_tools import PSDImage
    src = argv[1]
    dest = argv[2] if len(argv) == 3 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "dash_masks")
    count = 0
    for car in sorted(os.listdir(src)):
        psd_path = os.path.join(src, car, PSD_NAME)
        if not os.path.isfile(psd_path):
            continue
        classes = psd_mask(PSDImage.open(psd_path))
        os.makedirs(os.path.join(dest, car), exist_ok=True)
        path = os.path.join(dest, car, PNG_NAME)
        save_mask(classes, path)
        count += 1
        print(path)
    print(count, "masks")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
