#!/bin/sh
# The sky's sun disc (Restomod, clear weather, by day): AL1 node 300 in the chase view, the track's `sun AZ 25 -` line moved to two azimuths: a soft bright disc (cream, near white at its core) is in the sky and its
# place follows the azimuth (sun 90 is left of sun 60 on the screen, both above the horizon); with the sun behind the camera (180, 270) no such pixel; the same sun at 60 with fog, rain, storm, by night, or in Stock and Restored gives none.
# usage: tests/sky_sun_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_sunsky.XXXXXX")
for p in restomod restored stock; do
  mkdir -p "$work/user_$p"
  printf 'profile=%s\npsx_dir=\n' "$p" > "$work/user_$p/port.cfg"
done
for a in 60 90 180 270; do
  mkdir -p "$work/mods_$a/track_light"
  cp data/track_light/AL1.txt "$work/base.txt"
  { grep -v '^ *sun ' "$work/base.txt" | awk -v a="$a" '{print} /^time B/ && !d {print "  sun " a " 25 -"; d=1}'; } > "$work/mods_$a/track_light/AL1.txt"
  grep -q "^  sun $a 25 -" "$work/mods_$a/track_light/AL1.txt" || { echo "FAIL: AL1's file has no 'time B' line to put the sun after"; exit 1; }
done
shot() {   # shot <name> <azimuth> <profile> <weather> <time>
  TNFS_LIGHT_TIME=$5 TNFS_WEATHER=$4 TNFS_AUTOPILOT=1 TNFS_VIEW=3 TNFS_NO_HUD=1 TNFS_START_NODE=300 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user_$3" --mods "$work/mods_$2" --drive AL1 --car 3 --type 0 --node 3 --screenshot "$work/$1.ppm" > /dev/null 2>&1
}
shot a60 60 restomod clear day
shot a90 90 restomod clear day
shot a180 180 restomod clear day
shot a270 270 restomod clear day
shot fog 60 restomod fog day
shot rain 60 restomod rain day
shot storm 60 restomod storm day
shot night 60 restomod clear night
shot restored 60 restored clear day
shot stock 60 stock clear day
python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def blob(name):
    d = open("%s/%s.ppm" % (w, name), "rb").read().split(b"\n", 3)
    W, H = map(int, d[1].split()); pix = d[3]
    xs = []; ys = []
    for y in range(H // 2):   # the sky: the car's own lights are lower
        for x in range(W):
            i = (y * W + x) * 3
            if pix[i] >= 240 and pix[i + 1] >= 225 and pix[i + 2] >= 170: xs.append(x); ys.append(y)
    return W, H, len(xs), (sum(xs) / len(xs) if xs else 0), (sum(ys) / len(ys) if ys else 0)
ok = True
try:
    r = {n: blob(n) for n in ("a60", "a90", "a180", "a270", "fog", "rain", "storm", "night", "restored", "stock")}
except Exception as e:
    print("FAIL: no screenshots", e); sys.exit(1)
for n, v in r.items(): print("%-8s px %5d  centre %.0f, %.0f" % (n, v[2], v[3], v[4]))
W, H = r["a60"][0], r["a60"][1]
for n in ("a60", "a90"):
    if r[n][2] < 200: print("FAIL: no sun disc in %s (%d bright pixels)" % (n, r[n][2])); ok = False
    elif not r[n][4] < 0.5 * H: print("FAIL: the disc of %s is not in the sky (centre row %.0f of %d)" % (n, r[n][4], H)); ok = False
if ok and not r["a90"][3] < r["a60"][3] - 0.05 * W:
    print("FAIL: the disc does not follow the azimuth: sun 90 at x %.0f, sun 60 at x %.0f" % (r["a90"][3], r["a60"][3])); ok = False
for n in ("a180", "a270", "fog", "rain", "storm", "night", "restored", "stock"):
    if r[n][2] > 20: print("FAIL: %s shows a sun disc (%d bright pixels)" % (n, r[n][2])); ok = False
sys.exit(0 if ok else 1)
PY
rc=$?
rm -rf "$work"
exit $rc
