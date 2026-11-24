#!/bin/sh
# The dash follows the track's light at dawn and dusk too, as the car's body does (drive_light_palette_half: the world's whole tint times the car's ratio, not half of it): Cliffside's dash
# (the Dash view, node 30) at dawn and at dusk is darker than at noon by at least a fifth (the half tint gave 0.86 and 0.85 of noon's light, the whole 0.72 and 0.68; the car's body 0.66 and 0.62); in Stock the time of day changes nothing
# (no light effects: the same picture). usage: tests/dash_light_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_dashlight.XXXXXX")
for mode in restomod stock; do
  mkdir -p "$work/$mode"
  printf 'profile=%s\npsx_dir=\n' "$mode" > "$work/$mode/port.cfg"
  for t in day dawn dusk; do
    TNFS_LIGHT_TIME=$t TNFS_WEATHER=clear TNFS_VIEW=0 TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
      timeout -k 5 300 "$bin" --data "$original" --user "$work/$mode" --drive CL1 --car 3 --node 30 --screenshot "$work/$mode-$t.ppm" > /dev/null 2>&1
  done
done
res=$(python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def dash(p):
    d = open(p, "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split())
    px = d[3]
    s = n = 0
    for y in range(int(H * .62), int(H * .9)):
        for x in range(int(W * .15), int(W * .5)):
            i = (y * W + x) * 3
            s += px[i] + px[i + 1] + px[i + 2]
            n += 1
    return s / n, px
try:
    rd, _ = dash(w + "/restomod-day.ppm")
    ra, _ = dash(w + "/restomod-dawn.ppm")
    rc, _ = dash(w + "/restomod-dusk.ppm")
    _, s0 = dash(w + "/stock-day.ppm")
    _, s1 = dash(w + "/stock-dusk.ppm")
except Exception:
    print("none 0 0 0")
    sys.exit(0)
print("ok %.3f %.3f %d" % (ra / rd, rc / rd, int(s0 == s1)))
PY
)
set -- $res
status=0
[ "$1" = ok ] || { echo "FAIL: no screenshots"; status=1; }
if [ $status -eq 0 ]; then
  echo "dash light vs noon: dawn $2, dusk $3; stock identical: $4"
  python3 -I -c "import sys; sys.exit(0 if float(sys.argv[1]) <= 0.8 and float(sys.argv[2]) <= 0.8 else 1)" "$2" "$3" || { echo "FAIL: the dash is not darker than noon by a fifth at dawn and dusk"; status=1; }
  [ "$4" = 1 ] || { echo "FAIL: the Stock dash changes with the time of day"; status=1; }
fi
[ $status -eq 0 ] && echo "dash_light_game: ok" && rm -rf "$work"
exit $status
