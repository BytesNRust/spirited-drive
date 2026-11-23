#!/bin/sh
# Vertigo Ridge (TR3), the drive up to the tunnel by day: the halos of the tunnel's lamps must not blink. A lamp's halo is drawn while one of its bright texels shows; that test changes from frame to frame
# with the distance and the angle, so view_lamps_draw starts a halo at LAMP_SHOWS_BRIGHT_PERCENT, keeps it down to LAMP_KEEP_BRIGHT_PERCENT and lets it linger LAMP_HOLD_FRAMES frames. TNFS_LAMP_FLARE_TRACE=1
# logs one line per drawn halo; the sum of the frame-to-frame changes of the halo count was 102 over the 115 frames before the hysteresis, 40 after.
# usage: tests/tunnel_flare_flicker.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
command -v python3 > /dev/null 2>&1 || { echo "skipped: no python3"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_flicker.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
(cd "$work" && TNFS_LAMP_FLARE_TRACE=1 TNFS_AUTOPILOT=1 TNFS_FRAMES=200 TNFS_LIGHT_TIME=day SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
  timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive TR3 --node 480 --screenshot "$work/s.ppm" --verbose > "$work/log.txt" 2>&1)
python3 - "$work/log.txt" << 'PY'
import sys, collections
fr = collections.Counter()
for l in open(sys.argv[1], errors='replace'):
    if 'flare_trace:' in l:
        fr[int(l.split('flare_trace:')[1].split()[0])] += 1
if len(fr) < 50:
    print("FAIL: only %d frames with halos traced" % len(fr)); sys.exit(1)
cs = [fr[n] for n in range(min(fr), max(fr) + 1)]
d = [abs(cs[i] - cs[i - 1]) for i in range(1, len(cs))]
print("frames %d, total change %d, max step %d" % (len(cs), sum(d), max(d)))
if sum(d) > 60 or max(d) > 4:
    print("FAIL: the tunnel lamps' halos blink (total change over 60 or a step over 4)"); sys.exit(1)
print("tunnel_flare_flicker: ok")
PY
rc=$?
rm -f "$work"/*.txt "$work"/*.ppm "$work/user/port.cfg"; rmdir "$work/user" "$work" 2>/dev/null
exit $rc
