#!/bin/sh
# The sun's cast shadow in the game (sun_shadow, terrain_light.h): AL1's light at dawn, hashed (TNFS_LIGHT_FINGERPRINT), with TNFS_SUN_SHADOW=1 in a clear race differs from the one without it; the same
# in the rain, and at night (the moon casts none), is the one without it. TNFS_LIGHT_ROWS_DUMP (a dump of the made rows, drive_light.c) shows the shadow keeps off a tunnel node's rows and changes an open node's.
# usage: tests/sun_shadow_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_sun.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
hash_of() {   # sun weather time
  rm -f "$work/fp"
  TNFS_SUN_SHADOW=$1 TNFS_WEATHER=$2 TNFS_LIGHT_TIME=$3 TNFS_LIGHT_FINGERPRINT="$work/fp" TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive AL1 --node 30 --screenshot "$work/s.ppm" > /dev/null 2>&1
  cut -d' ' -f2 "$work/fp" 2> /dev/null
}
off=$(hash_of 0 clear dawn); on=$(hash_of 1 clear dawn); rain=$(hash_of 1 rain dawn)
night_off=$(hash_of 0 clear night); night_on=$(hash_of 1 clear night)
row_at() {   # sun node: the rows the game made for AL1's dawn at a node (TNFS_LIGHT_ROWS_DUMP): its 11 points, then its tunnel mark
  rm -f "$work/rows$1"
  TNFS_SUN_SHADOW=$1 TNFS_WEATHER=clear TNFS_LIGHT_TIME=dawn TNFS_LIGHT_ROWS_DUMP="$work/rows$1" TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
    timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive AL1 --node 30 --screenshot "$work/s.ppm" > /dev/null 2>&1
  awk -v n="$2" '$1 == "node" && $2 == n { $1 = ""; $2 = ""; print }' "$work/rows$1" 2> /dev/null
}
status=0
t_off=$(row_at 0 1600); t_on=$(row_at 1 1600); o_off=$(row_at 0 700); o_on=$(row_at 1 700)
[ -n "$t_off" ] || { echo "FAIL: no dumped rows"; status=1; }
[ "${t_off##* }" = 1 ] || { echo "FAIL: AL1 node 1600 is not marked a tunnel"; status=1; }
[ "$t_on" = "$t_off" ] || { echo "FAIL: the sun's shadow changed a tunnel node's rows"; status=1; }
[ "$o_on" != "$o_off" ] || { echo "FAIL: the sun's shadow changed no open node's rows"; status=1; }
[ -n "$off" ] || { echo "FAIL: no fingerprint"; status=1; }
[ "$on" != "$off" ] || { echo "FAIL: the sun's shadow changed nothing in a clear dawn"; status=1; }
[ "$rain" = "$off" ] || { echo "FAIL: the rain's light is not the one without shadow"; status=1; }
[ "$night_on" = "$night_off" ] || { echo "FAIL: the night's light changed"; status=1; }
[ $status -eq 0 ] && echo "sun_shadow_game: ok"
find "$work" -delete
exit $status
