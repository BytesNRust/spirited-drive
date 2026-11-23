#!/bin/sh
# The owner's real migrated workspace (a backup copy of it, never the folder itself) through the flows of the originals, on a temporary copy, with a temporary data/ as the game repo:
#  - two original cars (ANSX, LDIABL): open the kit, an artist's PSD saved again (its mask layer painted), Studio converts it by itself, Copy to game repo lists the changes the kit
#    already had before and exactly one more mask, the Preview shows the edit, Copy writes only the changed masks;
#  - an original track (AL1): the workspace has the OLD light format (four files): opened as it is, the window has no light file (said); after Add originals the new file is there, the sun is
#    touched with the mouse, saved, and Copy writes that one light file into the repo's data/.
# A kit of the migration is not a test kit: what this finds is reported by the failing line. Skipped without the backup, the game files, Pillow (to paint the PSD) or a data/ folder.
# usage: editor_real_workspace.sh EDITOR ORIGINAL_DIR DATA_DIR   (TNFS_REAL_WORKSPACE: the backup folder, default ~/workspace/bytes-n-rust/backups/originals-workspace-before-add-originals-2026-11-23)
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2 data=$3
repo_src="$(cd "$(dirname "$0")/.." && pwd)"
backup=${TNFS_REAL_WORKSPACE:-$HOME/workspace/bytes-n-rust/backups/originals-workspace-before-add-originals-2026-11-23}
[ -d "$backup/cars" ] && [ -d "$backup/tracks" ] || { echo "skip: no backup of the real workspace at $backup"; exit 0; }
[ -r "$orig/SIMDATA/CARFAMS/ANSX.CFM" ] && [ -d "$data/car_masks/ANSX" ] || { echo "skip: no game files in $orig"; exit 0; }
python3 -c "import PIL" 2>/dev/null || { echo "skip: python3 has no Pillow (the PSD is painted with tools/psd_edit.py)"; exit 0; }
before=$(cd "$backup" && find . -type f | sort | xargs cksum | cksum)   # the backup is only read: its listing is compared at the end
clicked() { "$editor" "$@" --window 1280x800; }
names_of() { awk '/^(changed|new|missing): /{print $2}' "$1" | sort -u; }

one_car() {   # STEM (lower case folder in the workspace), STEM (upper), car_masks folder, PSD names to try (the first the kit has not edited yet is painted)
    low=$1 up=$2 folder=$3 cands=$4
    ws=$work/ws-$low repo=$work/repo-$low kit=$work/ws-$low/cars/$low
    mkdir -p "$ws/cars" "$repo/data/car_masks" "$repo/data/dash_masks" "$repo/core/formats"
    cp -Rp "$backup/cars/$low" "$ws/cars/$low"
    cp -R "$data/car_masks/$folder" "$repo/data/car_masks/$folder"
    [ -d "$data/dash_masks/$up" ] && cp -R "$data/dash_masks/$up" "$repo/data/dash_masks/$up"
    cp "$repo_src/core/formats/car_paint.c" "$repo/core/formats/car_paint.c"
    # the kit as it is, converted once (a run without steps), then what Copy lists already (the owner's own edits), and the Preview of the mask's texture
    clicked "$kit" --user "$work/u-$low" --port-data "$repo/data" --cd "$orig" --frames 250 > /dev/null 2>&1 || true
    clicked "$kit" --user "$work/u-$low" --port-data "$repo/data" --cd "$orig" --frames 20 --steps repo-ask,print-publish > "$work/$low.before" 2>&1 || true
    names_of "$work/$low.before" > "$work/$low.n.before"
    psd=; for c in $cands; do grep -qx "${c}_mask.png" "$work/$low.n.before" || { psd=$c.psd; png=${c}_mask.png; break; }; done
    [ -n "$psd" ] || { fail "$low: every candidate mask was already in the list"; return; }
    clicked "$kit" --user "$work/u-$low" --port-data "$repo/data" --cd "$orig" --view night --lights head --preview-tex "${png%_mask.png}" --frames 20 --shot "$work/$low.before.png" > "$work/$low.log" 2>&1 || fail "$low: Preview of the kit as migrated: $(tail -3 "$work/$low.log")"
    [ -s "$work/$low.before.png" ] || fail "$low: no Preview picture"
    (cd "$kit/art-generated" && find . -type f | sort | xargs cksum) > "$work/$low.sums.before"
    # an artist saves the PSD again
    sleep 2   # the PSD must be newer than the mask by the clock Studio compares
    python3 "$repo_src/tests/psd_poke_layer.py" "$kit/art-source/masks/$psd" 200 || { fail "$low: the PSD could not be painted"; return; }
    # Studio converts it by itself (a run without steps); the list of changes is the one before plus this mask
    clicked "$kit" --user "$work/u-$low" --port-data "$repo/data" --cd "$orig" --frames 250 > /dev/null 2>&1 || true
    clicked "$kit" --user "$work/u-$low" --port-data "$repo/data" --cd "$orig" --frames 20 --steps repo-ask,print-publish > "$work/$low.after" 2>&1 || fail "$low: Copy to game repo after the PSD changed: $(tail -3 "$work/$low.after")"
    names_of "$work/$low.after" > "$work/$low.n.after"
    grep -qx "$png" "$work/$low.n.after" || fail "$low: the painted PSD's mask $png did not reach the list (Studio did not convert it): $(cat "$work/$low.after")"
    extra=$(diff "$work/$low.n.before" "$work/$low.n.after" | grep '^[<>]' | grep -v "$png" || true)
    [ -z "$extra" ] || fail "$low: other masks changed with the PSD: $extra"
    (cd "$kit/art-generated" && find . -type f | sort | xargs cksum) > "$work/$low.sums.after"
    changed_files=$(diff "$work/$low.sums.before" "$work/$low.sums.after" | grep '^[<>]' | awk '{print $4}' | sort -u | tr '\n' ' ')
    [ "$changed_files" = "./car_masks/$png " ] || [ "$changed_files" = "./car_masks/$png ./.generated.tsv " ] || [ "$changed_files" = "./.generated.tsv ./car_masks/$png " ] || fail "$low: the conversion wrote more than $png: $changed_files"
    # the Preview shows the edit
    clicked "$kit" --user "$work/u-$low" --port-data "$repo/data" --cd "$orig" --view night --lights head --preview-tex "${png%_mask.png}" --frames 20 --shot "$work/$low.after.png" > "$work/$low.log" 2>&1 || fail "$low: Preview after the edit: $(tail -3 "$work/$low.log")"
    cmp -s "$work/$low.before.png" "$work/$low.after.png" && fail "$low: the Preview does not show the edit"
    # Copy writes into the temporary data/ only the listed masks
    (cd "$repo/data" && find . -type f | sort | xargs cksum) > "$work/$low.repo.before"
    clicked "$kit" --user "$work/u-$low" --port-data "$repo/data" --cd "$orig" --frames 200 --steps repo-ask,repo-copy,status > "$work/$low.copy" 2>&1 || fail "$low: Copy to game repo: $(tail -3 "$work/$low.copy")"
    (cd "$repo/data" && find . -type f | sort | xargs cksum) > "$work/$low.repo.after"
    written=$(diff "$work/$low.repo.before" "$work/$low.repo.after" | grep '^>' | awk '{print $4}' | xargs -n1 basename | sort -u)
    [ "$written" = "$(sort -u "$work/$low.n.after")" ] || fail "$low: Copy wrote $(echo $written) and the list said $(sort -u "$work/$low.n.after" | tr '\n' ' ')"
    cmp -s "$kit/art-generated/car_masks/$png" "$repo/data/car_masks/$folder/$png" || fail "$low: the copied $png is not the kit's"
}
one_car ansx ANSX ANSX "rsid side_lo topv frnt"
one_car ldiabl LDIABL LDiabl "frnt rear wing back_lo"

# an original track as it is in the migrated workspace: the old light format
tws=$work/ws-tracks trepo=$work/repo-tracks
mkdir -p "$tws/tracks" "$trepo/data" "$work/art"
cp -R "$backup/tracks/AL1" "$tws/tracks/AL1"
cp -R "$data/track_light" "$trepo/data/track_light"
sums_repo() { (cd "$trepo/data/track_light" && cksum *.txt); }
trun() { clicked --cd "$orig" --user "$work/u-tracks" --workspace "$tws" --port-data "$trepo/data" --art-dir "$work/art" --frames 40 "$@"; }
trun --steps open-original:0,light-print > "$work/t.old" 2>&1
grep -q '^light: AL1 no file$' "$work/t.old" || echo "note: the old-format workspace opened AL1 with: $(grep '^light:' "$work/t.old")" >&2
trun --steps add-originals,open-original:0,light-print > "$work/t.add" 2>&1 || fail "Add originals over the migrated workspace: $(tail -3 "$work/t.add")"
grep '^light: AL1 nodes [0-9]* time B' "$work/t.add" | tail -1 | grep -q . || fail "after Add originals AL1 has no light file: $(grep '^light:' "$work/t.add")"
lf=$tws/tracks/AL1/light/track_light/AL1.txt
cp "$lf" "$work/AL1.before"
sums_repo > "$work/repo.lights.before"
trun --steps open-original:0,ui-click:azimuth_tick,ui-at:azimuth_slider,0.25,light-print,ui-click:save,light-print,repo-ask,print-original,repo-copy,status > "$work/t.sun" 2>&1 || fail "the sun, Save and Copy on AL1: $(tail -4 "$work/t.sun")"
grep -q '^light: AL1 .*(unsaved)' "$work/t.sun" || fail "touching the sun did not mark the file unsaved: $(grep '^light:' "$work/t.sun")"
[ "$(grep -c '^  sun ' "$lf")" = 1 ] || fail "Save did not write one sun line: $(grep -c '^  sun' "$lf")"
[ "$(diff "$work/AL1.before" "$lf" | grep -c '^[<>]')" = 1 ] || fail "Save changed more than the sun line: $(diff "$work/AL1.before" "$lf")"
cmp -s "$lf" "$trepo/data/track_light/AL1.txt" || fail "Copy did not put AL1's light file in the repo: $(grep '^status' "$work/t.sun" | tail -1)"
sums_repo > "$work/repo.lights.after"
[ "$(diff "$work/repo.lights.before" "$work/repo.lights.after" | grep -c '^>')" = 1 ] || fail "Copy changed other track's light files: $(diff "$work/repo.lights.before" "$work/repo.lights.after")"

# AL1's Track masks tab at 1280x800: a tall texture's picture (slot_03, 128x256) is drawn whole, inside the panel (its bottom is above the window's), not cut off at the bottom
trun --steps open-original:0,ui-focus:Track\ masks,ui-click:tmask_only,ui-click:tmask2_row,ui-rect:tmask_picture > "$work/t.pic" 2>&1 || fail "the Track masks picture: $(tail -3 "$work/t.pic")"
pic=$(sed -n 's/^ui rect: tmask_picture //p' "$work/t.pic")
[ -n "$pic" ] && [ "$(echo "$pic" | awk '{ print ($2 + $4 <= 800 && $4 >= 100) ? "fits" : "cut" }')" = fits ] || fail "the mask's picture does not fit the panel at 1280x800 (x y w h: $pic)"

# the backup was only read
after=$(cd "$backup" && find . -type f | sort | xargs cksum | cksum)
[ "$before" = "$after" ] || fail "THE BACKUP CHANGED"
[ "$failures" = 0 ] && echo ok
exit "$failures"
