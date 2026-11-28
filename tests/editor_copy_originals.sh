#!/bin/sh
# Copy to game repo for all the originals, end to end, in a clean temp workspace: the 20 cars are exported as kits (Export original car) and the 16 tracks
# added (Add originals) into folders that did not exist before, each asked what it would copy against a copy of the repo's data/. Nothing was edited, so
# no car and no track may be refused or list a change (LDIABL's card lists more colours than the game's table: that is not a refusal). The workspace has no art folder.
# Needs the game's files (original/).
# PART=cars_a or cars_b (every other car of the list), tracks_a or tracks_b (every other track) runs that one alone (ctest runs the four side by side); unset, all of them.
# usage: editor_copy_originals.sh EDITOR ORIGINAL_DIR REPO
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2
repo=$3
part=${PART:-all}
case $part in all | cars_a | cars_b | tracks_a | tracks_b) ;; *) echo "FAIL: PART=$part is not cars_a, cars_b, tracks_a, tracks_b or all" >&2; exit 1 ;; esac
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ] || { echo "skip: no game files in $orig"; exit 77; }   # 77: ctest reads it as skipped, not passed
# a repo copy with the layout Copy needs: data/ with core/formats/car_paint.c beside it
mkdir -p "$work/repo/core/formats" "$work/cars" "$work/art" "$work/ws" || exit 1
cp -R "$repo/data" "$work/repo/data" && cp "$repo/core/formats/car_paint.c" "$work/repo/core/formats/car_paint.c" || { fail "a copy of the repo"; exit "$failures"; }
data="$work/repo/data"
cars_flow() {
total=$(ls "$data/car_masks" | wc -l | tr -d ' ')
[ "$total" -eq 20 ] || fail "data/car_masks has $total cars, not the 20 originals"
case $part in cars_a) take='NR % 2 == 1' want=10 ;; cars_b) take='NR % 2 == 0' want=10 ;; *) take=1 want=20 ;; esac
n=0
for c in $(ls "$data/car_masks" | awk "$take"); do
    before=$(ls "$work/cars")
    "$editor" --export-car "$c" --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 --user "$work/u" >"$work/log" 2>&1 || { fail "Export original car $c: $(cat "$work/log")"; continue; }
    kit=$(ls "$work/cars" | grep -vxF "$before" | head -n 1)
    [ -f "$work/cars/$kit/mod.ini" ] || { fail "the export of $c is not a kit"; continue; }
    "$editor" "$work/cars/$kit" --port-data "$data" --frames 2 --steps repo-ask,print-publish,status --user "$work/u" >"$work/log" 2>&1
    grep -q "^changed: \|^new: \|^missing: \|take the others off" "$work/log" && fail "Copy of the untouched $c lists a change or refuses: $(cat "$work/log")"
    n=$((n + 1))
done
[ "$n" -eq "$want" ] || fail "$n of the $want original cars of this part went through Copy"
}
case $part in all | cars_a | cars_b) cars_flow ;; esac
tracks_flow() {
# the tracks: no art folder, so each holds the light only; every one opens and has its one light file unchanged against data/
"$editor" --workspace "$work/ws" --port-data "$data" --art-dir "$work/art" --cd "$orig" --user "$work/u" --frames 4 --steps add-originals,print-workspace >"$work/log" 2>&1 || fail "Add originals: $(cat "$work/log")"
case $part in tracks_a) half=0 want=8 ;; tracks_b) half=1 want=8 ;; *) half= want=16 ;; esac
i=-1 n=0
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do
    i=$((i + 1))
    [ -z "$half" ] || [ $((i % 2)) -eq "$half" ] || continue
    n=$((n + 1))
    "$editor" --workspace "$work/ws" --port-data "$data" --art-dir "$work/art" --cd "$orig" --user "$work/u" --frames 4 --steps "open-original:$i,repo-ask,print-original" >"$work/log" 2>&1 || { fail "open $t: $(cat "$work/log")"; continue; }
    grep -q "^track: $t " "$work/log" || fail "$t is not the track opened as number $i: $(cat "$work/log")"
    grep -q "^preview: $t:.* unchanged" "$work/log" || fail "Copy of $t does not say its files are unchanged: $(cat "$work/log")"
    grep -q "[0-9a-z] changed\|no track_light\| new " "$work/log" && fail "Copy of the untouched $t lists a change: $(cat "$work/log")"
done
[ "$i" -eq 15 ] && [ "$n" -eq "$want" ] || fail "$n of the $want tracks of this part went through Copy"
}
case $part in all | tracks_a | tracks_b) tracks_flow ;; esac
[ "$failures" -eq 0 ] && echo "editor_copy_originals: ok"
exit "$failures"
