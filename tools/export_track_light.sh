#!/bin/sh
# Writes data/track_light/<TRACK><A|B|C|N>.txt: the light the port makes for each track at dawn (A), day (B), dusk (C) and night (N), with Restomod '26's settings and no PS1 disc
# (the made light, game/viewer/drive_light.c make_light_code; track_light_file.h has the format). The game reads these files instead of making the light when the port's flags are
# the ones they were made with. The PS1 disc's own light is never written: it is EA's and Sony's data.
#
# usage: tools/export_track_light.sh <tnfs binary> <original/ dir> [output dir, default data/track_light]
# Takes a few minutes: one headless run a track and time. TRACKS="CY1 TR3" restricts the tracks.
set -eu
bin=$1
original=$2
out=${3:-$(dirname "$0")/../data/track_light}
tracks=${TRACKS:-"AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR6 TR7"}
mkdir -p "$out"
mods=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_light_mods.XXXXXX")
mkdir -p "$mods/user"
printf 'profile=restomod\npsx_dir=\n' > "$mods/user/port.cfg"
for track in $tracks; do
  for time in dawn day dusk night; do
    TNFS_EXPORT_TRACK_LIGHT="$out" TNFS_TRACK_LIGHT_FILE=0 TNFS_PSX_TIME=$time TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
      timeout -k 5 300 "$bin" --data "$original" --mods "$mods" --drive "$track" --node 30 --screenshot "$mods/shot.ppm" > /dev/null 2>&1 || echo "failed: $track $time" >&2
  done
done
find "$mods" -delete
ls "$out" | wc -l
