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

# Physics from (dep102): in the open car, the performance rows become the game car's; the model's size and the assist words stay the car's own. The second car above was made
# from template 1, so car 1's physics copied into the first car (the Supra) are the second car's.
row() { grep -P "\t$2\t" "$1" 2>/dev/null || grep -E "[[:space:]]$2[[:space:]]" "$1"; }
supra="$ws/cars/car-supra/specs"
own_length=$(row "$supra/specs.tsv" half_length)
own_body=$(row "$supra/pdn.tsv" body_0c)
[ "$(row "$supra/specs.tsv" power)" != "$(row "$ws/cars/$second/specs/specs.tsv" power)" ] || fail "the two cars have the same power: nothing to prove"
out=$(run --frames 10 "$ws/cars/car-supra" --steps physics-from:1,status 2>&1) || fail "Physics from: $out"
echo "$out" | grep -q 'Physics from .*rows of specs.tsv' || fail "the status does not say what was copied: $out"
ws2="$ws/cars"
[ "$(row "$ws2/car-supra/specs/specs.tsv" power)" = "$(row "$ws2/$second/specs/specs.tsv" power)" ] || fail "the power is not the game car's"
[ "$(row "$ws2/car-supra/specs/pdn.tsv" mass)" = "$(row "$ws2/$second/specs/pdn.tsv" mass)" ] || fail "the mass is not the game car's"
[ "$(row "$ws2/car-supra/specs/specs.tsv" half_length)" = "$own_length" ] || fail "the model's size changed"
[ "$(row "$ws2/car-supra/specs/pdn.tsv" body_0c)" = "$own_body" ] || fail "the collision body changed"
ls "$supra" | grep -q '\.new$' && fail "Physics from left a scratch file: $(ls "$supra")"
# TNFS_PERF: off by default (nothing printed); on, one line a second with the time of each phase (90 frames of 16 ms pass the second)
out=$(run --frames 90 "$ws/cars/car-supra" 2>&1) || fail "a run with no TNFS_PERF: $out"
echo "$out" | grep -q '^perf:' && fail "TNFS_PERF is off and Studio printed its perf line: $out"
out=$(TNFS_PERF=1; export TNFS_PERF; run --frames 90 "$ws/cars/car-supra" 2>&1) || fail "a run with TNFS_PERF: $out"
echo "$out" | grep -Eq '^perf: [0-9]+ frames in [0-9]+ ms \| dash .* poll .* watch .* recompute .* model .* masks .* preview .* ui .* present .* ms$' || fail "the perf line: $out"
# no car open: refused in words
out=$(run --frames 6 --steps physics-from:1 2>&1) && fail "Physics from with no car open worked: $out"
echo "$out" | grep -q 'open a car kit' || fail "no word why: $out"
# the Warrior (the last of the nine) is offered and copies
out=$(run --frames 10 "$ws/cars/car-supra" --steps physics-from:8,status 2>&1) || fail "Physics from the Warrior: $out"
# the row says «not set» until a car is picked (it is no value of the open car), then the car picked
out=$(run --frames 6 "$ws/cars/car-supra" --steps physics-print 2>&1) && echo "$out" | grep -q "physics row: not set" || fail "Physics from row before a pick: $out"
out=$(run --frames 6 "$ws/cars/car-supra" --steps physics-open:2,physics-print 2>&1) && echo "$out" | grep -q "physics row: 911" || fail "Physics from row after a pick: $out"
# the question that asks first is drawn
run --frames 8 "$ws/cars/car-supra" --steps physics-open:2 --shot "$work/physics.png" > "$work/log" 2>&1 || fail "the Physics from question did not draw: $(cat "$work/log")"
[ -s "$work/physics.png" ] || fail "no picture of the Physics from question"

# no game files: refused in words, no folder for the car
mkdir "$work/emptycd"
out=$(CD="$work/emptycd" run --frames 10 --steps new-car:2 2>&1) && fail "a car was made with no game files: $out"
echo "$out" | grep -q 'car new' || fail "no word why: $out"
[ "$(ls "$ws/cars" | wc -l | tr -d ' ')" = 2 ] || fail "a refused car left a folder: $(ls "$ws/cars")"

# the window
run --frames 6 --steps new-car-open --shot "$work/dialog.png" > "$work/log" 2>&1 || fail "the New car window did not draw: $(cat "$work/log")"
[ -s "$work/dialog.png" ] || fail "no picture of the New car window"

[ "$failures" = 0 ] || exit 1
