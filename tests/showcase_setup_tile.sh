#!/bin/sh
# The SETUP tile of the car showcase is the PLAYER's car's (owner 2026-11-23): on the page of the player's own car it is there and a click on it opens the Car Setup
# panel; on the page of a rival (the opponent card's click in Control Central) it is not drawn and a click in its place opens nothing, the Color place as the original's.
# Checked in Stock and Restomod, against the real binary and the CD data, by the walk's own SHOTs (the frame on screen once it is still) and marks.
# usage: showcase_setup_tile.sh TNFS REPO ORIGINAL_DIR
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
mkdir -p "$work/shots"

step=3000
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
shot() { printf '1500:SHOT:%s' "$1"; }   # the frame on screen once it is still, as $work/shots/$1.ppm
to_central="1500:ESC,$(wait_for movie=),300:ESC,$(wait_for screen=title),300:ESC,$(wait_for screen=game_mode)"
to_central="$to_central,$(click 310 215),$(wait_for screen=name_box),300:TEXT:R,300:ENTER,$(wait_for screen=central)"
player_page="$(click 180 200),$(wait_for screen=showcase)"   # the player's card: its showcase
rival_page="$(click 480 170),$(wait_for screen=showcase)"    # the opponent card: the rival's showcase
over() { printf '300:MOUSE:135:130'; }                           # the pointer on the SETUP tile (VARIANT_LEFT..RIGHT 57..211, TOP..BOTTOM 100..163): the pointer is in both shots
tile() { click 135 130; }

# walk PROFILE SCRIPT: the game run with the input script, which ends on a WAIT for a mark that never comes: 0 when every step before it was taken
walk() {
    [ -d "$work/user" ] && find "$work/user" -delete
    mkdir -p "$work/user" && printf 'profile=%s\n' "$1" > "$work/user/port.cfg"
    script="$to_central,$2,0:WAIT:mark:walk_end:1"
    last=$(printf '%s' "$script" | tr ',' '\n' | grep -c .)
    (cd "$work" && env TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=500 TNFS_SHOT_DIR="$work/shots" TNFS_INPUT_SCRIPT="$script" \
        timeout -k 5 600 "$tnfs" --data "$orig" --user "$work/user" >"$work/out" 2>&1)
    rc=$?
    if [ $rc -eq 3 ] && grep -q "input script: step $last: WAIT mark:walk_end timed out" "$work/user/spirited-drive.log" &&
        ! grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log"; then return 0; fi
    echo "walk $1: status $rc; $(grep "^input script: step.*\(timed out\|not written\)" "$work/user/spirited-drive.log" | head -1)" >&2
    return 1
}
# differ A B: 0 when the two shots (PPM) differ inside the SETUP tile's box (ends included)
differ() {
    python3 -I - "$work/shots/$1.ppm" "$work/shots/$2.ppm" <<'PY'
import sys
def pixels(path):
    data = open(path, "rb").read()
    fields, at = [], 0
    while len(fields) < 4:   # P6, width, height, maximum, each after blanks
        while data[at:at + 1].isspace(): at += 1
        start = at
        while not data[at:at + 1].isspace(): at += 1
        fields.append(data[start:at])
    return int(fields[1]), data[at + 1:]
x0, x1, y0, y1 = 57, 211, 100, 163
(wa, pa), (wb, pb) = pixels(sys.argv[1]), pixels(sys.argv[2])
rows = lambda w, p: [p[(y * w + x0) * 3:(y * w + x1 + 1) * 3] for y in range(y0, y1 + 1)]
sys.exit(0 if rows(wa, pa) != rows(wb, pb) else 1)
PY
}

for profile in restomod stock; do
    # the player's page: the tile is lit by the pointer over it and a click opens the panel (the box changes)
    walk $profile "$player_page,$(over),$(shot pl_a),$(tile),$(shot pl_b)" || { fail "$profile: the walk to the player's showcase"; continue; }
    differ pl_a pl_b || fail "$profile: a click on the SETUP tile of the player's page opened nothing"
    # the rival's page: no tile, so the click in its place changes nothing
    walk $profile "$rival_page,$(over),$(shot rv_a),$(tile),$(shot rv_b)" || { fail "$profile: the walk to the rival's showcase"; continue; }
    differ rv_a rv_b && fail "$profile: a click where SETUP was opened a panel on a rival's page"
    # and its place is not the tile's: the player's page (another car) shows the tile there, the rival's does not
    differ pl_a rv_a || fail "$profile: the SETUP tile's place is the same on the player's page and on a rival's"
done

[ "$failures" = 0 ] || exit 1
echo "ok"
