#!/bin/sh
# The sun's cast shadow in the game (sun_shadow, terrain_light.h): AL1's light at dawn, hashed (TNFS_LIGHT_FINGERPRINT), with TNFS_SUN_SHADOW=1 in a clear race differs from the one without it; the same
# in the rain, and at night (the moon casts none), is the one without it.
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
status=0
[ -n "$off" ] || { echo "FAIL: no fingerprint"; status=1; }
[ "$on" != "$off" ] || { echo "FAIL: the sun's shadow changed nothing in a clear dawn"; status=1; }
[ "$rain" = "$off" ] || { echo "FAIL: the rain's light is not the one without shadow"; status=1; }
[ "$night_on" = "$night_off" ] || { echo "FAIL: the night's light changed"; status=1; }
[ $status -eq 0 ] && echo "sun_shadow_game: ok"
find "$work" -delete
exit $status
