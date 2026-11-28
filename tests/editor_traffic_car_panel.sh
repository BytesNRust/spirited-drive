#!/bin/sh
# A traffic car (Axxess) and the police Mustang opened as the game's own cars: the Car panel offers the masks only (no card, Add color, Physics from or photos: the game uses only the
# masks of these cars), the status says so, Check and the Tweak panel's Apply to game repo are there, and Test drive is in Drive. A driven car (the Diablo) keeps its Add color. Needs the game's files.
# usage: editor_traffic_car_panel.sh EDITOR TNFS ORIGINAL_DIR PORT_DATA
. "$(dirname "$0")/editor_headless_common.sh"
tnfs=$2 orig=$3 data=$4
[ -r "$orig/SIMDATA/CARFAMS/AXXESS.CFM" ] && [ -d "$data/car_masks/AXXESS" ] || { echo "skip: no game files in $orig"; exit 77; }
runs=0
mkdir -p "$work/ws"
studio() { runs=$((runs + 1)); "$editor" "$@" --workspace "$work/ws" --user "$work/player$runs" --game "$tnfs" --window 1280x800 --frames 30 >"$work/log" 2>&1; }
for car in AXXESS COPMUST; do
    studio --export-car "$car" --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || { fail "Export original car $car: $(cat "$work/log")"; continue; }
    kit=$(ls -d "$work/cars"/*/ | grep -i "/$car/\$" | head -n 1); kit=${kit%/}
    [ -d "$kit" ] || { fail "no kit for $car in $(ls "$work/cars")"; continue; }
    runs=$((runs + 1)); "$editor" "$kit" --port-data "$data" --user "$work/player$runs" --game "$tnfs" --window 1280x800 --frames 30 --steps status >"$work/log" 2>&1   # no workspace: the status is the one opening left
    grep -q "^status: opened: edit the masks; the Tweak panel applies them to the game repo (a traffic or police car has no card or colors)" "$work/log" || fail "$car: the status is not the true one: $(grep -i status "$work/log")"
    # the Car panel is up (its Check button is on screen), then Add color is waited for 60 frames, not 600: at 0.1 s a frame (software drawing on an old Mac) two full waits took the test past its time
    studio "$kit" --port-data "$data" --steps ui-rect:check,patience:60,ui-click:add_color && fail "$car: Add color is still offered for a car with no colors"
    grep -q "ui-click:add_color: no such widget drawn" "$work/log" || fail "$car: the run did not end at the missing Add color: $(cat "$work/log")"
    studio "$kit" --port-data "$data" --steps ui-click:check || fail "$car: Check is gone: $(cat "$work/log")"
    studio "$kit" --port-data "$data" --steps ui-click:tweak_apply || fail "$car: Apply to game repo is gone: $(cat "$work/log")"
    studio "$kit" --port-data "$data" --steps ui-focus:Drive###Race,ui-click:test_drive,drive-stop || fail "$car: Test drive is not in Drive: $(cat "$work/log")"
done
# a driven car keeps its card and colors
studio --export-car LDiabl --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export LDiabl: $(cat "$work/log")"
studio "$work/cars/ldiabl" --port-data "$data" --steps ui-click:add_color || fail "the Diablo lost Add color: $(cat "$work/log")"
[ "$failures" = 0 ] && echo ok
exit "$failures"
