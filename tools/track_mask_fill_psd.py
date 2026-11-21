#!/usr/bin/env python3
"""One-time migration of the original tracks' mask PSDs (dep103): a PSD whose "mask" layer is empty (nothing painted, only the hidden "draft" layer is there) gets its "mask" layer
filled from the mask PNG the game uses today (data/track_masks/<TRACK>/<name>.png: the auto-draft guess), so the PSD becomes a real source that reproduces that PNG.
Every other layer of the PSD is copied byte for byte (tools/psd_edit.py). A PSD whose mask is painted is not touched. Never run it on the owner's art/ folder: it refuses a folder that is
not a workspace's <TRACK>/art-source/masks (it writes in place, and the copies in the workspace are what it is for).

The filled layer is what track_mask_from_psd.py reads back: white opaque for a reflective texel, yellow (255, 255, 0) opaque for a lamp's mark, transparent elsewhere; the layer's own
opacity (50% in the PSDs track_mask_psd.py writes) stays as it was.

usage: track_mask_fill_psd.py <workspace tracks dir (<TRACK>/art-source/masks/<name>.psd)> <data/track_masks> [--check]
--check only lists what would be filled. Prints one line per PSD ("filled", "painted", "no PNG", "size") and the totals. Needs Pillow.
"""
import importlib.util
import os
import sys

from PIL import Image

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("psd_edit", os.path.join(_here, "psd_edit.py"))
psd_edit = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(psd_edit)

ALPHA_FROM, PAINT_FROM = 128, 64   # tools/track_mask_from_psd.py's thresholds


def mask_layer(psd):
    for layer in psd.layers:
        if layer.name.strip().lower() == "mask":
            return layer
    return None


def painted(layer, width, height):
    """Whether the layer has a texel the converter reads as reflective or a lamp."""
    planes = {cid: layer.plane(cid, width, height) for cid in (0, 1, 2, -1)}
    if planes[0] is None:
        return False
    for i in range(width * height):
        a = planes[-1][i] if planes[-1] is not None else 255
        if a >= ALPHA_FROM and max(planes[0][i], planes[1][i], planes[2][i]) >= PAINT_FROM:
            return True
    return False


def planes_from_png(path):
    """{channel id: bytes} of the layer to write for a data PNG: white reflective, yellow lamp, transparent rest."""
    image = Image.open(path).convert("RGBA")
    r, g, b, a = ([], [], [], [])
    for pr, pg, pb, pa in image.getdata():
        if pa != 0 and pr >= 128 and pg >= 128 and pb < 128:
            px = (255, 255, 0, 255)
        elif pa != 0 and pr >= 128:
            px = (255, 255, 255, 255)
        else:
            px = (0, 0, 0, 0)
        r.append(px[0]), g.append(px[1]), b.append(px[2]), a.append(px[3])
    return image.size, {0: bytes(r), 1: bytes(g), 2: bytes(b), -1: bytes(a)}


def main(argv):
    args = [x for x in argv[1:] if x != "--check"]
    check = "--check" in argv
    if len(args) != 2:
        print(__doc__)
        return 2
    root, masks = args
    filled = kept = bad = 0
    for track in sorted(os.listdir(root)):
        folder = os.path.join(root, track, "art-source", "masks")
        if not os.path.isdir(folder):
            continue
        for name in sorted(os.listdir(folder)):
            if not name.endswith(".psd"):
                continue
            path = os.path.join(folder, name)
            label = "%s/%s" % (track, name)
            psd = psd_edit.read_psd(path)
            layer = mask_layer(psd)
            if layer is None:
                print("no mask layer  ", label)
                bad += 1
                continue
            if painted(layer, psd.width, psd.height):
                print("painted        ", label)
                kept += 1
                continue
            png = os.path.join(masks, track, name[:-4] + ".png")
            if not os.path.exists(png):
                print("no PNG         ", label)
                bad += 1
                continue
            size, planes = planes_from_png(png)
            if size != (psd.width, psd.height):
                print("size           ", label, size, "vs", (psd.width, psd.height))
                bad += 1
                continue
            print("mask layer filled from data/", label)
            filled += 1
            if not check:
                psd.layers = [layer.replace(planes, psd.width, psd.height) if l is layer else l for l in psd.layers]
                psd_edit.write_psd(path, psd)
    print("%d filled from data/, %d already painted, %d not done" % (filled, kept, bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
