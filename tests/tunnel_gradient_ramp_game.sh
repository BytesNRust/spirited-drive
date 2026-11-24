#!/bin/sh
# A tunnel mouth's gradient darkens the road node by node: Alpine's first tunnel (nodes 1596..1607), the dawn, the bumper view with the car at nodes 1595..1599 (the road in the picture's lower band is the nearest
# polygons: the open road at 1595, the mouth's steps at 1596..1598, the tunnel's own from 1599). With the file's gradient the brightness of that band falls from one node to the next (no step brighter than the
# one before it by more than the lamps' alternation, 2.0) and the first node in takes under three quarters of the whole fall; with `gradient 0` (the straight cut) the first node in takes almost all of it.
# usage: tests/tunnel_gradient_ramp_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_ramp.XXXXXX")
mkdir -p "$work/user" "$work/cut/track_light"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
grep -q '^tunnel 1596 1607$' data/track_light/AL1.txt || { echo "FAIL: AL1's first tunnel is not 1596 1607 in data/track_light"; exit 1; }
sed 's/^tunnel 1596 1607$/tunnel 1596 1607 gradient 0/' data/track_light/AL1.txt > "$work/cut/track_light/AL1.txt"
shot() {   # node, out, [mods dir]
  if [ -n "${3:-}" ]; then m="--mods $3"; else m=""; fi
  TNFS_LIGHT_TIME=dawn TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=1 TNFS_NO_HUD=1 TNFS_START_NODE=$1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" $m --drive AL1 --car 3 --racers 1 --node 3 --screenshot "$2" > /dev/null 2>&1
}
for n in 1595 1596 1597 1598 1599; do
  shot $n "$work/g$n.ppm"
  shot $n "$work/c$n.ppm" "$work/cut"
done
res=$(python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def band(p):
    d = open(p, "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split())
    pix = d[3]
    tot = n = 0
    for y in range(int(H * .62), int(H * .95)):
        for x in range(int(W * .35), int(W * .65)):
            i = (y * W + x) * 3
            tot += (pix[i] + pix[i + 1] + pix[i + 2]) / 3
            n += 1
    return tot / n
try:
    g = [band("%s/g%d.ppm" % (w, n)) for n in range(1595, 1600)]
    c = [band("%s/c%d.ppm" % (w, n)) for n in range(1595, 1600)]
    print("ok", " ".join("%.2f" % v for v in g + c))
except Exception:
    print("none")
PY
)
set -- $res
status=0
[ "$1" = ok ] || { echo "FAIL: no screenshots"; status=1; }
if [ $status -eq 0 ]; then
  shift
  echo "with the gradient: $1 $2 $3 $4 $5; the straight cut: $6 $7 $8 $9 ${10}"
  python3 -I - "$@" << 'PY' || status=1
import sys
v = [float(a) for a in sys.argv[1:]]
g, c = v[:5], v[5:]
ok = True
for a, b in zip(g, g[1:]):
    if b > a + 2.0:
        print("FAIL: the road gets lighter going into the tunnel (%.1f then %.1f)" % (a, b)); ok = False
share_g = (g[0] - g[1]) / max(g[0] - g[4], 1e-6)
share_c = (c[0] - c[1]) / max(c[0] - c[4], 1e-6)
if share_g > 0.75:
    print("FAIL: the first node in takes %.0f %% of the whole fall with the gradient (over 75 %%)" % (100 * share_g)); ok = False
if share_c < share_g + 0.1:
    print("FAIL: the straight cut (%.0f %%) is not a harder step than the gradient (%.0f %%)" % (100 * share_c, 100 * share_g)); ok = False
sys.exit(0 if ok else 1)
PY
fi
[ $status -eq 0 ] && echo "tunnel_gradient_ramp_game: ok" && rm -rf "$work"
exit $status
