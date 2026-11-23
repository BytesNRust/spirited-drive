#!/bin/sh
# A car on a lit road is not black (view_light.c ratio_at_xz): a track with a light file (car_row) and no ratio of the world's own (Rusty Springs, TR1, by day) left the car's ratio unset at a full-light lane
# (lane 1), so the table was made from 0 and the cars were black in full sun while one lane in shade (<1) was right. TR1 at node 1500 has the white player 911 at the screen's left, behind the dash:
# its patch must be bright. usage: tests/car_lit_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_carlit.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
TNFS_SUN_SHADOW=0 TNFS_WEATHER=clear TNFS_LIGHT_TIME=day TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
  timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive TR1 --car 3 --node 1500 --screenshot "$work/s.ppm" > /dev/null 2>&1
lum=$(python3 -I - "$work/s.ppm" << 'PY'
import sys
d = open(sys.argv[1], "rb").read().split(b"\n", 3)
w, h = map(int, d[1].split())
px = d[3]
s = n = 0
for y in range(580, 640):
    for x in range(0, 150):
        i = (y * w + x) * 3
        s += (px[i] + px[i + 1] + px[i + 2]) / 3
        n += 1
print(int(s / n))
PY
)
status=0
[ -n "$lum" ] || { echo "FAIL: no screenshot"; status=1; }
[ "${lum:-0}" -gt 80 ] || { echo "FAIL: the white car in full light is black (mean $lum, want > 80)"; status=1; }
[ $status -eq 0 ] && echo "car_lit_game: ok (mean $lum)"
find "$work" -delete
exit $status
