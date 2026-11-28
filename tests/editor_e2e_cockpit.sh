#!/bin/sh
# The dashboard check (dep98 item 2): the Race photo of a car you drive from the cockpit at night, the gauges lit by the gauge light asked for. Export original
# car (the 911), then three photos at night: following the car, from the cockpit with gauge light 5 (the car's own dash colours) and with 0 (the exe's white).
# The cockpit is not the follow view, the gauge light changes the dash, and the photo says what it shows. Needs the game's files (original/).
# usage: editor_e2e_cockpit.sh EDITOR TNFS ORIGINAL_DIR PORT_DATA
. "$(dirname "$0")/editor_headless_common.sh"
tnfs=$2
orig=$3
data=$4
[ -r "$orig/SIMDATA/CARFAMS/P911.CFM" ] && [ -d "$data/car_masks/P911" ] && [ -d "$data/dash_masks/P911" ] || { echo "skip: no game files in $orig"; exit 77; }
runs=0
studio() { runs=$((runs + 1)); "$editor" "$@" --user "$work/player$runs" >"$work/log" 2>&1; }
car="$work/cars/p911"
photo() { out=$1; shift; studio "$car" --game "$tnfs" --cd "$orig" --race-photo --night --race-png "$out" --frames 4 "$@"; }

studio --export-car P911 --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car of the 911: $(cat "$work/log")"
[ -d "$car/art-generated/dash_masks" ] || fail "the export carries no art-generated/dash_masks/: the cockpit photo has no dash to light"

photo "$work/follow.png" || fail "Race photo at night, following: $(cat "$work/log")"
photo "$work/cockpit5.png" --race-view cockpit --gauge-light 5 || fail "Cockpit photo, gauge light 5: $(cat "$work/log")"
grep -q "from the cockpit, gauge light 5" "$work/log" || fail "the cockpit photo does not say what it shows: $(cat "$work/log")"
photo "$work/cockpit0.png" --race-view cockpit --gauge-light 0 || fail "Cockpit photo, gauge light 0: $(cat "$work/log")"
grep -q "from the cockpit, gauge light 0" "$work/log" || fail "the photo with gauge light 0 does not say so: $(cat "$work/log")"
[ -s "$work/cockpit5.png" ] || fail "no cockpit photo"
cmp -s "$work/follow.png" "$work/cockpit5.png" && fail "the cockpit photo is the follow view"
cmp -s "$work/cockpit5.png" "$work/cockpit0.png" && fail "the gauge light did not change the dash"

# a view that is not one, or the dash view without a Race photo: refused
studio "$car" --race-photo --race-view sideways --frames 2 && fail "--race-view took a view that is not one"
studio "$car" --gauge-light 6 --race-photo --frames 2 && fail "--gauge-light 6 was taken"
studio "$car" --race-view cockpit --frames 2 && fail "--race-view without --race-photo was taken"

[ "$failures" -eq 0 ] && echo "editor_e2e_cockpit: ok"
exit "$failures"
