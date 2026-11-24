#!/bin/sh
# The Diablo (LDIABL) from the game's own files, the way a person goes, with the panels clicked (ui_probe.h): Export original car (its second colour's name is 25 characters, the card
# holds 24: the status says so and the card has the cut name), the colour's name edited in its box, Save, Apply to game repo in the Tweak panel: the row of LDiabl in a copy of the repo's
# car_paint.c has the new name, the game's full "30th Anniversary Lavender" kept, and nothing else moved; Test drive clicked starts the game on the car and Stop ends it.
# Needs the game's files (original/).
# usage: editor_e2e_diablo.sh EDITOR TNFS ORIGINAL_DIR PORT_DATA
. "$(dirname "$0")/editor_headless_common.sh"
tnfs=$2 orig=$3 data=$4
repo_src="$(cd "$(dirname "$0")/.." && pwd)"
[ -r "$orig/SIMDATA/CARFAMS/LDIABL.CFM" ] && [ -d "$data/car_masks/LDiabl" ] || { echo "skip: no game files in $orig"; exit 0; }
runs=0
mkdir -p "$work/ws"
studio() { runs=$((runs + 1)); "$editor" "$@" --workspace "$work/ws" --user "$work/player$runs" --window 1280x800 >"$work/log" 2>&1; }
kit="$work/cars/ldiabl"

# 1. Export original car: the note about the cut name is in the status, the card holds the cut name
studio --export-car LDiabl --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car: $(cat "$work/log")"
grep -q 'Note: "30th Anniversary Lavender" is "30th Anniversary Lavende" on the card' "$work/log" || fail "the export does not say the colour's name was cut: $(cat "$work/log")"
grep -q "^paints=Rosso Diablo:8a0600,30th Anniversary Lavende:b57edc,Giallo Orion:eba628,Grigio Antares:494844,Blue Hera:091a30,Verde Hydra:081815,Celeste:48abb4,Bianco:e1e3db$" "$kit/mod.ini" || fail "the card's colours: $(grep paints "$kit/mod.ini")"

# 2. The first colour's name edited in its box, Save, Copy to game repo against a copy of the repo's source and masks, the window's Copy
repo="$work/repo"
mkdir -p "$repo/core/formats" "$repo/data/car_masks" "$repo/data/dash_masks"
cp "$repo_src/core/formats/car_paint.c" "$repo/core/formats/car_paint.c"
cp -R "$data/car_masks/LDiabl" "$repo/data/car_masks/LDiabl"
[ -d "$data/dash_masks/LDIABL" ] && cp -R "$data/dash_masks/LDIABL" "$repo/data/dash_masks/LDIABL"
studio "$kit" --port-data "$repo/data" --frames 30 --steps paint-print,ui-at:colour0_name,0.98,key:BackSpace,key:BackSpace,key:BackSpace,key:BackSpace,key:BackSpace,key:BackSpace,ui-text:Corsa,paint-print,save,ui-click:tweak_check,ui-click:tweak_apply,ui-click:tweak_confirm,status \
    || fail "the clicked flow: $(cat "$work/log")"
grep -q "^paints: Rosso Corsa #8a0600 30th Anniversary Lavende #b57edc Giallo Orion #eba628 Grigio Antares #494844 Blue Hera #091a30 Verde Hydra #081815 Celeste #48abb4 Bianco #e1e3db$" "$work/log" || fail "the name typed into the box is not in the card: $(grep '^paints' "$work/log")"
grep -q "^status: applied to the game repo: .*colors written" "$work/log" || fail "Apply to game repo did not say the colors were written: $(grep '^status' "$work/log")"
grep -q '{ "Rosso Corsa", { 138, 6, 0 } }' "$repo/core/formats/car_paint.c" && grep -q '"Bianco", { 225, 227, 219 }' "$repo/core/formats/car_paint.c" || fail "the table's row: $(diff "$repo/core/formats/car_paint.c" "$repo_src/core/formats/car_paint.c")"
[ "$(diff "$repo/core/formats/car_paint.c" "$repo_src/core/formats/car_paint.c" | grep -c '^[<>]')" = 2 ] || fail "Apply changed more than the row's one line: $(diff "$repo/core/formats/car_paint.c" "$repo_src/core/formats/car_paint.c")"

# 3. Test drive clicked, the game started on the car, Stop
studio "$kit" --game "$tnfs" --cd "$orig" --frames 4 --steps ui-focus:Drive###Race,ui-click:test_drive,race-log,race-stop || fail "Test drive clicked: $(cat "$work/log")"
grep -q '^race log: race car: slot 0 is ' "$work/log" || fail "the game's log has no car of the race: $(grep '^race' "$work/log")"
grep -q '^race end: Stopped.' "$work/log" || fail "Stop did not end the game: $(grep '^race' "$work/log")"

[ "$failures" = 0 ] && echo ok
exit "$failures"
