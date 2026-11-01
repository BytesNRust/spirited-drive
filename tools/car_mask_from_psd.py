#!/usr/bin/env python3
"""The masks from the owner's PSDs (tools/car_mask_psd.py): reads each PSD's "mask" layer and writes data/car_masks/<CAR>/<tex>_mask.png.
A PSD named <tex>_lo.psd (the low detail bank's texture) is written as <tex>_lo_mask.png, the file car_mask_load reads for that bank.

A texel of the mask layer is classed by its alpha and colour:
  alpha < 128                      0    PAINT (recoloured, shines)
  blue-dominant (blue > red, green) 128  KEEP its colour, shines as glass (glass, lamps)
  green-dominant (green > red, blue) 192 KEEP its colour, shines as the body (a two-tone car's roof and pillars: black in the texture,
                                         but body, so the car_paint recolour leaves it alone and it takes the paint's gain and dimming)
  magenta (red and blue both >= 128, green at most half the lesser, red and blue within 32 of each other: ff00ff) 224  the FIXED BLACK:
                                         repainted with the car's catalogue black whatever paint the car has, and shining as paint (a two-tone
                                         car's black roof whose texture is a light grey: green would leave it grey)
  red (ff0000: red >= 192, green and blue <= 64)          240  BRAKE light (emissive in low light; the position red when released)
  orange (ff8000: red >= 192, green 96..176, blue <= 64)  244  POSITION light (emissive red, dimmer, in low light)
  yellow (ffff00: red and green >= 192, blue <= 64)       248  HEADLIGHT (emissive warm white in low light)
  cyan (00ffff: red <= 64, green and blue >= 192)         252  REVERSING light (white, emissive while the car is in reverse gear, in any
                                         light; checked before the blue and green rules, since cyan has blue = green)
  dark (opaque, brightest channel < 64)  0   PAINT too (black is paint, as in the old two-value masks)
  anything else                    255  KEEP its colour, no shine (plastics, trim, plate, badges, vents)
The layer's own opacity is ignored: it is only how the mask shows over the texture. The PNG is grey, 0 / 128 / 192 / 224 / 240 / 244 / 248 / 252 / 255. The colour that means 192 is BODY_KEEPS below: change that one constant to use another.

usage: car_mask_from_psd.py <dir of CAR/<tex>.psd> [<output dir, default data/car_masks next to tools/>]
Needs psd-tools (pip install psd-tools) and Pillow.
"""
import os
import sys

from PIL import Image


# the mask layer's colour for "keeps its colour, shines as the body" (192): the dominant channel of the texel's RGB, an index into (r, g, b)
BODY_KEEPS = 1   # green (0 red; blue is glass's)


FIXED_BLACK = 224   # car_mask.h CAR_MASK_BLACK; its colour in the PSD is magenta (is_magenta below, as car_mask.c reads it)


LAMP_BRAKE, LAMP_POSITION, LAMP_HEAD, LAMP_REVERSE = 240, 244, 248, 252   # car_mask.h's lamp classes


def lamp_class(r, g, b):
    """The lamp class of a mask texel's colour: red brake, orange position, yellow headlight, cyan reversing light; 0 when it is none of them."""
    if r <= 64 and g >= 192 and b >= 192:   # cyan first: it has b == g, so the blue and green rules would not see it as a lamp
        return LAMP_REVERSE
    if r < 192 or b > 64:
        return 0
    if g <= 64:
        return LAMP_BRAKE
    if 96 <= g <= 176:
        return LAMP_POSITION
    if g >= 192:
        return LAMP_HEAD
    return 0


def is_magenta(r, g, b):
    pink = min(r, b)
    return pink >= 128 and g * 2 <= pink and abs(r - b) <= 32


def dominant(r, g, b):
    c = (r, g, b)
    top = max(c)
    return c.index(top) if c.count(top) == 1 else None


def find_layer(psd, name):
    stack = list(psd)
    while stack:
        layer = stack.pop(0)
        if layer.is_group():
            stack = list(layer) + stack
        elif layer.name.strip().lower() == name:
            return layer
    return None


def keep_mask(psd):
    layer = find_layer(psd, "mask")
    if layer is None:
        raise ValueError('no layer named "mask"')
    image = layer.topil()   # the layer's own pixels, over its own bounds (the layer may be moved or trimmed)
    width, height = psd.width, psd.height
    keep = Image.new("L", (width, height), 0)
    if image is None:
        return keep
    image = image.convert("RGBA")
    px = image.load()
    out = keep.load()
    for y in range(image.height):
        for x in range(image.width):
            tx, ty = layer.left + x, layer.top + y
            if not (0 <= tx < width and 0 <= ty < height):
                continue
            r, g, b, a = px[x, y]
            if a < 128:
                continue
            lamp = lamp_class(r, g, b)
            if lamp:
                out[tx, ty] = lamp
            elif is_magenta(r, g, b):
                out[tx, ty] = FIXED_BLACK
            elif b > r and b > g:
                out[tx, ty] = 128
            elif dominant(r, g, b) == BODY_KEEPS:
                out[tx, ty] = 192
            elif max(r, g, b) >= 64:
                out[tx, ty] = 255
    return keep


def main(argv):
    if len(argv) not in (2, 3):
        print(__doc__)
        return 2
    from psd_tools import PSDImage
    src = argv[1]
    dest = argv[2] if len(argv) == 3 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "car_masks")
    count = 0
    for car in sorted(os.listdir(src)):
        folder = os.path.join(src, car)
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".psd"):
                continue
            tex = name[: -len(".psd")]
            keep = keep_mask(PSDImage.open(os.path.join(folder, name)))
            os.makedirs(os.path.join(dest, car), exist_ok=True)
            path = os.path.join(dest, car, tex + "_mask.png")
            keep.save(path, optimize=True)
            count += 1
            print(path)
    print(count, "masks")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
