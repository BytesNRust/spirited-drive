#!/bin/sh
# A tunnel's portal on both mouths (game/race/portal_faces.h): Transtropolis 3 run backwards from node 906, the camera before the circuit's reversed tunnel 916-947; its portal (the
# model the CD puts at the other end) is there with TNFS_PORTAL_BOTH=1 and not by default (owner 2026-11-25: off everywhere until the faces are placed by hand), not even in a reversed race, nor with TNFS_PORTAL_BOTH=0; and a forward race (not reversed, no 360
# camera) is the same picture with the setting at its default as with it off.
# usage: tests/portal_faces_game.sh <tnfs binary> <original/ dir>    Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_portal.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
status=0
# shot REVERSE BOTH OUT (BOTH empty: the default)
shot() {
  if [ -n "$2" ]; then both="TNFS_PORTAL_BOTH=$2"; else both="TNFS_UNUSED=1"; fi
  env "$both" TNFS_REVERSE="$1" TNFS_AUTOPILOT=1 TNFS_FRAMES=30 TNFS_WEATHER=clear TNFS_LIGHT_TIME=1 TNFS_NO_HUD=1 TNFS_VIEW=1 TNFS_START_NODE="${START:-906}" SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout -k 5 200 "$bin" --data "$original" --user "$work/user" --drive TR3 --car 3 --type 0 --node 3 --screenshot "$3" > "$work/log" 2>&1 || { echo "FAIL: the game did not run ($1 $2): $(tail -3 "$work/log")"; status=1; }
}
# differ A B: how many pixels two P6 files of one size differ in
differ() {
  python3 -I - "$1" "$2" <<'P'
import sys
a, b = open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read()
if len(a) != len(b): print(-1); sys.exit(0)
n = len(a) // 3
a, b = a[len(a) - n * 3:], b[len(b) - n * 3:]
print(sum(1 for i in range(0, n * 3, 3) if a[i:i + 3] != b[i:i + 3]))
P
}
shot 1 0 "$work/rev_off.ppm"
shot 1 1 "$work/rev_on.ppm"
shot 1 "" "$work/rev_default.ppm"
START=60 shot 0 0 "$work/fwd_off.ppm"
START=60 shot 0 "" "$work/fwd_default.ppm"
on=$(differ "$work/rev_off.ppm" "$work/rev_on.ppm")
dflt=$(differ "$work/rev_off.ppm" "$work/rev_default.ppm")
fwd=$(differ "$work/fwd_off.ppm" "$work/fwd_default.ppm")
echo "reversed: portal on differs from off in $on px, default from off in $dflt px; forward default from off in $fwd px"
[ "$on" -ge 3000 ] || { echo "FAIL: the reversed tunnel's far portal is not drawn ($on px)"; status=1; }
[ "$dflt" = 0 ] || { echo "FAIL: a reversed race draws the second face by default ($dflt px)"; status=1; }
[ "$fwd" = 0 ] || { echo "FAIL: a forward race changed with the setting at its default ($fwd px)"; status=1; }
exit "$status"
