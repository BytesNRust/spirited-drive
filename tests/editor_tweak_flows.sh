#!/bin/sh
# Edit tweak: the user flows editor_tweak_list.sh and editor_tweak_parity.sh leave open, end to end by clicks, on copies (a temp workspace and a temp copy of the repo's data/ and car_paint.c).
#   1 the list once tweaks exist: a row names its tweak and what it holds against the game (changed, then "same as the game" after Apply)
#   2 reopening an existing tweak (a new run): what was saved is still in it; a second change replaces the first in the folder, not beside it
#   3 a track's mask changed from outside reaches the repo through the panel's buttons
#   4 Replace / Skip / Cancel on a car with several changed masks: Cancel writes nothing, Skip writes the files the repo did not touch and keeps the repo's own, Replace writes the rest
#   5 a car with colors: Cancel on the own-changes question writes no color either; Skip writes the colors and keeps the repo's own mask; Cancel on the first confirmation writes nothing
#   6 Apply of a tweak that holds nothing new: it writes nothing and says so
# Needs the game's files (original/). usage: editor_tweak_flows.sh EDITOR ORIGINAL_DIR REPO
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2
repo=$3
here=$(dirname "$0")
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ] || { echo "skip: no game files in $orig"; exit 77; }
mkdir -p "$work/ws" "$work/art" "$work/r/core/formats" || exit 1
cp -R "$repo/data" "$work/r/data" && cp "$repo/core/formats/car_paint.c" "$work/r/core/formats/car_paint.c"
studio() { "$editor" "$@" --cd "$orig" --art-dir "$work/art" --user "$work/u" --workspace "$work/ws" >"$work/log" 2>&1; }
opts="--port-data $work/r/data"
poke() { python3 -I "$here/mask_poke.py" "$@" >/dev/null; }
same() { cmp -s "$1" "$2"; }

# ---- 1 and 2: a track tweak, made, listed, reopened, changed again (Save makes the tweak again: the panel and the list say so with no Check, Apply or reopening between)
studio $opts --frames 4 --steps ui-click:door_tweak,ui-click:tweak_tab_track,ui-click:tweak_row_CY1,ui-click:tweak_open,sun:120,30,70,save,poll-wait:1,tweak-shown || fail "CY1 first change: $(tail -3 "$work/log")"
grep -q '^tweak shown: changed' "$work/log" || fail "right after Save the panel does not say the tweak changed (no Check, Apply or reopening between): $(grep 'tweak shown' "$work/log")"
t=$work/ws/tweaks/cy1-tweak
[ -f "$t/track_light/CY1.txt" ] || fail "CY1: no tweak light file after the first change"
cp "$t/track_light/CY1.txt" "$work/cy1.first"
studio $opts --frames 4 --steps tweak-list || fail "list with a tweak: $(tail -3 "$work/log")"
grep -q '^tweak row: CY1 | track | cy1-tweak | changed' "$work/log" || fail "the CY1 row does not name its tweak and say it changed: $(grep 'row: CY1' "$work/log")"
grep -q '^tweak row: AL1 | track | - | ' "$work/log" || fail "a track with no tweak got one in the list: $(grep 'row: AL1' "$work/log")"

studio $opts --frames 4 --steps tweak-open-row:CY1,tweak-status || fail "CY1 reopened: $(tail -3 "$work/log")"
same "$t/track_light/CY1.txt" "$work/cy1.first" || fail "reopening CY1 changed the tweak's light file"
grep -q '^  track_light/CY1.txt: changed' "$work/log" || fail "the reopened tweak does not list its light file as changed: $(grep -A3 '^tweak cy1' "$work/log")"

studio $opts --frames 4 --steps tweak-open-row:CY1,sun:140,50,85,save,poll-wait:1,tweak-shown || fail "CY1 second change: $(tail -3 "$work/log")"
same "$t/track_light/CY1.txt" "$work/cy1.first" && fail "a second change in a reopened tweak did not reach its light file"
[ "$(ls "$t/track_light" | wc -l)" -eq 1 ] || fail "the reopened tweak has more than one light file: $(ls "$t/track_light")"
cp "$t/track_light/CY1.txt" "$work/cy1.second"

# ---- 3: a mask of the track changed from outside, then Apply by its buttons
mkdir -p "$work/ws/tracks/CY1/art-generated/track_masks"
poked=""
for p in "$repo/data/track_masks/CY1"/*.png; do
    [ -f "$p" ] || continue
    b=$(basename "$p")
    cp "$p" "$work/ws/tracks/CY1/art-generated/track_masks/$b"
    if poke "$work/ws/tracks/CY1/art-generated/track_masks/$b" 10 10 12 12 250 2>/dev/null; then poked=$b; break; fi
    rm -f "$work/ws/tracks/CY1/art-generated/track_masks/$b"
done
if [ -z "$poked" ]; then
    fail "CY1: no 8-bit grey mask to change from outside"
else
    studio $opts --frames 4 --steps tweak-open-row:CY1,tweak-status,ui-click:tweak_apply,ui-click:tweak_confirm,tweak-status || fail "CY1 mask Apply: $(tail -3 "$work/log")"
    [ -f "$t/track_masks/CY1/$poked" ] || fail "the changed mask $poked is not in the CY1 tweak"
    grep -q "track_masks/CY1/$poked: changed" "$work/log" || fail "the panel does not list $poked as changed: $(grep -A4 '^tweak cy1' "$work/log" | head -8)"
    same "$work/r/data/track_masks/CY1/$poked" "$repo/data/track_masks/CY1/$poked" && fail "Apply did not write the changed mask $poked into the repo"
    same "$work/r/data/track_light/CY1.txt" "$work/cy1.second" || fail "Apply did not write the second light file (the reopened tweak's)"
fi
grep -q '^tweak cy1-tweak: same as the game' "$work/log" || fail "after Apply (mask and light) the tweak is not the game's: $(grep -A3 '^tweak cy1' "$work/log" | tail -4)"
studio $opts --frames 4 --steps tweak-list || fail "list after Apply: $(tail -3 "$work/log")"
grep -q '^tweak row: CY1 | track | cy1-tweak | same as the game' "$work/log" || fail "the CY1 row after Apply is not 'same as the game': $(grep 'row: CY1' "$work/log")"

# ---- 6: Apply of a tweak that holds nothing new
cp "$work/r/data/track_light/CY1.txt" "$work/before.txt"
studio $opts --frames 4 --steps tweak-open-row:CY1,ui-click:tweak_apply,ui-click:tweak_confirm,status || fail "nothing to apply: $(tail -3 "$work/log")"
same "$work/r/data/track_light/CY1.txt" "$work/before.txt" || fail "Apply of a tweak that is the repo's changed the repo"
grep -q '^status: applied to the game repo: 0 files written, 0 left as the repo has them' "$work/log" || fail "Apply with nothing new does not say it wrote nothing: $(grep '^status' "$work/log")"

# ---- 4: a car with several changed masks (the traffic Wagon), the repo's own change in one of them
studio $opts --frames 4 --steps tweak-open-row:Wagon || fail "Wagon open: $(tail -3 "$work/log")"
kit=$(ls -d "$work"/ws/cars/* | head -n 1)
set -- $(ls -S "$kit/art-generated/car_masks"/*_mask.png 2>/dev/null | head -n 2)
[ $# -eq 2 ] || fail "the Wagon project has fewer than two masks"
m1=$1 m2=$2
poke "$m1" 30 30 20 20 200
poke "$m2" 30 30 20 20 180
w=$work/ws/tweaks/wagon-tweak
studio $opts --frames 4 --steps tweak-open-row:Wagon,tweak-status || fail "Wagon with two changed masks: $(tail -3 "$work/log")"
[ "$(grep -c '^  car_masks/Wagon/.*: changed' "$work/log")" -ge 2 ] || fail "two masks were changed from outside but the tweak lists fewer than two as changed: $(grep -A6 '^tweak wagon' "$work/log")"
f1=car_masks/Wagon/$(basename "$m1")
f2=car_masks/Wagon/$(basename "$m2")
[ -f "$w/$f1" ] && [ -f "$w/$f2" ] || fail "the Wagon tweak does not hold both masks ($f1, $f2)"
poke "$work/r/data/$f1" 5 5 10 10 100   # the repo's own change to the first mask, from outside
cp "$work/r/data/$f1" "$work/own1.png"
cp "$work/r/data/$f2" "$work/orig2.png"
asks() { studio $opts --frames 4 --steps tweak-open-row:Wagon,ui-click:tweak_apply,ui-click:tweak_confirm,"$1",status; }
asks ui-click:tweak_cancel || fail "Wagon, Cancel: $(tail -3 "$work/log")"
same "$work/r/data/$f1" "$work/own1.png" && same "$work/r/data/$f2" "$work/orig2.png" || fail "Cancel on the own-changes question wrote into the repo"
asks ui-click:tweak_skip || fail "Wagon, Skip: $(tail -3 "$work/log")"
same "$work/r/data/$f1" "$work/own1.png" || fail "Skip overwrote the mask the repo changed on its own"
same "$work/r/data/$f2" "$work/orig2.png" && fail "Skip did not write the mask the repo had not touched"
same "$work/r/data/$f2" "$w/$f2" || fail "Skip wrote a different $f2 than the tweak's"
grep -q '^status: applied to the game repo: 1 file written, 1 left as the repo has them' "$work/log" || fail "Skip does not say 1 written, 1 left: $(grep '^status' "$work/log")"
asks ui-click:tweak_replace || fail "Wagon, Replace: $(tail -3 "$work/log")"
same "$work/r/data/$f1" "$w/$f1" || fail "Replace did not write the mask the repo had changed"
grep -q '^status: applied to the game repo: 1 file written, 0 left as the repo has them' "$work/log" || fail "Replace does not say 1 written, 0 left: $(grep '^status' "$work/log")"

# ---- 5: a car with colors (the Diablo): a color changed and a mask changed, the repo's own change in that mask
studio $opts --frames 4 --steps tweak-open-row:LDiabl || fail "Diablo open: $(tail -3 "$work/log")"
dia=$(ls -d "$work"/ws/cars/* | grep -i diabl | head -n 1)
dm=$(ls -S "$dia/art-generated/car_masks"/*_mask.png 2>/dev/null | head -n 1)
[ -n "$dm" ] || fail "the Diablo project has no mask"
poke "$dm" 40 40 20 20 200
df=car_masks/LDiabl/$(basename "$dm")
studio $opts --frames 4 --steps tweak-open-row:LDiabl,paint-rgb:0,9,9,9,save,poll-wait:1,tweak-shown,tweak-status || fail "Diablo changed: $(tail -3 "$work/log")"
grep -q '^tweak shown: changed' "$work/log" || fail "right after Save of a color the Diablo's panel does not say the tweak changed: $(grep 'tweak shown' "$work/log")"
grep -q "^  $df: changed" "$work/log" && grep -q '^  colors: changed' "$work/log" || fail "the Diablo tweak does not list its mask and its colors as changed: $(grep -A6 '^tweak ldiabl' "$work/log")"
poke "$work/r/data/$df" 5 5 10 10 100
cp "$work/r/data/$df" "$work/dia.own"
cp "$work/r/core/formats/car_paint.c" "$work/table.before"
studio $opts --frames 4 --steps tweak-open-row:LDiabl,ui-click:tweak_apply,ui-click:tweak_confirm_cancel || fail "Diablo, Cancel on the first question: $(tail -3 "$work/log")"
same "$work/r/core/formats/car_paint.c" "$work/table.before" && same "$work/r/data/$df" "$work/dia.own" || fail "Cancel on the first confirmation wrote into the repo"
studio $opts --frames 4 --steps tweak-open-row:LDiabl,ui-click:tweak_apply,ui-click:tweak_confirm,ui-click:tweak_cancel || fail "Diablo, Cancel on the own-changes question: $(tail -3 "$work/log")"
same "$work/r/core/formats/car_paint.c" "$work/table.before" || fail "Cancel on the own-changes question wrote the colors"
same "$work/r/data/$df" "$work/dia.own" || fail "Cancel on the own-changes question changed the repo's mask"
studio $opts --frames 4 --steps tweak-open-row:LDiabl,ui-click:tweak_apply,ui-click:tweak_confirm,ui-click:tweak_skip,status || fail "Diablo, Skip: $(tail -3 "$work/log")"
grep -q '{ 9, 9, 9 }' "$work/r/core/formats/car_paint.c" || fail "Skip did not write the colors"
same "$work/r/data/$df" "$work/dia.own" || fail "Skip overwrote the Diablo's mask the repo changed on its own"
grep -q '0 files written, 1 left as the repo has them, colors written' "$work/log" || fail "Skip does not say 0 written, 1 left, colors written: $(grep '^status' "$work/log")"

[ "$failures" -eq 0 ] && echo "editor_tweak_flows: ok"
exit "$failures"
