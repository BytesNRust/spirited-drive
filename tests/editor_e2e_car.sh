#!/bin/sh
# A car from the game to the Studio and back to the game, the happy path a user takes: Export original car (the Wagon), the mask changed from
# outside Studio and read by the mask view (never written by it), the Preview shows the edit, a In-game photo from the
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
car="$work/cars/wagon"
mask="$car/art-generated/car_masks/side_mask.png"
photo() { studio "$car" --game "$tnfs" --cd "$orig" --race-photo --night --race-png "$1" --frames 4; }
preview() { studio "$car" --view night --lights head --preview-tex side --frames 14 --shot "$1"; }

# 1. Export original car
studio --export-car Wagon --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car: $(cat "$work/log")"
[ -f "$car/mod.ini" ] && [ -f "$mask" ] || { fail "Export made no kit: $(ls "$car")"; exit "$failures"; }
photo "$work/photo-exported.png" || fail "In-game photo of the car as exported: $(cat "$work/log")"   # what the edit is told apart from
preview "$work/preview-exported.png" || fail "Preview of the car as exported: $(cat "$work/log")"

# 2. The mask changed from outside Studio (an image editor writes the file): the mask view reads it, Studio never writes it, and --mask-rect is refused
cp "$mask" "$work/mask-before.png"
python3 -I "$(dirname "$0")/mask_poke.py" "$mask" 142 26 8 5 248 || fail "the outside change of the mask"
cp "$mask" "$work/mask-poked.png"
studio "$car" --mask side --frames 8 || fail "mask view of the changed mask: $(cat "$work/log")"
cmp -s "$mask" "$work/mask-poked.png" || fail "the mask view wrote the mask file"
cmp -s "$mask" "$data/car_masks/Wagon/side_mask.png" && fail "the changed mask is still the game's file"
studio "$car" --mask side --mask-rect 142,26,8,5,248 --frames 2 && fail "--mask-rect was taken: masks are no longer painted in Studio"
grep -q "^usage: " "$work/log" || fail "an option that is gone was not refused with the usage: $(cat "$work/log")"

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
# the kit's masks are flat: the game is given a scratch folder with the stem level it reads, not the kit's own folder
real_car=$(cd "$car" && pwd -P)   # the editor names the folder as it really is (/private/var on a Mac)
grep -q "^race env: TNFS_CAR_MASKS=" "$work/log" || fail "the game is not given the car's masks: $(grep '^race' "$work/log")"
grep -q "^race env: TNFS_CAR_MASKS=$real_car/art-generated/car_masks\$" "$work/log" && fail "the game is given the kit's flat folder: it reads <STEM>/<tex>_mask.png"
grep -q '^race log: race car: slot 0 is ' "$work/log" || fail "the game's log has no car of the race: $(grep '^race' "$work/log")"
grep -q '^race end: Stopped.' "$work/log" || fail "Stop did not end the game: $(grep '^race' "$work/log")"

# 6. The same on a car you drive: its kit opens, its mask is edited in art-generated/car_masks/, the Preview and the Race photo show the edit, Build and Install are
# refused in words (an original car has no package), and the Preview's car file is all the build writes
if [ -r "$orig/SIMDATA/CARFAMS/P911.CFM" ] && [ -d "$data/car_masks/P911" ]; then
    drv="$work/cars/p911"
    dmask="$drv/art-generated/car_masks/topv_mask.png"
    studio --export-car P911 --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car of the 911: $(cat "$work/log")"
    [ -f "$drv/mod.ini" ] && [ -f "$dmask" ] || { fail "the 911's export is not a kit: $(ls "$drv")"; exit "$failures"; }
    # a view of the car where the top mask shows: in the framed default view the small patch the edit lights is not visible
    studio "$drv" --orbit 30,20 --view night --lights head --preview-tex topv --frames 14 --shot "$work/drv-preview-exported.png" || fail "Preview of the exported 911: $(cat "$work/log")"
    studio "$drv" --game "$tnfs" --cd "$orig" --race-photo --race-png "$work/drv-photo-exported.png" --frames 4 || fail "Race photo of the exported 911: $(cat "$work/log")"
    python3 -I "$(dirname "$0")/mask_poke.py" "$dmask" 16 16 48 48 90 || fail "the outside change of the exported 911's mask"
    studio "$drv" --mask topv --frames 8 || fail "mask view on the exported 911: $(cat "$work/log")"
    cmp -s "$dmask" "$data/car_masks/P911/topv_mask.png" && fail "the exported 911's mask is still the game's file"
    studio "$drv" --orbit 30,20 --view night --lights head --preview-tex topv --frames 14 --shot "$work/drv-preview-edited.png" || fail "Preview of the edited 911: $(cat "$work/log")"
    cmp -s "$work/drv-preview-exported.png" "$work/drv-preview-edited.png" && fail "the Preview of the exported 911 does not show the edit"
    studio "$drv" --game "$tnfs" --cd "$orig" --race-photo --race-png "$work/drv-photo-edited.png" --frames 4 || fail "Race photo of the edited 911: $(cat "$work/log")"
    cmp -s "$work/drv-photo-exported.png" "$work/drv-photo-edited.png" && fail "the Race photo of the exported 911 does not show the edit"
    studio "$drv" --build --frames 2 && fail "Build of an original car's kit went"
    grep -q "original car" "$work/log" || fail "no word that an original car has no package: $(cat "$work/log")"
    studio "$drv" --install --game "$tnfs" --frames 2 && fail "Install of an original car's kit went"
    grep -q "original car" "$work/log" || fail "no word that an original car is not installed: $(cat "$work/log")"
    ls "$drv/build" | grep -q '\.zip$' && fail "a package was made of an original car"
    # Test drive on the 911's kit: the game's own car, its masks staged from the kit
    studio "$drv" --game "$tnfs" --cd "$orig" --frames 2 --steps race,race-log,race-stop || fail "Test drive of the exported 911: $(cat "$work/log")"
    grep -q "^race env: TNFS_CAR_MASKS=" "$work/log" && grep -q "^race env: TNFS_DASH_MASKS=" "$work/log" || fail "Test drive of the 911 does not give the game its masks: $(grep '^race' "$work/log")"
    grep -q '^race end: Stopped.' "$work/log" || fail "Stop did not end the game of the 911: $(grep '^race' "$work/log")"
fi

[ "$failures" -eq 0 ] && echo "editor_e2e_car: ok"
exit "$failures"
