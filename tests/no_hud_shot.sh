#!/bin/sh
# TNFS_NO_HUD=1 (game/viewer/drive_wide.c, env only, for the Track light window's photo): outside the car the picture is the 3D alone. On a CD track (TR5, day, the chase
# camera behind the car) against the same shot without it: the top bar is black, the tach and the map are gone, and the sky, road and car are the very same pixels; in the dash
# view (the dash is the car) its top band and map go and the dash itself is as it was; the bumper view has no mirror (the radar's LEDs and the mirror are the bumper extras').
# Needs the CD's data (original/): the test is only registered when it is there. usage: no_hud_shot.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT
mkdir -p "$WORK/user" "$WORK/mods"
printf 'profile=restomod\nlight_effects=1\n' > "$WORK/user/port.cfg"
shot() {   # NO_HUD (0/1) VIEW OUT
    env TNFS_AUTOPILOT=1 TNFS_FRAMES=1 TNFS_WEATHER=clear TNFS_VIEW="$2" TNFS_LIGHT_TIME=day TNFS_START_NODE=5 ${1:+TNFS_NO_HUD=$1} \
        SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy \
        "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --drive TR5 --node 200 --screenshot "$3" > "$WORK/run.out" 2>&1 || { cat "$WORK/run.out" >&2; echo "FAIL: the game failed" >&2; exit 1; }
}
shot "" 3 "$WORK/hud3.ppm"
shot 1 3 "$WORK/no3.ppm"
shot "" 0 "$WORK/hud0.ppm"
shot 1 0 "$WORK/no0.ppm"
shot "" 1 "$WORK/hud1.ppm"
shot 1 1 "$WORK/no1.ppm"
python3 -I - "$WORK" <<'PY' || exit 1
import sys
w_ = sys.argv[1]
def rd(name):
    d = open(w_ + "/" + name, "rb").read()
    head = d.split(b"\n", 3)
    w, h = map(int, head[1].split())
    return w, h, head[3]
def box(px, w, h, fx0, fy0, fx1, fy1):   # a box as fractions of the picture
    x0, y0, x1, y1 = int(fx0 * w), int(fy0 * h), int(fx1 * w), int(fy1 * h)
    return [px[(y * w + x) * 3:(y * w + x) * 3 + 3] for y in range(y0, y1) for x in range(x0, x1)]
def equal(a, b): return sum(1 for x, y in zip(a, b) if x == y) / len(a)
def black(a): return sum(1 for x in a if x == b"\0\0\0") / len(a)
w, h, hud = rd("hud3.ppm")
w2, h2, no = rd("no3.ppm")
assert (w, h) == (w2, h2) and w >= 640 and h >= 480, (w, h, w2, h2)
TOP, TACH, MAP = (0, 0, 1, 0.06), (0.015, 0.6, 0.27, 0.92), (0.81, 0.64, 0.99, 0.93)
SKY, ROAD, CAR = (0.31, 0.2, 0.69, 0.4), (0.39, 0.62, 0.61, 0.69), (0.44, 0.62, 0.56, 0.83)
assert black(box(hud, w, h, *TOP)) < 0.05, "the shot with the HUD has no top bar"
assert black(box(no, w, h, *TOP)) > 0.99, "the top bar is still there without the HUD"
assert equal(box(hud, w, h, *TACH), box(no, w, h, *TACH)) < 0.6, "the tach is still there"
assert equal(box(hud, w, h, *MAP), box(no, w, h, *MAP)) < 0.5, "the map is still there"
for name, b in (("sky", SKY), ("road", ROAD), ("car", CAR)):
    assert equal(box(hud, w, h, *b), box(no, w, h, *b)) > 0.99, "the 3D's %s changed with TNFS_NO_HUD" % name
_, _, a = rd("hud0.ppm"); _, _, b = rd("no0.ppm")
DASH_TOP, DASH_MAP, WHEEL, DASH_LEFT = (0, 0, 1, 0.06), (0.81, 0.64, 0.99, 0.93), (0.28, 0.6, 0.55, 0.95), (0, 0.7, 0.2, 0.98)
assert equal(box(a, w, h, *DASH_TOP), box(b, w, h, *DASH_TOP)) < 0.2, "the dash view's top band (the bar, the start lights) is still there"
assert equal(box(a, w, h, *DASH_MAP), box(b, w, h, *DASH_MAP)) < 0.3, "the dash view's map is still there"
for name, bx in (("wheel", WHEEL), ("left of the dash", DASH_LEFT)):   # the dash itself (the car) stays as it is
    assert equal(box(a, w, h, *bx), box(b, w, h, *bx)) > 0.99, "the dash's %s changed with TNFS_NO_HUD" % name
_, _, a = rd("hud1.ppm"); _, _, b = rd("no1.ppm")
MIRROR = (0.67, 0.06, 0.99, 0.2)
assert equal(box(a, w, h, *MIRROR), box(b, w, h, *MIRROR)) < 0.5, "the bumper's mirror is still there without the HUD"
print("no_hud_shot: ok")
PY
