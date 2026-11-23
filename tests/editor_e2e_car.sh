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

# 5b. Copy to game repo for the Wagon's kit: its changed mask (the poke of step 2) is listed against a copy of the game repo's masks, and Copy writes it there and nothing else;
# asked again, there is nothing left to copy. A car that is not one of the game's has no such item (the refusal says so)
repo="$work/repo"
mkdir -p "$repo/car_masks" && cp -R "$data/car_masks/Wagon" "$repo/car_masks/Wagon" || fail "a copy of the repo's masks"
studio "$car" --port-data "$repo" --frames 2 --steps repo-ask,print-publish >"$work/log" 2>&1 || fail "Copy to game repo, the list: $(cat "$work/log")"
grep -q "^publish: window up for a car" "$work/log" && grep -q "^changed: side_mask.png" "$work/log" || fail "the Copy to game repo window does not list the changed mask: $(cat "$work/log")"
grep -q "car_masks/Wagon: 1 changed, 0 new" "$work/log" || fail "the list has more than the one changed mask: $(cat "$work/log")"
cmp -s "$repo/car_masks/Wagon/side_mask.png" "$data/car_masks/Wagon/side_mask.png" || fail "the preview wrote into the repo"
studio "$car" --port-data "$repo" --frames 2 --steps repo-ask,repo-copy,status >"$work/log" 2>&1 || fail "Copy to game repo, the copy: $(cat "$work/log")"
grep -q "^status: copied to the game repo: 1 mask written into $repo/car_masks/Wagon" "$work/log" || fail "Copy did not say what it wrote: $(cat "$work/log")"
cmp -s "$repo/car_masks/Wagon/side_mask.png" "$mask" || fail "the repo's mask is not the kit's"
for f in "$data"/car_masks/Wagon/*_mask.png; do
    n=${f##*/}
    [ "$n" = side_mask.png ] || cmp -s "$f" "$repo/car_masks/Wagon/$n" || fail "Copy changed $n, which was not changed"
done
studio "$car" --port-data "$repo" --frames 2 --steps repo-ask,status >"$work/log" 2>&1 && fail "Copy to game repo with nothing changed went on"
grep -q "every mask is the game repo's own" "$work/log" || fail "the second ask does not say nothing is left: $(cat "$work/log")"
cmp -s "$data/car_masks/Wagon/side_mask.png" "$mask" && fail "the game's own data/ was written"

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
    # Copy to game repo also takes the dash folder: another picture for the kit's dash mask is listed against the repo's data/dash_masks/P911 and written there, the mask of the top view with it
    repo2="$work/repo2"
    mkdir -p "$repo2/car_masks" "$repo2/dash_masks" && cp -R "$data/car_masks/P911" "$repo2/car_masks/P911" && cp -R "$data/dash_masks/P911" "$repo2/dash_masks/P911" || fail "a copy of the repo's 911 masks"
    cp "$data/dash_masks/CZR1/dash_mask.png" "$drv/art-generated/dash_masks/dash_mask.png" || fail "another dash for the 911's kit"
    studio "$drv" --port-data "$repo2" --frames 2 --steps repo-ask,print-publish >"$work/log" 2>&1 || fail "Copy to game repo of the 911, the list: $(cat "$work/log")"
    grep -q "^dash_masks/P911: 1 changed, 0 new, 0 missing, 0 unchanged\|^dash_masks/P911: 1 changed, 0 new, 0 missing, 1 unchanged" "$work/log" && grep -q "^changed: dash_mask.png" "$work/log" || fail "the Copy to game repo window does not list the dash: $(cat "$work/log")"
    grep -q "^car_masks/P911: 1 changed" "$work/log" || fail "the list does not hold the top view's mask: $(cat "$work/log")"
    studio "$drv" --port-data "$repo2" --frames 2 --steps repo-ask,repo-copy,status >"$work/log" 2>&1 || fail "Copy to game repo of the 911, the copy: $(cat "$work/log")"
    grep -q "^status: copied to the game repo: 1 mask written into $repo2/car_masks/P911, 1 dash file into $repo2/dash_masks/P911" "$work/log" || fail "Copy did not say what it wrote for the 911: $(cat "$work/log")"
    cmp -s "$repo2/dash_masks/P911/dash_mask.png" "$drv/art-generated/dash_masks/dash_mask.png" || fail "the repo's dash is not the kit's"
    cmp -s "$data/dash_masks/P911/dash_mask.png" "$repo2/dash_masks/P911/dash_mask.png" && fail "the dash was not written"
    studio "$drv" --port-data "$repo2" --frames 2 --steps repo-ask,status >"$work/log" 2>&1 && fail "Copy of the 911 with nothing changed went on"

    # Test drive on the 911's kit: the game's own car, its masks staged from the kit
    studio "$drv" --game "$tnfs" --cd "$orig" --frames 2 --steps race,race-log,race-stop || fail "Test drive of the exported 911: $(cat "$work/log")"
    grep -q "^race env: TNFS_CAR_MASKS=" "$work/log" && grep -q "^race env: TNFS_DASH_MASKS=" "$work/log" || fail "Test drive of the 911 does not give the game its masks: $(grep '^race' "$work/log")"
    grep -q '^race end: Stopped.' "$work/log" || fail "Stop did not end the game of the 911: $(grep '^race' "$work/log")"
fi

# 7. LDIABL, whose kit card lists more colours (8) than the game's table holds: Copy to game repo no longer refuses. It writes the dash (and masks),
# says the colours are not compared, and leaves core/formats/car_paint.c as it was. The repo has the layout Copy needs (data/ with core/formats/car_paint.c beside it)
ldir=LDiabl   # the car masks folder; the dash folder is LDIABL
if [ -r "$orig/SIMDATA/CARFAMS/LDIABL.CFM" ] && [ -d "$data/car_masks/$ldir" ] && [ -d "$data/dash_masks/LDIABL" ]; then
    repo3="$work/repo3"
    studio --export-car LDiabl --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car of the Diablo: $(cat "$work/log")"
    dia=$(ls -d "$work"/cars/*iab* 2>/dev/null | head -n 1)
    [ -f "$dia/mod.ini" ] || { fail "the Diablo's export is not a kit: $(ls "$work/cars")"; exit "$failures"; }
    mkdir -p "$repo3/data/car_masks" "$repo3/data/dash_masks" "$repo3/core/formats" && cp -R "$data/car_masks/$ldir" "$repo3/data/car_masks/$ldir" && cp -R "$data/dash_masks/LDIABL" "$repo3/data/dash_masks/LDIABL" && cp "$data/../core/formats/car_paint.c" "$repo3/core/formats/car_paint.c" || fail "a copy of the repo for the Diablo"
    cp "$data/dash_masks/CZR1/dash_mask.png" "$dia/art-generated/dash_masks/dash_mask.png" || fail "another dash for the Diablo's kit"
    # the card the owner kept lists 8 colours: give the kit 6, more than the game's table holds
    sed -i.bak "s/^paints=.*/paints=A:101010,B:202020,C:303030,D:404040,E:505050,F:606060/" "$dia/mod.ini" || fail "six colours on the Diablo's card"
    cp "$repo3/core/formats/car_paint.c" "$work/car_paint.before"
    studio "$dia" --port-data "$repo3/data" --frames 2 --steps repo-ask,print-publish >"$work/log" 2>&1 || fail "Copy to game repo of the Diablo, the list: $(cat "$work/log")"
    grep -q "^changed: dash_mask.png" "$work/log" || fail "the Diablo's window does not list its dash: $(cat "$work/log")"
    grep -q "colors: .*not compared" "$work/log" || fail "the window does not say the colors are not compared: $(cat "$work/log")"
    studio "$dia" --port-data "$repo3/data" --frames 2 --steps repo-ask,repo-copy,status >"$work/log" 2>&1 || fail "Copy to game repo of the Diablo, the copy: $(cat "$work/log")"
    grep -q "^status: copied to the game repo: .*1 dash file" "$work/log" || fail "Copy did not say it wrote the Diablo's dash: $(cat "$work/log")"
    cmp -s "$repo3/data/dash_masks/LDIABL/dash_mask.png" "$dia/art-generated/dash_masks/dash_mask.png" || fail "the repo's Diablo dash is not the kit's"
    cmp -s "$repo3/core/formats/car_paint.c" "$work/car_paint.before" || fail "Copy rewrote the colour table of a car whose colours it does not compare"
fi

[ "$failures" -eq 0 ] && echo "editor_e2e_car: ok"
exit "$failures"
