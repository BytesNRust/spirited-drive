#!/bin/sh
# A car from the game to the Studio and back to the game, the happy path a user takes: Export original car (the Wagon), Edit mask and
# Save mask, an edit undone and redone with the window's unsaved mark following, the Preview shows the edit, a In-game photo from the
# real game shows it too (it differs from the photo of the car as exported), and Test drive starts the game on the car and stops it. Each editor run has a player folder of its own, so a window
# layout kept by one run cannot change another's picture.
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
photo "$work/photo-exported.png" || fail "In-game photo of the car as exported: $(cat "$work/log")"   # what the edit is told apart from
preview "$work/preview-exported.png" || fail "Preview of the car as exported: $(cat "$work/log")"

# 2. Edit mask and Save mask
studio "$car" --mask side --mask-rect 142,26,8,5,248 --mask-save --frames 8 || fail "Edit mask and Save mask: $(cat "$work/log")"
grep -q "saved side_mask.png" "$work/log" || fail "no word that the mask was saved: $(cat "$work/log")"
cmp -s "$mask" "$data/car_masks/Wagon/side_mask.png" && fail "the saved mask is still the game's file"

# 2b. Edit, undo, redo, save on the open mask: the window's title has the unsaved mark after an edit, not after its undo, again after the redo,
# not after Save, and again after an undo past the save; the file has what was saved
cp "$mask" "$work/mask-before-2b.png"
studio "$car" --mask side --frames 2 --steps title,edit,title,undo,title,redo,title,save,title,undo,title || fail "edit, undo, redo, save: $(cat "$work/log")"
grep '^title: ' "$work/log" | sed 's/^title: Spirited Drive Studio .* wagon-masks/wagon/' > "$work/titles"
printf 'wagon\nwagon *\nwagon\nwagon *\nwagon\nwagon *\n' | cmp -s - "$work/titles" || fail "the unsaved mark in the title: $(tr '\n' '|' < "$work/titles")"
cmp -s "$mask" "$work/mask-before-2b.png" && fail "Save after a redo did not write the texel redone"
cp "$mask" "$work/mask-after-2b.png"
studio "$car" --mask side --frames 2 --steps edit,save,undo,save || fail "a mask saved and undone past the save: $(cat "$work/log")"
cmp -s "$mask" "$work/mask-after-2b.png" || fail "undoing past a save and saving again did not give the file back"

# 3. Preview shows the edit
preview "$work/preview-edited.png" || fail "Preview of the edited car: $(cat "$work/log")"
cmp -s "$work/preview-exported.png" "$work/preview-edited.png" && fail "the Preview does not show the edit"

# 4. In-game photo shows the edit
photo "$work/photo-edited.png" || fail "In-game photo of the edited car: $(cat "$work/log")"
[ -s "$work/photo-edited.png" ] || fail "no In-game photo of the edited car"
cmp -s "$work/photo-exported.png" "$work/photo-edited.png" && fail "the In-game photo does not show the edit"

# 5. Test drive: the game starts on the car's masks (its window headless here, the SDL drivers passed on), its log has the race's cars,
# and Stop ends it
studio "$car" --game "$tnfs" --cd "$orig" --frames 2 --steps race,race-log,race-stop || fail "Test drive: $(cat "$work/log")"
grep '^race: ' "$work/log" | grep -q -- "--data $orig --user .* --drive [A-Z0-9]* --car [0-9]* --type 1" \
    || fail "the command line of Test drive: $(grep '^race' "$work/log")"
real_car=$(cd "$car" && pwd -P)   # the editor names the folder as it really is (/private/var on a Mac)
grep -q "^race env: TNFS_CAR_MASKS=$real_car/car_masks\$" "$work/log" || fail "the game is not given the car's masks: $(grep '^race' "$work/log")"
grep -q '^race log: race car: slot 0 is ' "$work/log" || fail "the game's log has no car of the race: $(grep '^race' "$work/log")"
grep -q '^race end: Stopped.' "$work/log" || fail "Stop did not end the game: $(grep '^race' "$work/log")"

# 6. The same on a car you drive, exported in the shape Export original car writes (mod.ini and SIMDATA at the top, masks under car_masks/<folder>/): it opens,
# its mask is edited and saved there, the Preview and the Race photo show the edit, and nothing of the kit layout (build/, model/) appears in it
if [ -r "$orig/SIMDATA/CARFAMS/P911.CFM" ] && [ -d "$data/car_masks/P911" ]; then
    drv="$work/cars/p911-original"
    dmask="$drv/car_masks/P911/topv_mask.png"
    studio --export-car P911 --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car of the 911: $(cat "$work/log")"
    [ -f "$drv/mod.ini" ] && [ -f "$dmask" ] || { fail "the 911's export is not the old shape: $(ls "$drv")"; exit "$failures"; }
    studio "$drv" --view night --lights head --preview-tex topv --frames 14 --shot "$work/drv-preview-exported.png" || fail "Preview of the exported 911: $(cat "$work/log")"
    studio "$drv" --game "$tnfs" --cd "$orig" --race-photo --race-png "$work/drv-photo-exported.png" --frames 4 || fail "Race photo of the exported 911: $(cat "$work/log")"
    studio "$drv" --mask topv --mask-rect 16,16,48,48,90 --mask-save --frames 8 || fail "Edit mask and Save mask on the exported 911: $(cat "$work/log")"
    grep -q "saved topv_mask.png" "$work/log" || fail "no word that the exported 911's mask was saved: $(cat "$work/log")"
    cmp -s "$dmask" "$data/car_masks/P911/topv_mask.png" && fail "the exported 911's mask is still the game's file"
    studio "$drv" --view night --lights head --preview-tex topv --frames 14 --shot "$work/drv-preview-edited.png" || fail "Preview of the edited 911: $(cat "$work/log")"
    cmp -s "$work/drv-preview-exported.png" "$work/drv-preview-edited.png" && fail "the Preview of the exported 911 does not show the edit"
    studio "$drv" --game "$tnfs" --cd "$orig" --race-photo --race-png "$work/drv-photo-edited.png" --frames 4 || fail "Race photo of the edited 911: $(cat "$work/log")"
    cmp -s "$work/drv-photo-exported.png" "$work/drv-photo-edited.png" && fail "the Race photo of the exported 911 does not show the edit"
    [ ! -e "$drv/build" ] && [ ! -e "$drv/model" ] && [ ! -e "$drv/card.png" ] || fail "opening an exported original made kit folders in it: $(ls "$drv")"
fi

[ "$failures" -eq 0 ] && echo "editor_e2e_car: ok"
exit "$failures"
