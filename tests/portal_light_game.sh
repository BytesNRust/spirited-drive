#!/bin/sh
# The 3D object at a tunnel's mouth (AL1's portal, node 1595, with its sign) is lit by the OPEN road, not by the mouth node's terrain (shaded by the mountain, darkened with the tunnel): AL1's first tunnel
# at dawn and at day, the chase far view with the car at node 1592, with the portal's light as now and with TNFS_PORTAL_LIGHT=0 (the old way): the portal's box (x 407..855, y 224..425 of the 1280x960
# picture) is lighter (by over 5 %: the open road at the mouth is itself darkened toward the tunnel by the gradient's outer steps), and nothing outside the box changes.
# usage: tests/portal_light_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_portal.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
shot() {   # time, portal light 0|1, out
  TNFS_PORTAL_LIGHT=$2 TNFS_LIGHT_TIME=$1 TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=4 TNFS_NO_HUD=1 TNFS_START_NODE=1592 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive AL1 --car 3 --racers 1 --node 3 --screenshot "$3" > /dev/null 2>&1
}
for t in dawn day; do
  shot $t 0 "$work/old_$t.ppm"
  shot $t 1 "$work/new_$t.ppm"
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
    for t in ("dawn", "day"):
        W, H, o = load("%s/old_%s.ppm" % (w, t))
        _, _, n = load("%s/new_%s.ppm" % (w, t))
        x0, y0, x1, y1 = (407, 224, 855, 425) if W == 1280 else (int(407 * W / 1280), int(224 * H / 960), int(855 * W / 1280), int(425 * H / 960))
        inside = outside = 0
        so = sn = cnt = 0
        for y in range(H):
            for x in range(W):
                i = (y * W + x) * 3
                if o[i:i + 3] == n[i:i + 3]:
                    continue
                if x0 <= x < x1 and y0 <= y < y1:
                    inside += 1
                else:
                    outside += 1
        for y in range(y0, y1):
            for x in range(x0, x1):
                i = (y * W + x) * 3
                so += sum(o[i:i + 3]); sn += sum(n[i:i + 3]); cnt += 1
        out += [inside, outside, "%.2f" % (so / cnt / 3), "%.2f" % (sn / cnt / 3)]
    print("ok", *out)
except Exception:
    print("none")
PY
)
set -- $res
status=0
[ "$1" = ok ] || { echo "FAIL: no screenshots"; status=1; }
if [ $status -eq 0 ]; then
  echo "dawn: $2 px differ in the portal's box, $3 outside, box mean $4 -> $5; day: $6 in, $7 outside, box mean $8 -> $9"
  [ "$2" -ge 5000 ] && [ "$6" -ge 5000 ] || { echo "FAIL: the portal's light did not change (dawn $2, day $6 pixels)"; status=1; }
  [ "$3" -le 200 ] && [ "$7" -le 200 ] || { echo "FAIL: the picture changed outside the portal (dawn $3, day $7 pixels)"; status=1; }
  python3 -I -c "import sys; a=[float(v) for v in sys.argv[1:]]; sys.exit(0 if a[1] > a[0] * 1.05 and a[3] > a[2] * 1.05 else 1)" "$4" "$5" "$8" "$9" || { echo "FAIL: the portal is not lighter lit by the open road"; status=1; }
fi
[ $status -eq 0 ] && echo "portal_light_game: ok" && rm -rf "$work"
exit $status
