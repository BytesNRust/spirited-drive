#!/bin/sh
# The reflectors shine in low light, not only at night (view_light.c reflect_low_light): Cliffside's signs with a reflective mask (data/track_masks/CL1) at dawn, node 300, with the player's lamps lit
# (drive_lamps_low): the picture with reflectors=1 has brighter texels than with reflectors=0 (the sign); the road's own light is not touched (no pool of light at dawn): the lowest rows of the
# screen (the road under the car, the dash) are the same in both. usage: tests/reflectors_dawn_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_reflect.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
for r in 0 1; do
  TNFS_REFLECTORS=$r TNFS_LIGHT_TIME=dawn TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive CL1 --car 3 --node 300 --screenshot "$work/s$r.ppm" > /dev/null 2>&1
done
res=$(python3 -I - "$work/s0.ppm" "$work/s1.ppm" << 'PY'
import sys
def load(p):
    d = open(p, "rb").read().split(b"\n", 3)
    w, h = map(int, d[1].split())
    return w, h, d[3]
try:
    w, h, a = load(sys.argv[1])
    _, _, b = load(sys.argv[2])
except Exception:
    print("none 0 0 0")
    sys.exit(0)
up = dn = low = 0
for i in range(0, w * h * 3, 3):
    d = sum(b[i:i + 3]) - sum(a[i:i + 3])
    up += d > 0
    dn += d < 0
    if i // 3 // w >= h - 60 and d != 0: low += 1
print("ok", up, dn, low)
PY
)
set -- $res
status=0
[ "$1" = ok ] || { echo "FAIL: no screenshots"; status=1; }
if [ $status -eq 0 ]; then
  [ "$2" -ge 50 ] || { echo "FAIL: no sign shines at dawn with the lamps lit ($2 brighter texels, want >= 50)"; status=1; }
  [ "$3" -le 10 ] || { echo "FAIL: the reflectors darkened $3 texels"; status=1; }
  [ "$4" -eq 0 ] || { echo "FAIL: the road and dash at the screen's foot changed ($4 texels)"; status=1; }
fi
[ $status -eq 0 ] && echo "reflectors_dawn_game: ok ($2 brighter, $3 darker, $4 at the foot)"
find "$work" -delete
exit $status
