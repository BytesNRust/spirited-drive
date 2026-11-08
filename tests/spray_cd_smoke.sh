#!/bin/bash
# spray_cd_smoke: 0 draws the spray as it was (the same frame with the setting unset and set to 0); 1 draws the CD's smoke sprite, so the rain spray behind a car on the wet road changes.
# usage: spray_cd_smoke.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
[ -d "$DATA/GAMEDATA" ] || { echo "skip (no $DATA)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
mkdir -p "$WORK/mods" "$WORK/user"
printf 'profile=restomod\npsx_light=1\n' > "$WORK/user/port.cfg"

shot() {   # the setting's value ("" = unset), the output name
    local e=(); [ -n "$1" ] && e=(TNFS_SPRAY_CD_SMOKE=$1)
    env ${e[@]+"${e[@]}"} TNFS_WEATHER=rain TNFS_PSX_TIME=day TNFS_VIEW=2 TNFS_START_NODE=100 TNFS_FRAMES=200 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout -k 5 200 "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --drive TR1 --type 0 --car 2 --node 360 --screenshot "$WORK/$2.ppm" > "$WORK/$2.log" 2>&1
    [ -s "$WORK/$2.ppm" ] || { echo "FAIL: no frame for '$1'"; exit 1; }
}
fail() { echo "FAIL: $1"; exit 1; }

shot "" unset
shot 0 off
shot 1 on
cmp -s "$WORK/unset.ppm" "$WORK/off.ppm" || fail "the setting at 0 changes the frame"
cmp -s "$WORK/off.ppm" "$WORK/on.ppm" && fail "the setting at 1 does not change the rain spray"
echo PASS
