#!/bin/sh
# The owner's flow on an original car (his ANSX, 2026-11-23): the kit has the game's masks as PNGs but no manifest (the migration copied them in), he paints a PSD, and the PSD must win.
#  1. a PNG that is the game's own picture and has no manifest line is not "edited by hand": Studio writes its hash down and Convert PSD (the automatic one) applies the newer PSD;
#  2. a PNG that really was edited (not the game's picture, no manifest line) stays protected by Convert PSD, and says so in words;
#  3. Regenerate from PSD is then enabled (it was grey), clicked by name through ui_probe, and replaces it; the PSD is never written.
# Needs the game's files (original/). usage: editor_original_psd_newer.sh EDITOR ORIGINAL_DIR DATA_DIR MAKE_TEST_PSD
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2 data=$3 make_psd=$4
[ -r "$orig/SIMDATA/CARFAMS/ANSX.CFM" ] && [ -d "$data/car_masks/ANSX" ] || { echo "skip: no game files in $orig"; exit 0; }
runs=0
studio() { runs=$((runs + 1)); t0=$SECONDS; "$editor" "$@" --user "$work/player$runs" >"$work/log" 2>&1; rc=$?; echo "editor_original_psd_newer: run $runs took $((SECONDS - t0))s" >&2; return $rc; }
mkdir -p "$work/cars"

# a kit as the migration left it: the game's masks, a PSD of the owner's saved after them, and no .generated.tsv
kit_of() {   # kit_of NAME: a new ANSX kit in $work/cars/NAME with the rsid PSD painted after its PNG
    studio --export-car ANSX --into "$work/cars/$1" --cd "$orig" --port-data "$data" --frames 1 || { fail "Export original car for $1: $(cat "$work/log")"; return 1; }
    k=$(ls -d "$work/cars/$1"/*/ | head -n 1); k=${k%/}
    rm -f "$k/art-generated/.generated.tsv"
    mkdir -p "$k/art-source/masks" && "$make_psd" "$k/art-source/masks/rsid.psd" paint || { fail "no PSD fixture"; return 1; }
    python3 -I -c "import os,sys,time; t=time.time()+120; os.utime(sys.argv[1], (t, t))" "$k/art-source/masks/rsid.psd"   # saved after the PNG
    echo "$k"
}

# 1. the game's own PNG: the newer PSD is applied by itself
k1=$(kit_of one) || exit "$failures"
cmp -s "$k1/art-generated/car_masks/rsid_mask.png" "$data/car_masks/ANSX/rsid_mask.png" || fail "the exported rsid mask is not the game's"
studio "$k1" --port-data "$data" --frames 30 --steps psd-wait:2500
cmp -s "$k1/art-generated/car_masks/rsid_mask.png" "$data/car_masks/ANSX/rsid_mask.png" && fail "the game's own PNG was kept as edited by hand: the newer PSD was not applied: $(tail -5 "$work/log")"
grep -q '^rsid_mask.png' "$k1/art-generated/.generated.tsv" 2>/dev/null || fail "the converted mask is not in the manifest: $(cat "$k1/art-generated/.generated.tsv" 2>/dev/null)"

# 2. a PNG somebody edited (not the game's picture, no manifest line): kept by Convert PSD, with words
k2=$(kit_of two) || exit "$failures"
python3 -I "$(dirname "$0")/mask_poke.py" "$k2/art-generated/car_masks/rsid_mask.png" 40 40 20 20 248 || fail "the outside change of the mask"
cp "$k2/art-generated/car_masks/rsid_mask.png" "$work/edited.png"
studio "$k2" --port-data "$data" --frames 30 --steps psd-wait:2500
cmp -s "$k2/art-generated/car_masks/rsid_mask.png" "$work/edited.png" || fail "Convert PSD replaced a PNG that was edited by hand: $(tail -5 "$work/log")"
grep -q "^psd: idle: .*Kept, not replaced: rsid_mask.png" "$work/log" && grep -q "Regenerate from PSD" "$work/log" || fail "no words about what was kept and what to press: $(tail -8 "$work/log")"

# 3. Regenerate from PSD is enabled and replaces it (clicked by its name, then the Replace of its question)
studio "$k2" --port-data "$data" --frames 30 --steps panel:4,panel:4,ui-click:psd_regenerate,ui-click:psd_replace,psd-wait:2500
cmp -s "$k2/art-generated/car_masks/rsid_mask.png" "$work/edited.png" && fail "Regenerate from PSD did not replace the PNG (the button was grey or the click missed): $(tail -8 "$work/log")"
[ -f "$k2/art-source/masks/rsid.psd" ] && cmp -s "$k2/art-source/masks/rsid.psd" "$k1/art-source/masks/rsid.psd" || fail "the PSD changed or went"

[ "$failures" -eq 0 ] && echo "editor_original_psd_newer: ok"
exit "$failures"
