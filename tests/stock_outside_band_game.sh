#!/bin/sh
# Stock keeps the original's black strip under the outside (chase) view (0041e3f4 clips the 3D view above it); Restored and Restomod draw the road down to the bottom of the screen: Alpine's chase view at a start
# node, the frame's lines 418..421 (the middle columns 200..400, clear of the tach and the map: the black line under the view's bottom edge, the strip's top) read black in Stock and are the road in Restomod and Restored.
# usage: tests/stock_outside_band_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_band.XXXXXX")
shot() {   # profile, out
  mkdir -p "$work/user_$1"
  printf 'profile=%s\npsx_dir=\n' "$1" > "$work/user_$1/port.cfg"
  TNFS_LIGHT_TIME=1 TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=3 TNFS_START_NODE=300 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref_$1" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user_$1" --drive AL1 --car 3 --type 0 --node 3 --screenshot "$2" > /dev/null 2>&1
}
for p in stock restored restomod; do shot $p "$work/$p.ppm"; done
python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def band(p):
    d = open(p, "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split())
    pix = d[3]
    t = n = 0
    for y in range(int(H * 418 / 480), int(H * 422 / 480)):
        for x in range(int(W * 200 / 640), int(W * 400 / 640)):
            i = (y * W + x) * 3
            t += (pix[i] + pix[i + 1] + pix[i + 2]) / 3
            n += 1
    return t / n
try:
    v = {p: band("%s/%s.ppm" % (w, p)) for p in ("stock", "restored", "restomod")}
except Exception as e:
    print("FAIL: no screenshots", e); sys.exit(1)
print("strip's top lines, mean: " + ", ".join("%s %.1f" % kv for kv in v.items()))
ok = True
if v["stock"] > 3:
    print("FAIL: Stock's lines under the view are not black (%.1f): the strip is gone" % v["stock"]); ok = False
for p in ("restored", "restomod"):
    if v[p] < 12:
        print("FAIL: %s has the strip's black lines (%.1f): the road reaches the bottom there" % (p, v[p])); ok = False
sys.exit(0 if ok else 1)
PY
