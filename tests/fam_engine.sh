#!/bin/sh
# A stage's art built from PNG textures (tnfs-studio fam build) is loaded by the game: a track piece of the CD's AL1 layout gets the
# built FAM in both of its art folders, and the game drives it headless. `--mods` does not serve a loose FAM to --track (a changed
# track is a piece: docs/modding.md), hence the piece. Not in ctest: the spec's textures are the owner's files, outside the repo.
# usage: fam_engine.sh TNFS STUDIO ORIGINAL SPEC SHOT.ppm
# Every run is headless and silent, with its player folder in the scratch directory (--user), never the real one.
set -eu
tnfs=$1
studio=$2
orig=$3
spec=$4
shot=$5
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_RENDER_DRIVER=software

printf 'section 24\nsection 40 turn=90 grade=3 bank=6 width=50\nsection 30 turn=-120 grade=-3 bank=-6\nsection 30 bank=0 grade=0 width=40\n' > "$work/loop.trk"
mkdir -p "$work/user"
echo "profile=restomod" > "$work/user/port.cfg"
"$studio" track new "$work/loop.trk" --template AL1 --piece --id walk-al1 --stem MYSTAGE --out "$work/piece" --data "$orig" >/dev/null
"$studio" fam build "$spec" --out "$work/stage.FAM"
cp "$work/stage.FAM" "$work/piece/SIMDATA/ETRACKFM/MYSTAGE1_001.FAM"
cp "$work/stage.FAM" "$work/piece/SIMDATA/GTRACKFM/MYSTAGE1_001.FAM"
timeout -k 10 120 "$tnfs" --data "$orig" --user "$work/user" --mods "$work/piece" --drive walk-al1 --car 5 --racers 1 --node 120 --screenshot "$shot"
[ -s "$shot" ] || { echo "no screenshot at $shot" >&2; exit 1; }
echo "wrote $shot"
