#!/bin/bash
# A race with the light editor on, its light file saved mid-race: the race must survive the reload and its
# on-screen message. usage: light_edit_reload.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT   # its run dir goes with it; kept only on a failure with TNFS_KEEP_RUN=1
mkdir -p "$WORK/mods/track_light" "$WORK/mods/track" "$WORK/user"
cp "$REPO/data/track_light/CY1B.txt" "$WORK/mods/track_light/"
cp "$REPO/data/track/CY1.txt" "$WORK/mods/track/"
printf 'profile=restomod\npsx_light=1\n' > "$WORK/user/port.cfg"
LOG="$WORK/user/tnfs.log"

env TNFS_LIGHT_EDIT=1 TNFS_FRAMES=100000 TNFS_PSX_TIME=day TNFS_WEATHER=clear TNFS_VIEW=1 \
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
sleep 3   # the editor has taken the file's checksum by now
echo "# saved mid-race" >> "$WORK/mods/track_light/CY1B.txt"
wait_log "light made again" 15 || finish "the file was not read again, or the game died reading it"
sleep 4   # the message is on screen for 3 s: the frames that draw it
# the drawn track must follow the file too, not only the overlay: the rows round the start made dark, saved, the built track's light changes
wait_log "the track built again with the new light" 15 || finish "the first reload did not build the track again"
sum_of() { grep -a "the track built again with the new light" "$LOG" | tail -1 | sed 's/.*light sum \([0-9]*\).*/\1/'; }
BEFORE=$(sum_of)
awk '$1=="rows" && $2>=0 && $2<=400 && $2==$3 { printf "rows %s %s", $2, $3; for (i=4;i<=NF;i++) printf " 100"; print ""; next } { print }' \
    "$WORK/mods/track_light/CY1B.txt" > "$WORK/dark.txt" && cp "$WORK/dark.txt" "$WORK/mods/track_light/CY1B.txt"
for _ in $(seq 1 15); do
    [ "$(grep -a -c "the track built again with the new light" "$LOG")" -ge 2 ] && break
    sleep 1
done
AFTER=$(sum_of)
[ -n "$BEFORE" ] && [ -n "$AFTER" ] && [ "$AFTER" -lt "$BEFORE" ] || finish "the drawn track kept its light (sum $BEFORE -> $AFTER)"
kill -0 "$GAME" 2>/dev/null || finish "the game died after the reload"
kill -9 "$GAME" 2>/dev/null
wait "$GAME" 2>/dev/null
echo PASS
