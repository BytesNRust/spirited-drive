#!/bin/sh
# The small far lamps of a tunnel (4..20 px) show a bright texel in one frame and none in the next, and their halos blinked (view_lamps.c): a halo is matched to the last frame's (the nearest pair first,
# each once, within the lamp's width) and fades in and out (lamp_flare.h's lamp_fade_*: in by 34 % a frame, held 10 frames when not found, out by 10 %). City 3 (CY3), by day, from node 1585 up to its
# tunnel mouth (TNFS_LAMP_FLARE_TRACE=1 logs each halo with the frame and its strength): over the frames after the first five, a halo that was shown (strength 50 or more) and came back within
# eight frames at the same place ("gaps") was 41 before the fade, 0 after; the frame to frame change of the number of halos shown was 17, 3 after.
# usage: tests/tunnel_flare_fade.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
command -v python3 > /dev/null 2>&1 || { echo "skipped: no python3"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_fade.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
(cd "$work" && TNFS_START_NODE=1585 TNFS_LAMP_FLARE_TRACE=1 TNFS_AUTOPILOT=1 TNFS_FRAMES=300 TNFS_LIGHT_TIME=day TNFS_WEATHER=clear SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
  timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive CY3 --car 3 --node 20 --screenshot "$work/s.ppm" --verbose > "$work/log.txt" 2>&1)
python3 -I - "$work/log.txt" << 'PY'
import sys, collections
fr = collections.defaultdict(list)
for l in open(sys.argv[1], errors='replace'):
    if 'flare_trace:' in l:
        a = l.split('flare_trace:')[1].split()
        fr[int(a[0])].append((int(a[1]), int(a[2]), int(a[3]) if len(a) > 3 else 100))
if len(fr) < 15:
    print("FAIL: only %d frames with halos traced" % len(fr)); sys.exit(1)
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
print("frames %d, halos shown change %d, gaps %d" % (len(vis), change, gaps))
if gaps > 5 or change > 8:
    print("FAIL: the tunnel lamps' halos blink (gaps over 5 or a change of the count over 8)"); sys.exit(1)
print("tunnel_flare_fade: ok")
PY
rc=$?
rm -f "$work"/*.txt "$work"/*.ppm "$work/user/port.cfg"; rmdir "$work/user" "$work" 2>/dev/null
exit $rc
