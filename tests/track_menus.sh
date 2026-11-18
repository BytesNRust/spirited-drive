#!/bin/sh
# The menus' walk of added tracks (dep59 step 5, T8): an edition with two installed track pieces and none of the game's tracks, from Game Mode
# to Control Central and back, by the game's own marks (every WAIT's text under 32 bytes).
#   central      the track card of the first piece on opening (mark:card=16), the arrows to the second (17), round to the first and back
#                (the list is the edition's alone), no screen but Control Central opened and none of the original's tables read
#   race         the second piece raced from Control Central (the autopilot, one lap), its three results screens, Control Central again;
#                its record is in race_records_tracks.dat under its id, none under the first piece's
#   card         tools/menu_walk.sh's added-track target (Control Central on the piece's card, the picture menu_compare compares) saves a picture
#   best_times   (prepared: set BEST=1 once T7d's page opens it) the card's click opens the track's page and its BEST TIMES the records by id
# usage: track_menus.sh TNFS STUDIO ORIGINAL_DIR [PART...]   (no PART: central race card). KEEP=1 keeps the scratch folder.
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from the scratch folder
tnfs=$(abs "$1")
studio=$(abs "$2")
orig=$(abs "$3")
shift 3
parts=${*:-central race card}
[ -f "$orig/SIMDATA/MISC/AL1.TRI" ] || { echo "skip: no AL1.TRI in $orig"; exit 0; }
work=$(mktemp -d)
trap '[ -n "${KEEP:-}" ] && echo "kept $work" >&2 || find "$work" -delete' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
step=60
nomods=$work/nomods
mkdir -p "$nomods"

# two pieces, installed as <player folder>/mods/<id>/ (an edition reads them there)
printf 'section 24\nsection 40 turn=90 grade=3 bank=6 width=50\nsection 30 turn=-120 grade=-3 bank=-6\nsection 30 bank=0 grade=0 width=40\n' > "$work/loop.trk"
for p in dune-run:DUNERUN hill-run:HILLRUN; do
    "$studio" track new "$work/loop.trk" --template AL1 --piece --id "${p%%:*}" --stem "${p##*:}" --out "$work/pieces/${p%%:*}" --data "$orig" >"$work/studio.log" 2>&1 ||
        { echo "FAIL: track new --piece ${p%%:*}: $(cat "$work/studio.log")" >&2; exit 1; }
done

# a fresh player folder in Restomod with the pieces installed and an edition of them alone (cars=supra tracks=dune-run,hill-run)
user_in() {
    [ -d "$work/user" ] && find "$work/user" -delete
    mkdir -p "$work/user/editions" "$work/user/mods"
    printf 'profile=restomod\n' > "$work/user/port.cfg"
    printf 'name=Pieces\ncars=supra\ntracks=dune-run,hill-run\n' > "$work/user/editions/pieces.ini"
    printf 'edition=pieces\n' > "$work/user/restomod.cfg"
    cp -R "$work/pieces/dune-run" "$work/pieces/hill-run" "$work/user/mods/"
}
log() { grep -v "^input script" "$work/user/spirited-drive.log" 2>/dev/null; }
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
script="" items=0 sections=""
section() { sections="$sections $1:$((items + 1))"; }
add() { script="${script:+$script,}$1"; items=$(printf '%s' "$script" | tr ',' '\n' | grep -c .); }
section_of() {
    name=start
    for s in $sections; do [ "${s#*:}" -le "$1" ] && name=${s%%:*}; done
    echo "$name"
}
to_game_mode="1500:ESC,$(wait_for movie=),300:ESC,$(wait_for screen=title),300:ESC,$(wait_for screen=game_mode)"
single="$(click 310 215),$(wait_for screen=name_box),300:TEXT:R,300:ENTER,$(wait_for screen=central)"
track_right() { printf '%s,%s' "$(click 535 319)" "$(wait_for "card=$1")"; }   # the track arrow's right (Central's forward box), the card's mark
track_left() { printf '%s,%s' "$(click 350 319)" "$(wait_for "card=$1")"; }    # its left
walk() {   # the game run with $script, which ends on a WAIT for a mark that never comes, a second long; 0 when every step before it was taken
    add "0:WAIT:mark:walk_end:1"
    (cd "$work" && env ${extra:-} TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=500 TNFS_INPUT_SCRIPT="$script" \
        timeout -k 5 540 "$tnfs" --data "$orig" --mods "$nomods" --user "$work/user" >"$work/out" 2>&1)
    rc=$? extra=""
    all=$items walked=$sections script="" items=0 sections=""
    walks=$((${walks:-0} + 1))
    cp "$work/user/spirited-drive.log" "$work/walk$walks.log" 2>/dev/null
    if grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log"; then
        echo "walk: $(grep -m1 "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log")" >&2
        return 1
    fi
    [ $rc -eq 3 ] && grep -q "input script: step $all: WAIT mark:walk_end timed out" "$work/user/spirited-drive.log" && return 0
    stopped=$(grep -m1 "^input script: step.*\(timed out\|not written\)" "$work/user/spirited-drive.log")
    at=$(printf '%s' "$stopped" | sed -n 's/^input script: step \([0-9]*\):.*/\1/p')
    sections=$walked
    echo "walk: stopped in $(section_of "${at:-0}") (status $rc; $stopped); the log ends: $(log | tail -3)" >&2
    sections=""
    return 1
}
values() { sed -n "s/^mark:$1=\([^ ]*\).*/\1/p" | tr '\n' ' ' | sed 's/ $//'; }

part_central() {
    user_in
    section opening; add "$to_game_mode,$single,$(wait_for card=16)"
    section arrows;  add "$(track_right 17),$(track_right 16),$(track_left 17),$(track_left 16)"
    walk || { fail "central: the walk did not end"; return; }
    log | grep -q "track piece: dune-run is track 16" || fail "central: dune-run is not track 16: $(log | grep "track piece" | head -3)"
    log | grep -q "track piece: hill-run is track 17" || fail "central: hill-run is not track 17"
    [ "$(log | values card)" = "16 17 16 17 16" ] || fail "central: the card did not walk 16, 17, round to 16 and back: $(log | values card)"
    log | grep -q "^mark:screen=track_select" && fail "central: the arrows opened the track screen"
    log | grep -q "^mark:screen=best_times" && fail "central: a menu read the original's tables"
}

part_race() {
    user_in
    section race;    add "$to_game_mode,$single,$(wait_for card=16),$(track_right 17),$(click 56 453)"
    section results; add "0:WAIT:mark:results=segment:300,300:ENTER,300:ENTER,0:WAIT:mark:results=track:300,300:ENTER,$(wait_for results=best_totals),300:ENTER,$(wait_for car=2)"
    extra="TNFS_AUTOPILOT=1 TNFS_LAPS=1" walk || { fail "race: the second piece did not race and come back to Control Central"; return; }
    log | grep -q "^finish: 0=" || fail "race: the race did not finish"
    log | grep -q "could not start" && fail "race: the race did not start: $(log | grep "could not start")"
    [ -s "$work/user/race_records_tracks.dat" ] || fail "race: race_records_tracks.dat was not made"
    grep -q "hill-run" "$work/user/race_records_tracks.dat" || fail "race: the record is not kept under hill-run"
    grep -q "dune-run" "$work/user/race_records_tracks.dat" && fail "race: the first piece has a record it never raced"
    log | grep -q "track screen: track .* is an added track" && fail "race: the track screen was asked for an added track"
}

part_card() {
    command -v magick >/dev/null 2>&1 && command -v zsh >/dev/null 2>&1 || { echo "skip: card (needs zsh and ImageMagick)"; return; }
    printf 'name=Solo\ncars=supra\ntracks=dune-run\n' > "$work/solo.ini"
    mkdir -p "$work/pics"
    MODS="$work/pieces/dune-run" EDITION="$work/solo.ini" DATA="$orig" zsh "$(dirname "$0")/../tools/menu_walk.sh" "$tnfs" "$work/pics" added-track >"$work/walk.log" 2>&1 ||
        { fail "card: menu_walk.sh added-track: $(tail -3 "$work/walk.log")"; return; }
    [ -s "$work/pics/added-track.png" ] || fail "card: no picture of Control Central on the added track"
}

part_best_times() {
    [ -n "${BEST:-}" ] || { echo "skip: best_times (T7d's page does not open it yet: BEST=1 once it does)"; return; }
    user_in
    # the card's click opens the track's page, its BEST TIMES (the footer's right) the records by id
    section page;  add "$to_game_mode,$single,$(wait_for card=16),$(click 400 320),$(wait_for screen=track_page)"
    section best;  add "$(click 560 460),$(wait_for screen=best_times)"
    walk || { fail "best_times: the walk did not end"; return; }
    log | grep -q "^mark:screen=best_times" || fail "best_times: no Best Times screen"
}

for part in $parts; do
    case $part in
    central|race|card|best_times) "part_$part" ;;
    *) echo "FAIL: no part $part" >&2; exit 2 ;;
    esac
done
[ "$failures" -eq 0 ] && echo "track_menus ($parts): all checks passed"
exit "$failures"
