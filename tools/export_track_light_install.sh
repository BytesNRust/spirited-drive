#!/bin/sh
# Puts the files tools/export_track_light.sh made into the data folder: <staged>/<TRACK>.places.keep to <data>/track/<TRACK>.txt, <staged>/<TRACK><A|B|C|N>.txt to <data>/track_light/.
# A file that already exists is KEPT (it may carry hand edits: tunnels, tones, lamps, rows) and the line "kept <file> (exists; FORCE=1 to overwrite)" says so; only the missing ones are written.
# FORCE=1 overwrites them all, and an edit in them is lost.
# usage: tools/export_track_light_install.sh <staged dir> <data dir>
set -eu
staged=$1
data=$2
mkdir -p "$data/track_light" "$data/track"
put() {   # SOURCE DEST
  if [ -f "$2" ] && [ "${FORCE:-0}" != 1 ]; then echo "kept $2 (exists; FORCE=1 to overwrite)"; else cp "$1" "$2"; fi
}
for file in "$staged"/*.keep; do [ -f "$file" ] && put "$file" "$data/track/$(basename "$file" .places.keep).txt"; done
for file in "$staged"/[A-Z]*[ABCN].txt; do [ -f "$file" ] && put "$file" "$data/track_light/$(basename "$file")"; done
exit 0
