#!/usr/bin/env python3
"""Layered PSD drafts for the hand-made dash masks (game/race/dash_mask.h), the dash's 2D layer as the game draws it.

Each dash becomes one PSD at 640 x 480, 8-bit RGB, with two layers:
  dash   the dash's art (the windshield black), locked
  mask   the class of a pixel, at layer opacity 50%:
           transparent   not marked: the cockpit's own gauge ellipses decide, as without a mask
           green (0,255,0)     inside a dial: its bright marks (numerals, ticks) are lit, its dark face is not
           blue (0,0,255)      the dash's art: never lit (the speedo's rim)
           white               lit whatever its brightness (a warning lamp)
The draft's green is the cockpit's two gauge ellipses (the game's own guess): paint it to the dials' real outline, and the dials the game does not find (the small
gauges), then tools/dash_mask_from_psd.py reads the "mask" layer into data/dash_masks/<CAR>/dash_mask.png.

usage: dash_mask_psd.py <dump dir> <output dir>
<dump dir> holds <STEM>.ppm (the dash) and <STEM>.mask.png (the draft classes) per dash, written by the game with TNFS_DASH_DUMP=<dump dir> (a night race in a car's cockpit).
Writes <output dir>/<CAR>/dash.psd: <CAR> is the output dir's own folder for that stem when there is one (any case), else the stem. A file already there is never touched:
the draft is written as dash_draft.psd, dash_draft_2.psd, ... instead. Needs Pillow; writes the PSD by hand (car_mask_psd.py's writer).
"""
import importlib.util
import os
import sys

from PIL import Image

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("car_mask_psd", os.path.join(here, "car_mask_psd.py"))
writer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(writer)

# dash_mask.h's classes and the colour each has in the mask layer (dash_mask_from_psd.py reads them back); AUTO is transparent
DASH, DIAL, LIT = 64, 128, 255
CLASS_COLOUR = {DASH: (0, 0, 255), DIAL: (0, 255, 0), LIT: (255, 255, 255)}


def class_of(value):
    """A draft PNG's grey/colour index as the class it stands for."""
    return value if value in CLASS_COLOUR else 0


def mask_planes(classes):
    """The mask layer's alpha and red, green and blue planes (bytes) for `classes` (a pixel's class a byte each)."""
    colours = [CLASS_COLOUR.get(c, (0, 0, 0)) for c in classes]
    alpha = bytes(255 if c in CLASS_COLOUR else 0 for c in classes)
    return (alpha,) + tuple(bytes(colour[k] for colour in colours) for k in range(3))


def classes_of_png(path):
    """The class image ('L') of a draft mask PNG, by its colours as dash_mask.c reads them."""
    img = Image.open(path).convert("RGB")
    out = Image.new("L", img.size, 0)
    px, dst = img.load(), out.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = px[x, y]
            dst[x, y] = LIT if min(r, g, b) >= 192 else DASH if b > r and b > g and max(r, g, b) >= 64 else DIAL if g > r and g > b and max(r, g, b) >= 64 else 0
    return out


def free_name(folder):
    """dash.psd, or the first of dash_draft.psd, dash_draft_2.psd, ... that does not exist: a file there is the owner's."""
    path = os.path.join(folder, "dash.psd")
    n = 1
    while os.path.exists(path):
        path = os.path.join(folder, "dash_draft.psd" if n == 1 else "dash_draft_%d.psd" % n)
        n += 1
    return path


def folder_for(out_dir, stem):
    """The output dir's own folder for the stem (any case), else the stem."""
    if os.path.isdir(out_dir):
        for name in sorted(os.listdir(out_dir)):
            if name.lower() == stem.lower() and os.path.isdir(os.path.join(out_dir, name)):
                return os.path.join(out_dir, name)
    return os.path.join(out_dir, stem)


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    dump, out_dir = argv[1], argv[2]
    writer.mask_planes = mask_planes   # the PSD writer's mask layer, in the dash's colours
    count = 0
    for name in sorted(os.listdir(dump)):
        if not name.endswith(".ppm") or name.startswith("shot"):
            continue
        stem = name[: -len(".ppm")]
        mask_path = os.path.join(dump, stem + ".mask.png")
        if not os.path.isfile(mask_path):
            continue
        rgb = Image.open(os.path.join(dump, name)).convert("RGB")
        keep = classes_of_png(mask_path)
        folder = folder_for(out_dir, stem)
        os.makedirs(folder, exist_ok=True)
        path = free_name(folder)
        writer.write_psd(path, rgb, keep, "dash")
        count += 1
        print(path)
    print(count, "files")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
