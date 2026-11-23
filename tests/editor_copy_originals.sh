#!/bin/sh
# Copy to game repo for all the originals, end to end, in a clean temp workspace: the 20 cars are exported as kits (Export original car) and the 16 tracks
# added (Add originals) into folders that did not exist before, each asked what it would copy against a copy of the repo's data/. Nothing was edited, so
# no car and no track may be refused or list a change (LDIABL's card lists more colours than the game's table: that is not a refusal). The workspace has no art folder.
# Needs the game's files (original/).
# usage: editor_copy_originals.sh EDITOR ORIGINAL_DIR REPO
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2
repo=$3
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ] || { echo "skip: no game files in $orig"; exit 0; }
# a repo copy with the layout Copy needs: data/ with core/formats/car_paint.c beside it
mkdir -p "$work/repo/core/formats" "$work/cars" "$work/art" "$work/ws" || exit 1
cp -R "$repo/data" "$work/repo/data" && cp "$repo/core/formats/car_paint.c" "$work/repo/core/formats/car_paint.c" || { fail "a copy of the repo"; exit "$failures"; }
data="$work/repo/data"
n=0
for c in $(ls "$data/car_masks"); do
    before=$(ls "$work/cars")
    "$editor" --export-car "$c" --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 --user "$work/u" >"$work/log" 2>&1 || { fail "Export original car $c: $(cat "$work/log")"; continue; }
    kit=$(ls "$work/cars" | grep -vxF "$before" | head -n 1)
    [ -f "$work/cars/$kit/mod.ini" ] || { fail "the export of $c is not a kit"; continue; }
    "$editor" "$work/cars/$kit" --port-data "$data" --frames 2 --steps repo-ask,print-publish,status --user "$work/u" >"$work/log" 2>&1
    grep -q "^changed: \|^new: \|^missing: \|take the others off" "$work/log" && fail "Copy of the untouched $c lists a change or refuses: $(cat "$work/log")"
    n=$((n + 1))
done
[ "$n" -eq 20 ] || fail "$n of the 20 original cars went through Copy"
# the tracks: no art folder, so each holds the light only; every one opens and has its one light file unchanged against data/
"$editor" --workspace "$work/ws" --port-data "$data" --art-dir "$work/art" --cd "$orig" --user "$work/u" --frames 4 --steps add-originals,print-workspace >"$work/log" 2>&1 || fail "Add originals: $(cat "$work/log")"
i=-1
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do
    i=$((i + 1))
    "$editor" --workspace "$work/ws" --port-data "$data" --art-dir "$work/art" --cd "$orig" --user "$work/u" --frames 30 --steps "open-original:$i,repo-ask,print-original" >"$work/log" 2>&1 || { fail "open $t: $(cat "$work/log")"; continue; }
    grep -q "^track: $t " "$work/log" || fail "$t is not the track opened as number $i: $(cat "$work/log")"
    grep -q "^preview: $t:.* unchanged" "$work/log" || fail "Copy of $t does not say its files are unchanged: $(cat "$work/log")"
    grep -q "[0-9a-z] changed\|no track_light\| new " "$work/log" && fail "Copy of the untouched $t lists a change: $(cat "$work/log")"
done
[ "$failures" -eq 0 ] && echo "editor_copy_originals: ok"
exit "$failures"
