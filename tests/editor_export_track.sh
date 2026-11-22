#!/bin/sh
# Export original track, headless: File > Export original track with a workspace makes <workspace>/tracks/<id> as a track project that follows a CD circuit's road, opens it (its road is
# built), refuses a second track with the same id and a track too long for the editor, and refuses when the game's files
# are not there, leaving nothing behind. The window is drawn. Needs the player's game files (original/): skipped without them.
# usage: editor_export_track.sh EDITOR REPO
. "$(dirname "$0")/editor_headless_common.sh"
repo=$2
[ -f "$repo/original/SIMDATA/MISC/TR1.TRI" ] || { echo "skip: no original/"; exit 0; }
ws="$work/ws"
mkdir -p "$ws"
run() { "$editor" --workspace "$ws" --user "$work/u" --cd "${CD:-$repo/original}" --port-data "$repo/data" "$@"; }

# TR1 (index 9 in the game's order is not assumed: the step takes the Nth original track; find TR1's)
n=0
for s in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7 ; do
    [ "$s" = TR2 ] && tr2=$n
    [ "$s" = TR1 ] && tr1=$n
    [ "$s" = AL1 ] && al1=$n
    n=$((n + 1))
done
out=$(run --frames 12 --steps export-track:"$tr2",title 2>&1) || fail "Export original track: $out"
echo "$out" | grep -q '^title: .*road-tr2' || fail "the exported track is not the one open: $out"
for line in id=road-tr2 'name=Road TR2'; do
    grep -qx "$line" "$ws/tracks/road-tr2/project.ini" 2>/dev/null || fail "project.ini lacks $line"
done
[ -s "$ws/tracks/road-tr2/trace.json" ] || fail "the track has no trace"
[ -e "$ws/tracks/road-tr2.making" ] && fail "Export left its scratch folder: $(ls "$ws/tracks")"

# every circuit builds, in a workspace of its own: no red node, a zip (TR7 has a drop of 64 %: its road needs the points closer than the others')
mkdir -p "$work/ws2/tracks"
for s in $(seq 9 15); do
    out=$("$editor" --workspace "$work/ws2" --user "$work/u" --cd "$repo/original" --port-data "$repo/data" --frames 12 --steps export-track:"$s",problems,build 2>&1) || fail "Export original track $s: $out"
    echo "$out" | grep -q '^build: built ' || fail "the road exported from track $s does not build: $out"
done

# the same name again: refused in words, the first untouched
before=$(cksum < "$ws/tracks/road-tr2/trace.json")
out=$(run --frames 10 --steps export-track:"$tr2" 2>&1) && fail "a second track with the same id was made: $out"
echo "$out" | grep -q 'already' || fail "no word why the second track was refused: $out"
[ "$(cksum < "$ws/tracks/road-tr2/trace.json")" = "$before" ] || fail "the refused track changed the first"

# a long track (Alpine 1): refused in words, nothing left
out=$(run --frames 10 --steps export-track:"$al1" 2>&1) && fail "AL1 was exported: $out"
echo "$out" | grep -q 'segments' || fail "no word why AL1 was refused: $out"
[ -e "$ws/tracks/road-al1" ] || [ -e "$ws/tracks/road-al1.making" ] && fail "a refused track left a folder: $(ls "$ws/tracks")"

# the exported road is the one Studio opens as a track: its summary line is there
out=$(run --frames 12 "$ws/tracks/road-tr2" --steps print-road 2>&1) || fail "the exported track does not open: $out"

# no game files: refused in words
mkdir "$work/emptycd"
out=$(CD="$work/emptycd" run --frames 10 --steps export-track:"$tr1" 2>&1) && fail "a track was made with no game files: $out"
[ "$(ls "$ws/tracks" | wc -l | tr -d ' ')" = 1 ] || fail "a refused track left a folder: $(ls "$ws/tracks")"

# the window
run --frames 6 --steps export-open --shot "$work/dialog.png" > "$work/log" 2>&1 || fail "the Export original track window did not draw: $(cat "$work/log")"
[ -s "$work/dialog.png" ] || fail "no picture of the Export original track window"

[ "$failures" = 0 ] || exit 1
