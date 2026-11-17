#!/bin/sh
# dep59 step 4b (T8): the CONTENT screen ("Manage content..." in Game Mode) against the real binary and the CD data. Every step waits on
# the game's own lines: its marks (mark:screen=, mark:dialog=, mark:edition=, mark:car=, mark:card=, mark:results=) and, where a dialog
# leaves no mark, the stores' own log lines (edition_store: saved <id> / deleted <id>; mods: installed / updated / restored / uninstalled
# <id>). The car piece is tnfs-studio's (car new + mod pack, as mod_install.sh's), dropped on the window by the script's DROP step. Each
# PART is one game session (one per player folder set up), and a failure names the check it belongs to and, when the walk stopped, the
# section it stopped in.
# PART (one ctest each, so they run side by side):
#   own       a fresh Restomod player, no edition: Manage content opens CONTENT;
#               new        New, the name box: "Mine" (editions/mine.ini, the standard set);
#               install    the car's zip (0.1) dropped: installed, the prompt "Add to Mine?" answered yes, Mine saved with it;
#               live       Done: Game Mode puts Mine in use (mark:edition=mine) and Control Central opens on its cars at a fresh
#                          player's car 6; the car arrow's right takes 7, then the added car (Mine's last: the stock eight, then it);
#               race       a whole race of the added car on Rusty Springs (a fresh player's track card 1, the arrow's right twice):
#                          race_records_cars.dat keeps its table;
#               update     CONTENT again, on Mine; the zip of 0.2 dropped: an update, 0.1 kept as a backup;
#               previous   the car's row (the ninth line of the one page) and Previous version: 0.1 back;
#               uninstall  Uninstall, yes: mods/test-car gone;
#               delete     Delete, yes, with Mine in use: editions/mine.ini gone, the standard edition put back (mark:edition=-);
#               standard   Done on the standard edition: Game Mode and Control Central go on, restomod.cfg keeps no edition.
#   standard  a fresh Restomod player, the standard edition shown: the zip dropped is installed with the prompt's OK alone (no edition
#             to add it to, no file in editions/); New left with Escape makes no edition; Install... with no window picks nothing (the
#             log says so) and the screen goes on; Done.
# usage: content_menus.sh TNFS STUDIO ORIGINAL_DIR PORT_DATA_DIR [PART] (no PART: all of them, in turn). KEEP=1 keeps the scratch folder.
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }   # the walks run the game from the scratch folder
tnfs=$(abs "$1")
studio=$(abs "$2")
orig=$(abs "$3")
port=$(abs "$4")
parts=${5:-own standard}
[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
trap '[ -n "${KEEP:-}" ] && echo "kept $work" >&2 || find "$work" -delete' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
export TNFS_DROP_DIR="$work/drop"
step=60
mkdir -p "$work/nomods" "$work/drop"

# the car piece's zips in the drop folder: test-car.zip (0.1) and, with `2`, test-car-2.zip (0.2)
car_zips() {
    "$studio" car new --template viper --id test-car --stem TESTCAR --out "$work/p" --data "$orig" --port-data "$port" >"$work/zip.log" 2>&1 ||
        { fail "car new: $(cat "$work/zip.log")"; return 1; }
    "$studio" mod pack "$work/p" "$work/drop/test-car.zip" >"$work/zip.log" 2>&1 || { fail "mod pack: $(cat "$work/zip.log")"; return 1; }
    [ "${1:-}" = 2 ] || return 0
    sed 's/^version=0.1$/version=0.2/' "$work/p/mod.ini" > "$work/mod.ini" && cp "$work/mod.ini" "$work/p/mod.ini"
    "$studio" mod pack "$work/p" "$work/drop/test-car-2.zip" >"$work/zip.log" 2>&1 || { fail "mod pack 0.2: $(cat "$work/zip.log")"; return 1; }
}
# a fresh Restomod player folder, no edition
user_fresh() {
    [ -d "$work/user" ] && find "$work/user" -delete
    mkdir -p "$work/user" && printf 'profile=restomod\n' > "$work/user/port.cfg"
}
log() { grep -v "^input script" "$work/user/spirited-drive.log" 2>/dev/null; }
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
# a line of the log that is no mark (a store's): no commas, and at most 31 bytes (a longer WAIT is dropped)
wait_line() { printf '0:WAIT:%s:%s' "$1" "$step"; }
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
# Game Mode's "Manage content" (game_mode.c's ROW[MANAGE], y 291..340)
manage="$(click 320 315),$(wait_for screen=content)"
# CONTENT's places (content_nav.h): the foot's buttons at y 446 (content_nav.c's FOOT_X), the card's Uninstall (y 358, x 500 on) and
# Previous version (y 386), the page indicator's next (x 330, y 370), the list's first line (y 162) by its name (a click there selects)
new="$(click 40 456)" delete="$(click 290 456)" install="$(click 470 456)" done_="$(click 580 456)"
uninstall="$(click 560 356)" previous="$(click 450 384)" car_line="$(click 150 358)"
# a click on the card's text, which takes nothing: it closes a banner still up (a banner ends on a click), else does nothing
quiet="$(click 600 300)"
yes="300:Y"   # the yes / no box (yes_no_box.c) logs no mark: its Y, once it is up (the click before it opened it)

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
# the values of a mark in the lines on stdin, in order, on one line: car -> "1 5 7 1"
values() { sed -n "s/^mark:$1=\([^ ]*\).*/\1/p" | tr '\n' ' ' | sed 's/ $//'; }
# 1 when the log line $1 (a fixed string) comes before the line $2
before() { log | awk -v a="$1" -v b="$2" 'index($0, a) && !seen_a { seen_a = NR } index($0, b) && !seen_b { seen_b = NR }
    END { exit !(seen_a && seen_b && seen_a < seen_b) }'; }

part_own() {
    user_fresh
    car_zips 2 || return
    section new;       add "$to_game_mode,$manage,$new,$(wait_for dialog=name),300:TEXT:Mine,300:ENTER,$(wait_line "saved mine")"
    section install;   add "500:DROP:test-car.zip,$(wait_for dialog=install),300:ENTER,$(wait_line "saved mine")"
    section live;      add "$done_,$(wait_for edition=mine),$single,$(click 319 197),$(wait_for car=7),$(click 319 197),$(wait_for car=)"
    # Rusty Springs (card 3) from card 1; one lap with the harness's autopilot; Enter on each results screen once it has faded in
    # (headless the first needs two: one wakes the pointer over Done, the second takes it)
    section race;      add "$(click 535 319),$(wait_for card=2),$(click 535 319),$(wait_for card=3)"
    section race;      add "$(click 56 453),0:WAIT:mark:results=circuit:300,300:ENTER,300:ENTER,$(wait_for results=best_totals)"
    section race;      add "300:ENTER,$(wait_for car=),$back"
    section update;    add "$manage,500:DROP:test-car-2.zip,$(wait_line "updated test-car"),$quiet"
    section previous;  add "$car_line,$previous,$(wait_line "restored test-car")"
    section uninstall; add "$uninstall,$yes,$(wait_line "uninstalled test-car")"
    section delete;    add "$delete,$yes,$(wait_line "deleted mine")"
    section standard;  add "$done_,$single"
    extra="TNFS_AUTOPILOT=1 TNFS_LAPS=1" walk "$work/nomods" || fail "own: the walk did not end"
    # new: Mine made by the name box, before anything was installed
    before "edition_store: saved mine" "mods: installed test-car 0.1" || fail "new: New did not save Mine before the install"
    # install: the drop installed 0.1, and the prompt's yes added it to Mine (saved again)
    log | grep -q "^content: install of .* 0.1: added$" || fail "install: the prompt's yes did not add the car: $(log | grep '^content: install' | head -1)"
    [ "$(log | grep -c "^edition_store: saved mine$")" -ge 2 ] || fail "install: Mine was not saved with the car"
    # live: Mine put in use before Control Central, which opened on car 6; the arrow's right took 7, then one that is no stock car
    before "mark:edition=mine" "mark:screen=central" || fail "live: Done did not put Mine in use before Control Central"
    cars=$(segment 1 | values car)
    case "$cars" in "6 7 "[0-7]|"6 7 "[0-7]" "*) fail "live: the car after the 7 is a stock car, not the added one: '$cars'" ;;
    "6 7 "*) ;; *) fail "live: Control Central did not walk Mine's cars 6, 7 and the added car: '$cars'" ;; esac
    # race: run to its finish with the added car
    log | grep -q "^finish: 0=" || fail "race: the race did not finish"
    grep -aq "test-car" "$work/user/race_records_cars.dat" 2>/dev/null || fail "race: race_records_cars.dat has no table of test-car"
    # update, previous, uninstall: each said by the mod store, the backups kept
    log | grep -q "^mods: updated test-car 0.2$" || fail "update: the drop of 0.2 did not update the car"
    ls "$work/user/mods_backup" 2>/dev/null | grep -q "^test-car-0.1-" || fail "update: no backup of 0.1"
    log | grep -q "^mods: restored test-car from " || fail "previous: Previous version did not bring a backup back"
    [ -e "$work/user/mods/test-car" ] && fail "uninstall: mods/test-car is still there"
    # delete: Mine's file gone, the standard edition put back, before the second Control Central
    [ -e "$work/user/editions/mine.ini" ] && fail "delete: editions/mine.ini is still there"
    before "mark:edition=-" "edition_store: deleted mine" || fail "delete: the standard edition was not put back as Mine went"
    grep -q "^edition=." "$work/user/restomod.cfg" 2>/dev/null && fail "standard: restomod.cfg keeps an edition: $(cat "$work/user/restomod.cfg")"
    [ -n "$(segment 2 | values car)" ] || fail "standard: Control Central did not open after CONTENT's Done"
}

part_standard() {
    user_fresh
    car_zips || return
    section install; add "$to_game_mode,$manage,500:DROP:test-car.zip,$(wait_for dialog=installed),300:ENTER"
    section new;     add "$new,$(wait_for dialog=name),300:ESC"
    section picker;  add "$install,$(wait_line "no window for a file dialog")"
    section done;    add "$done_,$single"
    walk "$work/nomods" || fail "standard: the walk did not end"
    log | grep -q "^mods: installed test-car 0.1$" || fail "install: the drop did not install the car"
    log | grep -q "^content: install of .* 0.1: noted$" || fail "install: the standard edition's prompt was not the OK alone"
    [ -n "$(ls "$work/user/editions" 2>/dev/null)" ] && fail "new: an edition was made: $(ls "$work/user/editions")"
    log | grep -q "^edition_store: saved" && fail "new: Escape in the name box saved an edition"
    grep -q "^edition=." "$work/user/restomod.cfg" 2>/dev/null && fail "done: restomod.cfg keeps an edition: $(cat "$work/user/restomod.cfg")"
    [ -n "$(segment 1 | values car)" ] || fail "done: Control Central did not open after CONTENT's Done"
}

for part in $parts; do
    case $part in
    own|standard) "part_$part" ;;
    *) echo "FAIL: no part $part" >&2; exit 2 ;;
    esac
done
[ "$failures" -eq 0 ] && echo "content_menus ($parts): all checks passed"
exit "$failures"
