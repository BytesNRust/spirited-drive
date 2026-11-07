#!/bin/sh
# Writes the track light files from the light the port makes: data/track_light/<TRACK><A|B|C|N>.txt, the time's light at dawn (A), day (B), dusk (C) and night (N), with Restomod '26's settings and
# no PS1 disc (the made light, game/viewer/drive_light.c make_light_code; track_light_file.h has the format), and data/track/<TRACK>.txt, the track's places (its tunnels, canyon, car_dim), the same at
# every time of day. The game reads these files instead of making the light when the port's flags are the ones they were made with. The PS1 disc's own light is never written: it is EA's and Sony's data.
#
# The export writes only the files that are missing: a file that exists is kept ("kept <file> (exists; FORCE=1 to overwrite)"), since it may hold hand edits. FORCE=1 rewrites them all and loses those
# edits. A mod's copy of a file goes first all the same.
#
# usage: [FORCE=1] tools/export_track_light.sh <tnfs binary> <original/ dir> [data dir, default data]
# Takes a few minutes: one headless run a track and time. TRACKS="CY1 TR3" restricts the tracks.
set -eu
bin=$1
original=$2
data=${3:-$(dirname "$0")/../data}
tracks=${TRACKS:-"AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR6 TR7"}
mkdir -p "$data/track_light" "$data/track"
new=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_light_new.XXXXXX")
mods=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_light_mods.XXXXXX")
mkdir -p "$mods/user"
printf 'profile=restomod\npsx_dir=\n' > "$mods/user/port.cfg"
for track in $tracks; do
  for time in dawn day dusk night; do
    TNFS_EXPORT_TRACK_LIGHT="$new" TNFS_TRACK_LIGHT_FILE=0 TNFS_PSX_TIME=$time TNFS_AUTOPILOT=1 TNFS_FRAMES=1 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy \
      timeout -k 5 300 "$bin" --data "$original" --user "$mods/user" --drive "$track" --node 30 --screenshot "$mods/shot.ppm" > /dev/null 2>&1 || echo "failed: $track $time" >&2
    # the places are a fact of the track: the four times must give the same file
    if [ -f "$new/$track.places.txt" ]; then
      if [ -f "$new/$track.places.keep" ] && ! cmp -s "$new/$track.places.txt" "$new/$track.places.keep"; then echo "the places of $track differ at $time" >&2; fi
      mv "$new/$track.places.txt" "$new/$track.places.keep"
    fi
  done
done
find "$mods" -delete
sh "$(dirname "$0")/export_track_light_install.sh" "$new" "$data"   # keeps the files that exist unless FORCE=1
find "$new" -delete
ls "$data/track_light"/*.txt "$data/track"/*.txt | wc -l
