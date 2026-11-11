#!/usr/bin/env python3
"""Layered PSDs of track masks that already exist as PNGs, the lamp masks above all (the tunnel lamps' yellow, tools/track_lamp_marks), for the owner to edit as the others.

Each mask becomes one PSD at the texture's size, 8-bit RGB, with two layers, as tools/track_mask_psd.py writes them:
  texture  the track's texture (locked)
  mask     the PNG's marks at layer opacity 50%: yellow (255, 255, 0) a lamp, white a reflective texel, transparent the rest
tools/track_mask_from_psd.py reads the mask layer back into the same PNG (yellow stays a lamp). The yellow is the masks' palette's (tnfs-mascaras.aco's) lamp colour.

usage: track_lamp_psd.py <textures dir> <masks dir> <output dir> TRACK/slot_NN ...
<textures dir>/<TRACK>/<name>.rgb.png come from track_mask_draft (the game's pixels: they stay out of the repository); <masks dir> is data/track_masks (or a mod's
track_masks/); writes <output dir>/<TRACK>/<name>.psd. A mask the size of its texture only. Needs Pillow; reuses car_mask_psd.py's writer.
"""
import importlib.util
import os
import sys

from PIL import Image

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("car_mask_psd", os.path.join(_here, "car_mask_psd.py"))
_writer = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_writer)
_spec = importlib.util.spec_from_file_location("track_mask_from_psd", os.path.join(_here, "track_mask_from_psd.py"))
_reader = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_reader)

LAMP = 248     # car_mask_psd.py's class drawn yellow (255, 255, 0)
WHITE = 255    # its class drawn white


def classes_of(mask):
    """The writer's classes for a mask PNG (any mode): a lamp's yellow 248, a reflective texel 255, the rest 0, by track_mask_from_psd.py's reading of a texel."""
    rgba = mask.convert("RGBA")
    out = Image.new("L", rgba.size, 0)
    px, dst = rgba.load(), out.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = px[x, y]
            if _reader.texel_lamp(r, g, b, a):
                dst[x, y] = LAMP
            elif _reader.texel_reflective(r, g, b, a):
                dst[x, y] = WHITE
    return out


def write_lamp_psd(path, texture, mask):
    """One PSD: `texture` (an Image) locked under the marks of `mask` (an Image of the same size). False, and nothing written, when the sizes differ."""
    if texture.size != mask.size:
        return False
    _writer.write_psd(path, texture.convert("RGB"), classes_of(mask))
    return True


def main(argv):
    if len(argv) < 5:
        print(__doc__)
        return 2
    textures, masks, out_dir = argv[1], argv[2], argv[3]
    failed = 0
    for name in argv[4:]:
        track, _, slot = name.partition("/")
        tex_path = os.path.join(textures, track, slot + ".rgb.png")
        mask_path = os.path.join(masks, track, slot + ".png")
        if not os.path.isfile(tex_path) or not os.path.isfile(mask_path):
            print("%s: no %s" % (name, tex_path if not os.path.isfile(tex_path) else mask_path))
            failed += 1
            continue
        os.makedirs(os.path.join(out_dir, track), exist_ok=True)
        path = os.path.join(out_dir, track, slot + ".psd")
        if not write_lamp_psd(path, Image.open(tex_path), Image.open(mask_path)):
            print("%s: the mask is not the texture's size" % name)
            failed += 1
            continue
        print(path)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
