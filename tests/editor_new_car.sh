#!/bin/sh
# New car (dep102), headless: File > New car with a workspace makes <workspace>/cars/<id> as a kit from one of the eight cars you drive, opens it, refuses a second car with
# the same id, and refuses when the game's files are not there, leaving nothing behind. The window of the dialog is drawn. Needs the player's game files (original/): skipped
# without them.
# usage: editor_new_car.sh EDITOR REPO
. "$(dirname "$0")/editor_headless_common.sh"
repo=$2
[ -f "$repo/original/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no original/"; exit 0; }
ws="$work/ws"
mkdir -p "$ws"
run() { "$editor" --workspace "$ws" --user "$work/u" --cd "${CD:-$repo/original}" --port-data "$repo/data" "$@"; }

# the first car: template 0 (the Supra), made, opened, the card and the kit as `car new` and `car kit` make them
out=$(run --frames 10 --steps new-car:0,title 2>&1) || fail "New car: $out"
echo "$out" | grep -q '^title: .*car-supra' || fail "the new car is not the one open: $out"
for line in id=car-supra stem=CARSUPRA sound_from=supra type=car; do
    grep -qx "$line" "$ws/cars/car-supra/mod.ini" 2>/dev/null || fail "mod.ini lacks $line"
done
[ -f "$ws/cars/car-supra/specs/specs.tsv" ] && [ -f "$ws/cars/car-supra/specs/pdn.tsv" ] || fail "the kit has no specs"
[ -d "$ws/cars/car-supra/model/near" ] || fail "the kit has no model"
[ -e "$ws/cars/car-supra.piece" ] || [ -e "$ws/cars/car-supra.making" ] && fail "New car left its scratch folders: $(ls "$ws/cars")"

# the same name again: refused in words, nothing changed
before=$(cksum < "$ws/cars/car-supra/mod.ini")
out=$(run --frames 10 --steps new-car:0 2>&1) && fail "a second car with the same id was made: $out"
echo "$out" | grep -q 'already' || fail "no word why the second car was refused: $out"
[ "$(cksum < "$ws/cars/car-supra/mod.ini")" = "$before" ] || fail "the refused car changed the first"

# another template: its own id and stem, and its own sound
out=$(run --frames 10 --steps new-car:1,title 2>&1) || fail "New car 1: $out"
second=$(ls "$ws/cars" | grep -v '^car-supra$' | head -1)
[ -n "$second" ] && [ -f "$ws/cars/$second/mod.ini" ] || fail "the second car was not made: $(ls "$ws/cars")"
grep -q '^sound_from=supra$' "$ws/cars/$second/mod.ini" && fail "the second car has the first's sound"
[ "$(grep '^stem=' "$ws/cars/$second/mod.ini")" != "$(grep '^stem=' "$ws/cars/car-supra/mod.ini")" ] || fail "two cars share a stem"

# no game files: refused in words, no folder for the car
mkdir "$work/emptycd"
out=$(CD="$work/emptycd" run --frames 10 --steps new-car:2 2>&1) && fail "a car was made with no game files: $out"
echo "$out" | grep -q 'car new' || fail "no word why: $out"
[ "$(ls "$ws/cars" | wc -l | tr -d ' ')" = 2 ] || fail "a refused car left a folder: $(ls "$ws/cars")"

# the window
run --frames 6 --steps new-car-open --shot "$work/dialog.png" > "$work/log" 2>&1 || fail "the New car window did not draw: $(cat "$work/log")"
[ -s "$work/dialog.png" ] || fail "no picture of the New car window"

[ "$failures" = 0 ] || exit 1
