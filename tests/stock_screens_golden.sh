#!/bin/sh
# Stock and Restored show the original's screens (owner 2026-11-23): nothing of the port may appear or change there. One game session per profile walks the front end and takes a picture
# (SHOT) of each screen: Game Mode, Control Central, the showcase of the player's car and of a rival's, the track screen; each is compared pixel by pixel with its golden picture in
# tests/golden/screens/<profile>/<screen>.png. A difference fails the test: a port element that came into one of these screens, or a change of the art. When the change is meant (and looked at
# against the original), the goldens are made again with UPDATE=1 and reviewed one by one before they are committed. Needs the CD in ORIGINAL_DIR: skipped without it.
# EXCEPTIONS (owner 2026-11-23): three screens carry the port's own elements in Stock and Restored and are not held to the original: Game Mode (the mode selector, the Manage content row and
# the version text: its goldens are taken as they are, and the footer's box is left out of the comparison), Options and Credits (not walked here). See docs/settings.md, "Stock and Restored keep the original's screens".
# usage: [UPDATE=1] stock_screens_golden.sh TNFS REPO ORIGINAL_DIR
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from the scratch folder
tnfs=$(abs "$1")
repo=$(abs "$2")
orig=$(abs "$3")
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
trap 'find "$work" -delete' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
golden="$repo/tests/golden/screens"

step=3000
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
park() { printf '300:MOUSE:5:475'; }                 # the pointer in the corner: the same in every picture, over nothing
shot() { printf '%s,1500:SHOT:%s' "$(park)" "$1"; }   # the frame on screen once it is still, as $work/shots/$1.ppm
to_game_mode="1500:ESC,$(wait_for movie=),500:ESC,$(wait_for screen=game_mode)"
# the screens, in one session: the Game Mode screen is shot on the way to Control Central
script="$to_game_mode,$(shot game_mode),$(click 310 215),$(wait_for screen=name_box),300:TEXT:R,300:ENTER,$(wait_for screen=central),$(shot central)"
script="$script,$(click 180 200),$(wait_for screen=showcase),$(shot showcase_player),1000:ESC,$(wait_for car=)"
script="$script,$(click 480 170),$(wait_for screen=showcase),$(shot showcase_rival),1000:ESC,$(wait_for car=)"
script="$script,$(click 400 320),$(wait_for screen=track_select),$(shot track_select)"
screens="game_mode central showcase_player showcase_rival track_select"

# compare SCREEN A B: 0 when the two pictures (PNG or PPM) have the same size and pixels. Game Mode's footer (x 0..199, y 448..475: the version line and the update notice) is left out:
# TNFS_VERSION_TEXT only works in a development build, so a TNFS_RELEASE build (CI's, the gate's) draws the commit's hash there, and the update notice depends on the network
same() {
    python3 -I - "$repo/tools" "$1" "$2" "$3" <<'PY'
import sys
sys.path.insert(0, sys.argv[1])
from rgb_image import Image
a, b = Image.open(sys.argv[3]), Image.open(sys.argv[4])
if a.size != b.size: sys.exit(1)
w, h = a.size
def row(im, y):
    r = bytes(im._rows[y])
    if sys.argv[2] == "game_mode" and 448 <= y <= 475: r = bytes(600) + r[600:]
    return r
sys.exit(0 if all(row(a, y) == row(b, y) for y in range(h)) else 1)
PY
}

for profile in ${PROFILES:-stock restored}; do
    rm -rf "$work/user" "$work/shots"
    mkdir -p "$work/user" "$work/shots" && printf 'profile=%s\n' "$profile" > "$work/user/port.cfg"
    full="$script,0:WAIT:mark:walk_end:1"
    last=$(printf '%s' "$full" | tr ',' '\n' | grep -c .)
    (cd "$work" && env TNFS_VERBOSE=1 TNFS_VERSION_TEXT=walk TNFS_INPUT_SETTLE=500 TNFS_SHOT_DIR="$work/shots" TNFS_INPUT_SCRIPT="$full" \
        timeout -k 5 ${WALK_TIMEOUT:-300} "$tnfs" --data "$orig" --user "$work/user" >"$work/out" 2>&1)
    rc=$?
    if ! { [ $rc -eq 3 ] && grep -q "input script: step $last: WAIT mark:walk_end timed out" "$work/user/spirited-drive.log" &&
           ! grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log"; }; then
        echo "walk $profile: status $rc; $(grep "^input script: step.*\(timed out\|not written\)" "$work/user/spirited-drive.log" | head -1)" >&2
        tail -12 "$work/user/spirited-drive.log" >&2
        fail "$profile: the walk through the front end did not finish"
        continue
    fi
    for s in $screens; do
        [ -f "$work/shots/$s.ppm" ] || { fail "$profile: no picture of $s"; continue; }
        if [ "${UPDATE:-}" = 1 ]; then
            mkdir -p "$golden/$profile" && python3 -I "$repo/tools/ppm2png.py" "$work/shots/$s.ppm" "$golden/$profile/$s.png" && echo "golden: $profile/$s"
        elif [ ! -f "$golden/$profile/$s.png" ]; then
            fail "$profile: no golden picture for $s (UPDATE=1 makes them)"
        elif ! same "$s" "$work/shots/$s.ppm" "$golden/$profile/$s.png"; then
            fail "$profile: $s differs from its golden picture: something of the port shows in the original's screen, or the art changed"
            [ -n "${KEEP:-}" ] && mkdir -p "$KEEP" && python3 -I "$repo/tools/ppm2png.py" "$work/shots/$s.ppm" "$KEEP/${profile}_$s.png"
        fi
    done
done

[ "$failures" = 0 ] || exit 1
echo "ok"
