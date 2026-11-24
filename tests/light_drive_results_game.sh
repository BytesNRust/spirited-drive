#!/bin/sh
# The Track light test drive takes the player's own game and fixes only what it must (window, 16:9, the top render scale, the light's effects), and none of that outlives the race:
# the scratch folder is made as the Studio makes it (test_editor_light_drive --scratch <player> <scratch>) from a player folder that is 21:9, full screen, render scale 1, with a gamepad
# map; the real game runs one lap to its results and the best totals and is left. Checks: the race window is 16:9, the menus after the results are 16:9 too (never another shape), the
# gamepad's map the game reads is the player's, and the player's files are byte for byte as they were. Exit 77 (skipped) without the CD's data.
# usage: tests/light_drive_results_game.sh <tnfs binary> <original/ dir> <test_editor_light_drive binary>      (run from the repo's root)
set -u
bin=$1
original=$2
maker=$3
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_ldrive.XXXXXX")
mkdir -p "$work/player" "$work/user"
printf 'profile=restomod\n' > "$work/player/port.cfg"
printf 'format=21:9\nfullscreen=1\nrender_scale=1\nfov_adjust=5\nsun_shadow=0\n' > "$work/player/restomod.cfg"
printf 'steer_left=axis0-\nsteer_right=axis0+\nthrottle=axis5\nbrake=axis4\n' > "$work/player/controls.cfg"
cp "$work/player/port.cfg" "$work/before.port"; cp "$work/player/restomod.cfg" "$work/before.mode"; cp "$work/player/controls.cfg" "$work/before.controls"
status=0
fail() { echo "FAIL: $1"; status=1; }
"$maker" --scratch "$work/player" "$work/user" || fail "the scratch was not made"
script="0:WAIT:mark:results=circuit:240,300:ENTER,300:ENTER,0:WAIT:mark:results=best_totals:120,300:ENTER,300:ENTER"
for race in 1 2; do   # the next race too: nothing fixed is left in the scratch for it to find different
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy TNFS_VERBOSE=1 TNFS_AUTOPILOT=1 TNFS_LAPS=1 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script TNFS_PREF_DIR="$work/pref" \
        timeout -k 5 280 "$bin" --data "$original" --user "$work/user" --drive TR1 --type 0 > "$work/out$race" 2> "$work/err$race"
    [ $? -eq 0 ] || fail "race $race ended with a status"
    log="$work/user/spirited-drive.log"
    grep -q "mark:results=best_totals" "$log" || fail "race $race did not reach the results"
    # the windows: created at the scratch's 16:9, the race's frame 16:9 (render scale 3), the menus after it 16:9 again
    sizes=$(grep -E "window (created|reconfigured)" "$log" | sed -n 's/.* for \([0-9]*\)x\([0-9]*\)$/\1 \2/p')
    [ -n "$sizes" ] || fail "race $race logged no window sizes"
    echo "$sizes" | while read -r w h; do
        d=$((w * 9 - h * 16))   # 854 x 480 and 2562 x 1440 are 16:9 to a few pixels
        [ "$d" -ge -32 ] && [ "$d" -le 32 ] || { echo "FAIL: race $race: a frame of $w x $h is not 16:9"; exit 1; }
    done || status=1
    cp "$log" "$work/log$race"; rm -f "$log"
done
grep -q '^format=16:9$' "$work/user/restomod.cfg" || fail "the scratch lost its 16:9"
grep -q '^format=21:9' "$work/user/restomod.cfg" && fail "the player's 21:9 reached the drive"
cmp -s "$work/player/port.cfg" "$work/before.port" && cmp -s "$work/player/restomod.cfg" "$work/before.mode" && cmp -s "$work/player/controls.cfg" "$work/before.controls" || fail "a file of the player's was written"
cmp -s "$work/user/controls.cfg" "$work/before.controls" || fail "the game's controls are not the player's"
[ $status -eq 0 ] && echo "light_drive_results_game: ok" && rm -rf "$work"
exit $status
