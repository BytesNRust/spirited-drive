#!/bin/bash
# wipers: on by default in Restomod (unset draws the frame of 1; in Restored unset is 0); 1 in rain draws the sweeping blades and hides the art's own, so the frame changes; 1 in the dry
# is the frame as it was (the blades parked in the art, nothing drawn).
# (the 640x480 path too)
# usage: wipers.sh TNFS REPO WORK [DATA]
set -u
TNFS=$1 REPO=$2 WORK=$3 DATA=${4:-$2/original}
[ -d "$DATA/GAMEDATA" ] || { echo "skip (no $DATA)"; exit 0; }
mkdir -p "$WORK"
WORK=$(mktemp -d "$WORK/run.XXXXXX")
mkdir -p "$WORK/mods" "$WORK/user"
UW=$WORK/user
printf 'profile=restomod\npsx_light=1\n' > "$WORK/user/port.cfg"

shot() {   # the setting's value ("" = unset), the weather, the name
    local e=(); [ -n "$1" ] && e=(TNFS_WIPERS=$1)
    (cd "$REPO" && env ${e[@]+"${e[@]}"} TNFS_WEATHER=$2 TNFS_PSX_TIME=day TNFS_VIEW=0 TNFS_FRAMES=20 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
        timeout -k 5 200 "$TNFS" --data "$DATA" --user "$UW" --mods "$WORK/mods" --drive TR1 --type 0 --car 2 --node 30 --screenshot "$WORK/$3.ppm" > "$WORK/$3.log" 2>&1)
    [ -s "$WORK/$3.ppm" ] || { echo "FAIL: no frame for '$1' in $2"; exit 1; }
}
fail() { echo "FAIL: $1"; exit 1; }

shot "" rain rain_unset
shot 0 rain rain_off
shot 1 rain rain_on
shot 0 clear dry_off
shot 1 clear dry_on
cmp -s "$WORK/rain_unset.ppm" "$WORK/rain_on.ppm" || fail "Restomod does not default the setting on"
cmp -s "$WORK/rain_off.ppm" "$WORK/rain_on.ppm" && fail "the setting at 1 changes nothing in rain"
cmp -s "$WORK/dry_off.ppm" "$WORK/dry_on.ppm" || fail "the setting at 1 changes the frame in the dry"
# the 640x480 path (the exe's: the restored profile, no render scale): the blades sweep there too
mkdir -p "$WORK/user_narrow"
printf 'profile=restored\n' > "$WORK/user_narrow/port.cfg"
UW=$WORK/user
USER_SAVE=$UW; UW=$WORK/user_narrow
shot "" rain n_unset; shot 0 rain n_off; shot 1 rain n_on
UW=$USER_SAVE
cmp -s "$WORK/n_unset.ppm" "$WORK/n_off.ppm" || fail "640x480: the setting at 0 changes the frame"
cmp -s "$WORK/n_off.ppm" "$WORK/n_on.ppm" && fail "640x480: the setting at 1 changes nothing in rain"
echo PASS
