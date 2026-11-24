#!/bin/sh
# The reflectors by day follow the weather (owner's rule 2026-11-24, view_light.c reflect_low_light + lamp_emissive.c reflect_by_day_ok): Cliffside's signs (data/track_masks/CL1), node 300, in
# the day's light with the player's lamps lit: in fog the marks shine (reflectors=1 has brighter texels than reflectors=0), in rain and in clear sun they do not; the road and dash at the screen's
# foot are the same either way (no puddle of light by day). usage: tests/reflectors_day_weather_game.sh <tnfs binary> <original/ dir>   (run from the repo's root)  Exit 77 without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_reflect_day.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\nlamp_flares=1\nrain_lamps=1\n' > "$work/user/port.cfg"
status=0
for w in clear rain fog; do
  for r in 0 1; do
    TNFS_REFLECTORS=$r TNFS_LIGHT_TIME=day TNFS_WEATHER=$w TNFS_AUTOPILOT=1 TNFS_FRAMES=60 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
      timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive CL1 --car 3 --node 300 --screenshot "$work/$w$r.ppm" > /dev/null 2>&1
  done
  res=$(python3 -I - "$work/${w}0.ppm" "$work/${w}1.ppm" << 'PY'
import sys
def load(p):
    d = open(p, "rb").read().split(b"\n", 3)
    w, h = map(int, d[1].split())
    return w, h, d[3]
try:
    w, h, a = load(sys.argv[1])
    _, _, b = load(sys.argv[2])
except Exception:
    print("none 0 0")
    sys.exit(0)
up = low = 0
for i in range(0, w * h * 3, 3):
    d = sum(b[i:i + 3]) - sum(a[i:i + 3])
    up += d > 0
    if i // 3 // w >= h - 60 and d != 0: low += 1
print("ok", up, low)
PY
)
  set -- $res
  [ "$1" = ok ] || { echo "FAIL: $w: no screenshots"; status=1; continue; }
  echo "$w: $2 brighter texels with reflectors, $3 at the foot"
  [ "$3" -eq 0 ] || { echo "FAIL: $w: the road or dash at the foot changed"; status=1; }
  if [ "$w" = fog ]; then [ "$2" -ge 50 ] || { echo "FAIL: no mark shines in day fog with the lamps lit"; status=1; }
  else [ "$2" -le 10 ] || { echo "FAIL: $w by day: marks shine ($2 texels)"; status=1; }; fi
done
[ $status -eq 0 ] && echo "reflectors_day_weather_game: ok"
find "$work" -delete
exit $status
