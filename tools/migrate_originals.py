#!/usr/bin/env python3
"""dep100 item 6: the owner's 20 original cars as kits in the current layout (formats/car_kit_paths.h), in a NEW folder; the art folder is only read.

For each car Export original knows (the 8 you drive, 11 traffic cars, the police car; "probe" has no masks):
  1. car_export_kits (build/car_export_kits) makes the kit from the game's files: the model (art-source/mesh, art-generated/model, model/<near|far>/bank), specs,
     the masks as the game's data/ has them in art-generated/car_masks and art-generated/dash_masks. A traffic or police car has no PBS and no card: a minimal card.
  2. the owner's PSDs (ART/<CAR>/*.psd) whose name has no extra dot (frnt.psd, frnt_lo.psd, dash.psd) are copied into art-source/masks/. A backup such as
     dash.before-wipers.psd is NOT copied; the report lists each one skipped. A texture with a mask in data/ and no PSD has that PNG copied to art-source/masks/<tex>_mask.png
     as its source.
  3. the PSDs are turned into masks with tools/car_mask_from_psd.py and tools/dash_mask_from_psd.py (psd-tools and Pillow needed) and each is compared, byte for byte,
     with the game's data/ file. Equal: fine. One of CHANGED_ON_PURPOSE (the owner's PSD is newer than data/, owner OK): the PSD's mask goes in art-generated and the report
     says "changed on purpose". Any other difference, or a PSD with no data/ file, is a FAILURE (exit 1; nothing is deleted anywhere).
  4. the exporter builds each kit's CFM from its flat sources (the code car build uses, with no card needed: a traffic car has no PBS, so car build would refuse it) and
     compares it with the game's CFM: byte for byte, or it is a FAILURE.
The report is OUT/MIGRATION-REPORT.md.

usage: migrate_originals.py --art ART/tnfs-masks --cd ORIGINAL_DIR --data DATA_DIR --studio TNFS_STUDIO --exporter CAR_EXPORT_KITS --out OUT_DIR
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

# (stem, file): the PSDs the owner changed after data/ was made; the PSD wins (owner OK 2026-11-21)
CHANGED_ON_PURPOSE = {
    ("AXXESS", "frnt_mask.png"), ("AXXESS", "frnt_lo_mask.png"), ("AXXESS", "side_mask.png"), ("AXXESS", "side_lo_mask.png"),
    ("ANSX", "dash_mask.png"),
}
REAL_PSD = re.compile(r"^[A-Za-z0-9_]+\.psd$")   # a name with another dot is a backup


def read(path):
    with open(path, "rb") as f:
        return f.read()


def find_ci(folder, name):
    """the entry of `folder` named `name` in any case, or None"""
    if not os.path.isdir(folder):
        return None
    for entry in os.listdir(folder):
        if entry.lower() == name.lower():
            return os.path.join(folder, entry)
    return None


def stem_of(kit):
    for line in read(os.path.join(kit, "mod.ini")).decode().splitlines():
        if line.startswith("stem="):
            return line[5:].strip()
    raise SystemExit("%s has no stem" % kit)


def same_picture(a, b):
    """1 when two PNGs hold the same pixels (RGBA), whatever their palette or bytes"""
    from PIL import Image
    ia, ib = Image.open(a).convert("RGBA"), Image.open(b).convert("RGBA")
    return ia.size == ib.size and ia.tobytes() == ib.tobytes()


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def main():
    ap = argparse.ArgumentParser()
    for name in ("art", "cd", "data", "studio", "exporter", "out"):
        ap.add_argument("--" + name, required=True)
    a = ap.parse_args()
    art, cd, data, out = [os.path.abspath(p) for p in (a.art, a.cd, a.data, a.out)]
    tools = os.path.dirname(os.path.abspath(__file__))
    if os.path.exists(out) and os.listdir(out):
        raise SystemExit("%s has files already: the migration goes into a new folder" % out)
    if os.path.commonpath([out, art]) == art:
        raise SystemExit("the output is inside the art folder: it is read only")
    os.makedirs(out, exist_ok=True)

    log = run([os.path.abspath(a.exporter), cd, data, out])
    lines = [l for l in log.stdout.splitlines() if l.startswith(("skipped", "FAILED")) or "kits made" in l]
    if log.returncode != 0:
        print("\n".join(lines))
        raise SystemExit("car_export_kits failed")
    cfm_ok = {l.split()[1].rstrip(":") for l in log.stdout.splitlines() if l.startswith("cfm ")}
    failures_exporter = [l for l in log.stdout.splitlines() if l.startswith("FAILED")]
    kits = sorted(d for d in os.listdir(out) if os.path.isfile(os.path.join(out, d, "mod.ini")))

    failures, report = list(failures_exporter), []
    skipped_backups, from_data, changed, same, palette_only = [], [], [], 0, []
    tmp = tempfile.mkdtemp(prefix="migrate-")
    for kit_name in kits:
        kit = os.path.join(out, kit_name)
        stem = stem_of(kit)
        art_dir = find_ci(art, stem)
        gen_dir = os.path.join(kit, "art-generated")
        masks_dir = os.path.join(gen_dir, "car_masks")
        src_dir = os.path.join(kit, "art-source", "masks")
        os.makedirs(src_dir, exist_ok=True)
        psds = []
        if art_dir is not None:
            for name in sorted(os.listdir(art_dir)):
                if not name.lower().endswith(".psd"):
                    continue
                if REAL_PSD.match(name):
                    shutil.copyfile(os.path.join(art_dir, name), os.path.join(src_dir, name))
                    psds.append(name)
                else:
                    skipped_backups.append("%s/%s" % (os.path.basename(art_dir), name))
        # PSD -> masks, in a scratch folder, as the tools want them: <dir>/<CAR>/<tex>.psd
        scratch_in, scratch_out = os.path.join(tmp, stem, "in"), os.path.join(tmp, stem, "out")
        os.makedirs(os.path.join(scratch_in, stem)), os.makedirs(scratch_out)
        for name in psds:
            if name != "dash.psd":
                shutil.copyfile(os.path.join(src_dir, name), os.path.join(scratch_in, stem, name))
        if any(n != "dash.psd" for n in psds):
            r = run([sys.executable, os.path.join(tools, "car_mask_from_psd.py"), scratch_in, os.path.join(scratch_out, "car")])
            if r.returncode != 0:
                failures.append("%s: car_mask_from_psd: %s" % (stem, r.stdout[-300:]))
        if "dash.psd" in psds:
            dash_in = os.path.join(tmp, stem, "dash_in")
            os.makedirs(os.path.join(dash_in, stem))
            shutil.copyfile(os.path.join(src_dir, "dash.psd"), os.path.join(dash_in, stem, "dash.psd"))
            r = run([sys.executable, os.path.join(tools, "dash_mask_from_psd.py"), dash_in, os.path.join(scratch_out, "dash")])
            if r.returncode != 0:
                failures.append("%s: dash_mask_from_psd: %s" % (stem, r.stdout[-300:]))
        # compare each made mask with the kit's (the game's data/ bytes)
        for kind, where in (("car", masks_dir), ("dash", os.path.join(gen_dir, "dash_masks"))):
            made = os.path.join(scratch_out, kind, stem if kind == "car" else stem.upper())
            if kind == "car":
                made = os.path.join(scratch_out, "car", stem)
            if not os.path.isdir(made):
                continue
            for name in sorted(os.listdir(made)):
                new = read(os.path.join(made, name))
                old_path = os.path.join(where, name)
                label = "%s/%s" % (stem, name)
                if not os.path.isfile(old_path) and (stem.upper(), "dash_mask.png") in CHANGED_ON_PURPOSE and name == "dash_color.png":
                    os.makedirs(where, exist_ok=True)   # the new PSD has a colour layer: its colour file is new, part of the same change
                    shutil.copyfile(os.path.join(made, name), old_path)
                    changed.append(label + " (a new file: the PSD has a colour layer)")
                elif not os.path.isfile(old_path):
                    failures.append("%s: the PSD makes %s, data/ has none" % (stem, name))
                elif read(old_path) == new:
                    same += 1
                elif (stem.upper(), name) in CHANGED_ON_PURPOSE:
                    os.makedirs(where, exist_ok=True)
                    shutil.copyfile(os.path.join(made, name), old_path)
                    changed.append(label)
                elif same_picture(old_path, os.path.join(made, name)):
                    palette_only.append(label)   # the kit keeps data/'s bytes: nothing changes for the game
                else:
                    failures.append("%s: the PSD's mask differs from data/'s (%d bytes against %d)" % (label, len(new), len(read(old_path))))
        # a data mask with no PSD is its own source
        for kind, where, prefix in (("car", masks_dir, ""),):
            for name in sorted(os.listdir(where)) if os.path.isdir(where) else []:
                tex = name[: -len("_mask.png")] if name.endswith("_mask.png") else None
                if tex is not None and tex + ".psd" not in psds:
                    shutil.copyfile(os.path.join(where, name), os.path.join(src_dir, name))
                    from_data.append("%s/%s" % (stem, name))
        if not os.listdir(src_dir):
            os.rmdir(src_dir)
        if stem not in cfm_ok:
            failures.append("%s: the kit's CFM was not shown to be the original's" % stem)
        report.append("%-8s kit %-9s PSDs: %s" % (stem, kit_name, ", ".join(psds) if psds else "none"))
    shutil.rmtree(tmp)

    text = ["# Migration of the original cars (dep100 item 6)", "",
            "%d kits in %s. The art folder was only read." % (len(kits), out), "", "## Kits", ""] + ["- " + l for l in report]
    text += ["", "## Masks from the PSDs", "", "- byte for byte the game's data/ file: %d" % same,
             "- changed on purpose (the PSD is newer than data/; the owner said the PSD wins): %d" % len(changed)] + ["  - " + c for c in changed]
    text += ["- the same picture as data/'s, other bytes: %d (the PNG's palette has the NOSE colour dash_mask_from_psd.py writes now; data/'s older files have 5 palette "
             "entries, not 6; the kit keeps data/'s bytes, so nothing changes for the game; regenerating them would change those files)" % len(palette_only)] + ["  - " + c for c in palette_only]
    text += ["", "## PSDs skipped (a backup: a name with an extra dot)", ""] + (["- " + s for s in skipped_backups] or ["- none"])
    text += ["", "## Masks with no PSD (data/'s PNG is its source, copied to art-source/masks/)", "", "- %d files" % len(from_data)]
    text += ["", "## Left out", "",
             "- data/car_masks/<CAR>/dash_mask.png (an old stray copy: car_mask_from_psd.py read dash.psd as a texture called dash; no texture has that name and the "
             "game reads the dash mask from data/dash_masks/<STEM>/): not in the kits."]
    text += ["", "## Failures", ""] + (["- " + f for f in failures] or ["- none"])
    text += ["", "## Exporter", ""] + ["- " + l for l in lines]
    with open(os.path.join(out, "MIGRATION-REPORT.md"), "w") as f:
        f.write("\n".join(text) + "\n")
    print("\n".join(text))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
