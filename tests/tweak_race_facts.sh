#!/bin/sh
# An installed tweak may not change a race through track_light/<TRACK>.txt (owner 2026-11-24): its tunnel and snow facts change the dry zones and the grip. Real binary and CD data, TR1 in the rain with
# rain_grip on, a one-lap race driven by the harness's autopilot, slot 0's car printed every 30 ticks:
#   A  no tweak: the race as it is
#   B  an edition with a tweak whose track_light/TR1.txt holds `tunnel 1 500`: the file is left out (said in the log), and the race is A's, tick for tick (the same trace, the same finish) and keeps its records
#   C  the same file as --mods (the developer's folder, which is served): the race differs from A. It shows that A and B being the same means something
# The three races are independent: PART A, B or C runs one of them (each its own user folder) into WORK, and PART compare reads the three and makes the 7 checks, so the gate can run them side by side
# (the CMake tests tweak_race_facts_A/_B/_C then _compare, which needs the three). With no PART (or `all`) the one script does the four as it did.
# The three races run their clock four times faster (TNFS_CLOCK_RATE: the same steps and results; no key is sent while a race runs).
# usage: tweak_race_facts.sh TNFS REPO ORIGINAL_DIR [PART [WORK]]
set -u
abs() { case $1 in /*) echo "$1" ;; *) echo "$PWD/$1" ;; esac; }
tnfs=$(abs "$1")
repo=$(abs "$2")
orig=$(abs "$3")
part=${4:-all}
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
if [ -n "${5:-}" ]; then work=$(abs "$5"); mkdir -p "$work"; else work=$(mktemp -d); trap 'find "$work" -delete' EXIT; fi
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

# the file with a tunnel fact: the repo's own light for TR1 and a tunnel over the first 500 of its 512 nodes
tunnel_file() { (head -2 "$repo/data/track_light/TR1.txt"; echo "tunnel 1 500"; tail -n +3 "$repo/data/track_light/TR1.txt") > "$1"; }
# each part writes only its own files: the three run side by side in one WORK
case $part in all | A) mkdir -p "$work/ua" && printf 'profile=restomod\n' > "$work/ua/port.cfg" ;; esac
case $part in all | B)
    mkdir -p "$work/ub/mods/twk/track_light" "$work/ub/editions"
    printf 'profile=restomod\n' > "$work/ub/port.cfg"
    tunnel_file "$work/ub/mods/twk/track_light/TR1.txt"
    printf 'name=Test\ncars=rx7\ntracks=city\ntweaks=twk\n' > "$work/ub/editions/ed.ini"
    printf 'edition=ed\n' > "$work/ub/restomod.cfg" ;;
esac
case $part in all | C)
    mkdir -p "$work/uc" "$work/cli/track_light"
    printf 'profile=restomod\n' > "$work/uc/port.cfg"
    tunnel_file "$work/cli/track_light/TR1.txt" ;;
esac

# race NAME USER [args]: the race, its output in $work/NAME.out; the trace lines that came out whole (the log's own lines may cut into one)
race() {
    name=$1 user=$2
    shift 2
    TNFS_WEATHER=rain TNFS_RAIN_GRIP=1 TNFS_BANDS=0 TNFS_VERBOSE=1 TNFS_AUTOPILOT=1 TNFS_LAPS=1 TNFS_CLOCK_RATE=4 TNFS_TRACE_PLAYER=30 TNFS_INPUT_SETTLE=300 \
        TNFS_INPUT_SCRIPT="0:WAIT:mark:results=circuit:240,300:ENTER,300:ENTER,0:WAIT:mark:results=best_totals:120,300:ENTER" \
        timeout -k 5 400 "$tnfs" --data "$orig" --user "$user" "$@" --drive TR1 --type 0 > "$work/$name.out" 2>&1
    rc=$?
    [ "$rc" -eq 0 ] || fail "race $name ended with status $rc: $(tail -3 "$work/$name.out")"
    sed -n 's/^\(trace [0-9]* [0-9]*: node [0-9]* x [-0-9]* y [-0-9]* z [-0-9]* yaw [-0-9]* speed [-0-9]*\)$/\1/p' "$work/$name.out" > "$work/$name.trace"
    grep -m1 '^finish:' "$work/$name.out" > "$work/$name.finish"
    [ -s "$work/$name.trace" ] && [ -s "$work/$name.finish" ] || fail "race $name has no trace or no finish"
}

case $part in all | A) race A "$work/ua" ;; esac
case $part in all | B) race B "$work/ub" ;; esac
case $part in all | C) race C "$work/uc" --mods "$work/cli" ;; esac
case $part in A | B | C) [ "$failures" = 0 ] || exit 1; echo "ok (part $part)"; exit 0 ;; esac

grep -q "mods: twk: track_light/TR1.txt is left out: its tunnel or snow lines change the race" "$work/ub/spirited-drive.log" || fail "B: the log does not say the tweak's track_light file is left out: $(grep 'mods:' "$work/ub/spirited-drive.log")"
cmp -s "$work/A.trace" "$work/B.trace" || fail "B: the race with the tweak is not A's, tick for tick: $(diff "$work/A.trace" "$work/B.trace" | head -3)"
cmp -s "$work/A.finish" "$work/B.finish" || fail "B: the finish is not A's: $(cat "$work/A.finish") and $(cat "$work/B.finish")"
[ -s "$work/ub/race_records.dat" ] || fail "B: the race with the tweak kept no records"
grep -q "keeps no records" "$work/ub/spirited-drive.log" && fail "B: the race with the tweak is one that keeps no records"
cmp -s "$work/A.trace" "$work/C.trace" && fail "C: the tunnel file served as --mods did not change the race: the comparison of A and B shows nothing"

[ "$failures" = 0 ] || exit 1
echo ok
