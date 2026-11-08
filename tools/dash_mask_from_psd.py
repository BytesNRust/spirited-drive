#!/usr/bin/env python3
"""The dash masks from the owner's PSDs: reads each PSD's "mask" layer and writes data/dash_masks/<CAR>/dash_mask.png (game/race/dash_mask.h); with an optional "color" layer, also
data/dash_masks/<CAR>/dash_color.png (game/race/dash_color.h), the colours of the lit parts for gauge_light = 5 ("Del coche").

The PSD is a dash's 640 x 480 2D layer (tools/dash_mask_psd.py makes the drafts) with a layer named "mask" painted over it. A pixel of the mask layer is classed by its alpha and colour:
  alpha < 128, or black (brightest channel < 64)   0    AUTO: not marked, the cockpit's own gauge ellipses decide
  yellow (red and green >= 192, blue <= 64), or white or light grey (every channel >= 192)
                                                   255  LIT: lit whatever its brightness (a warning lamp). Paint it yellow ffff00: it contrasts on the dash; white still works
  blue-dominant (blue > red, green)                64   DASH: the dash's art, never lit (the speedo's rim)
  green-dominant (green > red, blue)               128  DIAL: inside a dial, its bright marks (numerals, ticks) are lit, its dark face is not
  red (red >= 192, green and blue <= 64)           192  BLADE: a wiper blade the dash's art draws; with port_wipers it is not drawn in rain or snow. An optional layer named "limpia" (or "wipers") holds them:
                                                        whatever is painted on it (any colour but black) is BLADE
  anything else                                    0    AUTO
The layer's own opacity is ignored: it is only how the mask shows over the dash.

The optional "color" layer: paint it over the lit parts, in the colour each is to glow. Each pixel's colour is the colour of the light there, so the speedo and the tach can differ.
Only its strongest channel sets the hue (the game takes the colour's strongest channel as full: paint it bright, the dash's art gives the brightness). A transparent pixel (alpha < 128)
or a black one (every channel < 24) has no colour: a lit part there glows orange (gauge_light = 3), as does every lit part of a dash with no "color" layer, and a pixel the mask does not
make lit stays unlit whatever the colour. The game folds the colours into 12 flat ones (a colour within 48 of an earlier one, by the sum of its channels' differences, shares its
slot), so paint with a few flat colours. The layer's opacity is ignored. The file is RGBA, the layer's colour and 255 or 0 for its alpha; a dash whose PSD has no "color" layer
gets no dash_color.png (an old one is deleted).
 The PNG is indexed (black, blue, green, white: a yellow LIT is written white), the colours dash_mask.h reads.
A PSD of another size is written at its own size (the game scales a mask to 640 x 480 by nearest neighbour).

usage: dash_mask_from_psd.py <dir of CAR/dash.psd> [<output dir, default data/dash_masks next to tools/>]
Needs psd-tools (pip install psd-tools) and Pillow.
"""
import os
import sys

from PIL import Image

AUTO, DASH, DIAL, BLADE, LIT = 0, 64, 128, 192, 255   # game/race/dash_mask.h's classes
PALETTE = {AUTO: (0, 0, 0), DASH: (0, 0, 255), DIAL: (0, 255, 0), LIT: (255, 255, 255), BLADE: (255, 0, 0)}
BLADE_LAYERS = ("limpia", "wipers")   # the optional layer of the wiper blades the dash's art draws (hidden in rain and snow)
PSD_NAME = "dash.psd"
PNG_NAME = "dash_mask.png"
COLOR_PNG_NAME = "dash_color.png"
COLOR_LAYERS = ("color", "colour")   # the optional layer of lit colours


def mask_class(r, g, b, a=255):
    """The class of a mask layer's pixel (colour and alpha), as dash_mask.c's dash_mask_class reads a painted PNG."""
    if a < 128 or max(r, g, b) < 64:
        return AUTO
    if min(r, g, b) >= 192 or (r >= 192 and g >= 192 and b <= 64):   # white, or yellow (ffff00)
        return LIT
    if r >= 192 and g <= 64 and b <= 64:   # red: a wiper blade the dash's art draws
        return BLADE
    if b > r and b > g:
        return DASH
    if g > r and g > b:
        return DIAL
    return AUTO


def find_layer(psd, names):
    stack = list(psd)
    while stack:
        layer = stack.pop(0)
        if layer.is_group():
            stack = list(layer) + stack
        elif layer.name.strip().lower() in ((names,) if isinstance(names, str) else names):
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


def color_image(width, height, layer_image, left, top):
    """The colour image ('RGBA', width x height) of a colour layer's pixels placed at (left, top): the painted RGB where alpha >= 128 (made fully opaque), else transparent."""
    out = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    if layer_image is None:
        return out
    px = layer_image.convert("RGBA").load()
    dst = out.load()
    for y in range(layer_image.height):
        for x in range(layer_image.width):
            tx, ty = left + x, top + y
            r, g, b, a = px[x, y]
            if a >= 128 and 0 <= tx < width and 0 <= ty < height:
                dst[tx, ty] = (r, g, b, 255)
    return out


def psd_color(psd):
    """The colour image of the PSD's optional "color" layer, or None when it has none."""
    layer = find_layer(psd, COLOR_LAYERS)
    if layer is None:
        return None
    return color_image(psd.width, psd.height, layer.topil(), layer.left, layer.top)


def save_mask(classes, path):
    """The class image as the indexed PNG dash_mask.h reads."""
    index = {AUTO: 0, DASH: 1, DIAL: 2, LIT: 3, BLADE: 4}
    img = Image.frombytes("P", classes.size, bytes(index.get(v, 0) for v in classes.tobytes()))
    img.putpalette([c for v in (AUTO, DASH, DIAL, LIT, BLADE) for c in PALETTE[v]])
    img.save(path, optimize=True)


def psd_mask(psd):
    layer = find_layer(psd, "mask")
    if layer is None:
        raise ValueError('no layer named "mask"')
    classes = mask_image(psd.width, psd.height, layer.topil(), layer.left, layer.top)
    blades = find_layer(psd, BLADE_LAYERS)
    if blades is not None:   # the wiper blades layer: whatever is painted on it (alpha >= 128, not black) is BLADE, over whatever the mask has there
        px = blades.topil().convert("RGBA").load()
        dst = classes.load()
        for y in range(blades.topil().height):
            for x in range(blades.topil().width):
                tx, ty = blades.left + x, blades.top + y
                r, g, b, a = px[x, y]
                if a >= 128 and max(r, g, b) >= 64 and 0 <= tx < psd.width and 0 <= ty < psd.height:
                    dst[tx, ty] = BLADE
    return classes


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
        psd = PSDImage.open(psd_path)
        classes = psd_mask(psd)
        folder = car.upper()   # the game looks a dash up by its stem, upper case (dash_mask.c)
        os.makedirs(os.path.join(dest, folder), exist_ok=True)
        path = os.path.join(dest, folder, PNG_NAME)
        save_mask(classes, path)
        count += 1
        print(path)
        colour_path = os.path.join(dest, folder, COLOR_PNG_NAME)
        colours = psd_color(psd)
        if colours is not None:
            colours.save(colour_path, optimize=True)
            print(colour_path)
        elif os.path.isfile(colour_path):
            os.remove(colour_path)   # the PSD has no colour layer now
    print(count, "masks")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
