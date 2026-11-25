#!/bin/sh
# The Track light test drive plays with the player's own setup: the scratch folder is made as the Studio makes it (test_editor_light_drive --scratch <player> <scratch>) from a player folder whose config.dat
# says manual gearbox (the words the menus' race takes: 0x97 is 0 for automatic), whose restomod.cfg says speeds in km/h and whose controls.cfg is the owner's pad (gear up X, gear down A, camera B,
# handbrake Y); the real game's race (the Studio's own command: --drive TR1 --type 0, no frame) logs what it took: gearbox manual, speeds in km/h, and the controls it reads are the player's file.
# A player folder with none of it drives automatic, in mph. The player's files are byte for byte as they were. Exit 77 (skipped) without the CD's data.
# usage: tests/light_drive_setup_game.sh <tnfs binary> <original/ dir> <test_editor_light_drive binary>      (run from the repo's root)
set -u
bin=$1
original=$2
maker=$3
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_ldsetup.XXXXXX")
mkdir -p "$work/own" "$work/none" "$work/user_own" "$work/user_none"
python3 -I -c "
b = bytearray(24402)
b[0x97] = 1
open('$work/own/config.dat', 'wb').write(b)"
printf 'profile=restomod\npsx_dir=\n' > "$work/own/port.cfg"
printf 'units=kmh\n' > "$work/own/restomod.cfg"
printf 'steer=\nsteer_left=axis0-\nsteer_right=axis0+\ngas=axis5+\nbrake=axis4+\ngear_up=button2\ngear_down=button0\ncamera=button1\nhandbrake=button3\nhorn=hat0.4\npause=button7\nnitro=button5\nlook_back=button4\n' > "$work/own/controls.cfg"
printf 'profile=restomod\npsx_dir=\n' > "$work/none/port.cfg"
for f in config.dat port.cfg restomod.cfg controls.cfg; do cp "$work/own/$f" "$work/before.$f"; done
status=0
fail() { echo "FAIL: $1"; status=1; }
race() {   # player, scratch
  "$maker" --scratch "$work/$1" "$work/$2" 2> "$work/$2.copied" || fail "the scratch of $1 was not made"
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy TNFS_VERBOSE=1 TNFS_AUTOPILOT=1 TNFS_LAPS=1 TNFS_PREF_DIR="$work/pref" \
    timeout -k 3 20 "$bin" --data "$original" --user "$work/$2" --drive TR1 --type 0 > /dev/null 2>&1
  grep -a "race: gearbox" "$work/$2/spirited-drive.log" | head -1
}
own=$(race own user_own)
none=$(race none user_none)
echo "owner's setup: $own"
echo "no setup:      $none"
case "$own" in *"gearbox manual"*"km/h"*) ;; *) fail "the drive did not take manual and km/h from the player's folder" ;; esac
case "$none" in *"gearbox automatic"*"mph"*) ;; *) fail "a player folder with no setup did not drive automatic in mph" ;; esac
grep -q "config.dat copied" "$work/user_own.copied" || fail "the log does not say config.dat was copied"
cmp -s "$work/user_own/controls.cfg" "$work/before.controls.cfg" || fail "the game's controls are not the player's pad"
for f in config.dat port.cfg restomod.cfg controls.cfg; do cmp -s "$work/own/$f" "$work/before.$f" || fail "the player's $f was written"; done
[ $status -eq 0 ] && echo "light_drive_setup_game: ok" && rm -rf "$work"
exit $status
