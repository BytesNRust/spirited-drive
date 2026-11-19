#!/bin/sh
# tools/ui_kit_check.sh TNFS REPO ORIGINAL_DIR: the UI kit's measures (docs/ui-kit.md) against the real binary and the CD data. So far the
# pager (E4): the screens whose page indicator the kit draws are walked to a middle page and ui_kit_pager_check.py measures the picture; a
# triangle, a gap or the centring more than a pixel off the CD's fails. The labels (E2): RECORDS' DONE, ui_kit_label_check.py (height, mixed edges). It reads the CD's game data, so without one (or without Pillow)
# it is a skip (exit 77, ctest's SKIP_RETURN_CODE), which is what the CI jobs do. The walks also need ImageMagick (magick) and zsh; the gate's ctest may have no magick on its PATH. Run by ctest as ui_kit_check; KEEP=1 keeps the pictures' folder.
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from a scratch folder
tnfs=$(abs "$1")
repo=$(abs "$2")
orig=$(abs "$3")
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "skip: no Pillow"; exit 77; }
command -v zsh >/dev/null 2>&1 || { echo "skip: no zsh (tools/menu_walk.sh is one)"; exit 77; }
command -v magick >/dev/null 2>&1 || { echo "skip: no magick (ImageMagick 7: tools/menu_walk.sh saves its pictures with it)"; exit 77; }
work=$(mktemp -d)
[ -n "${KEEP:-}" ] && echo "pictures in $work" || trap 'rm -r "$work"' EXIT
fail=0
# Controls Setup and Customize Game (Extras), their middle pages, in the mode that draws both
# an edition of the player's own, its name wide, for the selector's gaps (E2b)
printf 'name=MMMMMMMMMMMM\ncars=rx7,nsx,supra\ntracks=city,coastal\n' > "$work/wide.ini"
(DATA="$orig" PROFILE=restomod EDITION="$work/wide.ini" zsh "$repo/tools/menu_walk.sh" "$tnfs" "$work/content" content >"$work/content.log" 2>&1 ||
    { echo "FAIL content: the walk did not reach the screen"; tail -5 "$work/content.log"; touch "$work/content.failed"; }) &
for target in controls-p2 extras-p2 records; do   # side by side: each walk is a game session of its own
    (DATA="$orig" PROFILE=restomod zsh "$repo/tools/menu_walk.sh" "$tnfs" "$work" "$target" >"$work/$target.log" 2>&1 ||
        { echo "FAIL $target: the walk did not reach the screen"; tail -5 "$work/$target.log"; touch "$work/$target.failed"; }) &
done
wait
for target in controls-p2 extras-p2 records content; do [ -f "$work/$target.failed" ] && fail=1; done
for target in controls-p2 extras-p2; do
    [ -f "$work/$target.png" ] || continue
    python3 "$repo/tools/ui_kit_pager_check.py" "$work/$target.png" || fail=1
done
# the labels (E2): RECORDS' DONE is the CD's height and its edges are mixed with the page
[ -f "$work/content/content.png" ] && { python3 "$repo/tools/ui_kit_selector_check.py" "$work/content/content.png" || fail=1; }
[ -f "$work/content/content.png" ] && { python3 "$repo/tools/ui_kit_title_check.py" "$work/content/content.png" || fail=1; }
[ -f "$work/content/content.png" ] && { python3 "$repo/tools/ui_kit_foot_check.py" "$work/content/content.png" || fail=1; }
[ -f "$work/records.png" ] && { python3 "$repo/tools/ui_kit_label_check.py" "$work/records.png" || fail=1; }
exit $fail
