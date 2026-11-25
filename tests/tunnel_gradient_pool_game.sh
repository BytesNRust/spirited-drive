#!/bin/sh
# The headlight pool is the same with the tunnel gradient as without it: Alpine's first tunnel (nodes 1596..1607) at night with the light effects on (the night's second look, the beam) and its lamps at 0 %
# (an unlit tunnel: the car's beam lights it at any hour), the chase view with the car at node 1594 (outside, the mouth ahead; the gradient's outer steps are on the road in front of it): the road in
# front of the car is as light with the gradient's outer steps (the file's own) as with TNFS_GRADIENT_OUTER=0 (none), within 5 %. The outer ramp used to darken the byte the pool scales, so the pool lit a third less.
# usage: tests/tunnel_gradient_pool_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_gradient_pool.XXXXXX")
mkdir -p "$work/user" "$work/unlit/track_light"
printf 'profile=restomod\npsx_dir=\nlight_effects=1\n' > "$work/user/port.cfg"
printf 'light_effects=1\n' > "$work/user/restomod.cfg"
awk '{ print } /^time N$/ { print "  tunnel_look 1596 1607 60 lamps 0" }' data/track_light/AL1.txt > "$work/unlit/track_light/AL1.txt"
grep -q 'lamps 0' "$work/unlit/track_light/AL1.txt" || { echo "FAIL: AL1's night block not found in data/track_light"; exit 1; }
shot() {   # outer steps 0|1, out
  TNFS_GRADIENT_OUTER=$1 TNFS_LIGHT_TIME=night TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=3 TNFS_NO_HUD=1 TNFS_START_NODE=1594 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --mods "$work/unlit" --drive AL1 --car 3 --racers 1 --node 20 --screenshot "$2" > /dev/null 2>&1
}
shot 1 "$work/on.ppm"
shot 0 "$work/off.ppm"
res=$(python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def mean(p):
    d = open(p, "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split())
    px = d[3]
    s = n = 0
    for y in range(int(H * .45), int(H * .95)):
        for x in range(int(W * .15), int(W * .85)):
            i = (y * W + x) * 3
            s += px[i] + px[i + 1] + px[i + 2]
            n += 3
    return s / n
try:
    on, off = mean(w + "/on.ppm"), mean(w + "/off.ppm")
    print("%.2f %.2f" % (on, off))
except Exception as e:
    print("ERR %s" % e)
PY
)
set -- $res
if [ "$1" = ERR ] || [ -z "${2:-}" ]; then echo "FAIL: $res"; exit 1; fi
echo "road in front of the car: with the outer steps $1, without $2"
python3 -I -c "import sys; on, off = float('$1'), float('$2'); sys.exit(0 if off > 8 and on >= 0.95 * off else 1)" || { echo "FAIL: the outer steps darken the beam's pool ($1 against $2)"; exit 1; }
echo "ok"
