#!/bin/sh
# A track's light at one node at the four times of day, as a 2x2 contact sheet: dawn (A) and day (B) on the top row, dusk (C) and night (N) under them.
#
# usage: tools/light_photo.sh <tnfs binary> <original/ dir> TRACK NODE [OUTDIR]
#   TRACK   a CD track's stem: AL1 .. TR7 (three characters, as the light files are named)
#   NODE    the road node the car starts at (TNFS_START_NODE); the frame is a few ticks later, so the node in the picture is a little past it (the log's line is printed)
#   OUTDIR  where the pictures go (default $TMPDIR/light_photo): <TRACK>_<NODE>_sheet.png and <TRACK>_<NODE>_{A,B,C,N}.png
# environment:
#   LIGHT_DIR  a folder with track_light/ and track/ (the working copies of the files to judge): they go first, as a mod's do. Default: the repo's data/ (what the game reads).
#   TICKS      the headless run's ticks before the frame (default 4)
#   VIEW       the game's camera (TNFS_VIEW; default 0, the in-car view with the road ahead, as a menu race shows it; the harness's own default looks at the car from beside)
#   FULL=1     the sheet at full size (4 pictures of 1280x960 side by side), not halved
# The run is Restomod with the port's light on, autopilot, clear weather, a scratch player folder; it leaves nothing outside OUTDIR. Edit a file in LIGHT_DIR, run again, look.
# Not for CI: it needs the CD's data (original/) and a built game; tests/light_photo_check.py covers the sheet and the arguments, tests/light_photo_run.sh one real run.
set -u
usage() { echo "usage: $0 <tnfs binary> <original/ dir> TRACK NODE [OUTDIR]" >&2; exit 2; }
[ $# -ge 4 ] && [ $# -le 5 ] || usage
bin=$1 original=$2 track=$3 node=$4
here=$(cd "$(dirname "$0")" && pwd)
upper='[ABCDEFGHIJKLMNOPQRSTUVWXYZ]'   # not A-Z: a locale's ranges can take lower case
case $track in $upper$upper[0-9]) ;; *) echo "TRACK is a CD track's stem such as CY1 or TR7, not '$track'" >&2; exit 2 ;; esac
case $node in ''|*[!0-9]*) echo "NODE is a whole number of nodes, not '$node'" >&2; exit 2 ;; esac
[ -x "$bin" ] || { echo "no game binary at '$bin'" >&2; exit 2; }
[ -d "$original" ] || { echo "no CD data folder at '$original'" >&2; exit 2; }
out=${5:-${TMPDIR:-/tmp}/light_photo}
ticks=${TICKS:-4}
case $ticks in ''|*[!0-9]*) echo "TICKS is a whole number, not '$ticks'" >&2; exit 2 ;; esac
lights=${LIGHT_DIR:-$here/../data}
[ -d "$lights/track_light" ] || { echo "no track_light/ folder in '$lights'" >&2; exit 2; }
mkdir -p "$out" || exit 1
work=$(mktemp -d "${TMPDIR:-/tmp}/light_photo.XXXXXX") || exit 1
trap 'find "$work" -delete' EXIT
mkdir -p "$work/mods/track_light" "$work/mods/track" "$work/user"
cp "$lights/track_light/$track"?.txt "$work/mods/track_light/" 2>/dev/null || echo "note: no $track light files in $lights: the light is made in code" >&2
[ -f "$lights/track/$track.txt" ] && cp "$lights/track/$track.txt" "$work/mods/track/"
printf 'profile=restomod\nlight_effects=1\n' > "$work/user/port.cfg"
guard=
for t in timeout gtimeout; do command -v "$t" >/dev/null 2>&1 && { guard="$t -k 5 300"; break; }; done   # the game is bounded where the system has a timeout; CI never reaches here

pics=
for pair in A:dawn B:day C:dusk N:night; do
  letter=${pair%%:*} time=${pair#*:}
  ppm="$work/$letter.ppm"
  env TNFS_AUTOPILOT=1 TNFS_FRAMES=1 TNFS_WEATHER=clear TNFS_VIEW=${VIEW:-0} TNFS_LIGHT_TIME=$time TNFS_START_NODE=$node SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy \
    $guard "$bin" --data "$original" --user "$work/user" --mods "$work/mods" --drive "$track" --node "$ticks" --screenshot "$ppm" > "$work/$letter.log" 2>&1
  rc=$?
  if [ $rc -ne 0 ] || [ ! -s "$ppm" ]; then echo "the $time run of $track failed (rc $rc); its log:" >&2; tail -5 "$work/$letter.log" >&2; exit 1; fi
  echo "$time: $(grep -a '^tick' "$work/$letter.log" | tail -1 | cut -c1-110)"
  pics="$pics $ppm"
done

sheet="$out/${track}_${node}_sheet.png"
python3 -I "$here/light_sheet.py" "$sheet" $pics ${FULL:+--full} || exit 1
for letter in A B C N; do
  python3 -I - "$work/$letter.ppm" "$out/${track}_${node}_$letter.png" "$here" <<'PY' || exit 1
import sys
sys.path.insert(0, sys.argv[3])
import light_sheet
w, h, px = light_sheet.read_ppm(sys.argv[1])
light_sheet.write_png(sys.argv[2], w, h, px)
PY
done
echo "sheet: $sheet"
