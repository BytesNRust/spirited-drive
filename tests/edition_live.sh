#!/bin/sh
# dep59 step 4 (T11): an edition changed in Game Mode with no restart, against the real binary and the CD data. One game session: Game
# Mode's selector on to edition a (cars=nsx), Control Central; back, on to b (cars=diablo), Control Central; back, two choices back to
# Restomod '26 (the standard edition), Control Central. Each change says mark:edition=<id> (- for the standard one) before Control Central
# opens, and Control Central opens on that edition's cars: a's nsx (1), b's diablo (7), and in the standard edition the car arrow goes on
# from the diablo to the Warrior (11, in Restomod's standard list), which b alone would not.
# usage: edition_live.sh TNFS ORIGINAL_DIR
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }
tnfs=$(abs "$1")
orig=$(abs "$2")
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
trap '[ -n "${KEEP:-}" ] && echo "kept $work" >&2 || find "$work" -delete' EXIT   # KEEP=1: the scratch folder kept
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
step=60
mkdir -p "$work/nomods" "$work/user/editions"
printf 'profile=restomod\n' > "$work/user/port.cfg"
printf 'name=A\ncars=nsx\ntracks=city\n' > "$work/user/editions/a.ini"
printf 'name=B\ncars=diablo\ntracks=alpine\n' > "$work/user/editions/b.ini"

log() { grep -v "^input script" "$work/user/spirited-drive.log" 2>/dev/null; }
wait_for() { printf '0:WAIT:mark:%s:%s' "$1" "$step"; }
click() { printf '300:CLICK:%s:%s' "$1" "$2"; }
# Game Mode's selector row (game_mode.c's ROW[CHOICE], y 296..365): a click on its right third shows the next choice, on its left third the one before;
# the choice applies once the arrows rest. The choices: Stock '96, Restored '96, Restomod '26, then a, b by id
next="$(click 500 330)" prev="$(click 100 330)"
single="$(click 310 170),$(wait_for screen=name_box),300:TEXT:R,300:ENTER,$(wait_for screen=central)"
back="$(click 571 453),$(wait_for screen=game_mode)"   # Control Central's Cancel
script="1500:ESC,$(wait_for movie=),300:ESC,$(wait_for screen=title),300:ESC,$(wait_for screen=game_mode)"
script="$script,$next,$(wait_for edition=a),$single,$back"
script="$script,$next,$(wait_for edition=b),$single,$back"
script="$script,$prev,$prev,$(wait_for edition=-),$single,$(click 319 197),$(wait_for car=11),0:WAIT:mark:walk_end:1"
last=$(printf '%s' "$script" | tr ',' '\n' | grep -c .)
(cd "$work" && TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=500 TNFS_INPUT_SCRIPT="$script" \
    timeout -k 5 600 "$tnfs" --data "$orig" --mods "$work/nomods" --user "$work/user" >"$work/out" 2>&1)
rc=$?
if [ $rc -ne 3 ] || ! grep -q "input script: step $last: WAIT mark:walk_end timed out" "$work/user/spirited-drive.log"; then
    fail "the walk: status $rc; $(grep "^input script: step.*\(timed out\|not written\)" "$work/user/spirited-drive.log" | head -1); the log ends: $(log | tail -3)"
fi
grep -q "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log" && fail "$(grep -m1 "bad TNFS_INPUT_SCRIPT item" "$work/user/spirited-drive.log")"
# each Control Central's first car after the edition said last: "a:1 b:7 -:7"
seen=$(log | awk -F= '/^mark:edition=/ { e = $2 } /^mark:screen=central$/ { open = 1 } /^mark:car=/ && open { printf "%s%s:%s", sep, e, $2; sep = " "; open = 0 }')
[ "$seen" = "a:1 b:7 -:7" ] || fail "Control Central did not open on each edition's cars after its mark: '$seen'"
[ "$(log | grep "^mark:car=" | tail -1)" = "mark:car=11" ] || fail "the standard edition's car arrow did not go on from the diablo to the Warrior"
grep -q "^edition=." "$work/user/restomod.cfg" 2>/dev/null && fail "restomod.cfg keeps an edition= in the standard edition: $(cat "$work/user/restomod.cfg")"
[ "$failures" -eq 0 ] && echo "edition_live: all checks passed"
exit "$failures"
