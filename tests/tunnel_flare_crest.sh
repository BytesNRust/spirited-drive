#!/bin/sh
# Vertigo Ridge (TR3), the climb at the start by day: the tunnel's lamps hang in the tunnel behind the crest, and their projected places run down the road in front of the car. A lamp's halo is drawn
# only where one of its own bright texels shows (view_lamps_draw): the road's asphalt and paint share the lamp's palette entries, so a halo used to be drawn on the road under the crest (two or three yellow
# flashes on the line) and the picture is now the same with and without lamp_flares there, while the lamps in the tunnel's mouth, which do show, keep theirs.
# usage: tests/tunnel_flare_crest.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
command -v python3 > /dev/null 2>&1 || { echo "skipped: no python3"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_crest.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
shot() {   # FLARES OUT
  (cd "$work" && TNFS_LAMP_FLARES="$1" TNFS_AUTOPILOT=1 TNFS_FRAMES=1 TNFS_LIGHT_TIME=day SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive TR3 --node 400 --screenshot "$2" > /dev/null 2>&1)
}
shot 1 "$work/on.ppm"; shot 0 "$work/off.ppm"
python3 - "$work/on.ppm" "$work/off.ppm" << 'PY'
import sys
def load(p):
    d = open(p, 'rb').read()
    parts = d.split(None, 4)   # P6 w h max, then the pixels after one whitespace
    w, h = int(parts[1]), int(parts[2])
    return w, h, parts[4] if len(parts) > 4 else b''
w, h, on = load(sys.argv[1]); w2, h2, off = load(sys.argv[2])
if (w, h) != (w2, h2) or not on:
    print("FAIL: no pictures"); sys.exit(1)
def region(px, x0, y0, x1, y1):
    return [px[(y * w + x) * 3:(y * w + x) * 3 + 3] for y in range(y0, y1) for x in range(x0, x1)]
sx, sy = w / 1280.0, h / 960.0   # the frame is 1280 x 960 at the render scale the screenshot has
r = lambda a, b, c, d: (int(a * sx), int(b * sy), int(c * sx), int(d * sy))
road = r(585, 322, 625, 366)      # the road under the crest: no halo may be there
mouth = r(570, 296, 672, 318)     # the tunnel's mouth: its lamps show and keep their halos
fails = 0
if region(on, *road) != region(off, *road):
    print("FAIL: a tunnel lamp's halo is drawn on the road under the crest"); fails = 1
if region(on, *mouth) == region(off, *mouth):
    print("FAIL: the lamps showing in the tunnel's mouth lost their halos"); fails = 1
if not fails:
    print("tunnel_flare_crest: ok")
sys.exit(fails)
PY
status=$?
find "$work" -delete
exit $status
