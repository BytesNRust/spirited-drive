#!/bin/sh
# A car kit in the current layout in the Studio, the happy path a user takes: Open (the car file is built as it opens), Preview, Edit mask and Save mask
# (in the kit's flat car_masks/), Preview shows the edit, Build (build/piece and the zip carry the edit), Install into a player folder. The kit is a car you
# drive, laid out by `tnfs-studio car kit`, its card given a stem of its own (a mod's stem is no original car's). Needs the game's files (original/).
# usage: editor_e2e_kit.sh EDITOR TNFS ORIGINAL_DIR PORT_DATA STUDIO
. "$(dirname "$0")/editor_headless_common.sh"
tnfs=$2
orig=$3
data=$4
studio_cli=$5
[ -r "$orig/SIMDATA/CARFAMS/P911.CFM" ] && [ -d "$data/car_masks/P911" ] || { echo "skip: no game files in $orig"; exit 0; }
runs=0
studio() { runs=$((runs + 1)); "$editor" "$@" --user "$work/player$runs" >"$work/log" 2>&1; }

# 1. a kit of a car you drive: the game's 911 laid out as sources (export, then car kit), with a card of its own
studio --export-car P911 --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car: $(cat "$work/log")"
"$studio_cli" car kit "$work/cars/p911-original" "$work/kit" >"$work/log" 2>&1 || fail "car kit: $(cat "$work/log")"
[ -f "$work/kit/mod.ini" ] && [ -d "$work/kit/model/near/bank" ] || { fail "no kit was made"; exit "$failures"; }
sed 's/^stem=.*/stem=FLOW911/; s/^id=.*/id=flow911/' "$work/kit/mod.ini" > "$work/mod.ini.new" && mv "$work/mod.ini.new" "$work/kit/mod.ini"
kit="$work/kit"
magick -size 309x166 xc:'#808080' "$kit/card.png" || fail "no card.png: the game wants Control Central's picture (309 x 166) in a piece"
mask="$kit/car_masks/frnt_mask.png"
[ -f "$mask" ] || fail "the kit has no flat car_masks/frnt_mask.png"
[ ! -d "$kit/build" ] || fail "a fresh kit has a build/"

# 2. Open: the car file is built as the kit opens (a headless run waits for it), the Preview shows the car
preview() { studio "$kit" --view day --preview-tex frnt --frames 14 --shot "$1"; }
preview "$work/preview-before.png" || fail "Open and Preview of the kit: $(cat "$work/log")"
[ -f "$kit/build/piece/SIMDATA/CARFAMS/FLOW911.CFM" ] || fail "Open did not build the car file: $(cat "$work/log")"
[ -s "$work/preview-before.png" ] || fail "no PNG of the Preview"

# 3. Edit mask and Save mask: the kit's flat file is written, and nothing else in the kit
cp "$mask" "$work/mask-before.png"
studio "$kit" --mask frnt --mask-rect 40,40,8,8,90 --mask-save --frames 8 || fail "Edit mask and Save mask: $(cat "$work/log")"
grep -q "saved frnt_mask.png" "$work/log" || fail "no word that the mask was saved: $(cat "$work/log")"
cmp -s "$mask" "$work/mask-before.png" && fail "the saved mask is the file it was"

# 4. Preview shows the edit
preview "$work/preview-after.png" || fail "Preview of the edited kit: $(cat "$work/log")"
cmp -s "$work/preview-before.png" "$work/preview-after.png" && fail "the Preview does not show the edit"

# 5. Build: the package is made again from the sources, and carries the edit under the stem
studio "$kit" --build --frames 2 || fail "Build: $(cat "$work/log")"
[ -s "$kit/build/flow911.zip" ] || fail "Build made no build/flow911.zip: $(cat "$work/log")"
cmp -s "$mask" "$kit/build/piece/car_masks/FLOW911/frnt_mask.png" || fail "the built piece does not carry the edited mask"

# 6. Install: the game unpacks the package into the player folder
"$editor" "$kit" --install --game "$tnfs" --frames 2 --user "$work/installed" >"$work/log" 2>&1 || fail "Install: $(cat "$work/log")"
grep -q "installed flow911" "$work/log" || fail "Install does not say it installed the piece: $(cat "$work/log")"
grep -rlq "^id=flow911" "$work/installed" || fail "the piece is not in the player folder"

[ "$failures" -eq 0 ] && echo "editor_e2e_kit: ok"
exit "$failures"
