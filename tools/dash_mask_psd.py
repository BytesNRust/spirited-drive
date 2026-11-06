#!/usr/bin/env python3
"""Layered PSD drafts for the hand-made dash masks (game/race/dash_mask.h), the dash's 2D layer as the game draws it.

Each dash becomes one PSD at 640 x 480, 8-bit RGB, with these layers (the first is the bottom one):
  dash   the whole dash as if there were no steering wheel (the windshield black), locked: what the wheel covers in the straight pose is rebuilt from the steering frames,
         where some pose shows it (tools/dash_rebuild.py), the dials the game draws over it (tach, sped) are in; what no pose ever shows (the rim, the hub) is flat magenta
  volante (guía)   the straight-ahead wheel the dash's art holds, hidden and locked: switch it on to see what the wheel covers (the converter reads only "mask")
  nunca visible    the pixels no pose shows (magenta, at 50%), locked: what is painted there does not matter in the game
  mask   the class of a pixel, at layer opacity 50%:
           transparent   not marked: the cockpit's own gauge ellipses decide, as without a mask
           green (0,255,0)     inside a dial: its bright marks (numerals, ticks) are lit, its dark face is not
           blue (0,0,255)      the dash's art: never lit (the speedo's rim)
           white (yellow ffff00 reads the same, and contrasts better)   lit whatever its brightness (a warning lamp)
The draft's green is the cockpit's two gauge ellipses (the game's own guess): paint it to the dials' real outline, and the dials the game does not find (the small
gauges), then tools/dash_mask_from_psd.py reads the "mask" layer into data/dash_masks/<CAR>/dash_mask.png.

usage: dash_mask_psd.py <dump dir> <output dir>
       dash_mask_psd.py --update <dump dir> <PSD dir> <output dir>
<dump dir> holds <STEM>.ppm (the dash) and <STEM>.mask.png (the draft classes) per dash, written by the game with TNFS_DASH_DUMP=<dump dir> (a night race in a car's cockpit),
and the steering frames the dash is rebuilt from (<STEM>.art.pgm, <STEM>.w<row>_<frame>.pgm, <STEM>.dials.pgm, <STEM>.pal).
Writes <output dir>/<CAR>/dash.psd: <CAR> is the output dir's own folder for that stem when there is one (any case), else the stem. A file already there is never touched:
the draft is written as dash_draft.psd, dash_draft_2.psd, ... instead. Needs Pillow; writes the PSD by hand (car_mask_psd.py's writer).
--update: for each stem, reads <PSD dir>/<CAR>/dash.psd (the owner's, with the "mask" and "color" layers he has painted) and writes <output dir>/<CAR>/dash.psd with only
its "dash" and "volante" layers rewritten ("nunca visible" is added or rewritten): every other layer is copied byte for byte (tools/psd_edit.py). The output dir must not be
the PSD dir: the owner's files are never written.
"""
import importlib.util
import os
import struct
import sys

from PIL import Image

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("car_mask_psd", os.path.join(here, "car_mask_psd.py"))
writer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(writer)


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, os.path.join(here, name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


rebuild_tool = load_tool("dash_rebuild")
psd_edit = load_tool("psd_edit")
WIDTH, HEIGHT = 640, 480
MAGENTA = (255, 0, 255)
GUIDE_NAME, NEVER_NAME = "volante (guía)", "nunca visible"
NEVER_OPACITY = 128

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


def pgm(path):
    """A dump's PGM as a list of bytes."""
    return list(Image.open(path).convert("L").tobytes())


def has_frames(dump, stem):
    return os.path.isfile(os.path.join(dump, stem + ".art.pgm")) and os.path.isfile(os.path.join(dump, stem + ".pal"))


def layers_from_frames(dump, stem):
    """(dash RGB, guide (RGB, alpha), never alpha) for a dash dumped with its steering frames: the dash rebuilt without the wheel (dash_rebuild.py), the dials over it, the
    never-visible pixels flat magenta; the guide the art where the straight wheel was (what changed or is never visible); the alpha 255 on the never-visible pixels."""
    art = pgm(os.path.join(dump, stem + ".art.pgm"))
    pal = open(os.path.join(dump, stem + ".pal"), "rb").read()
    frames = [pgm(os.path.join(dump, "%s.w%d_%d.pgm" % (stem, row, frame))) for row in (0, 1) for frame in range(8)
              if os.path.isfile(os.path.join(dump, "%s.w%d_%d.pgm" % (stem, row, frame)))]
    dials = pgm(os.path.join(dump, stem + ".dials.pgm")) if os.path.isfile(os.path.join(dump, stem + ".dials.pgm")) else [255] * len(art)
    dash, never = rebuild_tool.rebuild(art, frames, WIDTH, HEIGHT)

    def colour(index):
        return (0, 0, 0) if index == 255 else tuple(pal[3 * index: 3 * index + 3])

    rgb, guide_rgb, guide_alpha = [], [], []
    for i, index in enumerate(dash):
        rgb.append(MAGENTA if never[i] else colour(dials[i] if dials[i] != 255 else index))
        wheel = never[i] or dash[i] != art[i]
        guide_rgb.append(colour(art[i]) if wheel else (0, 0, 0))
        guide_alpha.append(255 if wheel and art[i] != 255 else 0)
    dash_img = Image.new("RGB", (WIDTH, HEIGHT))
    dash_img.putdata(rgb)
    guide_img = Image.new("RGB", (WIDTH, HEIGHT))
    guide_img.putdata(guide_rgb)
    guide_a = Image.new("L", (WIDTH, HEIGHT))
    guide_a.putdata(guide_alpha)
    never_a = Image.new("L", (WIDTH, HEIGHT))
    never_a.putdata([255 if x else 0 for x in never])
    return dash_img, (guide_img, guide_a), never_a


def solid(colour, alpha):
    return Image.merge("RGB", [Image.new("L", alpha.size, c) for c in colour]), alpha


def new_layer(name, opacity, planes, hidden=False):
    """An psd_edit.Layer for a layer the PSD does not have yet, over the whole canvas, raw, locked like the writer's."""
    extra = struct.pack(">I", 0) + struct.pack(">I", 0) + writer.pascal(name, 4)
    extra += b"8BIM" + b"lspf" + struct.pack(">II", 4, writer.LOCK_ALL)
    ids = [-1, 0, 1, 2]
    return psd_edit.Layer((0, 0, HEIGHT, WIDTH), [(c, 2 + WIDTH * HEIGHT) for c in ids], b"norm", opacity, 0, 2 if hidden else 0, extra,
                          [struct.pack(">H", 0) + planes[c] for c in ids])


def update_psd(source, target, dash_img, guide, never_a):
    """Writes `target` from the owner's PSD `source`: its "dash", "volante" and "nunca visible" layers replaced, every other layer as it was."""
    psd = psd_edit.read_psd(source)
    if (psd.width, psd.height) != (WIDTH, HEIGHT):
        raise ValueError("%s is %d x %d, not %d x %d" % (source, psd.width, psd.height, WIDTH, HEIGHT))
    names = [layer.name for layer in psd.layers]
    if "dash" not in names:
        raise ValueError("%s has no layer named dash" % source)
    dash_planes = dict(zip((0, 1, 2), (band.tobytes() for band in dash_img.split())))
    dash_planes[-1] = b"\xff" * (WIDTH * HEIGHT)   # in case the owner's dash layer has an alpha channel
    guide_rgb, guide_alpha = guide
    guide_planes = dict(zip((-1, 0, 1, 2), (guide_alpha.tobytes(),) + tuple(band.tobytes() for band in guide_rgb.split())))
    magenta, _ = solid(MAGENTA, never_a)
    never_planes = dict(zip((-1, 0, 1, 2), (never_a.tobytes(),) + tuple(band.tobytes() for band in magenta.split())))
    layers = []
    for layer in psd.layers:
        if layer.name == "dash":
            layers.append(layer.replace(dash_planes, WIDTH, HEIGHT))
            if not any(n.startswith("volante") for n in names):
                layers.append(new_layer(GUIDE_NAME, 255, guide_planes, hidden=True))
            if NEVER_NAME not in names and not any(n.startswith("volante") for n in names):
                layers.append(new_layer(NEVER_NAME, NEVER_OPACITY, never_planes))
        elif layer.name.startswith("volante"):
            layers.append(layer.replace(guide_planes, WIDTH, HEIGHT, hidden=True))
            if NEVER_NAME not in names:
                layers.append(new_layer(NEVER_NAME, NEVER_OPACITY, never_planes))
        elif layer.name == NEVER_NAME:
            layers.append(layer.replace(never_planes, WIDTH, HEIGHT))
        else:
            layers.append(layer)
    psd.layers = layers
    psd_edit.write_psd(target, psd)


def update(dump, psd_dir, out_dir):
    """--update: the owner's PSDs in psd_dir rewritten (dash and guide layers only) into out_dir."""
    if os.path.realpath(psd_dir) == os.path.realpath(out_dir):
        print("the output dir must not be the PSD dir: the owner's files are never written")
        return 2
    count = 0
    for name in sorted(os.listdir(dump)):
        if not name.endswith(".ppm") or name.startswith("shot"):
            continue
        stem = name[: -len(".ppm")]
        if not has_frames(dump, stem):
            continue
        folder = folder_for(psd_dir, stem)
        source = os.path.join(folder, "dash.psd")
        if not os.path.isfile(source):
            print("no", source)
            continue
        target_dir = os.path.join(out_dir, os.path.basename(folder))
        os.makedirs(target_dir, exist_ok=True)
        target = os.path.join(target_dir, "dash.psd")
        dash_img, guide, never_a = layers_from_frames(dump, stem)
        update_psd(source, target, dash_img, guide, never_a)
        count += 1
        print(target)
    print(count, "files")
    return 0


def main(argv):
    if len(argv) == 5 and argv[1] == "--update":
        return update(argv[2], argv[3], argv[4])
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
        guide = None
        guide_rgb, zone = os.path.join(dump, stem + ".guide.ppm"), os.path.join(dump, stem + ".zone.pgm")
        if os.path.isfile(guide_rgb) and os.path.isfile(zone):
            guide = (GUIDE_NAME, Image.open(guide_rgb).convert("RGB"), Image.open(zone).convert("L"))
        if has_frames(dump, stem):   # the dash rebuilt without the wheel, the never-visible pixels in their own layer
            rgb, (guide_img, guide_a), never_a = layers_from_frames(dump, stem)
            magenta, _ = solid(MAGENTA, never_a)
            guide = [(GUIDE_NAME, guide_img, guide_a), (NEVER_NAME, magenta, never_a, False, NEVER_OPACITY)]
        writer.write_psd(path, rgb, keep, "dash", guide)
        count += 1
        print(path)
    print(count, "files")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
