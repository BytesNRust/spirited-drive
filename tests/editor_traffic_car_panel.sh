#!/bin/sh
# A traffic car (Axxess) and the police Mustang opened as the game's own cars: the Car panel offers the masks only (no card, Add color, Physics from or photos: the game uses only the
# masks of these cars), the status says so, Check and the Tweak panel's Apply to game repo are there, and Test drive in Drive starts a race (the game's own log says so). A driven car (the Diablo) keeps its Add color. Needs the game's files.
# usage: editor_traffic_car_panel.sh EDITOR TNFS ORIGINAL_DIR PORT_DATA    Exit 77 (skipped) without the game's files.
# PART=axxess (and the Diablo) or copmust runs that one alone (ctest runs both side by side); unset, all.
. "$(dirname "$0")/editor_headless_common.sh"
tnfs=$2 orig=$3 data=$4
[ -r "$orig/SIMDATA/CARFAMS/AXXESS.CFM" ] && [ -d "$data/car_masks/AXXESS" ] || { echo "skip: no game files in $orig"; exit 77; }
part=${PART:-all}
case $part in all) cars="AXXESS COPMUST" ;; axxess) cars=AXXESS ;; copmust) cars=COPMUST ;; *) echo "FAIL: PART=$part is not axxess, copmust or all" >&2; exit 1 ;; esac
runs=0
mkdir -p "$work/ws"
studio() { runs=$((runs + 1)); "$editor" "$@" --workspace "$work/ws" --user "$work/player$runs" --game "$tnfs" --window 1280x800 --frames 4 >"$work/log" 2>&1; }
for car in $cars; do
    studio --export-car "$car" --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || { fail "Export original car $car: $(cat "$work/log")"; continue; }
    kit=$(ls -d "$work/cars"/*/ | grep -i "/$car/\$" | head -n 1); kit=${kit%/}
    [ -d "$kit" ] || { fail "no kit for $car in $(ls "$work/cars")"; continue; }
    runs=$((runs + 1)); "$editor" "$kit" --port-data "$data" --user "$work/player$runs" --game "$tnfs" --window 1280x800 --frames 4 --steps status >"$work/log" 2>&1   # no workspace: the status is the one opening left
    grep -q "^status: opened: edit the masks; the Tweak panel applies them to the game repo (a traffic or police car has no card or colors)" "$work/log" || fail "$car: the status is not the true one: $(grep -i status "$work/log")"
    # the Car panel is up (its Check button is on screen), then Add color is waited for 10 frames, not 600: at 0.1 s a frame (software drawing on an old Mac) full waits took the test past
    # its time. The Diablo below finds Add color by the same steps
    studio "$kit" --port-data "$data" --steps ui-rect:check,patience:10,ui-click:add_color && fail "$car: Add color is still offered for a car with no colors"
    grep -q "ui-click:add_color: no such widget drawn" "$work/log" || fail "$car: the run did not end at the missing Add color: $(cat "$work/log")"
    studio "$kit" --port-data "$data" --steps ui-click:check || fail "$car: Check is gone: $(cat "$work/log")"
    studio "$kit" --port-data "$data" --steps ui-click:tweak_apply || fail "$car: Apply to game repo is gone: $(cat "$work/log")"
    # the click starts a race: the game's own log names the player's car (race-log fails when the game ends first), then Stop
    # --cd: the game needs the player's files, and with no --cd the editor looks for them from the folder the test is run in (ctest's is the build's: none there)
    studio "$kit" --cd "$orig" --port-data "$data" --steps ui-focus:Drive###Race,ui-click:test_drive,race-log,race-stop || fail "$car: Test drive did not start a race: $(grep -v '^ui rect' "$work/log" | tail -3)"
    grep -q "^race log: race car: slot 0 is " "$work/log" || fail "$car: no race in the game's log: $(tail -3 "$work/log")"
done
# a driven car keeps its card and colors
if [ "$part" != copmust ]; then
studio --export-car LDiabl --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export LDiabl: $(cat "$work/log")"
studio "$work/cars/ldiabl" --port-data "$data" --steps ui-rect:check,patience:10,ui-click:add_color || fail "the Diablo lost Add color: $(cat "$work/log")"
fi
[ "$failures" = 0 ] && echo ok
exit "$failures"
