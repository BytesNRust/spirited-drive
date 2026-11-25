#!/bin/sh
# The portal does not outshine the rock beside it: Alpine's second stage (AL3, day, clear), the outside camera at node 1077 with the tunnel that starts at 1080 ahead: the portal's lintel is at most 1.25 times
# as bright as the mean of the two rock walls beside it (the road's light at the mouth is ramped down by the gradient's outer steps; without them the lintel was 1.6 to 2.3 times the walls).
# usage: tests/portal_wall_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_portal_wall.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
TNFS_VIEW=1 TNFS_LIGHT_TIME=day TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_NO_HUD=1 TNFS_START_NODE=1077 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
  timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive AL3 --car 3 --racers 1 --node 3 --screenshot "$work/a.ppm" > /dev/null 2>&1
res=$(python3 -I - "$work/a.ppm" << 'PY'
import sys
try:
    d = open(sys.argv[1], "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split())
    p = d[3]
    def mean(x0, y0, x1, y1):   # boxes given for a 1280x960 picture
        x0, x1, y0, y1 = x0 * W // 1280, x1 * W // 1280, y0 * H // 960, y1 * H // 960
        s = n = 0
        for y in range(y0, y1):
            for x in range(x0, x1):
                i = (y * W + x) * 3
                s += p[i] + p[i + 1] + p[i + 2]
                n += 3
        return s / n
    print("%.2f %.2f %.2f" % (mean(420, 185, 960, 235), mean(20, 150, 350, 400), mean(1000, 150, 1260, 400)))
except Exception as e:
    print("ERR %s" % e)
PY
)
set -- $res
if [ "$1" = ERR ] || [ -z "${3:-}" ]; then echo "FAIL: $res"; exit 1; fi
echo "lintel $1, left wall $2, right wall $3"
python3 -I -c "import sys; l, a, b = map(float, sys.argv[1:]); sys.exit(0 if l <= 1.25 * (a + b) / 2 else 1)" "$1" "$2" "$3" || { echo "FAIL: the portal's lintel outshines the walls (lintel $1, walls $2 and $3)"; exit 1; }
echo "portal_wall_game: ok" && rm -rf "$work"
