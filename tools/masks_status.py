#!/usr/bin/env python3
"""What is left of the original cars' masks and dashboards: one row per car, from the owner's PSD folder and the port's data folder.
Read only. Stdlib only.

  masks_status.py [--art DIR] [--data DIR] [--todo]

--art   the owner's PSDs, <CAR>/<tex>.psd and <CAR>/dash.psd (default ~/workspace/bytes-n-rust/art/tnfs-masks)
--data  the port's data folder: car_masks/<CAR>/<tex>_mask.png and dash_masks/<CAR>/dash_mask.png, dash_color.png (default: data/ of this repository)
--todo  only the cars that are not done

A car is done when every PSD texture has its mask PNG and the PNG is not older than the PSD (tools/car_mask_from_psd.py converts), and the
eight cars you drive also have a dash: its PSD, its dash_mask.png (not older than the PSD) and its dash_color.png (the colour file is optional in the game, orange without it; here it is asked so the list shows which dashes still glow orange). Names match without case
(the PSD folder says TSupra, dash_masks says TSUPRA). A backup such as frnt.before-1603.psd is not a texture. Textures and files are matched without case too. The stale check goes by file times, which a sync or a checkout of data/ resets: a PNG copied after its PSD never shows as stale. The PNG column counts every *_mask.png of the car."""
import argparse
import os
import sys

DRIVABLE = {"ansx", "czr1", "dviper", "f512tr", "ldiabl", "mrx7", "p911", "tsupra"}


def folders(root):
    """{lowercase name: real name} of the folders in root (not ._-led ones); {} when there is none or it cannot be read"""
    try:
        names = sorted(os.listdir(root))
    except OSError:
        return {}
    return {n.lower(): n for n in names if not n.startswith((".", "_")) and os.path.isdir(os.path.join(root, n))}


def files(folder):
    """{lowercase name: (real name, mtime)} of the regular files in folder; None when it is not there or cannot be read"""
    if folder is None:
        return None
    try:
        names = sorted(os.listdir(folder))
    except OSError:
        return None
    out = {}
    for n in names:
        path = os.path.join(folder, n)
        if os.path.isfile(path):
            try:
                out.setdefault(n.lower(), (n, os.stat(path).st_mtime))
            except OSError:
                pass
    return out


def psd_textures(art_files):
    """{texture (lowercase): mtime} of the texture PSDs: not the dash, not a backup (a name with a second dot)"""
    out = {}
    for lower, (name, m) in (art_files or {}).items():
        stem, ext = os.path.splitext(lower)
        if ext == ".psd" and "." not in stem and stem != "dash":
            out[stem] = m
    return out


def judge(data, key, names, art_files, mask_files, dash_files):
    psds = psd_textures(art_files)
    todo = []
    for what, entries, shown in (("art", art_files, names["art"]), ("masks", mask_files, names["masks"]), ("dash", dash_files, names["dash"])):
        if shown is not None and entries is None:
            todo.append("cannot read the %s folder" % what)
    if not psds and art_files is not None:
        todo.append("no PSD")
    masks = {k: m for k, (n, m) in (mask_files or {}).items() if k.endswith("_mask.png")}
    wanted = {t: masks.get(t + "_mask.png") for t in psds}
    missing = [t for t, m in wanted.items() if m is None]
    stale = [t for t, m in wanted.items() if m is not None and m < psds[t]]
    if stale:
        todo.append("stale: " + ", ".join(stale))
    if missing:
        todo.append("no mask: " + ", ".join(missing))
    if key in DRIVABLE:
        dash_psd = (art_files or {}).get("dash.psd", (None, None))[1]
        dmask = (dash_files or {}).get("dash_mask.png", (None, None))[1]
        dcolor = (dash_files or {}).get("dash_color.png", (None, None))[1]
        if dash_psd is None:
            todo.append("no dash PSD")
        if dmask is None:
            todo.append("dash mask")
        elif dash_psd is not None and dmask < dash_psd:
            todo.append("stale: dash")
        if dcolor is None:
            todo.append("dash colour")
    return len(psds), len(masks), todo


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
    art_f = {k: n for k, n in art_f.items() if k in mask_f or any(x.lower().endswith(".psd") for x in (files(os.path.join(args.art, n)) or {}))}   # tracks/ and the like hold no car
    if not os.path.isdir(os.path.join(args.data, "car_masks")):
        print("masks_status: %s has no car_masks folder (--data)" % args.data, file=sys.stderr)
        return 2
    keys = sorted(set(art_f) | set(mask_f))   # a car is one that has PSDs or masks: a dash folder alone does not make one
    rows, done = [], 0
    for k in keys:
        names = {"art": art_f.get(k), "masks": mask_f.get(k), "dash": dash_f.get(k)}
        shown = names["masks"] or names["art"]
        art_files = files(os.path.join(args.art, names["art"])) if names["art"] else None
        mask_files = files(os.path.join(args.data, "car_masks", names["masks"])) if names["masks"] else None
        dash_files = files(os.path.join(args.data, "dash_masks", names["dash"])) if names["dash"] else None
        n_psd, n_png, todo = judge(args.data, k, names, art_files, mask_files, dash_files)
        if names["art"] is None:
            todo.insert(0, "no PSD")
        done += not todo
        if todo or not args.todo:
            rows.append("%-10s %2d PSD %2d PNG  %s" % (shown, n_psd, n_png, "done" if not todo else "; ".join(todo)))
    if rows:
        print("\n".join(rows))
    print("done %d of %d cars" % (done, len(keys)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
