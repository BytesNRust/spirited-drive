#!/usr/bin/env python3
"""Layered PSDs for the reflective masks of a track's textures (game/race/track_mask.h).

Each texture becomes one PSD at its native size, 8-bit RGB, with two layers:
  texture  the original texture (locked)
  mask     empty, at layer opacity 50%: paint over what is reflective (road signs, posts, the cat's eyes on the asphalt) in any colour but black.
tools/track_mask_from_psd.py reads the mask layer back into data/track_masks/<TRACK>/<name>.png.

usage: track_mask_psd.py <drafts dir> <output dir>
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


def write_track_psd(path, rgba):
    """One PSD: `rgba` (an RGBA or RGB Image) as the locked texture layer, an empty mask layer over it. A transparent texel of the texture shows as its colour."""
    rgb = rgba.convert("RGB")
    _writer.write_psd(path, rgb, Image.new("L", rgb.size, 0))   # class 0 everywhere: the mask layer is fully transparent


def main(argv):
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
            path = os.path.join(out_dir, track, name[: -len(".rgb.png")] + ".psd")
            write_track_psd(path, Image.open(os.path.join(folder, name)))
            count += 1
            print(path)
    print(count, "files")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
