#!/bin/sh
# A tunnel mouth's gradient darkens the road node by node, on the open road before the mouth and past the tunnel's other end as well as inside it: Alpine's first tunnel (nodes 1596..1607), by day, the car's own
# view with no car (a time trial), the road's centre in the picture's lower band, from the open road toward the tunnel at both ends (the cameras run the other way along the road, so each end is seen as a
# camera node by node toward it). The brightness falls with no step over 40 % of the whole fall (from the open road to the tunnel's floor, the mean of three nodes inside), at both ends, and never rises (by more than a palette step, 1 of 255) toward the
# tunnel; with `gradient 0` (the straight cut) the step at the mouth is far over 40 %.
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
  TNFS_LIGHT_TIME=1 TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=1 TNFS_NO_HUD=1 TNFS_START_NODE=$1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" $m --drive AL1 --car 3 --type 0 --node 3 --screenshot "$2" > /dev/null 2>&1
}
for n in 1591 1592 1593 1594 1595 1596 1597 1598 1601 1603 1605 1606 1607 1608 1609 1610 1611; do shot $n "$work/g$n.ppm"; done
for n in 1595 1596 1598 1601; do shot $n "$work/c$n.ppm" "$work/cut"; done
res=$(python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def band(p):
    d = open(p, "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split())
    pix = d[3]
    tot = n = 0
    for y in range(int(H * .62), int(H * .95)):
        for x in list(range(int(W * .40), int(W * .46))) + list(range(int(W * .54), int(W * .60))):
            i = (y * W + x) * 3
            tot += (pix[i] + pix[i + 1] + pix[i + 2]) / 3
            n += 1
    return tot / n
try:
    out = []
    for n in (1591, 1592, 1593, 1594, 1595, 1596, 1597, 1598, 1601, 1603, 1605, 1606, 1607, 1608, 1609, 1610, 1611):
        out.append(band("%s/g%d.ppm" % (w, n)))
    for n in (1595, 1596, 1598, 1601):
        out.append(band("%s/c%d.ppm" % (w, n)))
    print("ok", " ".join("%.2f" % v for v in out))
except Exception:
    print("none")
PY
)
set -- $res
status=0
[ "$1" = ok ] || { echo "FAIL: no screenshots"; status=1; }
if [ $status -eq 0 ]; then
  shift
  echo "bands: $*"
  python3 -I - "$@" << 'PY' || status=1
import sys
v = [float(a) for a in sys.argv[1:]]
low = v[0:7]            # cameras 1591..1597: toward the tunnel from the open road, to its first node in
inside = v[7:10]        # 1598, 1601, 1603: the tunnel's floor
high = v[16:9:-1]       # cameras 1611..1605: toward the tunnel past its other end, to its first nodes in
cut = v[17:21]          # gradient 0: 1595, 1596, 1598, 1601
floor = sum(inside) / 3.0
ok = True
for name, seq in (("before the first node", low), ("past the other end", high)):
    top = max(seq)
    fall = top - floor
    if fall < 40:
        print("FAIL: %s: the open road (%.1f) is not much brighter than the tunnel's floor (%.1f)" % (name, top, floor)); ok = False
        continue
    path = list(seq) + [floor]
    for a, b in zip(path, path[1:]):
        if b > a + 1.0:
            print("FAIL: %s: the road gets lighter toward the tunnel (%.1f then %.1f)" % (name, a, b)); ok = False
        if (a - b) > 0.4 * fall:
            print("FAIL: %s: a step of %.1f is %.0f %% of the whole fall %.1f (over 40 %%)" % (name, a - b, 100 * (a - b) / fall, fall)); ok = False
cut_fall = max(cut[0], 1e-6) - floor
if cut_fall < 40 or (cut[0] - cut[1]) < 0.6 * cut_fall:
    print("FAIL: the straight cut (%.1f then %.1f, floor %.1f) is not a step of over 60 %%" % (cut[0], cut[1], floor)); ok = False
sys.exit(0 if ok else 1)
PY
fi
[ $status -eq 0 ] && echo "tunnel_gradient_ramp_game: ok" && rm -rf "$work"
exit $status
