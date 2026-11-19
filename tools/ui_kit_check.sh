#!/bin/sh
# tools/ui_kit_check.sh TNFS REPO ORIGINAL_DIR: the UI kit's measures (docs/ui-kit.md) against the real binary and the CD data. So far the
# pager (E4): the screens whose page indicator the kit draws are walked to a middle page and ui_kit_pager_check.py measures the picture; a
# triangle, a gap or the centring more than a pixel off the CD's fails. It reads the CD's game data, so without one (or without Pillow)
# it is a skip, which is what the CI jobs do. Run by ctest as ui_kit_check; KEEP=1 keeps the pictures' folder.
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from a scratch folder
tnfs=$(abs "$1")
repo=$(abs "$2")
orig=$(abs "$3")
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
python3 -c 'import PIL' 2>/dev/null || { echo "skip: no Pillow"; exit 0; }
command -v zsh >/dev/null 2>&1 || { echo "skip: no zsh (tools/menu_walk.sh is one)"; exit 0; }
work=$(mktemp -d)
[ -n "${KEEP:-}" ] && echo "pictures in $work" || trap 'rm -r "$work"' EXIT
fail=0
# Controls Setup and Customize Game (Extras), their middle pages, in the mode that draws both
for target in controls-p2 extras-p2; do   # side by side: each walk is a game session of its own
    (DATA="$orig" PROFILE=restomod zsh "$repo/tools/menu_walk.sh" "$tnfs" "$work" "$target" >"$work/$target.log" 2>&1 ||
        { echo "FAIL $target: the walk did not reach the screen"; tail -5 "$work/$target.log"; touch "$work/$target.failed"; }) &
done
wait
for target in controls-p2 extras-p2; do [ -f "$work/$target.failed" ] && fail=1; done
for target in controls-p2 extras-p2; do
    [ -f "$work/$target.png" ] || continue
    python3 "$repo/tools/ui_kit_pager_check.py" "$work/$target.png" || fail=1
done
exit $fail
