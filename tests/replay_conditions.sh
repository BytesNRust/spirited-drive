#!/bin/bash
# A replay plays with the conditions it was recorded with, not the settings': the CD's demos (no record) as the original plays, day and clear; a file with the record, its night and rain.
# usage: replay_conditions.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
RPL=$DATA/GAMEDATA/REPLAY/REPLAY1.RPL
[ -f "$RPL" ] || { echo "skip (no $RPL)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT   # its run dir goes with it; kept only on a failure with TNFS_KEEP_RUN=1
mkdir -p "$WORK/mods" "$WORK/user"

play() {   # the replay file, the port.cfg's time and weather: the log after a short headless run
    mkdir -p "$WORK/user"
    printf 'profile=restomod\nlight_effects=1\nlight_time=%s\nweather=%s\n' "$2" "$3" > "$WORK/user/port.cfg"
    rm -f "$WORK/user/tnfs.log"
    env TNFS_FRAMES=30 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout -k 5 120 "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --replay "$1" > "$WORK/run.out" 2>&1
    grep -a "replay conditions" "$WORK/user/tnfs.log" | tail -1
}
fail() { echo "FAIL: $1"; exit 1; }

# the CD's replay, the settings at night and rain: it plays day and clear, as the original
got=$(play "$RPL" night rain)
[ "$got" = "replay conditions (the original's): time 0, weather 0, reverse 0" ] || fail "a CD replay under night and rain settings: '$got'"

# the same replay with the record of a race at night (3) in rain (2), the settings day and clear: it plays night and rain
python3 - "$RPL" "$WORK/night.rpl" <<'P'
import sys
d = open(sys.argv[1], "rb").read()
d += b"TNFSPNT1" + bytes([0xff] * 8)
d += b"TNFSSNW1" + bytes(8)
d += b"TNFSCND1" + bytes([3, 2, 0]) + bytes(5)
open(sys.argv[2], "wb").write(d)
P
got=$(play "$WORK/night.rpl" day clear)
[ "$got" = "replay conditions (recorded): time 3, weather 2, reverse 0" ] || fail "a recorded night and rain replay under day and clear settings: '$got'"
echo PASS
