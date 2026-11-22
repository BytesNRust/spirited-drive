#!/bin/sh
# The fingerprint of the light the game makes for each of the 16 tracks at each of the 4 times of day (game/race/track_light_fingerprint.h), against tests/track_light_fingerprint.txt.
# Any change of a light file, of the format or of the generator that changes a light shows here; one that is meant is written with  UPDATE=1 sh tests/track_light_fingerprint.sh ...
# usage: tests/track_light_fingerprint.sh <tnfs binary> <original/ dir> [expected file]      (run from the repo's root: the game reads data/ from there)
# Exit 77 (skipped) without the CD's data. TRACKS="CY1 TR3" restricts the tracks (the comparison then covers only them).
set -u
bin=$1
original=$2
expected=${3:-tests/track_light_fingerprint.txt}
tracks=${TRACKS:-"AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7"}
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_light_print.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
run_track() {   # one track at its four times, into its own file (the tracks run side by side)
  for time in dawn day dusk night; do
    TNFS_LIGHT_FINGERPRINT="$work/$1.txt" TNFS_LIGHT_TIME=$time TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref_$1" \
      timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive "$1" --node 30 --screenshot "$work/$1.ppm" > /dev/null 2>&1 || echo "failed: $1 $time" >&2
  done
}
n=0
for track in $tracks; do
  run_track "$track" &
  n=$((n + 1))
  [ $((n % 3)) -eq 0 ] && wait
done
wait
cat "$work"/*.txt 2>/dev/null | LC_ALL=C sort > "$work/now"
lines=$(wc -l < "$work/now" | tr -d ' ')
want=$(($(echo $tracks | wc -w) * 4))
status=0
if [ "$lines" != "$want" ]; then echo "FAIL: $lines fingerprints, $want expected"; status=1; fi
if [ "${UPDATE:-0}" = 1 ] && [ $status -eq 0 ]; then
  cp "$work/now" "$expected"; echo "wrote $expected ($lines lines)"
elif [ $status -eq 0 ]; then
  if [ -z "${TRACKS:-}" ]; then cp "$expected" "$work/want"; else grep -E "^($(echo $tracks | tr ' ' '|'))[ABCN] " "$expected" > "$work/want"; fi
  if diff "$work/want" "$work/now" > "$work/diff"; then echo "track_light_fingerprint: ok ($lines)"; else echo "FAIL: the light changed:"; cat "$work/diff"; status=1; fi
fi
find "$work" -delete
exit $status
