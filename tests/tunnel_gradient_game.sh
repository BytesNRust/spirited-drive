#!/bin/sh
# A tunnel mouth's light falls off over a few nodes instead of cutting (`tunnel A B gradient N`, the default 3): Alpine's first tunnel (nodes 1596..1607, the dawn's look), the chase far view with the car
# standing at node 1590 (outside, the mouth far ahead) and at 1599 (three nodes in): with the file's own gradient and with `gradient 0` (the straight cut) the picture at 1590 differs only over the mouth's own
# steps, now on the open road before it as well (the road darkens node by node toward the tunnel: between 5000 and 150000 pixels of a 1.2 million frame, not the whole frame) and the car's light, and at 1599 it differs over a large part of the frame, the road near the camera being lighter with the gradient (the first nodes
# inside are not yet the tunnel's full dark). usage: tests/tunnel_gradient_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_gradient.XXXXXX")
mkdir -p "$work/user" "$work/cut/track_light"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
grep -q '^tunnel 1596 1607$' data/track_light/AL1.txt || { echo "FAIL: AL1's first tunnel is not 1596 1607 in data/track_light"; exit 1; }
sed 's/^tunnel 1596 1607$/tunnel 1596 1607 gradient 0/' data/track_light/AL1.txt > "$work/cut/track_light/AL1.txt"
shot() {   # node, out, [mods dir]
  if [ -n "${3:-}" ]; then m="--mods $3"; else m=""; fi
  TNFS_LIGHT_TIME=dawn TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=4 TNFS_NO_HUD=1 TNFS_START_NODE=$1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" $m --drive AL1 --car 3 --racers 1 --node 25 --screenshot "$2" > /dev/null 2>&1
}
for n in 1590 1599; do
  shot $n "$work/g$n.ppm"
  shot $n "$work/c$n.ppm" "$work/cut"
done
res=$(python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def load(p):
    d = open(p, "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split())
    return W, H, d[3]
try:
    out = []
    for n in (1590, 1599):
        W, H, g = load("%s/g%d.ppm" % (w, n))
        _, _, c = load("%s/c%d.ppm" % (w, n))
        diff = sum(1 for i in range(0, W * H * 3, 3) if g[i:i + 3] != c[i:i + 3])
        lo = range(int(H * .55) * W * 3, H * W * 3, 3)
        mg = sum(sum(g[i:i + 3]) for i in lo) / len(lo)
        mc = sum(sum(c[i:i + 3]) for i in lo) / len(lo)
        out += [diff, "%.2f" % (mg / 3), "%.2f" % (mc / 3)]
    print("ok", *out)
except Exception:
    print("none")
PY
)
set -- $res
status=0
[ "$1" = ok ] || { echo "FAIL: no screenshots"; status=1; }
if [ $status -eq 0 ]; then
  echo "outside: $2 px differ; inside (3 nodes in): $5 px differ, lower third mean with the gradient $6, cut $7"
  [ "$2" -le 150000 ] || { echo "FAIL: the gradient changed too much of the picture outside the mouth ($2 pixels)"; status=1; }   # the mouth seen from outside shows its steps, and the open road before it darkens toward the tunnel: tens of thousands of pixels; the whole frame is 1.2 million
  [ "$2" -ge 5000 ] || { echo "FAIL: the gradient changed too little of the picture outside the mouth ($2 pixels): the open road does not darken before it"; status=1; }
  [ "$5" -ge 20000 ] || { echo "FAIL: the gradient changed too little of the picture inside the mouth ($5 pixels)"; status=1; }
  python3 -I -c "import sys; sys.exit(0 if float(sys.argv[1]) > float(sys.argv[2]) else 1)" "$6" "$7" || { echo "FAIL: the road inside the mouth is not lighter with the gradient than with the cut"; status=1; }
fi
[ $status -eq 0 ] && echo "tunnel_gradient_game: ok" && rm -rf "$work"
exit $status
