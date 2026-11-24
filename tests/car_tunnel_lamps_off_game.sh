#!/bin/sh
# Alpine 1's tunnel (nodes 1596-1607) by night: with the tunnel's lamps at their default the car's ratio alternates node to node (every second node 0.75 of the one before: the lamps' rhythm), and with
# `lamps 0` in every tunnel_look (an unlit tunnel) it is the same on every node: the car does not blink going through (TNFS_CAR_RATIO_DUMP in view_light.c; light_synth.c car_tunnel_look).
# The track's file is read from data/track_light/ of the working directory (data_path.h): a copy with the lamps changed is put there.
# usage: tests/car_tunnel_lamps_off_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
src=$(pwd)/data/track_light/AL1.txt
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
[ -f "$src" ] || { echo "FAIL: no $src"; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_lampsoff.XXXXXX")
mkdir -p "$work/user" "$work/data/track_light"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
run() {   # FILE OUT
  cp "$1" "$work/data/track_light/AL1.txt"
  rm -f "$2"
  (cd "$work" && TNFS_CAR_RATIO_DUMP="$2" TNFS_AUTOPILOT=1 TNFS_FRAMES=300 TNFS_LIGHT_TIME=night SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive AL1 --node 5200 --screenshot "$work/s.ppm" > /dev/null 2>&1)
}
sed 's/^\(  tunnel_look .*\)$/\1 lamps 0/' "$src" > "$work/off.txt"
run "$src" "$work/on_ratios"; run "$work/off.txt" "$work/off_ratios"
# the red channel of the car's ratio at each node of the tunnel's middle (1599..1604, past the mouths' gradient steps): the spread of the nodes' values
spread() { awk '$1 >= 1599 && $1 <= 1604 { if (!($1 in v)) { v[$1] = $2 } } END { n = 0; for (k in v) { if (n == 0 || v[k] < lo) lo = v[k]; if (n == 0 || v[k] > hi) hi = v[k]; n++ } printf "%d %.3f\n", n, hi - lo }' "$1"; }
set -- $(spread "$work/on_ratios"); on_n=$1; on_spread=$2
set -- $(spread "$work/off_ratios"); off_n=$1; off_spread=$2
status=0
if [ "$on_n" -lt 6 ] || [ "$off_n" -lt 6 ]; then echo "FAIL: the run did not reach the tunnel (nodes seen: $on_n, $off_n)"; status=1
elif ! awk -v s="$on_spread" 'BEGIN { exit !(s >= 0.1) }'; then echo "FAIL: the lamps' default lost the alternation (spread $on_spread)"; status=1
elif ! awk -v s="$off_spread" 'BEGIN { exit !(s < 0.005) }'; then echo "FAIL: the unlit tunnel's car still alternates (spread $off_spread)"; status=1
else echo "car_tunnel_lamps_off_game: ok (spread $on_spread with the lamps, $off_spread at lamps 0)"; fi
find "$work" -delete
exit $status
