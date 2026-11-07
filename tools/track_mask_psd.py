#!/usr/bin/env python3
"""Layered PSDs for the reflective masks of a track's textures (game/race/track_mask.h).

Each texture becomes one PSD at its native size, 8-bit RGB, with three layers:
  texture  the original texture (locked)
  mask     empty, at layer opacity 50%: paint over what is reflective (road signs, posts, the cat's eyes on the asphalt) in any colour but black. This one is the owner's.
  draft    hidden, over the mask: a first guess (tools/track_mask_auto.py), white where it thinks the texture is reflective. Copy it into `mask` or touch it up; it is never read.
tools/track_mask_from_psd.py reads the mask layer back into data/track_masks/<TRACK>/<name>.png (and nothing else).

usage: track_mask_psd.py <drafts dir> <output dir> [--png <dir>]
--png also writes each guess as <dir>/<TRACK>/<name>.png (white reflective, black not: the format the game reads), to try the masks without painting. They are for trying only.
<drafts dir>/<TRACK>/slot_<NN>.rgb.png (a road texture slot) and scenery_<NN>.rgb.png (a scenery board) come from track_mask_draft (the game's pixels: they stay out of the
repository). Writes <output dir>/<TRACK>/<name>.psd. Needs Pillow; reuses car_mask_psd.py's writer (no other dependency).
"""
import importlib.util
import os
import sys

from PIL import Image

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("car_mask_psd", os.path.join(_here, "car_mask_psd.py"))
_writer = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_writer)
_spec = importlib.util.spec_from_file_location("track_mask_auto", os.path.join(_here, "track_mask_auto.py"))
_auto = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_auto)


def write_track_psd(path, rgba, draft=None):
    """One PSD: `rgba` (an RGBA or RGB Image) as the locked texture layer, an empty mask layer over it, and, when `draft` (an 'L' Image of the texture's size, 255 reflective) is given,
    a hidden `draft` layer over the mask holding it in white. A transparent texel of the texture shows as its colour."""
    rgb = rgba.convert("RGB")
    above = None
    if draft is not None:
        above = [("draft", Image.new("RGB", rgb.size, (255, 255, 255)), draft.point(lambda v: 255 if v else 0))]
    _writer.write_psd(path, rgb, Image.new("L", rgb.size, 0), above=above)   # class 0 everywhere: the mask layer is fully transparent


def main(argv):
    png_dir = None
    if len(argv) == 5 and argv[3] == "--png":
        png_dir = argv[4]
        argv = argv[:3]
    if len(argv) != 3:
        print(__doc__)
        return 2
    drafts, out_dir = argv[1], argv[2]
    count = 0
    for track in sorted(os.listdir(drafts)):
        folder = os.path.join(drafts, track)
        if not os.path.isdir(folder):
            continue
        os.makedirs(os.path.join(out_dir, track), exist_ok=True)
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".rgb.png"):
                continue
            stem = name[: -len(".rgb.png")]
            path = os.path.join(out_dir, track, stem + ".psd")
            texture = Image.open(os.path.join(folder, name))
            guess = _auto.auto_mask(stem, texture)
            write_track_psd(path, texture, guess)
            if png_dir is not None:
                os.makedirs(os.path.join(png_dir, track), exist_ok=True)
                guess.save(os.path.join(png_dir, track, stem + ".png"), optimize=True)
            count += 1
            print(path)
    print(count, "files")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
