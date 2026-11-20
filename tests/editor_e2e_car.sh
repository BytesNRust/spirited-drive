#!/bin/sh
# A car from the game to the Studio and back to the game, the happy path a user takes: Export original car (the Wagon), Edit mask and
# Save mask, the Preview shows the edit, and a Race photo from the real game shows it too (it differs from the photo of the car as
# exported). Each editor run has a player folder of its own, so a window layout kept by one run cannot change another's picture.
# Needs the game's files (original/).
# usage: editor_e2e_car.sh EDITOR TNFS ORIGINAL_DIR PORT_DATA
. "$(dirname "$0")/editor_headless_common.sh"
tnfs=$2
orig=$3
data=$4
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$data/car_masks/Wagon" ] || { echo "skip: no game files in $orig"; exit 0; }
runs=0
studio() { runs=$((runs + 1)); "$editor" "$@" --user "$work/player$runs" >"$work/log" 2>&1; }
car="$work/cars/wagon-masks"
mask="$car/car_masks/Wagon/side_mask.png"
photo() { studio "$car" --game "$tnfs" --cd "$orig" --race-photo --night --race-png "$1" --frames 4; }
preview() { studio "$car" --view night --lights head --preview-tex side --frames 14 --shot "$1"; }

# 1. Export original car
studio --export-car Wagon --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car: $(cat "$work/log")"
[ -f "$car/project.ini" ] || { fail "Export made no project"; exit "$failures"; }
photo "$work/photo-exported.png" || fail "Race photo of the car as exported: $(cat "$work/log")"   # what the edit is told apart from
preview "$work/preview-exported.png" || fail "Preview of the car as exported: $(cat "$work/log")"

# 2. Edit mask and Save mask
studio "$car" --mask side --mask-rect 142,26,8,5,248 --mask-save --frames 8 || fail "Edit mask and Save mask: $(cat "$work/log")"
grep -q "saved side_mask.png" "$work/log" || fail "no word that the mask was saved: $(cat "$work/log")"
cmp -s "$mask" "$data/car_masks/Wagon/side_mask.png" && fail "the saved mask is still the game's file"

# 3. Preview shows the edit
preview "$work/preview-edited.png" || fail "Preview of the edited car: $(cat "$work/log")"
cmp -s "$work/preview-exported.png" "$work/preview-edited.png" && fail "the Preview does not show the edit"

# 4. Race photo shows the edit
photo "$work/photo-edited.png" || fail "Race photo of the edited car: $(cat "$work/log")"
[ -s "$work/photo-edited.png" ] || fail "no Race photo of the edited car"
cmp -s "$work/photo-exported.png" "$work/photo-edited.png" && fail "the Race photo does not show the edit"

[ "$failures" -eq 0 ] && echo "editor_e2e_car: ok"
exit "$failures"
