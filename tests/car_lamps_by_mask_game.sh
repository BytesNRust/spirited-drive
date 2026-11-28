#!/bin/sh
# A car with masks lights its brake lamps by the masks alone, in Restomod only (car_livery_unlamp, drive_load_textures). The Viper's rsid texture paints its brake lights in palette entry 0xfe (the exe lights that
# entry by index: the oval y42-49 and a block y5-7, 185 texels). With the game's own mask (brake class on all but two of those) only those two change; with the oval moved elsewhere in the mask, Restomod points all 185 at a
# static entry (the game says how many in its log; the game's own mask leaves two of the 185 unmarked) and Stock and Restored leave them to the exe as today. Needs the CD's data (exit 77 without it).
# usage: tests/car_lamps_by_mask_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)
set -u
bin=$1
original=$2
[ -r "$original/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_lamps_mask.XXXXXX")
fail() { echo "FAIL: $*" >&2; echo "kept: $work" >&2; exit 1; }
here=$(dirname "$0")
mkdir -p "$work/own/DVIPER" "$work/moved/DVIPER"
cp data/car_masks/DViper/*.png "$work/own/DVIPER/" || fail "no data/car_masks/DViper"
cp data/car_masks/DViper/*.png "$work/moved/DVIPER/"
m=$work/moved/DVIPER/rsid_mask.png
python3 -I "$here/mask_poke.py" "$m" 20 0 50 60 255 || fail "mask_poke (erase)"      # the oval's and the block's texels (x26-57, y5-50) are no brake light any more ...
python3 -I "$here/mask_poke.py" "$m" 100 10 20 10 240 || fail "mask_poke (move)"     # ... the class is on a block that has no lamp texel
run() {   # profile masks-root -> the game's log
  u=$work/u_$1_$(basename "$2")
  mkdir -p "$u/editions"
  printf 'profile=%s\npsx_dir=\nlight_effects=1\n' "$1" > "$u/port.cfg"; printf 'light_effects=1\n' > "$u/$1.cfg"
  TNFS_VERBOSE=1 TNFS_CAR_MASKS="$2" TNFS_FRAMES=8 TNFS_AUTOPILOT=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$u/pref" \
    timeout -k 5 120 "$bin" --data "$original" --user "$u" --drive CY1 --car 5 --racers 1 --node 300 --screenshot "$u/shot.ppm" > "$u/out" 2>&1   # without --screenshot the race never ends: each run sat out the 120 s
  rc=$?   # (run() is a subshell: a failure here is left in a file, or Stock and Restored would pass on a game that never ran)
  { [ $rc -eq 0 ] && [ -s "$u/shot.ppm" ] && [ -s "$u/spirited-drive.log" ]; } || { echo "FAIL: $1 $(basename "$2"): the game did not run (exit $rc): $(tail -3 "$u/out")" >&2; : > "$work/bad"; }
  cat "$u/spirited-drive.log" 2>/dev/null
}
n_of() { sed -n 's/^car_lamps: slot [0-9]* DVIPER: \([0-9]*\) texels.*/\1/p' | head -n 1; }
got=$(run restomod "$work/moved" | n_of)
[ "$got" = 185 ] || fail "Restomod with the oval moved: the 185 lamp texels were not taken out of entry 0xfe (log said '${got}')"
got=$(run restomod "$work/own" | n_of)
[ "$got" = 2 ] || fail "Restomod with the game's own mask: '${got}' texels changed (the mask marks 183 of the 185 lamp texels: (44,5) and (50,6) are paint class in it, so only those two stop being lamps)"
for p in stock restored; do
  got=$(run $p "$work/moved" | n_of)
  [ -z "$got" ] || fail "$p with the oval moved: the exe's lamps were changed ($got texels)"
done
[ ! -e "$work/bad" ] || fail "a game run failed (above)"
echo "car_lamps_by_mask_game: ok"
find "$work" -mindepth 1 -delete && rmdir "$work"
