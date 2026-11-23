#!/bin/sh
# The road before a tunnel's mouth is not the tunnel's (track_node_light.c tunnel_in_file): the made light's rows ramp in a lit wall foot with 0s beyond it ahead of the mouth, and
# row_is_tunnel read those approach nodes as tunnel, so the road polygons there drew dark and red (28,8,4) as a rectangle. Vertigo Ridge (TR3) at node 470 looks at it: the road at
# (400,520) must be grey asphalt, not dark or tinted. usage: tests/tunnel_mouth_road_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_tunmouth.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
  timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive TR3 --car 3 --node 470 --screenshot "$work/s.ppm" > /dev/null 2>&1
rgb=$(python3 -I - "$work/s.ppm" << 'PY'
import sys
d = open(sys.argv[1], "rb").read().split(b"\n", 3)
w, h = map(int, d[1].split())
i = (520 * w + 400) * 3
print(d[3][i], d[3][i + 1], d[3][i + 2])
PY
)
status=0
[ -n "$rgb" ] || { echo "FAIL: no screenshot"; status=1; }
set -- $rgb
if [ $status -eq 0 ]; then
  hi=$1; lo=$1
  for c in $2 $3; do [ "$c" -gt "$hi" ] && hi=$c; [ "$c" -lt "$lo" ] && lo=$c; done
  [ "$lo" -ge 40 ] && [ $((hi - lo)) -le 16 ] || { echo "FAIL: the road before the tunnel mouth is dark or tinted ($rgb, want grey, every channel >= 40)"; status=1; }
fi
[ $status -eq 0 ] && echo "tunnel_mouth_road_game: ok ($rgb)"
find "$work" -delete
exit $status
