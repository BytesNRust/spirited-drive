#!/bin/sh
# No car is drawn black on any track with a light file (the ratio a car's table is made from, TNFS_CAR_RATIO_DUMP in view_light.c): every track of data/track_light by day, dawn and dusk, sun_shadow on
# (the floor of a shadow is 0.55 of full), must give every car a ratio of at least MIN in one channel (a warm cast dims the blue, not the car) (a ratio left at 0 made the cars black in full sun, car_lit_game.sh). Tunnel nodes at dusk dim a car to about 0.3, so the floor is 0.15.
# TRACKS="CY1 TR3" (environment) runs those tracks only: ctest runs four groups side by side (48 runs in all, 12 each; the groups are in CMakeLists.txt, checked by track_groups); with none, every file.
# usage: tests/car_lit_all_game.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_carlitall.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
status=0
checked=0
tracks=${TRACKS:-$(for file in data/track_light/*.txt; do basename "$file" .txt; done)}
for track in $tracks; do
  [ -f "data/track_light/$track.txt" ] || { echo "FAIL: no data/track_light/$track.txt"; status=1; continue; }
  min=0.15   # tunnels and dusk dim a car to ~0.3; a ratio left at 0 is the failure
  for time in day dawn dusk; do
    rm -f "$work/ratios"
    TNFS_SUN_SHADOW=1 TNFS_WEATHER=clear TNFS_LIGHT_TIME=$time TNFS_CAR_RATIO_DUMP="$work/ratios" TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" \
      timeout -k 5 300 "$bin" --data "$original" --user "$work/user" --drive "$track" --node 30 --screenshot "$work/s.ppm" > /dev/null 2>&1
    if [ ! -s "$work/ratios" ]; then echo "FAIL: $track $time: no car ratios dumped"; status=1; continue; fi
    checked=$((checked + 1))
    bad=$(awk -v m="$min" '{ if ($2 < m && $3 < m && $4 < m) n++ } END { print n + 0 }' "$work/ratios")
    [ "$bad" -eq 0 ] || { echo "FAIL: $track $time: $bad car draws with every channel's ratio under $min (black cars)"; status=1; }
  done
done
[ $status -eq 0 ] && echo "car_lit_all_game: ok ($checked runs)"
find "$work" -delete
exit $status
