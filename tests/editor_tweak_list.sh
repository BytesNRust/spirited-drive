#!/bin/sh
# Edit tweak, step 2: the Edit tweak list and the Tweak panel, end to end, on copies (a temp workspace and a temp copy of the repo's data/; never the owner's workspace). Start screen -> Edit tweak ->
# Track -> CY1 -> Open -> change the sun, Save -> the tweak folder has track_light/CY1.txt -> Apply to game repo writes it; the repo's own change to a file asks Replace / Skip / Cancel; the same with a
# traffic car (Wagon): its masks reach car_masks/Wagon and no color; an old workspace's car is turned into a tweak when opened, its folder untouched; the old buttons are gone from the menus.
# Needs the game's files (original/). usage: editor_tweak_list.sh EDITOR ORIGINAL_DIR REPO
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2
repo=$3
here=$(dirname "$0")
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ] || { echo "skip: no game files in $orig"; exit 0; }
mkdir -p "$work/ws" "$work/art" "$work/r/core/formats" || exit 1
cp -R "$repo/data" "$work/r/data" && cp "$repo/core/formats/car_paint.c" "$work/r/core/formats/car_paint.c"
studio() { "$editor" "$@" --cd "$orig" --art-dir "$work/art" --user "$work/u" --workspace "$work/ws" >"$work/log" 2>&1; }
opts="--port-data $work/r/data"

# the list: the 21 cars and the 16 tracks, none with a tweak yet
studio $opts --frames 4 --steps tweak-list || fail "tweak-list: $(tail -3 "$work/log")"
[ "$(grep -c '^tweak row:' "$work/log")" -eq 37 ] || fail "the list has $(grep -c '^tweak row:' "$work/log") rows, not 37"
grep -q '^tweak row: CY1 | track | - | ' "$work/log" || fail "no CY1 row without a tweak: $(grep 'row: CY1' "$work/log")"
grep -q '^tweak row: Wagon | traffic | - | ' "$work/log" || fail "no Wagon row (traffic) without a tweak: $(grep 'row: Wagon' "$work/log")"

# the track, by clicks: Edit tweak, Track, CY1, Open; the sun changed and saved; the tweak is in the workspace's tweaks/
studio $opts --frames 90 --steps ui-click:door_tweak,ui-click:tweak_tab_track,ui-click:tweak_row_CY1,ui-click:tweak_open,sun:120,30,70,save,tweak-status || fail "CY1 flow: $(tail -5 "$work/log")"
t=$work/ws/tweaks/cy1-tweak
[ -f "$t/mod.ini" ] && grep -q '^type=tweak' "$t/mod.ini" && [ -f "$t/track_light/CY1.txt" ] && [ -f "$t/studio/target.txt" ] || fail "CY1: no tweak folder with its light file"
grep -q 'sun' "$t/track_light/CY1.txt" && ! cmp -s "$t/track_light/CY1.txt" "$work/r/data/track_light/CY1.txt" || fail "CY1: the tweak's light file does not carry the new sun"
grep -q '^  track_light/CY1.txt: changed' "$work/log" || fail "the Tweak panel does not list track_light/CY1.txt as changed: $(grep -A3 '^tweak cy1' "$work/log")"
cmp -s "$work/r/data/track_light/CY1.txt" "$repo/data/track_light/CY1.txt" || fail "the repo changed before Apply"
# Check: ok, then a stray folder for another track is found
studio $opts --frames 30 --steps tweak-open-row:CY1,tweak-check || fail "Check: $(tail -3 "$work/log")"
grep -q '^check: ok' "$work/log" || fail "Check is not ok: $(grep check: "$work/log")"
mkdir -p "$t/track_masks/AL1" && : >"$t/track_masks/AL1/slot_01.png"
studio $opts --frames 30 --steps tweak-open-row:CY1,tweak-check && fail "Check passed with a folder for another track"
grep -q 'another' "$work/log" || fail "Check does not say what is wrong: $(grep check: "$work/log")"
rm -rf "$t/track_masks/AL1"
# Apply by its button: the repo has the tweak's file; the panel then says everything is the game's
studio $opts --frames 90 --steps tweak-open-row:CY1,ui-click:tweak_apply,ui-click:tweak_confirm,tweak-status || fail "Apply: $(tail -3 "$work/log")"
cmp -s "$work/r/data/track_light/CY1.txt" "$t/track_light/CY1.txt" || fail "Apply did not put the tweak's light file in the repo"
grep -q '^tweak cy1-tweak: same as the game' "$work/log" || fail "after Apply the tweak is not the game's: $(grep -A3 '^tweak cy1' "$work/log")"

# the repo's own change: another sun saved, the repo's file changed from outside, Apply asks; Skip leaves the repo's, Replace writes the tweak's
studio $opts --frames 60 --steps tweak-open-row:CY1,sun:130,40,80,save,tweak-status || fail "CY1 again: $(tail -3 "$work/log")"
echo "# the repo's own line" >>"$work/r/data/track_light/CY1.txt"
cp "$work/r/data/track_light/CY1.txt" "$work/own.txt"
studio $opts --frames 90 --steps tweak-open-row:CY1,tweak-status,ui-click:tweak_apply,ui-click:tweak_confirm,ui-click:tweak_cancel || fail "own change, Cancel: $(tail -3 "$work/log")"
grep -q 'repo has changes of its own' "$work/log" || fail "the panel does not mark the file the repo changed: $(grep -A3 '^tweak cy1' "$work/log")"
cmp -s "$work/r/data/track_light/CY1.txt" "$work/own.txt" || fail "Cancel changed the repo"
studio $opts --frames 90 --steps tweak-open-row:CY1,ui-click:tweak_apply,ui-click:tweak_confirm,ui-click:tweak_skip || fail "own change, Skip: $(tail -3 "$work/log")"
cmp -s "$work/r/data/track_light/CY1.txt" "$work/own.txt" || fail "Skip changed the repo's own file"
studio $opts --frames 90 --steps tweak-open-row:CY1,ui-click:tweak_apply,ui-click:tweak_confirm,ui-click:tweak_replace || fail "own change, Replace: $(tail -3 "$work/log")"
cmp -s "$work/r/data/track_light/CY1.txt" "$t/track_light/CY1.txt" || fail "Replace did not write the tweak's file"

# a traffic car: its masks reach car_masks/Wagon and no color goes anywhere
studio $opts --frames 60 --steps tweak-open-row:Wagon,tweak-status || fail "Wagon open: $(tail -3 "$work/log")"
kit=$(ls -d "$work"/ws/cars/* | head -n 1)
[ -d "$work/ws/tweaks/wagon-tweak/car_masks/Wagon" ] || fail "Wagon: no car_masks/Wagon in its tweak"
[ ! -f "$work/ws/tweaks/wagon-tweak/studio/paints.txt" ] || fail "a traffic car's tweak has colors"
m=$(ls -S "$kit/art-generated/car_masks"/*_mask.png 2>/dev/null | head -n 1)
[ -n "$m" ] && python3 -I "$here/mask_poke.py" "$m" 30 30 20 20 200
studio $opts --frames 90 --steps tweak-open-row:Wagon,tweak-status,ui-click:tweak_apply,ui-click:tweak_confirm,tweak-status || fail "Wagon apply: $(tail -3 "$work/log")"
grep -q 'changed' "$work/log" || fail "Wagon: the retouched mask is not listed as changed: $(grep -A4 '^tweak wagon' "$work/log")"
diff -rq "$repo/data/car_masks/Wagon" "$work/r/data/car_masks/Wagon" >/dev/null 2>&1 && fail "Wagon: Apply changed nothing in the repo"
cmp -s "$repo/core/formats/car_paint.c" "$work/r/core/formats/car_paint.c" || fail "Wagon: the colors table changed for a traffic car"

# the confirmation: Apply lists what it writes first and Cancel writes nothing
cp "$work/r/data/track_light/CY1.txt" "$work/before.txt"
studio $opts --frames 60 --steps tweak-open-row:CY1,sun:100,20,60,save,ui-click:tweak_check,ui-click:tweak_apply,ui-click:tweak_confirm_cancel || fail "confirm, Cancel: $(tail -3 "$work/log")"
cmp -s "$work/r/data/track_light/CY1.txt" "$work/before.txt" || fail "Cancel on the list wrote into the repo"
# Apply with no repo set asks for the folder (the dialog itself is the system's) and writes nothing
studio --frames 40 --port-data "" --steps tweak-open-row:CY1,ui-click:tweak_apply,status || fail "Apply with no repo: $(tail -3 "$work/log")"
grep -q "^status: choose the game repo's folder, then Apply again" "$work/log" || fail "Apply with no repo did not ask for the folder: $(grep status "$work/log")"
# a car with colors by the doors: the Diablo, a color changed, Save, Apply: its row in the repo's table has it
studio $opts --frames 90 --steps ui-click:door_tweak,ui-click:tweak_row_Lamborghini\ Diablo\ VT,ui-click:tweak_open,paint-rgb:0,9,9,9,save,ui-click:tweak_check,ui-click:tweak_apply,ui-click:tweak_confirm || fail "Diablo by the doors: $(tail -3 "$work/log")"
grep -q '{ 9, 9, 9 }' "$work/r/core/formats/car_paint.c" || fail "the Diablo's color is not in the repo's table: $(diff "$repo/core/formats/car_paint.c" "$work/r/core/formats/car_paint.c" | head -4)"

# an old workspace: a car exported the old way, no tweaks/: the list says so; Open makes the tweak and leaves the old folder as it was
rm -rf "$work/old" && mkdir -p "$work/old/cars" "$work/old/ws" || exit 1
"$editor" --export-car JETTA --into "$work/old/cars" --cd "$orig" --port-data "$repo/data" --frames 1 --user "$work/u" >"$work/log" 2>&1 || fail "old export: $(tail -3 "$work/log")"
cp -R "$work/old/cars" "$work/old/cars.before"
ln -s "$work/old/cars" "$work/old/ws/cars" 2>/dev/null
"$editor" --workspace "$work/old/ws" --cd "$orig" --port-data "$work/r/data" --user "$work/u" --frames 4 --steps tweak-list >"$work/log" 2>&1
grep -q '^tweak row: Jetta | traffic | - | from the old workspace' "$work/log" || fail "the old workspace's car is not marked: $(grep 'row: Jetta' "$work/log")"
"$editor" --workspace "$work/old/ws" --cd "$orig" --port-data "$work/r/data" --user "$work/u" --frames 30 --steps tweak-open-row:Jetta >"$work/log" 2>&1 || fail "old Jetta open: $(tail -3 "$work/log")"
[ -f "$work/old/ws/tweaks/jetta-tweak/mod.ini" ] || fail "opening the old workspace's Jetta made no tweak"
diff -r "$work/old/cars" "$work/old/cars.before" >/dev/null 2>&1 || fail "opening the old workspace's car changed its folder"

# the buttons the list replaces are gone from the menus
studio $opts --frames 4 --steps print-menu
for gone in "Export original car" "Open original track" "Add originals" "Copy to game repo" "Locate game"; do
    grep -q "$gone" "$work/log" && fail "the main menu still has \"$gone\""
done
[ "$failures" -eq 0 ] && echo "editor_tweak_list: ok"
exit "$failures"
