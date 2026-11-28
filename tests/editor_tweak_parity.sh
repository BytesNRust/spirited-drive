#!/bin/sh
# Edit tweak, step 1: Apply to the game repo of a tweak folder writes into the repo what Copy to game repo writes today, byte for byte. For each of the 20 original cars and the 16 tracks, in a clean
# temp workspace with a retouch (a mask changed from outside, a color changed on the card, a line added to the light file): one copy of the repo's data/ gets Copy (repo-copy), another gets
# tweak-make + tweak-apply on the same project, and the two trees (data/ and core/formats/car_paint.c) must be identical (diff -r). The traffic and police cars have a masks folder and no colors.
# Also: the tweak folder's layout (mod.ini type=tweak, data/ paths, studio/), and hand-copying the folders over a third data/ gives the same tree as Apply.
# Needs the game's files (original/). usage: editor_tweak_parity.sh EDITOR ORIGINAL_DIR REPO
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2
repo=$3
here=$(dirname "$0")
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ] || { echo "skip: no game files in $orig"; exit 0; }
mkdir -p "$work/cars" "$work/ws" "$work/art" || exit 1
fresh() {   # fresh NAME: a repo copy with the layout Copy needs, in $work/NAME (a clone where the volume has them: the data/ is copied twice a car and a track)
    mkdir -p "$work/$1/core/formats" && { cp -Rc "$repo/data" "$work/$1/data" 2>/dev/null || cp -R "$repo/data" "$work/$1/data"; } && cp "$repo/core/formats/car_paint.c" "$work/$1/core/formats/car_paint.c"
}
studio() { "$editor" "$@" --user "$work/u" --frames 4 >"$work/log" 2>&1; }
same_tree() {   # same_tree A B WHAT
    diff -r "$work/$1/data" "$work/$2/data" >"$work/diff" 2>&1 && cmp -s "$work/$1/core/formats/car_paint.c" "$work/$2/core/formats/car_paint.c" || fail "$3: Apply is not Copy: $(head -5 "$work/diff") $(diff "$work/$1/core/formats/car_paint.c" "$work/$2/core/formats/car_paint.c" | head -4)"
}
n=0 changed_cars=0
for c in $(ls "$repo/data/car_masks"); do
    before=$(ls "$work/cars")
    "$editor" --export-car "$c" --into "$work/cars" --cd "$orig" --port-data "$repo/data" --frames 1 --user "$work/u" >"$work/log" 2>&1 || { fail "Export original car $c: $(cat "$work/log")"; continue; }
    kit=$work/cars/$(ls "$work/cars" | grep -vxF "$before" | head -n 1)
    # the retouch: a mask changed from outside, and (a car with colors) the first color of the card changed
    m=$(ls -S "$kit/art-generated/car_masks"/*_mask.png 2>/dev/null | head -n 1)   # the largest: a small one (axxess back_lo is 19x16) has no texel at the poke
    [ -n "$m" ] && python3 -I "$here/mask_poke.py" "$m" 30 30 20 20 200
    python3 -I - "$kit/mod.ini" <<'PY'
import re, sys
p = sys.argv[1]
t = open(p).read()
t2 = re.sub(r'(?m)^(paints=[^#\n]*#)([0-9a-fA-F]{6})', lambda m: m.group(1) + ('123456' if m.group(2).lower() != '123456' else '654321'), t, count=1)
open(p, 'w').write(t2)
PY
    fresh copy; fresh tweak
    if ! studio "$kit" --workspace "$work/ws" --port-data "$work/copy/data" --steps repo-ask,repo-copy; then
        grep -q "nothing to copy" "$work/log" || { fail "Copy of $c: $(tail -3 "$work/log")"; continue; }   # the retouch did not reach the repo for this car: Apply must leave it alone too
    fi
    studio "$kit" --workspace "$work/ws" --port-data "$work/tweak/data" --steps "tweak-make,tweak-apply:$(echo "$c" | tr 'A-Z' 'a-z')-tweak" || { fail "tweak of $c: $(tail -3 "$work/log")"; continue; }
    [ -f "$work/ws/tweaks/$(echo "$c" | tr 'A-Z' 'a-z')-tweak/mod.ini" ] && grep -q '^type=tweak' "$work/ws/tweaks/$(echo "$c" | tr 'A-Z' 'a-z')-tweak/mod.ini" || fail "$c: no tweak folder with type=tweak"
    same_tree copy tweak "car $c"
    diff -r "$repo/data" "$work/copy/data" >/dev/null 2>&1 || changed_cars=$((changed_cars + 1))
    n=$((n + 1))
done
[ "$n" -eq 20 ] || fail "$n of the 20 original cars went through"
[ "$changed_cars" -ge 16 ] || fail "the retouch changed the repo for only $changed_cars cars: the parity proves nothing"
# the tracks: the light file with a line added, and a mask PNG changed from outside
"$editor" --workspace "$work/ws" --port-data "$repo/data" --art-dir "$work/art" --cd "$orig" --user "$work/u" --frames 4 --steps add-originals >"$work/log" 2>&1 || fail "Add originals: $(cat "$work/log")"
i=-1
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do
    i=$((i + 1))
    echo "# tweak parity" >>"$work/ws/tracks/$t/light/track_light/$t.txt"
    mkdir -p "$work/ws/tracks/$t/art-generated/track_masks"
    for p in "$repo/data/track_masks/$t"/*.png; do   # the first mask that is an 8-bit grey PNG, changed from outside
        [ -f "$p" ] || continue
        cp "$p" "$work/ws/tracks/$t/art-generated/track_masks/" && python3 -I "$here/mask_poke.py" "$work/ws/tracks/$t/art-generated/track_masks/$(basename "$p")" 10 10 12 12 250 >/dev/null 2>&1 && break
        rm -f "$work/ws/tracks/$t/art-generated/track_masks/$(basename "$p")"
    done
    fresh copy; fresh tweak
    studio --workspace "$work/ws" --port-data "$work/copy/data" --cd "$orig" --art-dir "$work/art" --steps "open-original:$i,repo-ask,repo-copy" || { fail "Copy of $t: $(tail -3 "$work/log")"; continue; }
    studio --workspace "$work/ws" --port-data "$work/tweak/data" --cd "$orig" --art-dir "$work/art" --steps "open-original:$i,tweak-make,tweak-apply:$(echo "$t" | tr 'A-Z' 'a-z')-tweak" || { fail "tweak of $t: $(tail -3 "$work/log")"; continue; }
    same_tree copy tweak "track $t"
    diff -r "$repo/data" "$work/copy/data" >/dev/null 2>&1 && fail "$t: the retouch changed nothing in the repo"
done
# hand-copying the tweak's folders over a data/ = Apply: every file the hand copy changed against the repo is the same in Apply's tree (a PNG with the same picture in other bytes is left alone by Apply)
fresh hand; fresh again
for d in "$work"/ws/tweaks/*-tweak; do
    for sub in car_masks dash_masks track_masks; do [ -d "$d/$sub" ] && cp -R "$d/$sub/." "$work/hand/data/$sub/"; done
    [ -d "$d/track_light" ] && cp "$d/track_light/"*.txt "$work/hand/data/track_light/"
    studio --workspace "$work/ws" --port-data "$work/again/data" --steps "tweak-apply:$(basename "$d")" || fail "Apply of $d: $(tail -3 "$work/log")"
done
( cd "$work/hand/data" && find . -type f ) | while read -r f; do
    cmp -s "$work/hand/data/$f" "$repo/data/$f" && continue
    cmp -s "$work/hand/data/$f" "$work/again/data/$f" || case "$f" in *.png) cmp -s "$work/again/data/$f" "$repo/data/$f" || echo "$f";; *) echo "$f";; esac
done >"$work/diff"
[ -s "$work/diff" ] && fail "hand-copying the tweaks' folders is not Apply: $(head -5 "$work/diff")"
[ "$failures" -eq 0 ] && echo "editor_tweak_parity: ok"
exit "$failures"
