#!/bin/sh
# dep59 step 4 (T11): editions of one's own in the menus, against the real binary and the CD data. Every step waits on the game's own
# marks (mark:screen=, mark:car= from Control Central, mark:card=<track> ... from its track card, mark:race_type=no_tournament,
# mark:game_mode=network_closed, mark:edition=, mark:results=). Each PART is one game session (a boot costs seconds: one per player
# folder set up), and a failure names the check it belongs to and, when the walk stopped, the section it stopped in.
# PART (one ctest each, so they run side by side):
#   menus  editions trio (cars=nsx,viper,diablo tracks=city,vegas, in use), a (cars=supra) and b (cars=diablo):
#            closed    Multiplayer is dim in trio: mark:game_mode=network_closed, no name box; the race type arrow steps over Tournament;
#            lists     Control Central opens on trio's first car, the car arrow walks its three and round, the track arrow its two and round;
#            live      Game Mode's selector to a, then b, then Restomod '26 with no restart: each mark:edition= before Control Central
#                      opens on that edition's cars (a's supra 2, b's diablo 7, the standard list's diablo 7);
#            standard  in the standard edition the car arrow goes on from the diablo to the Warrior (11, which b alone would not), the
#                      race type arrow does not step over Tournament, and restomod.cfg keeps no edition.
#   modes  Stock, then Restored (the selector, no restart), with trio in editions/ and Restomod's edition= set: no edition read, Control
#          Central opens on a fresh player's car 6 in both. (The pictures are menu_compare's stock and restored targets'.)
#   race   a whole race of car 2 (supra) on Rusty Springs from the menus, in an edition (cars=supra tracks=rusty): race_records_cars.dat
#          keeps the supra's table.
# usage: edition_menus.sh TNFS REPO ORIGINAL_DIR [PART] (no PART: all of them, in turn). KEEP=1 keeps the scratch folder.
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from the scratch folder
tnfs=$(abs "$1")
orig=$(abs "$3")
parts=${4:-menus modes race}
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
trap '[ -n "${KEEP:-}" ] && echo "kept $work" >&2 || find "$work" -delete' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
step=60
nomods=$work/nomods
mkdir -p "$nomods"

# a fresh player folder in mode $1, with the editions given as id=text pairs after it (\n in the text); Restomod's edition= the first one's
user_in() {
    [ -d "$work/user" ] && find "$work/user" -delete
    mkdir -p "$work/user/editions" && printf 'profile=%s\n' "$1" > "$work/user/port.cfg"
    shift
    first=""
    for pair in "$@"; do
        id=${pair%%=*}
        printf '%b' "${pair#*=}" > "$work/user/editions/$id.ini"
        [ -n "$first" ] || first=$id
    done
    [ -z "$first" ] || printf 'edition=%s\n' "$first" > "$work/user/restomod.cfg"
}
log() { grep -v "^input script" "$work/user/spirited-drive.log" 2>/dev/null; }
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
# the script, built a section at a time: `section NAME` starts one, `add ITEMS` appends; where a walk stops says which section it was in
script="" items=0 sections=""
section() { sections="$sections $1:$((items + 1))"; }
add() { script="${script:+$script,}$1"; items=$(printf '%s' "$script" | tr ',' '\n' | grep -c .); }
section_of() {   # the section step $1 belongs to
    name=start
    for s in $sections; do [ "${s#*:}" -le "$1" ] && name=${s%%:*}; done
    echo "$name"
}
# the way in: our boot screen, EA's first movie, the title (a key ends its wait), Game Mode; Single, the name box, Control Central
to_game_mode="1500:ESC,$(wait_for movie=),300:ESC,$(wait_for screen=title),300:ESC,$(wait_for screen=game_mode)"
single="$(click 310 215),$(wait_for screen=name_box),300:TEXT:R,300:ENTER,$(wait_for screen=central)"
back="$(click 571 453),$(wait_for screen=game_mode)"   # Control Central's Cancel
car_right() { printf '%s,%s' "$(click 319 197)" "$(wait_for "car=$1")"; }      # the car arrow's right (central_select.c), the card's mark
track_right() { printf '%s,%s' "$(click 535 319)" "$(wait_for "card=$1")"; }   # the track arrow's right, the track card's mark
race_type_right="$(click 317 352)"   # the race type arrow's right (central_select.c's race_type_item)
# Game Mode's selector row (game_mode.c's ROW[CHOICE], y 96..190): a click on its right third shows the next choice, on its left third
# the one before; the choice applies once the arrows rest, or as the selection leaves the row
next="$(click 500 140)" prev="$(click 100 140)"

# walk MODS: the game run with $script, which ends on a WAIT for a mark that never comes, a second long: the game stops there (status 3,
# that step named), every step before it taken. Set for one walk: extra (more of the game's environment). 0 when so; else it says the
# section the walk stopped in. The script and its sections are emptied for the next walk either way
walk() {
    add "0:WAIT:mark:walk_end:1"
    (cd "$work" && env ${extra:-} TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=500 TNFS_INPUT_SCRIPT="$script" \
        timeout -k 5 600 "$tnfs" --data "$orig" --mods "$1" --user "$work/user" >"$work/out" 2>&1)
    rc=$? extra=""   # sh keeps a function's prefix assignments: extra is for one walk only
    all=$items walked=$sections script="" items=0 sections=""
    walks=$((${walks:-0} + 1))
    cp "$work/user/spirited-drive.log" "$work/walk$walks.log" 2>/dev/null   # each walk's log, for KEEP=1
    if grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log"; then
        echo "walk: $(grep -m1 "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log")" >&2   # a step dropped: not the walk written
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
# the log from the Nth Control Central (1, 2 ...) to the Game Mode after it
segment() { log | awk -v n="$1" '/^mark:screen=central$/ { k++ } k == n { print } k == n && /^mark:screen=game_mode$/ { exit }'; }
# the values of a mark in the lines on stdin, in order, on one line: car -> "1 5 7 1"; card -> the track numbers alone
values() { sed -n "s/^mark:$1=\([^ ]*\).*/\1/p" | tr '\n' ' ' | sed 's/ $//'; }

trio='trio=name=Trio\ncars=nsx,viper,diablo\ntracks=city,vegas\n'

part_menus() {
    user_in restomod "$trio" 'a=name=A\ncars=supra\ntracks=city\n' 'b=name=B\ncars=diablo\ntracks=alpine\n'
    # the selector: Stock '96, Restored '96, Restomod '26, then a, b, trio by id; trio in use. A fresh player's race type: head to head
    section closed;   add "$to_game_mode,$(click 310 265),$(wait_for game_mode=network_closed),$(click 320 370)"   # the gap: the banner closed
    section lists;    add "$single,$(car_right 5),$(car_right 7),$(car_right 1),$(track_right 8),$(track_right 0)"
    section closed;   add "$race_type_right,$race_type_right,$(wait_for race_type=no_tournament),$back"   # single race, then over to time trial
    section live;     add "$prev,$prev,$(wait_for edition=a),$single,$back,$next,$(wait_for edition=b),$single,$back"
    section live;     add "$prev,$prev,$(wait_for edition=-),$single"
    # the standard edition: time trial on to Tournament, three steps (Tournament takes a car of its own: the Warrior is looked for before)
    section standard; add "$(car_right 11),$race_type_right,$race_type_right,$race_type_right,1500:MOUSE:10:10"
    walk "$nomods" || fail "menus: the walk did not end"
    # closed: Multiplayer opened no name box (one per Single), and trio's race type stepped over Tournament
    log | grep -q "^mark:game_mode=network_closed$" || fail "closed: Multiplayer in an edition said nothing"
    [ "$(log | grep -c "^mark:screen=name_box$")" = "$(log | grep -c "^mark:screen=central$")" ] || fail "closed: Multiplayer opened the name box"
    segment 1 | grep -q "^mark:race_type=no_tournament$" || fail "closed: the race type arrow did not step over Tournament"
    # lists: trio's cars and tracks round
    [ "$(segment 1 | values car)" = "1 5 7 1" ] || fail "lists: the car arrow did not walk nsx, viper, diablo and round: $(segment 1 | values car)"
    [ "$(segment 1 | values card)" = "0 8 0" ] || fail "lists: the track arrow did not walk city, vegas and round: $(segment 1 | values card)"
    # live: each Control Central's first car after the edition said last (none said before the first: trio, from restomod.cfg)
    seen=$(log | awk -F= '/^mark:edition=/ { e = $2 } /^mark:screen=central$/ { open = 1 } /^mark:car=/ && open { printf "%s%s:%s", sep, e, $2; sep = " "; open = 0 }')
    [ "$seen" = ":1 a:2 b:7 -:7" ] || fail "live: Control Central did not open on each edition's cars after its mark: '$seen'"
    # standard: the standard list's Warrior after the diablo, Tournament not stepped over, no edition kept
    segment 4 | values car | tr ' ' '\n' | grep -qx 11 || fail "standard: the car arrow did not go on from the diablo to the Warrior"
    segment 4 | grep -q "^mark:race_type=no_tournament$" && fail "standard: the standard edition stepped over Tournament"
    grep -q "^edition=." "$work/user/restomod.cfg" 2>/dev/null && fail "standard: restomod.cfg keeps an edition: $(cat "$work/user/restomod.cfg")"
}

part_modes() {
    user_in stock "$trio"
    section stock;    add "$to_game_mode,$single,$back"
    section restored; add "$next,$single"   # Single leaves the selector: Restored '96 applies
    walk "$nomods" || fail "modes: the walk did not end"
    grep -q "^profile=restored$" "$work/user/port.cfg" || fail "restored: the selector did not change the mode: $(cat "$work/user/port.cfg")"
    for n in 1 2; do
        mode=$([ $n = 1 ] && echo stock || echo restored)
        [ "$(segment $n | values car | cut -d' ' -f1)" = 6 ] || fail "$mode: Control Central did not open on a fresh player's car 6: $(segment $n | values car)"
    done
    log | grep -q "^mark:edition=\|^edition" && fail "modes: Stock or Restored read the edition: $(log | grep "^mark:edition=\|^edition" | head -1)"
}

# Rusty Springs alone in the edition: Control Central opens on it. One lap with the harness's autopilot; Enter on each results screen once
# it has faded in (headless the pointer is not over Done at first: on the first screen one Enter wakes it there and a second takes Done)
part_race() {
    user_in restomod 'solo=name=Solo\ncars=supra\ntracks=rusty\n'
    section race;    add "$to_game_mode,$single,$(wait_for card=3),$(click 56 453)"
    section results; add "0:WAIT:mark:results=circuit:300,300:ENTER,300:ENTER,$(wait_for results=best_totals),300:ENTER,$(wait_for car=2)"
    extra="TNFS_AUTOPILOT=1 TNFS_LAPS=1" walk "$nomods" || fail "race: a race of car 2 on Rusty Springs did not come back to Control Central"
    [ "$(log | values car | cut -d' ' -f1)" = 2 ] || fail "race: the edition did not open on the supra (2): $(log | values car)"
    log | grep -q "^finish: 0=" || fail "race: the race did not finish"
    grep -aq "supra" "$work/user/race_records_cars.dat" 2>/dev/null || fail "race: race_records_cars.dat has no table of the supra"
}

for part in $parts; do
    case $part in
    menus|modes|race) "part_$part" ;;
    *) echo "FAIL: no part $part" >&2; exit 2 ;;
    esac
done
[ "$failures" -eq 0 ] && echo "edition_menus ($parts): all checks passed"
exit "$failures"
