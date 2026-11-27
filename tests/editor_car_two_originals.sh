#!/bin/sh
# Two original cars through Copy to game repo, headless, from fixture kits (no game files): the Diablo (LDiabl, a table row of two colours) and the NSX (ANSX, four colours, the row
# that sits after the Viper's). Each: a colour changed and saved is listed as was/now, Copy writes that one row and nothing else, the second ask says nothing is left, a colour the
# row does not have is refused, and a mask changed beside the game's own is listed as a file to copy and copied byte for byte.
# usage: editor_car_two_originals.sh EDITOR
. "$(dirname "$0")/editor_headless_common.sh"
repo_src="$(cd "$(dirname "$0")/.." && pwd)"
studio() { "$editor" "$@" --frames 2; }

# the car's masks folder in the game repo is spelt as the table's row, which a case-sensitive file system tells apart from the car file's stem
masks_folder() { [ "$1" = LDIABL ] && echo LDiabl || echo "$1"; }

# one car: STEM, the paints= text of its card (the table's own), the index of the colour to change, the old and new colour as the window lists them, the mask folder's name
one_car() {
    stem=$1 paints=$2 index=$3 old=$4 new=$5
    kit=$work/kit-$stem repo=$work/repo-$stem table=$repo/core/formats/car_paint.c
    masks=$(masks_folder "$stem")
    mkdir -p "$kit/art-generated/car_masks" "$kit/model/near/bank" "$repo/data/car_masks/$masks" "$repo/core/formats"
    cp "$repo_src/core/formats/car_paint.c" "$table"
    printf 'id=orig\nname=Orig\nversion=1\ntype=car\nstem=%s\nsound_from=911\npaints=%s\n' "$stem" "$paints" > "$kit/mod.ini"
    studio "$kit" --port-data "$repo/data" --steps repo-ask,status > "$work/log" 2>&1 && fail "$stem: Copy with the game's own colors went on: $(cat "$work/log")"
    # the game's own card shows the lines it has and not empty boxes: Version is there, Year (not in the card) is not drawn
    studio "$kit" --port-data "$repo/data" --steps psd-wait:2,ui-click:card_Version > "$work/log" 2>&1 || fail "$stem: the card's Version line is not on screen: $(cat "$work/log")"
    studio "$kit" --port-data "$repo/data" --steps psd-wait:2,ui-click:card_Year > "$work/log" 2>&1 && fail "$stem: an empty Year box is drawn on the game's own card"
    grep -q "no such widget" "$work/log" || fail "$stem: the Year line was not looked for: $(cat "$work/log")"
    studio "$kit" --port-data "$repo/data" --steps paint-rgb:"$index",1,2,3,save,repo-ask,print-publish > "$work/log" 2>&1 || fail "$stem: a changed color: $(cat "$work/log")"
    grep -qi "core/formats/car_paint.c, the row of $stem" "$work/log" && grep -q "was: $old" "$work/log" && grep -q "now: $new" "$work/log" && grep -q "^  colors: [0-9]" "$work/log" || fail "$stem: the window does not list was/now and the count of colors: $(cat "$work/log")"
    cmp -s "$table" "$repo_src/core/formats/car_paint.c" || fail "$stem: the preview wrote into car_paint.c"
    studio "$kit" --port-data "$repo/data" --steps repo-ask,repo-copy,status > "$work/log" 2>&1 || fail "$stem: the copy: $(cat "$work/log")"
    grep -q "^status: copied to the game repo: .*the colors into $table" "$work/log" || fail "$stem: Copy did not say what it wrote: $(cat "$work/log")"
    [ "$(diff "$table" "$repo_src/core/formats/car_paint.c" | grep -c '^[<>]')" = 2 ] || fail "$stem: Copy changed more than the one line of its paints: $(diff "$table" "$repo_src/core/formats/car_paint.c")"
    grep -q '{ "[^"]*", { 1, 2, 3 } }' "$table" || fail "$stem: the row has no new color"
    studio "$kit" --port-data "$repo/data" --steps repo-ask,status > "$work/log" 2>&1 && fail "$stem: Copy with nothing changed went on"
    # a colour the row does not have (one past its last)
    count=$(( $(printf '%s' "$paints" | tr ',' '\n' | wc -l | tr -d ' ') + 1 ))
    studio "$kit" --port-data "$repo/data" --steps paint-rgb:"$count",9,9,9 > "$work/log" 2>&1 && fail "$stem: a colour past the card's list was taken"
    # a mask changed beside the game's own: listed, copied byte for byte
    printf 'game-mask' > "$repo/data/car_masks/$masks/rsid_mask.png"
    printf 'new-mask' > "$kit/art-generated/car_masks/rsid_mask.png"
    studio "$kit" --port-data "$repo/data" --steps repo-ask,print-publish > "$work/log" 2>&1 || fail "$stem: the mask change: $(cat "$work/log")"
    grep -q "rsid_mask.png" "$work/log" || fail "$stem: the changed mask is not listed: $(cat "$work/log")"
    studio "$kit" --port-data "$repo/data" --steps repo-ask,repo-copy,status > "$work/log" 2>&1 || fail "$stem: copying the mask: $(cat "$work/log")"
    [ "$(cat "$repo/data/car_masks/$masks/rsid_mask.png")" = new-mask ] || fail "$stem: the mask was not copied: $(cat "$work/log")"
    # the dash masks: the folder is spelt as the car file's stem (LDIABL), the car's masks as the table's row (LDiabl)
    mkdir -p "$kit/art-generated/dash_masks" "$repo/data/dash_masks/$stem"
    printf 'game-dash' > "$repo/data/dash_masks/$stem/dash_mask.png"
    printf 'new-dash' > "$kit/art-generated/dash_masks/dash_mask.png"
    studio "$kit" --port-data "$repo/data" --steps repo-ask,repo-copy,status > "$work/log" 2>&1 || fail "$stem: copying the dash mask: $(cat "$work/log")"
    [ "$(cat "$repo/data/dash_masks/$stem/dash_mask.png")" = new-dash ] || fail "$stem: the dash mask did not reach dash_masks/$stem: $(cat "$work/log")"
    # nothing else of the repo moved
    [ "$(diff "$table" "$repo_src/core/formats/car_paint.c" | grep -c '^[<>]')" = 2 ] || fail "$stem: the mask copy touched car_paint.c again"
}

one_car LDIABL "Rosso Diablo:8a0600,30th Anniversary Lavende:b57edc,Giallo Orion:eba628,Grigio Antares:494844,Blue Hera:091a30,Verde Hydra:081815,Celeste:48abb4,Bianco:e1e3db" 0 "Rosso Diablo #8a0600, 30th Anniversary Lavender #b57edc, Giallo Orion #eba628, Grigio Antares #494844, Blue Hera #091a30, Verde Hydra #081815, Celeste #48abb4, Bianco #e1e3db" "Rosso Diablo #010203, 30th Anniversary Lavender #b57edc, Giallo Orion #eba628, Grigio Antares #494844, Blue Hera #091a30, Verde Hydra #081815, Celeste #48abb4, Bianco #e1e3db"   # the table's row: the owner's 8 colours
one_car ANSX "Berlina Black:0e0e10,Grand Prix White:f1f1ee,Formula Red:b3121e,Citrous Yellow Metallic:e6d94a" 2 "Berlina Black #0e0e10, Grand Prix White #f1f1ee, Formula Red #b3121e, Citrous Yellow Metallic #e6d94a" "Berlina Black #0e0e10, Grand Prix White #f1f1ee, Formula Red #010203, Citrous Yellow Metallic #e6d94a"

# the card's Physics from row: a game car shows its own physics (the NSX: «nsx», as Sound from names its own sound), off, and its box starts where Sound from's does; a kit that is not a game car says «not set» until a car is picked
studio "$work/kit-ANSX" --steps psd-wait:2,physics-print,ui-rect:sound_from_box,ui-rect:physics_box > "$work/log" 2>&1 || fail "the Physics from row of the NSX: $(cat "$work/log")"
grep -qx 'physics row: nsx' "$work/log" || fail "an original car's Physics from row does not show its own physics (nsx): $(cat "$work/log")"
sound_x=$(sed -n 's/^ui rect: sound_from_box \([0-9]*\) .*/\1/p' "$work/log")
physics_x=$(sed -n 's/^ui rect: physics_box \([0-9]*\) .*/\1/p' "$work/log")
[ -n "$sound_x" ] && [ "$sound_x" = "$physics_x" ] || fail "the Physics from box is not aligned with Sound from's: sound x '$sound_x', physics x '$physics_x'"
mkdir -p "$work/kit-own/model/near/bank"
printf 'id=own\nname=Own\nversion=1\ntype=car\nstem=MYCAR\nsound_from=911\n' > "$work/kit-own/mod.ini"
studio "$work/kit-own" --steps psd-wait:2,physics-print > "$work/log" 2>&1 || fail "the Physics from row of an own car: $(cat "$work/log")"
grep -qx 'physics row: not set' "$work/log" || fail "an own car with no pick does not say not set: $(cat "$work/log")"

[ "$failures" = 0 ] && echo ok
exit "$failures"
