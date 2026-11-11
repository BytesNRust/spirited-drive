#!/bin/bash
# spray_cd_smoke: on by default in Restomod (unset is the frame of 1); 0 draws the spray as it was; 1 draws the CD's smoke sprite, so the rain spray behind a car on the wet road changes (water_mist at 0: the veil replaces the smoke for water).
# usage: spray_cd_smoke.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
[ -d "$DATA/GAMEDATA" ] || { echo "skip (no $DATA)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
trap 'rc=$?; if [ "$rc" -ne 0 ] && [ "${TNFS_KEEP_RUN:-0}" = 1 ]; then echo "kept: $WORK" >&2; else find "$WORK" -delete; fi' EXIT   # its run dir goes with it; kept only on a failure with TNFS_KEEP_RUN=1
mkdir -p "$WORK/mods" "$WORK/user"
printf 'profile=restomod\nlight_effects=1\n' > "$WORK/user/port.cfg"

shot() {   # the setting's value ("" = unset), the output name
    local e=(); [ -n "$1" ] && e=(TNFS_SPRAY_CD_SMOKE=$1)
    env ${e[@]+"${e[@]}"} TNFS_SPRAY_V4=0 TNFS_WATER_MIST=0 TNFS_WEATHER=rain TNFS_LIGHT_TIME=day TNFS_VIEW=2 TNFS_START_NODE=100 TNFS_FRAMES=200 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout -k 5 200 "$TNFS" --data "$DATA" --user "$WORK/user" --mods "$WORK/mods" --drive TR1 --type 0 --car 2 --node 360 --screenshot "$WORK/$2.ppm" > "$WORK/$2.log" 2>&1
    [ -s "$WORK/$2.ppm" ] || { echo "FAIL: no frame for '$1'"; exit 1; }
}
fail() { echo "FAIL: $1"; exit 1; }

shot "" unset
shot 0 off
shot 1 on
cmp -s "$WORK/unset.ppm" "$WORK/on.ppm" || fail "Restomod does not default the setting on"
cmp -s "$WORK/off.ppm" "$WORK/on.ppm" && fail "the setting at 1 does not change the rain spray"
echo PASS
