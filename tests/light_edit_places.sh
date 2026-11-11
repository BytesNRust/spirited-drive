#!/bin/bash
# A race with the light editor on, its PLACES file saved mid-race (a `tunnel` line added): the light and the dry zones are made again and the race survives. usage: light_edit_reload.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT   # its run dir goes with it; kept only on a failure with TNFS_KEEP_RUN=1
mkdir -p "$WORK/mods/track_light" "$WORK/mods/track" "$WORK/user"
cp "$REPO/data/track_light/CY1B.txt" "$WORK/mods/track_light/"
cp "$REPO/data/track/CY1.txt" "$WORK/mods/track/"
printf 'profile=restomod\nlight_effects=1\n' > "$WORK/user/port.cfg"
LOG="$WORK/user/tnfs.log"

env TNFS_LIGHT_EDIT=1 TNFS_FRAMES=100000 TNFS_LIGHT_TIME=day TNFS_WEATHER=clear TNFS_VIEW=1 \
    SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
    "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" \
    --drive CY1 --type 1 --car 2 --node 70 > "$WORK/run.out" 2>&1 &
GAME=$!

wait_log() {   # the text, the seconds to wait for it
    for _ in $(seq 1 "$2"); do
        grep -a -q "$1" "$LOG" 2>/dev/null && return 0
        kill -0 "$GAME" 2>/dev/null || return 1
        sleep 1
    done
    return 1
}
finish() { kill -9 "$GAME" 2>/dev/null; wait "$GAME" 2>/dev/null; echo "FAIL: $1"; tail -5 "$LOG" 2>/dev/null; exit 1; }

wait_log "track light: .*CY1B.txt" 60 || finish "the race did not start with the file"
# The editor takes the file's checksum at its first poll, one a second of the game's frames; a slow machine (ASan under load) can poll after the edit, and
# then the edit is the file it knows. So the edit is made, and a comment line is added to the file each second until the editor sees a change.
echo "tunnel 300 340" >> "$WORK/mods/track/CY1.txt"
for n in $(seq 1 45); do
    wait_log "light made again" 1 && break
    kill -0 "$GAME" 2>/dev/null || finish "the game died reading the places file"
    echo "# touched $n" >> "$WORK/mods/track/CY1.txt"
done
grep -a -q "light made again" "$LOG" 2>/dev/null || finish "the places file was not read again"
wait_log "wet zones: CY1 made again" 20 || finish "the dry zones were not made again"
sleep 4   # the message is on screen for 3 s: the frames that draw it
kill -0 "$GAME" 2>/dev/null || finish "the game died after the reload"
kill -9 "$GAME" 2>/dev/null
wait "$GAME" 2>/dev/null
echo PASS
