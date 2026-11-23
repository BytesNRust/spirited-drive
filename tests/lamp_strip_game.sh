#!/bin/sh
# A `lamp` line's STRIP in the game (track_light_file.h, drive_light.c place_stripped_lamps): AL1's night with one lamp at node 700 on `left` lights the row's left points more than its right ones, and on `right` the other way
# round (the dump of the made rows, TNFS_LIGHT_ROWS_DUMP); a lamp with no strip lights the road's middle. Runs on a scratch copy of data/ (the file gets a lamp line), never the tracked one.
# usage: tests/lamp_strip_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_lamp.XXXXXX")
mkdir -p "$work/user" "$work/run/data/track_light"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
root=$(pwd)
for d in "$root"/data/*; do [ "$(basename "$d")" = track_light ] || ln -s "$d" "$work/run/data/$(basename "$d")"; done
rows_with() {   # the lamp line's strip word(s) (none: ""): node 700's 11 points
  awk -v lamp="  lamp 700 100 $1" '{ print } /^time N/ { print lamp }' "$root/data/track_light/AL1.txt" > "$work/run/data/track_light/AL1.txt"
  rm -f "$work/rows"
  (cd "$work/run" && TNFS_WEATHER=clear TNFS_LIGHT_TIME=night TNFS_LIGHT_ROWS_DUMP="$work/rows" TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive AL1 --node 30 --screenshot "$work/s.ppm" > /dev/null 2>&1)
  awk '$1 == "node" && $2 == 700 { s = 0; for (p = 3; p <= 7; p++) s += $p; l = 0; for (p = 8; p <= 12; p++) l += $p; print s, l }' "$work/rows" 2> /dev/null
}
status=0
left=$(rows_with left); right=$(rows_with right); none=$(rows_with "")
[ -n "$left" ] && [ -n "$right" ] || { echo "FAIL: no dumped rows"; status=1; }
lr=${left% *}; ll=${left#* }; rr=${right% *}; rl=${right#* }
[ "${ll:-0}" -gt "${lr:-0}" ] || { echo "FAIL: a lamp on the left strip does not light the left points more ($left)"; status=1; }
[ "${rr:-0}" -gt "${rl:-0}" ] || { echo "FAIL: a lamp on the right strip does not light the right points more ($right)"; status=1; }
[ "$left" != "$right" ] || { echo "FAIL: the strip changed nothing"; status=1; }
[ $status -eq 0 ] && echo "lamp_strip_game: ok ($left | $right | $none)"
find "$work" -delete
exit $status
