#!/usr/bin/env python3
"""What is left of the original cars' masks and dashboards: one row per car, from the owner's PSD folder and the port's data folder.
Read only. Stdlib only.

  masks_status.py [--art DIR] [--data DIR] [--todo]

--art   the owner's PSDs, <CAR>/<tex>.psd and <CAR>/dash.psd (default ~/workspace/bytes-n-rust/art/tnfs-masks)
--data  the port's data folder: car_masks/<CAR>/<tex>_mask.png and dash_masks/<CAR>/dash_mask.png, dash_color.png (default: data/ of this repository)
--todo  only the cars that are not done

A car is done when every PSD texture has its mask PNG and the PNG is not older than the PSD (tools/car_mask_from_psd.py converts), and the
eight cars you drive also have a dash: its PSD, its dash_mask.png (not older than the PSD) and its dash_color.png (the colour file is optional in the game, orange without it; here it is asked so the list shows which dashes still glow orange). Names match without case
(the PSD folder says TSupra, dash_masks says TSUPRA). A backup such as frnt.before-1603.psd is not a texture."""
import argparse
import os
import sys

DRIVABLE = {"ansx", "czr1", "dviper", "f512tr", "ldiabl", "mrx7", "p911", "tsupra"}


def mtime(path):
    try:
        return os.stat(path).st_mtime
    except OSError:
        return None


def folders(root):
    """{lowercase name: real name} of the folders in root; {} when there is none"""
    try:
        return {n.lower(): n for n in sorted(os.listdir(root)) if os.path.isdir(os.path.join(root, n))}
    except OSError:
        return {}


def psd_textures(folder):
    """{texture: mtime} of the texture PSDs: not the dash, not a backup (a name with a second dot)"""
    out = {}
    if folder is None:
        return out
    for n in sorted(os.listdir(folder)):
        stem, ext = os.path.splitext(n)
        if ext.lower() == ".psd" and "." not in stem and stem.lower() != "dash":
            out[stem] = mtime(os.path.join(folder, n))
    return out


def judge(art, data, key, names):
    art_dir = os.path.join(art, names["art"]) if names["art"] else None
    mask_dir = os.path.join(data, "car_masks", names["masks"]) if names["masks"] else None
    dash_dir = os.path.join(data, "dash_masks", names["dash"]) if names["dash"] else None
    psds = psd_textures(art_dir)
    todo = []
    if not psds:
        todo.append("no PSD")
    pngs = 0
    if mask_dir:
        pngs = sum(1 for n in os.listdir(mask_dir) if n.endswith("_mask.png"))
    missing = [t for t in psds if mask_dir is None or mtime(os.path.join(mask_dir, t + "_mask.png")) is None]
    stale = [t for t, m in psds.items() if t not in missing and mtime(os.path.join(mask_dir, t + "_mask.png")) < m]
    if stale:
        todo.append("stale: " + ", ".join(stale))
    if missing:
        todo.append("no mask: " + ", ".join(missing))
    if key in DRIVABLE:
        dash_psd = mtime(os.path.join(art_dir, "dash.psd")) if art_dir else None
        dmask = mtime(os.path.join(dash_dir, "dash_mask.png")) if dash_dir else None
        dcolor = mtime(os.path.join(dash_dir, "dash_color.png")) if dash_dir else None
        if dash_psd is None:
            todo.append("no dash PSD")
        if dmask is None:
            todo.append("dash mask")
        elif dash_psd is not None and dmask < dash_psd:
            todo.append("stale: dash")
        if dcolor is None:
            todo.append("dash colour")
    return len(psds), pngs, todo


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description="what is left of the original cars' masks and dashboards")
    ap.add_argument("--art", default=os.path.join(os.path.expanduser("~"), "workspace", "bytes-n-rust", "art", "tnfs-masks"))
    ap.add_argument("--data", default=os.path.join(here, "..", "data"))
    ap.add_argument("--todo", action="store_true")
    args = ap.parse_args()
    if not os.path.isdir(args.art):
        print("masks_status: the PSD folder %s is not there (--art)" % args.art, file=sys.stderr)
        return 2
    if not os.path.isdir(args.data):
        print("masks_status: the data folder %s is not there (--data)" % args.data, file=sys.stderr)
        return 2
    art_f, mask_f, dash_f = folders(args.art), folders(os.path.join(args.data, "car_masks")), folders(os.path.join(args.data, "dash_masks"))
    keys = sorted(set(art_f) | set(mask_f) | set(dash_f))
    rows, done = [], 0
    for k in keys:
        names = {"art": art_f.get(k), "masks": mask_f.get(k), "dash": dash_f.get(k)}
        shown = names["masks"] or names["art"] or names["dash"]
        n_psd, n_png, todo = judge(args.art, args.data, k, names)
        done += not todo
        if todo or not args.todo:
            rows.append("%-10s %2d PSD %2d PNG  %s" % (shown, n_psd, n_png, "done" if not todo else "; ".join(todo)))
    print("\n".join(rows))
    print("done %d of %d cars" % (done, len(keys)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
