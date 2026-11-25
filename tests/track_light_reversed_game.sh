#!/bin/sh
# A track light file is in the track's own direction; a circuit driven the other way round runs its nodes mirrored (node j is the file's (nodes - j) mod nodes): TR3's tunnels (the file's 76..107, 328..351,
# 525..540 of 1024 nodes) are marked on the nodes they stand on in both directions: forward as the file says, reversed at 1024 - 351 .. 1024 - 328 = 673..696 and not at 328..351.
# usage: tests/track_light_reversed_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_light_reversed.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
grep -q '^tunnel 328 351$' data/track_light/TR3.txt || { echo "FAIL: TR3's tunnel is not 328 351 in data/track_light"; exit 1; }
dump() {   # reverse 0|1, out
  TNFS_LIGHT_ROWS_DUMP="$2" TNFS_REVERSE=$1 TNFS_LIGHT_TIME=day TNFS_WEATHER=clear TNFS_AUTOPILOT=1 TNFS_VIEW=1 TNFS_NO_HUD=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive TR3 --car 3 --racers 1 --node 3 --screenshot "$work/x$1.ppm" > /dev/null 2>&1
}
dump 0 "$work/fwd.txt"
dump 1 "$work/rev.txt"
res=$(python3 -I - "$work" << 'PY'
import sys
w = sys.argv[1]
def marks(p):
    out = set()
    for line in open(p):
        t = line.split()
        if len(t) >= 13 and t[0] == "node" and t[-1] == "1":
            out.add(int(t[1]))
    return out
try:
    f, r = marks(w + "/fwd.txt"), marks(w + "/rev.txt")
    n = 1024
    want_f = set(range(328, 352))
    want_r = set(range(n - 351, n - 327))
    ok = want_f <= f and not (set(range(300, 320)) & f) and want_r <= r and not (set(range(328, 352)) & r) and not (set(range(n - 351 - 20, n - 351 - 2)) & r)
    print("ok" if ok else "bad", len(f), len(r), min(r) if r else -1)
except Exception as e:
    print("ERR %s" % e)
PY
)
echo "$res"
case "$res" in ok*) echo ok ;; *) echo "FAIL: the file's tunnel marks are not on the tunnels' nodes in both directions"; exit 1 ;; esac
