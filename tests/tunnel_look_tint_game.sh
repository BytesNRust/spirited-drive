#!/bin/sh
# An unlit tunnel (lamps 0) keeps its tunnel_look colour at the four times of day, with the beam pool: AL1 node 1600 (inside the tunnel 1596-1607), the zone look at 40 % in a white and in the warm
# ffcc26, a photo each; the warm look's picture is redder than the white one at dawn, day, dusk and night, the white one is not warm, and the car's lamps are lit in it (the pool is the same photo).
# PART=early (dawn, day) or late (dusk, night) runs those alone (ctest runs both side by side); unset, the four.
# usage: tests/tunnel_look_tint_game.sh <tnfs binary> <original/ dir> <repo root>    Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
root=$(cd "$3" && pwd)
part=${PART:-all}
case $part in all | early | late) ;; *) echo "FAIL: PART=$part is not early, late or all" >&2; exit 1 ;; esac
case $part in early) times="dawn:A day:B" ;; late) times="dusk:C night:N" ;; *) times="dawn:A day:B dusk:C night:N" ;; esac
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_tint.XXXXXX")
mkdir -p "$work/user" "$work/mods/track_light"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
cp "$work/user/port.cfg" "$work/mods/port.cfg"
status=0
# shot TIME BLOCK COLOUR OUT: the repo's AL1 light with the unlit tunnel's zone look added to the block
shot() {
  python3 -I - "$root/data/track_light/AL1.txt" "$2" "tunnel_look 1596 1607 40 $3 lamps 0" > "$work/mods/track_light/AL1.txt" <<'P'
import sys
src, block, line = sys.argv[1], sys.argv[2], sys.argv[3]
out = []
for l in open(src).read().split('\n'):
    out.append(l)
    if l.strip() == 'time ' + block:
        out.append('  ' + line)
print('\n'.join(out))
P
  env TNFS_LIGHT_EDIT=1 TNFS_AUTOPILOT=1 TNFS_FRAMES=30 TNFS_WEATHER=clear TNFS_LIGHT_TIME="$1" TNFS_START_NODE=1600 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout -k 5 200 "$bin" --data "$original" --user "$work/user" --mods "$work/mods" --drive AL1 --car 0 --node 60 --screenshot "$4" > "$work/log" 2>&1 || { echo "FAIL: the game did not run ($1 $3): $(tail -3 "$work/log")"; status=1; }
}
# redness OUT: the mean R and B of the tunnel's road, as "R B" (a P6 file, the stdlib only)
redness() {
  python3 -I - "$1" <<'P'
import sys
d = open(sys.argv[1], 'rb').read()
parts = d.split(None, 4)
w, h = int(parts[1]), int(parts[2])
pix = parts[4] if len(parts) > 4 else b''
pix = d[len(d) - w * h * 3:]
r = b = n = 0
for y in range(420, 800, 2):
    for x in range(250, 1000, 2):
        o = (y * w + x) * 3
        r += pix[o]; b += pix[o + 2]; n += 1
print('%.2f %.2f' % (r / n, b / n))
P
}
for t in $times; do
  time=${t%:*}; block=${t#*:}
  shot "$time" "$block" ffffff "$work/white.ppm"
  shot "$time" "$block" ffcc26 "$work/warm.ppm"
  w=$(redness "$work/white.ppm"); c=$(redness "$work/warm.ppm")
  python3 -I - "$time" "$w" "$c" <<'P' || status=1
import sys
t, w, c = sys.argv[1], [float(x) for x in sys.argv[2].split()], [float(x) for x in sys.argv[3].split()]
wr, cr = w[0] / max(w[1], 0.1), c[0] / max(c[1], 0.1)
print('%s: R/B white %.2f warm %.2f' % (t, wr, cr))
if not (cr > wr * 1.5 and cr > 2.0):
    print('FAIL: %s: the unlit tunnel lost its look colour (white R/B %.2f, warm R/B %.2f)' % (t, wr, cr)); sys.exit(1)
P
done
[ $status -eq 0 ] && echo "tunnel_look_tint_game: ok"
find "$work" -delete
exit $status
