#!/bin/sh
# The tunnel's colour comes in over the gradient's outside step as well (CY3's first tunnel, nodes 1596..1843, by day, the car's own view with no car): the camera node by node from the open road to the tunnel, the road's
# left strip (rows 80..95 %, columns 15..35 %) goes from the open road's grey to the tunnel's amber with no step over 40 % of the whole rise of its chroma up to the first node in, (R - B) / (R + G + B), and never back (a jump of hue at the first tunnel node
# was a pale shoulder untinted by the half-open piece), and the first node in already carries a quarter of it (not a pale slab before the amber).
# usage: tests/tunnel_gradient_tint_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_tint.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
grep -q '^tunnel 1596 1843' data/track_light/CY3.txt || { echo "FAIL: CY3's tunnel is not 1596 1843 in data/track_light"; exit 1; }
for n in 1593 1594 1595 1596 1597 1598 1599 1601; do
  TNFS_LIGHT_TIME=1 TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=1 TNFS_NO_HUD=1 TNFS_START_NODE=$n SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive CY3 --car 3 --type 0 --node 3 --screenshot "$work/g$n.ppm" > /dev/null 2>&1
done
python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def strip(p, x0, x1):
    d = open(p, "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split())
    pix = d[3]
    t = [0, 0, 0]; n = 0
    for y in range(int(H * .80), int(H * .95)):
        for x in range(int(W * x0), int(W * x1)):
            i = (y * W + x) * 3
            for k in range(3): t[k] += pix[i + k]
            n += 1
    return [v / n for v in t]
try:
    nodes = (1593, 1594, 1595, 1596, 1597, 1598, 1599, 1601)
    left = {n: strip("%s/g%d.ppm" % (w, n), .15, .35) for n in nodes}
    right = {n: strip("%s/g%d.ppm" % (w, n), .65, .85) for n in nodes}
except Exception as e:
    print("FAIL: no screenshots", e); sys.exit(1)
def chroma(c): return (c[0] - c[2]) / max(1.0, c[0] + c[1] + c[2])
def luma(c): return 0.30 * c[0] + 0.59 * c[1] + 0.11 * c[2]
ramp_nodes = nodes[1:7]   # 1594..1599: the open road, the outside steps, the first nodes in, the tunnel's own colour
seq = [chroma(left[n]) for n in ramp_nodes]
print("left chroma 1594..1599:", " ".join("%.3f" % v for v in seq))
ok = True
rise = seq[-1] - seq[0]
if rise < 0.10:
    print("FAIL: the tunnel's amber is not in the picture (rise of chroma %.3f)" % rise); ok = False
else:
    for a, b, n in zip(seq, seq[1:], ramp_nodes):
        if b < a - 0.012: print("FAIL: the chroma falls toward the tunnel at camera %d (%.3f then %.3f)" % (n, a, b)); ok = False
        if b - a > 0.4 * rise: print("FAIL: a step of chroma %.3f after camera %d is %.0f %% of the whole rise %.3f (over 40 %%)" % (b - a, n, 100 * (b - a) / rise, rise)); ok = False
if rise >= 0.10 and seq[2] < 0.25 * rise:
    print("FAIL: at the first node (camera 1596) the tunnel's colour is not in (chroma %.3f, under 25 %% of the whole rise %.3f): a pale slab" % (seq[2], rise)); ok = False
sys.exit(0 if ok else 1)
PY
