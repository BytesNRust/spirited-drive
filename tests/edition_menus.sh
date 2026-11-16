#!/bin/sh
# dep59 step 4 (T11): an edition of one's own in the menus, against the real binary and the CD data. Every step waits on the game's own
# marks (mark:screen=, mark:car= from Control Central, mark:card=<track> ... from its track card, mark:race_type=no_tournament,
# mark:game_mode=network_closed, mark:results=); one game session walks through several checks.
# PART (one ctest each, so they run side by side):
#   lists    an edition of three cars and two tracks (cars=nsx,viper,diablo tracks=city,vegas): Control Central opens on its first car,
#            the car arrow walks its three and comes round, the track arrow its two and comes round.
#   modes    Stock and Restored with an edition in editions/ and Restomod's edition= set: no edition read, Control Central the same picture
#            as a player folder with none.
#   closed   in the edition: Multiplayer is dim (mark:game_mode=network_closed, no name box), and the race type arrow steps over
#            Tournament (mark:race_type=no_tournament).
#   race     a whole race of car 2 (supra) on Rusty Springs from the menus, in an edition (cars=supra tracks=rusty): race_records_cars.dat
#            keeps the supra's row.
# usage: edition_menus.sh TNFS REPO ORIGINAL_DIR [PART] (no PART: all of them, in turn)
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from the scratch folder
tnfs=$(abs "$1")
repo=$(abs "$2")
orig=$(abs "$3")
parts=${4:-lists modes closed race}
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
trap '[ -n "${KEEP:-}" ] && echo "kept $work" >&2 || find "$work" -delete' EXIT   # KEEP=1: the scratch folder kept
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
step=60
nomods=$work/nomods
mkdir -p "$nomods" "$work/shots"

# a fresh player folder in mode $1; with $2, the edition of that name in editions/ (its text $3) and Restomod's edition= set to it
user_in() {
    [ -d "$work/user" ] && find "$work/user" -delete
    mkdir -p "$work/user" && printf 'profile=%s\n' "$1" > "$work/user/port.cfg"
    [ $# -ge 3 ] || return 0
    mkdir -p "$work/user/editions" && printf '%s' "$3" > "$work/user/editions/$2.ini"
    printf 'edition=%s\n' "$2" > "$work/user/restomod.cfg"
}
log() { grep -v "^input script" "$work/user/spirited-drive.log" 2>/dev/null; }
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
shot() { printf '0:SHOT:%s' "$1"; }   # the frame on screen once it is still, as $work/shots/$1.ppm
# the way in: our boot screen, EA's first movie, the title (a key ends its wait), Game Mode, Single, the name box, Control Central
to_game_mode="1500:ESC,$(wait_for movie=),300:ESC,$(wait_for screen=title),300:ESC,$(wait_for screen=game_mode)"
to_central="$to_game_mode,$(click 310 170),$(wait_for screen=name_box),300:TEXT:R,300:ENTER,$(wait_for screen=central)"
car_right() { printf '%s,%s' "$(click 319 197)" "$(wait_for "car=$1")"; }      # the car arrow's right (central_select.c), the card's mark
track_right() { printf '%s,%s' "$(click 535 319)" "$(wait_for "card=$1")"; }   # the track arrow's right, the track card's mark
race_type_right="$(click 317 352)"   # the race type arrow's right (central_select.c's race_type_item)

# walk MODS SCRIPT: the game run with the input script, which ends on a WAIT for a mark that never comes, a second long: the game stops there
# (status 3, that step named), every step before it taken. Set for one walk: extra (more of the game's environment). 0 when so
walk() {
    mods=$1 script="$2,0:WAIT:mark:walk_end:1"
    last=$(printf '%s' "$script" | tr ',' '\n' | grep -c .)
    (cd "$work" && env ${extra:-} TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=500 TNFS_SHOT_DIR="$work/shots" TNFS_INPUT_SCRIPT="$script" \
        timeout -k 5 600 "$tnfs" --data "$orig" --mods "$mods" --user "$work/user" >"$work/out" 2>&1)
    rc=$? extra=""   # sh keeps a function's prefix assignments: extra is for one walk only
    walks=$((${walks:-0} + 1))
    cp "$work/user/spirited-drive.log" "$work/walk$walks.log" 2>/dev/null   # each walk's log, for KEEP=1
    if [ $rc -eq 3 ] && grep -q "input script: step $last: WAIT mark:walk_end timed out" "$work/user/spirited-drive.log"; then
        grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log" || return 0
        echo "walk: $(grep -m1 "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log")" >&2   # a step dropped: not the walk written
        return 1
    fi
    echo "walk: status $rc; $(grep "^input script: step.*\(timed out\|not written\)" "$work/user/spirited-drive.log" | head -1); the log ends: $(log | tail -3); the game said: $(tail -2 "$work/out")" >&2
    return 1
}
# the values of a mark in the log, in order, on one line: marks car -> "1 5 7 1"; marks card -> the track numbers alone
marks() { log | sed -n "s/^mark:$1=\([^ ]*\).*/\1/p" | tr '\n' ' ' | sed 's/ $//'; }

trio='name=Trio\ncars=nsx,viper,diablo\ntracks=city,vegas\n'

part_lists() {
    user_in restomod trio "$(printf "$trio")"
    walk "$nomods" "$to_central,$(car_right 5),$(car_right 7),$(car_right 1),$(track_right 8),$(track_right 0)" ||
        fail "the edition's three cars and two tracks were not walked round"
    [ "$(marks car)" = "1 5 7 1" ] || fail "the car arrow did not walk nsx, viper, diablo and round: $(marks car)"
    [ "$(marks card)" = "0 8 0" ] || fail "the track arrow did not walk city, vegas and round: $(marks card)"
}

part_modes() {
    for mode in stock restored; do
        user_in $mode trio "$(printf "$trio")"
        walk "$nomods" "$to_central,$(shot $mode-ed)" || fail "$mode with an edition: Control Central did not open"
        [ "$(marks car)" = 6 ] || fail "$mode with an edition did not open on a fresh player's car 6: $(marks car)"
        log | grep -q "^mark:edition=\|^edition" && fail "$mode read the edition: $(log | grep "^mark:edition=\|^edition" | head -1)"
        user_in $mode
        walk "$nomods" "$to_central,$(shot $mode)" || fail "$mode without an edition: Control Central did not open"
        cmp -s "$work/shots/$mode-ed.ppm" "$work/shots/$mode.ppm" || fail "$mode's Control Central is not the same with an edition there"
    done
}

part_closed() {
    user_in restomod trio "$(printf "$trio")"
    # Multiplayer (Game Mode's second row): dim, its banner, no name box; a click in the empty gap under the two rows (y 270: no row)
    # closes the banner, or does nothing once it has gone; then Single, and the race type arrow twice from a fresh
    # player's head to head: Single Race, then over Tournament to Time Trial
    walk "$nomods" "$to_game_mode,$(click 310 218),$(wait_for game_mode=network_closed),$(click 320 270),$(click 310 170),$(wait_for screen=name_box),300:TEXT:R,300:ENTER,$(wait_for screen=central),$race_type_right,$race_type_right,$(wait_for race_type=no_tournament)" ||
        fail "the edition's Multiplayer and Tournament were not closed"
    [ "$(log | grep -c "^mark:screen=name_box")" = 1 ] || fail "Multiplayer opened the name box in an edition"
    user_in restomod
    # (twice on is Tournament there, which takes its own car: the walk ends on a pause, no car mark to wait for)
    walk "$nomods" "$to_central,$race_type_right,$race_type_right,1500:MOUSE:10:10" || fail "the standard edition's race type arrow"
    log | grep -q "^mark:race_type=no_tournament" && fail "the standard edition stepped over Tournament"
}

# Rusty Springs alone in the edition: Control Central opens on it. One lap with the harness's autopilot; Enter on each results screen once
# it has faded in (headless the pointer is not over Done at first: on the first screen one Enter wakes it there and a second takes Done)
results="0:WAIT:mark:results=circuit:300,300:ENTER,300:ENTER,$(wait_for results=best_totals),300:ENTER,$(wait_for car=2)"

part_race() {
    user_in restomod solo "$(printf 'name=Solo\ncars=supra\ntracks=rusty\n')"
    extra="TNFS_AUTOPILOT=1 TNFS_LAPS=1" walk "$nomods" "$to_central,$(wait_for card=3),$(click 56 453),$results" ||
        fail "a race of car 2 on Rusty Springs in an edition did not come back to Control Central"
    [ "$(marks car | cut -d' ' -f1)" = 2 ] || fail "the edition did not open on the supra (2): $(marks car)"
    log | grep -q "^finish: 0=" || fail "the race did not finish"
    [ -s "$work/user/race_records_cars.dat" ] || fail "the race kept no race_records_cars.dat"
    grep -aq "supra" "$work/user/race_records_cars.dat" 2>/dev/null || fail "race_records_cars.dat has no table of the supra"
}

for part in $parts; do
    case $part in
    lists|modes|closed|race) "part_$part" ;;
    *) echo "FAIL: no part $part" >&2; exit 2 ;;
    esac
done
[ "$failures" -eq 0 ] && echo "edition_menus ($parts): all checks passed"
exit "$failures"
