#!/bin/zsh
# tools/menu_walk.sh <bin> <outdir> <target>: walk the front end headless to a menu screen and save its picture as <outdir>/<target>.png.
#
#   targets: options  controls (page 1/3, Keyboard)  controls-p2 (2/3, Game controller)  controls-p3 (3/3, Tune)
#            graphics  extras (page 1/3)  extras-p2  extras-p3
#
# Every step waits for the game's own "mark:screen=NAME" log line (the front end logs one as each screen opens), not for a time; the clicks wait for
# the screen to be still (TNFS_INPUT_SETTLE). A step that does not come in time stops the run and says which ("input script: step N: WAIT ... timed
# out"), exit 3. The run is the visible-run kind: dummy SDL drivers, timeout -k, a scratch --mods and --user, the repo's original/ as the game data.
#   env: DATA (the game data folder, default <repo>/original)  STEP_TIMEOUT (60 s a step)  SETTLE (1500 ms)  KEEP=1 (keep the scratch dir, for a look)
# exit: 0 reached and saved; 3 a step timed out; 2 bad arguments, no binary or no magick; 1 anything else (the log is printed).
bin=$1 out=$2 target=$3
[ -n "$bin" ] && [ -n "$out" ] && [ -n "$target" ] || { echo "usage: $0 <bin> <outdir> <target>" >&2; exit 2; }
[ -x "$bin" ] || { echo "no binary: $bin" >&2; exit 2; }
command -v magick >/dev/null 2>&1 || { echo "magick missing (ImageMagick 7 saves the frame as a PNG; on nix: nix shell --inputs-from . nixpkgs#imagemagick)" >&2; exit 2; }
here=${0:A:h}
data=${DATA:-$here/../original}
[ -d "$data" ] || { echo "no game data folder: $data (set DATA)" >&2; exit 2; }
step=${STEP_TIMEOUT:-60}
settle=${SETTLE:-1500}

wait_for() { printf '0:WAIT:mark:screen=%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
# Controls' pages: a click on the footer's right arrow, not the RIGHT key. With the pointer over a row (it is left where the Options click was), RIGHT
# sometimes moved that row's value (Dead zone 5 % in one walk, 0 % in the next, same binary) and sometimes turned the page, and the page 2 picture came
# out as page 3. park: the pointer moved to the empty middle of the page so the picture shows no row hovered.
next_page() { printf '300:MOUSE:%s:%s,300:CLICK:%s:%s' 355 452 355 452; }
park() { printf '300:MOUSE:%s:%s' 330 405; }

# the common way in: our boot screen skipped (the first thing the game shows), EA's first movie stopped (which ends the intro), Game Mode, Single, the
# name box left, Control Central
to_central="1500:ESC,0:WAIT:mark:movie=:$step,500:ESC,$(wait_for game_mode),$(click 310 217),$(wait_for name_box),500:TEXT:R,500:ENTER,$(wait_for central)"
to_options="$to_central,$(click 215 450),$(wait_for options)"

case $target in
options)      script="$to_options"; final=options ;;
controls)     script="$to_options,$(click 80 285),$(wait_for controls_keyboard)"; final=controls_keyboard ;;
controls-p2)  script="$to_options,$(click 80 285),$(wait_for controls_keyboard),$(next_page),$(wait_for controls_actions),$(park)"; final=controls_actions ;;
controls-p3)  script="$to_options,$(click 80 285),$(wait_for controls_keyboard),$(next_page),$(wait_for controls_actions),$(next_page),$(wait_for controls_tune),$(park)"; final=controls_tune ;;
graphics)     script="$to_options,$(click 80 250),$(wait_for graphics)"; final=graphics ;;
extras)       script="$to_options,$(click 100 392),$(wait_for extras_p1)"; final=extras_p1 ;;
extras-p2)    script="$to_options,$(click 100 392),$(wait_for extras_p1),500:PGDN,$(wait_for extras_p2)"; final=extras_p2 ;;
extras-p3)    script="$to_options,$(click 100 392),$(wait_for extras_p1),500:PGDN,$(wait_for extras_p2),500:PGDN,$(wait_for extras_p3)"; final=extras_p3 ;;
*) echo "unknown target: $target" >&2; exit 2 ;;
esac

mkdir -p "$out" || exit 1
work=$(mktemp -d "${TMPDIR:-/tmp}/menu_walk.XXXXXX") || exit 1
mkdir -p "$work/user" "$work/cap"
printf 'format=4:3\n' > "$work/user/port.cfg"   # 4:3: a wider format shifts the 640x480 screens
limit=$(( step * 12 + 60 ))
(cd "$work" && env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_CAPTURE_DIR="$work/cap" TNFS_CAPTURE_EVERY=${EVERY:-1} \
    TNFS_INPUT_SETTLE=$settle TNFS_INPUT_SCRIPT="$script" \
    timeout -k 5 $limit "$bin" --data "$data" --mods "$work" --user "$work/user" > "$work/log" 2>&1 &)
# the game runs on its own: watch its log for the last mark, then let the screen settle a moment and stop it
rc=1 waited=0
while [ $waited -lt $limit ]; do
    sleep 1; waited=$((waited + 1))
    if grep -q "mark:screen=$final" "$work/user/spirited-drive.log" 2>/dev/null; then sleep 3; rc=0; break; fi
    if grep -q "input script: step.*timed out" "$work/user/spirited-drive.log" 2>/dev/null; then rc=3; break; fi
    pgrep -f "$work/user" >/dev/null 2>&1 || { [ $waited -gt 3 ] && break; }   # the game ended on its own (a crash, or the script's end)
done
pkill -TERM -f "$work/user" 2>/dev/null; sleep 1; pkill -KILL -f "$work/user" 2>/dev/null
if [ $rc -eq 0 ]; then
    last=$(ls "$work/cap" | sort | tail -1)
    if [ -n "$last" ]; then
        magick "$work/cap/$last" "$out/$target.png" || { rc=1; echo "the frame did not convert to $out/$target.png" >&2; }
    else rc=1; echo "reached mark:screen=$final but no frame was captured" >&2; fi
fi
if [ $rc -ne 0 ]; then
    echo "menu_walk $target: failed (exit $rc); the game's last lines:" >&2
    grep -E "mark:screen|input script" "$work/user/spirited-drive.log" | tail -12 >&2
fi
[ -n "$KEEP" ] && echo "kept: $work" >&2 || { find "$work" -type f -delete; find "$work" -depth -type d -empty -delete 2>/dev/null; }
exit $rc
