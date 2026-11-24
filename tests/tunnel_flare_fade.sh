#!/bin/sh
# The small far lamps of a tunnel (4..20 px) show a bright texel in one frame and none in the next, and their halos blinked (view_lamps.c): a halo is matched to the last frame's (the nearest pair first,
# each once, within the lamp's width) and fades in and out (lamp_flare.h's lamp_fade_*: in by 34 % a frame, held 10 frames when not found, out by 10 %). City 3 (CY3), by day, from node 1585 up to its
# tunnel mouth (TNFS_LAMP_FLARE_TRACE=1 logs each halo with the frame and its strength): over the frames after the first five, a halo that was shown (strength 50 or more) and came back within
# eight frames at the same place ("gaps") was 41 before the fade, 0 after; the frame to frame change of the number of halos shown was 17, 3 after.
# From node 1700 the lamps near the screen's edge move several pixels a frame: a halo is matched to where it was or where it was going (its last move), and no lamp is matched anew.
# usage: tests/tunnel_flare_fade.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
command -v python3 > /dev/null 2>&1 || { echo "skipped: no python3"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_fade.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
for start in 1585 1700; do
  (cd "$work" && TNFS_START_NODE=$start TNFS_LAMP_FLARE_TRACE=1 TNFS_AUTOPILOT=1 TNFS_FRAMES=300 TNFS_LIGHT_TIME=day TNFS_WEATHER=clear SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive CY3 --car 3 --node 20 --screenshot "$work/s.ppm" --verbose > "$work/log$start.txt" 2>&1)
done
python3 -I - "$work/log1585.txt" "$work/log1700.txt" << 'PY'
import sys, collections
def load(path):
    fr, ids = collections.defaultdict(list), collections.defaultdict(dict)
    pos = {}
    for l in open(path, errors='replace'):
        if 'flare_trace:' in l:
            a = l.split('flare_trace:')[1].split()
            f, x, y, lv = int(a[0]), int(a[1]), int(a[2]), int(a[3]) if len(a) > 3 else 100
            fr[f].append((x, y, lv))
            if len(a) > 4: ids[int(a[4])][f] = lv; pos[(int(a[4]), f)] = (x, y)
    return fr, ids, pos
bad = 0
for path, start in ((sys.argv[1], 1585), (sys.argv[2], 1700)):
    fr, ids, pos = load(path)
    if len(fr) < 15:
        print("FAIL: only %d frames with halos traced from %d" % (len(fr), start)); sys.exit(1)
    fs = range(min(fr) + 5, max(fr) + 1)
    vis = [sum(1 for h in fr[f] if h[2] >= 50) for f in fs]
    change = sum(abs(vis[i] - vis[i - 1]) for i in range(1, len(vis)))
    active, gaps = [], 0
    for f in fs:
        nxt = []
        for (x, y, l) in fr[f]:
            if l < 50: continue
            m = next((a for a in active if abs(a[0] - x) <= 10 and abs(a[1] - y) <= 10 and 0 < f - a[2] <= 8), None)
            if m:
                gaps += f - m[2] > 1
                active.remove(m)
            nxt.append((x, y, f))
        active = [a for a in active if f - a[2] <= 8] + nxt
    if not ids:
        print("FAIL: the trace has no halo numbers (the halos are not matched from frame to frame)"); sys.exit(1)
    reversals = 0   # a halo's strength fell and rose again
    for d in ids.values():
        series = [d[f] for f in sorted(d)]
        down = False
        for k in range(1, len(series)):
            if series[k] < series[k - 1]: down = True
            elif series[k] > series[k - 1] and down: reversals, down = reversals + 1, False
    ends = [(max(d), pos[(i, max(d))], i) for i, d in ids.items()]
    rebirths = 0   # a new halo where another was lost a few frames before, after the first twelve frames: one lamp matched twice
    for i, d in ids.items():
        f0 = min(d)
        if f0 <= min(fr) + 11: continue
        x, y = pos[(i, f0)]
        rebirths += any(j != i and 0 < f0 - fe <= 8 and abs(ex - x) <= 14 and abs(ey - y) <= 14 for (fe, (ex, ey), j) in ends)
    print("from %d: frames %d, halos shown change %d, gaps %d, reversals %d, rebirths %d" % (start, len(vis), change, gaps, reversals, rebirths))
    # the count of halos shown and the gaps are read from node 1585 (lamps coming up the tunnel's mouth); from 1700 halos keep coming in (a ramp is not a blink): there the strengths and the matching
    if (start == 1585 and (gaps > 5 or change > 10)) or reversals > 0 or rebirths > 1:
        print("FAIL: the tunnel lamps' halos blink (gaps over 5, a change of the count over 10, a strength that falls and rises, or more than one lamp matched anew)"); bad = 1
if bad: sys.exit(1)
print("tunnel_flare_fade: ok")
PY
rc=$?
rm -f "$work"/*.txt "$work"/*.ppm "$work/user/port.cfg"; rmdir "$work/user" "$work" 2>/dev/null
exit $rc
