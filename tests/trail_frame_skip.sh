#!/bin/bash
# trail_frame_skip: the rain spray's and the rally dust's trails have no gaps when frames are missing: with TNFS_FRAME_SKIP=3 (a frame for each third step, as a slow machine draws) the last ticks have as many live drops/puffs as with every step drawn
# (the count of the last 64 ticks that have a live drop of the followed car's rain plume, or a live puff of its dust trail: "trail ticks: rain N dust M"); the dust trail also lays whole when the car's body is not drawn (the driver's view).
# usage: trail_frame_skip.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
[ -d "$DATA/GAMEDATA" ] || { echo "skip (no $DATA)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT   # its run dir goes with it; kept only on a failure with TNFS_KEEP_RUN=1
mkdir -p "$WORK/mods" "$WORK/user"
printf 'profile=restomod\n' > "$WORK/user/port.cfg"
printf 'format=4:3\nrender_scale=1\n' > "$WORK/user/restomod.cfg"

fail() { echo "FAIL: $1"; exit 1; }
# count TRACK RALLY WEATHER VIEW SKIP KEY(rain|dust)
count() {
    local rally=(); [ "$2" = 1 ] && rally=(TNFS_RALLY=1)
    env ${rally[@]+"${rally[@]}"} TNFS_SPRAY_V4=0 TNFS_FRAME_SKIP=$5 TNFS_AUTOPILOT=1 TNFS_VIEW=$4 TNFS_WEATHER=$3 TNFS_FRAMES=60 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout -k 5 300 "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --drive "$1" --type 0 --car 3 --racers 1 --node 1000 --screenshot "$WORK/shot.ppm" 2>&1 \
        | awk -v key="$6" '/^trail ticks:/ { print (key == "rain" ? $4 : $6) }'
}

r1=$(count CY1 0 rain 12 1 rain); r3=$(count CY1 0 rain 12 3 rain)
[ -n "$r1" ] && [ -n "$r3" ] || fail "no trail line for the rain"
[ "$r1" -ge 15 ] || fail "rain: skip 1 has $r1 ticks of plume (a trail of 20 expected)"
[ "$r3" -ge $((r1 - 1)) ] || fail "rain: a frame in three has a plume of $r3 ticks against $r1 with every frame (the missed ticks are not laid)"

d1=$(count TR1 1 clear 12 1 dust); d3=$(count TR1 1 clear 12 3 dust); d0=$(count TR1 1 clear 0 3 dust)
[ -n "$d1" ] && [ -n "$d3" ] && [ -n "$d0" ] || fail "no trail line for the dust"
[ "$d1" -ge 30 ] || fail "dust: skip 1 has $d1 ticks of dust (a trail of 36 expected)"
[ "$d3" -ge $((d1 - 1)) ] || fail "dust: a frame in three has a trail of $d3 ticks against $d1"
[ "$d0" -ge $((d1 - 1)) ] || fail "dust: the driver's view (no body drawn) has a trail of $d0 ticks against $d1"
echo PASS
