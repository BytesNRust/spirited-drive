#!/bin/zsh
# tools/menu_walk.sh <bin> <outdir> <target>: walk the front end headless to a menu screen and save its picture as <outdir>/<target>.png.
#
#   targets: game-mode (Game Mode, the first menu after the intro: our boot screen and EA's movie skipped)  central (Control Central, KEYS after it)  showcase (the gallery, from a click on Control Central's car card)
#            added-gallery (an added car's gallery: the same click with an added car picked, KEYS after it)  options  controls (page 1/3, Keyboard)  controls-p2 (2/3, Game controller)  controls-p3 (3/3, Tune)
#            graphics  extras (page 1/3)  extras-p2  extras-p3
#            track (the track screen)  best-times (Coastal, page 1/3)  best-times-p2  best-times-p3  best-circuit (Rusty Springs, 1/2)
#            best-circuit-p2  best-cheats (Coastal, the table with cheats: none)
#        KEYS (track and best-times: more steps after the screen opens, e.g. 1500:UP,800:RIGHT) and TAIL (a pause, in seconds, after the
#        target's mark before the walk looks for the still screen; with KEYS the walk also waits for the game's "input script: done", the last
#        step taken, so KEYS that outlast TAIL are no longer cut short: at most DONE_WAIT seconds, default 30, past the mark, as a binary from
#        before that line never logs it; the walk then says so and goes on). On central, the items car+ car- opponent+ opponent- are a click on that arrow and a
#        wait for the game's mark that the choice changed (mark:car= / mark:opponent=, logged once the card shows it); the walk then checks
#        that each such click changed the choice once (exit 4 if not)
#        USER_FILES (a folder whose files the walk starts with in its user folder: a race_records.dat)
#        MODS (a folder copied as the walk's --mods: a mod piece the walk runs with; default an empty one)
#        PROFILE (stock, restored or restomod; default the game's own first run): the mode the walk runs in (Best Times' pages are
#        Restomod's)
#
# Every step waits for the game's own "mark:screen=NAME" log line (the front end logs one as each screen opens), not for a time; the clicks wait for
# the screen to be still (TNFS_INPUT_SETTLE). A step that does not come in time stops the run and says which ("input script: step N: WAIT ... timed
# out"), exit 3. The run is the visible-run kind: dummy SDL drivers, timeout -k, a scratch --mods and --user, the repo's original/ as the game data.
#   env: DATA (the game data folder, default <repo>/original)  STEP_TIMEOUT (60 s a step)  SETTLE (1500 ms)  KEEP=1 (keep the scratch dir, for a look)
#        STILL (30): the most seconds the walk waits, after the target's mark and its tail, for the screen to stop changing
#        QUIET (2): the seconds without a new frame that make the screen still (the menus present a frame only when something changes)
#        FORMAT (4:3, 16:9 or 21:9; default 4:3): the screen format the walk runs in. The 640x480 menus sit centred on a wider screen, so every click
#        and pointer move is shifted right by the menu's left edge there ((width - 640) / 2: 107 at 16:9, 240 at 21:9)
# A target that goes on with keys after its screen's mark (Best Times' pages) waits `tail` seconds for them before the picture.
# The picture is the page the game ended on: the walk checks that the last screen the game logged is the target's, and fails (exit 4) when it is not,
# so a saved <target>.png is always that page.
# exit: 0 reached and saved; 3 a step timed out; 4 the game went on past the target (the last screen logged is another); 5 every frame captured
#       was one colour (a fade to black that never ended: no picture); 2 bad arguments, no binary
# or no magick; 1 anything else (the log is printed).
bin=$1 out=$2 target=$3
[ -n "$bin" ] && [ -n "$out" ] && [ -n "$target" ] || { echo "usage: $0 <bin> <outdir> <target>" >&2; exit 2; }
[ -x "$bin" ] || { echo "no binary: $bin" >&2; exit 2; }
command -v magick >/dev/null 2>&1 || { echo "magick missing (ImageMagick 7 saves the frame as a PNG; on nix: nix shell --inputs-from . nixpkgs#imagemagick)" >&2; exit 2; }
here=${0:A:h}
data=${DATA:-$here/../original}
[ -d "$data" ] || { echo "no game data folder: $data (set DATA)" >&2; exit 2; }
step=${STEP_TIMEOUT:-60}
settle=${SETTLE:-1500}
format=${FORMAT:-4:3}
case $format in
4:3)  dx=0 ;;
16:9) dx=107 ;;   # (854 - 640) / 2, screen_layout.c's WIDTH_16_9
21:9) dx=240 ;;   # (1120 - 640) / 2, WIDTH_21_9
*) echo "unknown FORMAT: $format (4:3, 16:9 or 21:9)" >&2; exit 2 ;;
esac

wait_for() { printf '0:WAIT:mark:screen=%s:%s' "$1" "$step"; }
# the clicks and moves are in the 640x480 menu's own coordinates, shifted to where the menu sits on the screen
click() { printf '300:CLICK:%s:%s' $(( $1 + dx )) "$2"; }
# The pages of Controls and Customize Game: a click on the footer's right triangle, not a key. With the pointer over a row (it is left where the
# Options click was), RIGHT sometimes moved that row's value (Dead zone 5 % in one walk, 0 % in the next, same binary) and sometimes turned the page,
# and the page 2 picture came out as page 3; on Customize Game PGDN went on round the pages (p2's picture was p3 or p1). One click turns one page.
# park: the pointer moved to the empty middle of the page so the picture shows no row hovered.
next_page() { printf '300:MOUSE:%s:%s,300:CLICK:%s:%s' $(( 355 + dx )) 452 $(( 355 + dx )) 452; }
park() { printf '300:MOUSE:%s:%s' $(( 330 + dx )) 405; }

# the common way in: our boot screen skipped (the first thing the game shows), EA's first movie stopped (which ends the intro), Game Mode; then
# Single, the name box left, Control Central
to_game_mode="1500:ESC,0:WAIT:mark:movie=:$step,500:ESC,$(wait_for game_mode)"
to_central="$to_game_mode,$(click 310 217),$(wait_for name_box),500:TEXT:R,500:ENTER,$(wait_for central)"
to_options="$to_central,$(click 215 450),$(wait_for options)"
to_track="$to_central,$(click 400 320),$(wait_for track_select)"
to_best="$to_track,$(click 560 460),$(wait_for best_times),$(park)"
# the footer's right arrow (the next track) and the pages' keys: Page Down the next page, Up thrice to the cheats field and Right to turn it
next_track="300:MOUSE:$(( 390 + dx )):455,300:CLICK:$(( 390 + dx )):455,3000:MOUSE:$(( 330 + dx )):405"
page_on="1500:PGDN"

# KEYS' arrow items: the click and its mark (the arrows' hit boxes, central_select.c: the car's at y 186..209, x 310..329 and 21..40; the
# opponent's at y 145..168, x 605..624 and 341..362)
expand_keys() {
    local item out=""
    for item in ${(s:,:)1}; do
        case $item in
        car+)      item="$(click 319 197),0:WAIT:mark:car=:$step" ;;
        car-)      item="$(click 30 197),0:WAIT:mark:car=:$step" ;;
        opponent+) item="$(click 614 156),0:WAIT:mark:opponent=:$step" ;;
        opponent-) item="$(click 351 156),0:WAIT:mark:opponent=:$step" ;;
        esac
        out+="${out:+,}$item"
    done
    print -r -- "$out"
}
car_clicks=0 opponent_clicks=0
if [ -n "$KEYS" ]; then
    for item in ${(s:,:)KEYS}; do
        case $item in car[+-]) car_clicks=$((car_clicks + 1)) ;; opponent[+-]) opponent_clicks=$((opponent_clicks + 1)) ;; esac
    done
    KEYS=$(expand_keys "$KEYS")
fi

case $target in
game-mode)    script="$to_game_mode${KEYS:+,$KEYS}"; final=game_mode; tail=${TAIL:-3} ;;
central)      script="$to_central${KEYS:+,$KEYS}"; final=central; tail=${TAIL:-3} ;;
showcase)     script="$to_central,$(click 180 200),$(wait_for showcase)"; final=showcase ;;
added-gallery) script="$to_central,$(click 180 200),$(wait_for added_gallery)${KEYS:+,$KEYS}"; final=added_gallery; tail=${TAIL:-3} ;;
options)      script="$to_options"; final=options ;;
controls)     script="$to_options,$(click 80 285),$(wait_for controls_keyboard)"; final=controls_keyboard ;;
controls-p2)  script="$to_options,$(click 80 285),$(wait_for controls_keyboard),$(next_page),$(wait_for controls_actions),$(park)"; final=controls_actions ;;
controls-p3)  script="$to_options,$(click 80 285),$(wait_for controls_keyboard),$(next_page),$(wait_for controls_actions),$(next_page),$(wait_for controls_tune),$(park)"; final=controls_tune ;;
graphics)     script="$to_options,$(click 80 250),$(wait_for graphics)"; final=graphics ;;
extras)       script="$to_options,$(click 100 392),$(wait_for extras_p1)"; final=extras_p1 ;;
extras-p2)    script="$to_options,$(click 100 392),$(wait_for extras_p1),$(next_page),$(wait_for extras_p2),$(park)"; final=extras_p2 ;;
extras-p3)    script="$to_options,$(click 100 392),$(wait_for extras_p1),$(next_page),$(wait_for extras_p2),$(next_page),$(wait_for extras_p3),$(park)"; final=extras_p3 ;;
track)         script="$to_track${KEYS:+,$KEYS}"; final=track_select; tail=${TAIL:-3} ;;
best-times)    script="$to_best${KEYS:+,$KEYS}"; final=best_times; tail=${TAIL:-4} ;;
best-times-p2) script="$to_best,$page_on"; final=best_times; tail=6 ;;
best-times-p3) script="$to_best,$page_on,$page_on"; final=best_times; tail=8 ;;
best-circuit)  script="$to_best,$next_track,$next_track"; final=best_times; tail=12 ;;
best-circuit-p2) script="$to_best,$next_track,$next_track,$page_on"; final=best_times; tail=14 ;;
best-cheats)   script="$to_best,1500:UP,800:UP,800:UP,800:RIGHT"; final=best_times; tail=9 ;;
*) echo "unknown target: $target" >&2; exit 2 ;;
esac

mkdir -p "$out" || exit 1
work=$(mktemp -d "${TMPDIR:-/tmp}/menu_walk.XXXXXX") || exit 1
mkdir -p "$work/user" "$work/cap" "$work/mods"
[ -n "$MODS" ] && cp -R "$MODS"/. "$work/mods/"   # a mod piece the walk runs with
printf 'format=%s\n' "$format" > "$work/user/port.cfg"   # the clicks above are shifted for it
[ -n "$PROFILE" ] && printf 'profile=%s\n' "$PROFILE" >> "$work/user/port.cfg"
[ -n "$USER_FILES" ] && cp "$USER_FILES"/* "$work/user/"   # files the walk starts with in the user folder (a race_records.dat)
limit=$(( step * 12 + 60 ))
# TNFS_VERSION_TEXT: Game Mode's footer shows "vwalk", not the build's commit, so two builds' game-mode pictures compare (a binary from
# before it ignores it, and its footer differs)
(cd "$work" && env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_VERSION_TEXT=walk TNFS_CAPTURE_DIR="$work/cap" TNFS_CAPTURE_EVERY=${EVERY:-1} \
    TNFS_INPUT_SETTLE=$settle TNFS_INPUT_SCRIPT="$script" \
    timeout -k 5 $limit "$bin" --data "$data" --mods "$work/mods" --user "$work/user" > "$work/log" 2>&1 &)
# the game runs on its own: watch its log for the last mark, then let the screen settle a moment and stop it
# with KEYS (steps after the target's own script) the walk also waits for the script's end ("input script: done"), at most DONE_WAIT seconds
# past the target's mark: a binary from before that line (a menu_compare against an older build, a bisect) never logs it, and the walk then
# says so and goes on as without KEYS
rc=1 waited=0 marked=-1
while [ $waited -lt $limit ]; do
    sleep 1; waited=$((waited + 1))
    if [ $marked -lt 0 ] && grep -q "mark:screen=$final" "$work/user/spirited-drive.log" 2>/dev/null; then marked=$waited; fi
    if [ $marked -ge 0 ]; then
        if [ -z "$KEYS" ] || grep -q "input script: done" "$work/user/spirited-drive.log" 2>/dev/null; then sleep ${tail:-3}; rc=0; break; fi
        if [ $(( waited - marked )) -ge ${DONE_WAIT:-30} ]; then
            echo "menu_walk: no 'input script: done' from this binary" >&2
            sleep ${tail:-3}; rc=0; break
        fi
    fi
    if grep -q "input script: step.*timed out" "$work/user/spirited-drive.log" 2>/dev/null; then rc=3; break; fi
    pgrep -f "$work/user" >/dev/null 2>&1 || { [ $waited -gt 3 ] && break; }   # the game ended on its own (a crash, or the script's end)
done
# the screen still before the game is stopped. The menus present a frame only when something changes, so a page whose fade-in is over sends no more:
# QUIET seconds without a new frame is still, and its newest frame, written long since, is the picture. A screen that keeps presenting (a blinking
# cursor) is still once two frames in a row are the same (the newest left out: it may still be being written), and that frame is the picture.
# Under load a fade-in outlasts the tail; STILL (default 30) is the most seconds this waits, and then the frame before the newest is taken
# a one-colour frame (all black, a fade's flat grey) is a fade caught, never still: the wait goes on past it (tools/frame_pick.py)
one_colour() { python3 -I "$here/frame_pick.py" --one-colour "$1" 2>/dev/null; }
pick=""
if [ $rc -eq 0 ]; then
    ticks=0 calm=0 seen=""
    while [ $ticks -lt $(( ${STILL:-30} * 2 )) ]; do
        frames=($(ls "$work/cap" | sort | tail -3))
        newest=${frames[-1]:-}
        if [ -n "$newest" ] && [ "$newest" = "$seen" ]; then
            calm=$((calm + 1))
            [ $calm -ge $(( ${QUIET:-2} * 2 )) ] && ! one_colour "$work/cap/$newest" && { pick=$newest; break; }
        else calm=0 seen=$newest; fi
        [ ${#frames[@]} -eq 3 ] && cmp -s "$work/cap/${frames[1]}" "$work/cap/${frames[2]}" && ! one_colour "$work/cap/${frames[2]}" &&
            { pick=${frames[2]}; break; }
        sleep 0.5; ticks=$((ticks + 1))
    done
fi
pkill -TERM -f "$work/user" 2>/dev/null; sleep 1; pkill -KILL -f "$work/user" 2>/dev/null
# the screen the game ended on: a key or click that went on past the target (a repeat) leaves another page up, and its picture is not the target's
if [ $rc -eq 0 ]; then
    ended=$(grep -o "mark:screen=[a-z_0-9]*" "$work/user/spirited-drive.log" | tail -1)
    ended=${ended#mark:screen=}
    if [ "$ended" != "$final" ]; then rc=4; echo "menu_walk $target: the game went on to $ended after $final; no picture saved" >&2; fi
fi
# each arrow click changed its choice once: the marks after Control Central's last opening, the first frame's own one apart
if [ $rc -eq 0 ] && [ $(( car_clicks + opponent_clicks )) -gt 0 ]; then
    since=$(grep -n "^mark:screen=central" "$work/user/spirited-drive.log" | tail -1 | cut -d: -f1)
    cars=$(tail -n +${since:-1} "$work/user/spirited-drive.log" | grep -c "^mark:car=")
    opps=$(tail -n +${since:-1} "$work/user/spirited-drive.log" | grep -c "^mark:opponent=")
    if [ $cars -ne $(( car_clicks + 1 )) ] || { [ $opponent_clicks -gt 0 ] && [ $opps -ne $(( opponent_clicks + 1 )) ]; }; then
        rc=4; echo "menu_walk $target: $car_clicks car and $opponent_clicks opponent clicks, but $((cars - 1)) car and $((opps - 1)) opponent changes; no picture saved" >&2
    fi
fi
if [ $rc -eq 0 ]; then
    # the still frame, else the newest before the last that is not one colour: the game is killed while it writes frames, and the last one
    # can be cut short (under load the walk's PNG then failed to convert); the one before was closed before the next was opened
    last=${pick:-$(python3 -I "$here/frame_pick.py" --fallback "$work/cap" 2>"$work/pick.err")}
    if [ -n "$last" ]; then
        magick "$work/cap/$last" "$out/$target.png" || { rc=1; echo "the frame did not convert to $out/$target.png" >&2; }
    elif [ -n "$(ls "$work/cap" 2>/dev/null)" ]; then rc=5; echo "menu_walk $target: $(cat "$work/pick.err"); no picture saved" >&2
    else rc=1; echo "reached mark:screen=$final but no frame was captured" >&2; fi
fi
if [ $rc -ne 0 ]; then
    echo "menu_walk $target: failed (exit $rc); the game's last lines:" >&2
    grep -E "mark:screen|input script" "$work/user/spirited-drive.log" | tail -12 >&2
fi
[ -n "$KEEP" ] && echo "kept: $work" >&2 || { find "$work" -type f -delete; find "$work" -depth -type d -empty -delete 2>/dev/null; }
exit $rc
