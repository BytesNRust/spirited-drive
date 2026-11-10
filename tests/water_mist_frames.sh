#!/bin/bash
# water_mist_frames: on (2) by default in Restomod (unset is the frame of 2); 0 draws the rain spray as it was; 1, 2 and 3 are looks of their own (the veil, the veil with droplet streaks, the cooler flatter veil), so the spray behind a car on the wet road changes with each.
# usage: water_mist_frames.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
[ -d "$DATA/GAMEDATA" ] || { echo "skip (no $DATA)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
mkdir -p "$WORK/mods" "$WORK/user"
printf 'profile=restomod\npsx_light=1\n' > "$WORK/user/port.cfg"

shot() {   # the setting's value ("" = unset), the output name
    local e=(); [ -n "$1" ] && e=(TNFS_WATER_MIST=$1)
    env ${e[@]+"${e[@]}"} TNFS_SPRAY_V4=0 TNFS_WEATHER=rain TNFS_PSX_TIME=day TNFS_AUTOPILOT=1 TNFS_VIEW=12 TNFS_FRAMES=40 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout -k 5 200 "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --drive CY1 --type 0 --car 3 --node 1200 --screenshot "$WORK/$2.ppm" > "$WORK/$2.log" 2>&1
    [ -s "$WORK/$2.ppm" ] || { echo "FAIL: no frame for '$1'"; exit 1; }
}
fail() { echo "FAIL: $1"; exit 1; }

shot "" unset
for v in 0 1 2 3; do shot $v v$v; done
cmp -s "$WORK/unset.ppm" "$WORK/v2.ppm" || fail "Restomod does not default the setting to 2"
for v in 1 2 3; do cmp -s "$WORK/v0.ppm" "$WORK/v$v.ppm" && fail "the setting at $v does not change the rain spray"; done
cmp -s "$WORK/v1.ppm" "$WORK/v2.ppm" && fail "2 draws as 1 (no droplet streaks)"
cmp -s "$WORK/v1.ppm" "$WORK/v3.ppm" && fail "3 draws as 1"
echo PASS
