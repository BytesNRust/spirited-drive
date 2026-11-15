#!/bin/bash
# A replay plays with the conditions it was recorded with, not the settings': the CD's demos (no record) as the original plays, day and clear; a file with the record, its night and rain.
# Each case is its own ctest test (they run in parallel): `cd` the CD's replay under night and rain settings, `recorded` the same replay
# with a night and rain record under day and clear settings. The conditions are logged as the replay starts, so each run ends it with
# Esc once it races, and TNFS_REPLAY_EXIT=1 ends the process there instead of holding the last picture: a clean exit, not the timeout.
# usage: replay_conditions.sh TNFS REPO WORK cd|recorded [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 CASE=$4 DATA=${5:-$2/original}
RPL=$DATA/GAMEDATA/REPLAY/REPLAY1.RPL
[ -f "$RPL" ] || { echo "skip (no $RPL)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT   # its run dir goes with it; kept only on a failure with TNFS_KEEP_RUN=1
mkdir -p "$WORK/mods" "$WORK/user"
fail() { echo "FAIL: $1"; exit 1; }

play() {   # the replay file, the port.cfg's time and weather: the log's conditions line after the replay is left with Esc
    printf 'profile=restomod\nlight_effects=1\nlight_time=%s\nweather=%s\n' "$2" "$3" > "$WORK/user/port.cfg"
    env TNFS_REPLAY_EXIT=1 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT="0:WAIT:mark:spray_v4=1:40,1000:ESC" \
        SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout -k 5 50 "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --replay "$1" > "$WORK/run.out" 2>&1
    rc=$?
    [ "$rc" -eq 0 ] || fail "the replay did not end by itself after Esc (status $rc): $(tail -3 "$WORK/run.out")"
    grep -a "replay conditions" "$WORK/user/spirited-drive.log" | tail -1
}

case $CASE in
cd)   # the CD's replay, the settings at night and rain: it plays day and clear, as the original
    got=$(play "$RPL" night rain) || { echo "$got"; exit 1; }
    [ "$got" = "replay conditions (the original's): time 0, weather 0, reverse 0" ] || fail "a CD replay under night and rain settings: '$got'"
    ;;
recorded)   # the same replay with the record of a race at night (3) in rain (2), the settings day and clear: it plays night and rain
    python3 - "$RPL" "$WORK/night.rpl" <<'P'
import sys
d = open(sys.argv[1], "rb").read()
d += b"TNFSPNT1" + bytes([0xff] * 8)
d += b"TNFSSNW1" + bytes(8)
d += b"TNFSCND1" + bytes([3, 2, 0]) + bytes(5)
open(sys.argv[2], "wb").write(d)
P
    got=$(play "$WORK/night.rpl" day clear) || { echo "$got"; exit 1; }
    [ "$got" = "replay conditions (recorded): time 3, weather 2, reverse 0" ] || fail "a recorded night and rain replay under day and clear settings: '$got'"
    ;;
*) fail "no case $CASE (cd or recorded)" ;;
esac
echo PASS
