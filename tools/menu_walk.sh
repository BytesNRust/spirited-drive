#!/bin/zsh
# tools/menu_walk.sh <bin> <outdir> <target>: walk the front end headless to a menu screen and save its picture as <outdir>/<target>.png.
#
#   targets: game-mode (Game Mode, the first menu after the intro: our boot screen and EA's movie skipped)  central (Control Central, KEYS after it)  showcase (the gallery, from a click on Control Central's car card)
#            records (a stock car's RECORDS screen, page 1)  records-p2 (its page 2)
#            added-gallery (an added car's gallery: the car arrow clicked twice to car 16, the first added car, then the same click, KEYS after it; needs PROFILE=restomod, an EDITION with the piece among its cars and MODS the piece's folder)  options  controls (page 1/3, Keyboard)  controls-p2 (2/3, Game controller)  controls-p3 (3/3, Tune)
#            graphics  extras (page 1/3)  extras-p2  extras-p3
#            content (CONTENT, from Game Mode's "Manage content...": open in Restomod alone, so PROFILE defaults to restomod there; with KEYS they follow 2.5 s after the click, the pointer parked off the list: a WAIT on the screen mark armed after it came never ended, and KEYS never ran)
#            added-page (the added track's own page, opened by its card's click; MODS and EDITION as added-track)
#            added-track (Control Central on an added track's card, mark:card=16: MODS a track piece's folder and EDITION an edition of it alone, both required)
#            track (the track screen)  best-times (Coastal, page 1/3)  best-times-p2  best-times-p3  best-circuit (Rusty Springs, 1/2)
#            best-circuit-p2  best-cheats (Coastal, the table with cheats: none)
#        KEYS (track and best-times: more steps after the screen opens, e.g. 1500:UP,800:RIGHT) and TAIL (a pause, in seconds, after the
#        target's mark before the walk looks for the still screen; with KEYS the walk also waits for the game's "input script: done", the last
#        step taken, so KEYS that outlast TAIL are no longer cut short: at most DONE_WAIT seconds, default 30, past the mark, as a binary from
#        before that line never logs it; the walk then says so and goes on). On central, the items car+ car- opponent+ opponent- are a click on that arrow and a
#        wait for the game's mark that the choice changed (mark:car= / mark:opponent=, logged once the card shows it); the walk then checks
#        that each such click changed the choice once (exit 4 if not)
#        USER_FILES (a folder whose files and folders the walk starts with in its user folder: a race_records.dat, a mods/ of pieces)
#        MODS (a folder copied as the walk's --mods: a mod piece the walk runs with; default an empty one)
#        PROFILE (stock, restored or restomod; default the game's own first run): the mode the walk runs in (Best Times' pages are
#        Restomod's)
#        EDITION (an edition's file, <id>.ini): copied to the walk's editions/ and made Restomod's edition in use (restomod.cfg's
#        edition=<id>); with PROFILE=restomod the walk runs in it
#
#   pack: several pictures from ONE game session (a Restomod player with the pieces of MODS and the edition EDITION, when given), which is
#         how menu_compare covers the screens the other targets cannot reach in the time a gate allows: SHOT steps of the input script, one picture
#         each, saved as <outdir>/<name>.png: cd-setup-lit (the CD showcase page of the Diablo, the pointer on SETUP), cd-setup-open (its Car Setup
#         modal open), added-setup (the added car's gallery with its modal open; only with MODS and EDITION naming a car piece, the edition's
#         cars ending in it) cd-records-512 (the 512TR's RECORDS page, the second face of the Video tile) and content-rows (CONTENT, with the edition's cars when MODS and EDITION are given). <outdir>/pack.status says for each
#         picture: "<name> ok", "<name> never" (the game never logged the screen the picture needs: a binary from before it; not a failure) or
#         "<name> failed". Exit 0 every picture made, 6 the rest never possible for this binary, 3 a step timed out or a picture is missing.
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
#       was one colour (a fade to black that never ended: no picture); 6 the content target on a binary that never opened CONTENT (one from
#       before it: its "Manage content..." step timed out, bounded by STEP_TIMEOUT, or the game quit on that click, and no mark:screen=content was logged); 2 bad arguments, no binary
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
# Single Player by Return: the menu starts on it with the pointer on it, in the exe's layout (its second place) and in L2b's (the first), so
# the same steps walk a binary from before L2b too
to_central="$to_game_mode,500:ENTER,$(wait_for name_box),500:TEXT:R,500:ENTER,$(wait_for central)"
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

pack="" shots=""   # pack: the target saves SHOT pictures (the names in shots, a mark each needs after a colon)
steps_after=""   # set by the targets whose own steps go on after their mark (the done wait below)
case $target in
game-mode)    script="$to_game_mode${KEYS:+,$KEYS}"; final=game_mode; tail=${TAIL:-3} ;;
content)      script="$to_game_mode,$(click 320 315),1500:CLICK:320:383,${KEYS:+2500:MOUSE:560:420,$KEYS}${KEYS:-$(wait_for content)}"; final=content; PROFILE=${PROFILE:-restomod} ;;   # ROW[MANAGE]: y 315 in the reordered Game Mode, y 383 in the one before (menu_compare walks both; the click that misses lands on the selector or on nothing)
central)      script="$to_central${KEYS:+,$KEYS}"; final=central; tail=${TAIL:-3} ;;
showcase)     script="$to_central,$(click 180 200),$(wait_for showcase)"; final=showcase ;;
records)      script="$to_central,$(click 180 200),$(wait_for showcase),1000:CLICK:$(( 430 + dx )):265,1500:CLICK:$(( 375 + dx )):265,$(wait_for car_records),$(park)"; final=car_records; tail=${TAIL:-3} ;;   # a stock car's RECORDS: the showcase's Video tile slid to its other face, then clicked
records-p2)   script="$to_central,$(click 180 200),$(wait_for showcase),1000:CLICK:$(( 430 + dx )):265,1500:CLICK:$(( 375 + dx )):265,$(wait_for car_records),$(next_page),0:WAIT:mark:records_page=1:$step,$(park)"; final=car_records; tail=${TAIL:-3} ;;
added-gallery) script="$to_central,$(click 319 197),0:WAIT:mark:car=:$step,$(click 319 197),0:WAIT:mark:car=16:$step,$(click 180 200),$(wait_for added_gallery)${KEYS:+,$KEYS}"; final=added_gallery; tail=${TAIL:-3} ;;
added-track)  [ -n "$MODS" ] && [ -n "$EDITION" ] || { echo "added-track needs MODS (a track piece's folder) and EDITION (an edition file naming it alone, tracks=<id>)" >&2; exit 2; }
              script="$to_central,0:WAIT:mark:card=16:$step${KEYS:+,$KEYS}"; final=central; tail=${TAIL:-3}; PROFILE=${PROFILE:-restomod} ;;   # the card's mark: 16 is the first added track
added-page)   [ -n "$MODS" ] && [ -n "$EDITION" ] || { echo "added-page needs MODS (a track piece's folder) and EDITION (an edition file naming it alone, tracks=<id>)" >&2; exit 2; }
              script="$to_central,0:WAIT:mark:card=16:$step,$(click 400 320),$(wait_for track_page)${KEYS:+,$KEYS}"; final=track_page; tail=${TAIL:-3}; PROFILE=${PROFILE:-restomod} ;;   # the card's click opens the added track's page
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
best-times-p2) script="$to_best,$page_on"; final=best_times; steps_after=1; tail=6 ;;
best-times-p3) script="$to_best,$page_on,$page_on"; final=best_times; steps_after=1; tail=8 ;;
best-circuit)  script="$to_best,$next_track,$next_track"; final=best_times; steps_after=1; tail=12 ;;
best-circuit-p2) script="$to_best,$next_track,$next_track,$page_on"; final=best_times; steps_after=1; tail=14 ;;
best-cheats)   script="$to_best,1500:UP,800:UP,800:UP,800:RIGHT"; final=best_times; steps_after=1; tail=9 ;;
pack)         # the Diablo's page (a fresh player is on the 512TR: car 6; the Diablo is 7, the piece after it, 16), then the piece's gallery, then back
              # to the 512TR for RECORDS: the one that came last, so a binary without it loses nothing before. The SETUP tile is at (57, 100) 300 x 170. RECORDS is the Video tile's second face: RIGHT on the lit tile turns it, but not while the tile
              # peeks (0.7 s after the page opens, about a second): the 3 s before RIGHT is for it to be over, whatever the machine's load.
              # The marks waited on are the ones every build has (mark:car=, screen=showcase, screen=added_gallery); only RECORDS' own
              # (screen=car_records) is new, with a short wait, and its picture is "never" for a binary that does not log it.
              pack=1; settle=${SETTLE:-1000}; PROFILE=${PROFILE:-restomod}; final=content; steps_after=1; tail=${TAIL:-0}
              card="$(click 180 200)"   # the car's card opens its page (the settle before the click is what lets the card take it: 800 ms did not)
              car_next="$(click 319 197),0:WAIT:mark:car=:$step"; car_back="$(click 30 197),0:WAIT:mark:car=:$step"
              leave="500:ESC,500:ESC,0:WAIT:mark:car=:$step"
              script="$to_central,$car_next,$card,$(wait_for showcase),300:MOUSE:$((130 + dx)):130,0:SHOT:cd-setup-lit"
              script="$script,300:CLICK:$((130 + dx)):130,0:SHOT:cd-setup-open,$leave"
              shots="cd-setup-lit cd-setup-open"
              if [ -n "$MODS" ] && [ -n "$EDITION" ]; then   # the piece is the edition's car after the Diablo
                  script="$script,$car_next,$card,$(wait_for added_gallery),300:CLICK:$((130 + dx)):130,0:SHOT:added-setup,$leave,$car_back,$car_back"
                  shots="$shots added-setup"
              else script="$script,$car_back"; fi
              script="$script,$card,$(wait_for showcase),300:MOUSE:$((380 + dx)):265,3000:RIGHT,1000:ENTER,0:WAIT:mark:screen=car_records:20,0:SHOT:cd-records-512"
              # CONTENT last, from Game Mode (Control Central's Done is at (571, 453); Manage content is at y 315 in the reordered Game Mode; a build from before the reorder, where it is at 383, does not make this picture):
              # with the edition and the piece it lists the edition's cars, which the plain content target does not
              script="$script,$leave,$(click 571 453),$(wait_for game_mode),$(click 320 315),0:WAIT:mark:screen=content:20,0:SHOT:content-rows"
              shots="$shots cd-records-512:screen=car_records content-rows:screen=content" ;;
*) echo "unknown target: $target" >&2; exit 2 ;;
esac

mkdir -p "$out" || exit 1
work=$(mktemp -d "${TMPDIR:-/tmp}/menu_walk.XXXXXX") || exit 1
mkdir -p "$work/user" "$work/cap" "$work/mods" "$work/shots"
[ -n "$MODS" ] && cp -R "$MODS"/. "$work/mods/"   # a mod piece the walk runs with
printf 'format=%s\n' "$format" > "$work/user/port.cfg"   # the clicks above are shifted for it
[ -n "$PROFILE" ] && printf 'profile=%s\n' "$PROFILE" >> "$work/user/port.cfg"
[ -n "$USER_FILES" ] && cp -R "$USER_FILES"/* "$work/user/"   # files the walk starts with in the user folder (a race_records.dat)
if [ -n "$EDITION" ]; then   # an edition: its file in editions/, its id in restomod.cfg
    mkdir -p "$work/user/editions" && cp "$EDITION" "$work/user/editions/" || exit 1
    id=${EDITION:t:r}
    printf 'edition=%s\n' "$id" >> "$work/user/restomod.cfg"
fi
limit=$(( step * 12 + 60 ))
# TNFS_VERSION_TEXT: Game Mode's footer shows "vwalk", not the build's commit, so two builds' game-mode pictures compare (a binary from
# before it ignores it, and its footer differs)
(cd "$work" && env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_VERSION_TEXT=walk TNFS_SHOT_DIR="$work/shots" TNFS_CAPTURE_DIR="$work/cap" TNFS_CAPTURE_EVERY=${EVERY:-1} \
    TNFS_INPUT_SETTLE=$settle TNFS_INPUT_SCRIPT="$script" \
    timeout -k 5 $limit "$bin" --data "$data" --mods "$work/mods" --user "$work/user" > "$work/log" 2>&1 &)
# the game runs on its own: watch its log for the last mark, then let the screen settle a moment and stop it
# with KEYS (steps after the target's own script), or a target whose own steps go on after its mark (steps_after: Best Times' pages and
# tracks, whose last step can come after the tail when a track's art is slow to load), the walk also waits for the script's end
# ("input script: done"), at most DONE_WAIT seconds
# past the target's mark: a binary from before that line (a menu_compare against an older build, a bisect) never logs it, and the walk then
# says so and goes on as without KEYS
rc=1 waited=0 marked=-1
while [ $waited -lt $limit ]; do
    sleep 1; waited=$((waited + 1))
    if [ $marked -lt 0 ] && grep -q "mark:screen=$final" "$work/user/spirited-drive.log" 2>/dev/null; then marked=$waited; fi
    if [ $marked -ge 0 ]; then
        if [ -z "$KEYS$steps_after" ] || grep -q "input script: done" "$work/user/spirited-drive.log" 2>/dev/null; then sleep ${tail:-3}; rc=0; break; fi
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
if [ $rc -eq 0 ] && [ -z "$pack" ]; then   # (a pack walk's pictures are its SHOTs, already still)
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
# a pack walk: its pictures are the script's SHOTs; each is "ok" (made), "never" (the game never logged the screen it needs) or "failed"
if [ -n "$pack" ]; then
    : > "$out/pack.status"
    made=0 missing=0 never=0
    for item in ${=shots}; do
        name=${item%%:*}; needs=""; case $item in *:*) needs=${item#*:} ;; esac
        if [ -f "$work/shots/$name.ppm" ] && magick "$work/shots/$name.ppm" "$out/$name.png" 2>/dev/null; then echo "$name ok" >> "$out/pack.status"; made=$((made + 1))
        elif [ -n "$needs" ] && ! grep -q "mark:$needs" "$work/user/spirited-drive.log" 2>/dev/null; then
            echo "$name never" >> "$out/pack.status"; never=$((never + 1)); echo "menu_walk pack: no $name: this binary never logged mark:$needs" >&2
        else echo "$name failed" >> "$out/pack.status"; missing=$((missing + 1)); echo "menu_walk pack: no picture $name" >&2; fi
    done
    if grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log" 2>/dev/null; then
        missing=$((missing + 1)); echo "menu_walk pack: $(grep -m1 'bad TNFS_INPUT_SCRIPT item' "$work/user/spirited-drive.log")" >&2
    fi
    pkill -TERM -f "$work/user" 2>/dev/null
    rc=0; [ $never -gt 0 ] && rc=6; [ $missing -gt 0 ] && rc=3
    [ $rc -ne 0 ] && grep -E "mark:screen|input script" "$work/user/spirited-drive.log" | tail -8 >&2
    [ -n "$KEEP" ] && echo "kept: $work" >&2 || { find "$work" -type f -delete; find "$work" -depth -type d -empty -delete 2>/dev/null; }
    exit $rc
fi
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
if { [ $rc -eq 3 ] || [ $rc -eq 1 ]; } && [ "$final" = content ] && ! grep -q "mark:screen=content" "$work/user/spirited-drive.log" 2>/dev/null; then
    rc=6; echo "menu_walk content: this binary never opened CONTENT (one from before it?): no picture" >&2
fi
if [ $rc -ne 0 ]; then
    echo "menu_walk $target: failed (exit $rc); the game's last lines:" >&2
    grep -E "mark:screen|input script" "$work/user/spirited-drive.log" | tail -12 >&2
fi
[ -n "$KEEP" ] && echo "kept: $work" >&2 || { find "$work" -type f -delete; find "$work" -depth -type d -empty -delete 2>/dev/null; }
exit $rc
