#!/bin/sh
# The Masks panel on an own track (dep103's plug-in: TrackArtSource), headless: a track project's art-source/masks PSDs make art-generated/track_masks PNGs, the rows carry the sizes of the
# project's own art (its GENERIC.FAM), the panel is in the track room's list, and a PSD saved again makes its PNG again. No CD data.
# usage: editor_own_track_masks.sh EDITOR REPO MAKE_TEST_PSD
. "$(dirname "$0")/editor_headless_common.sh"
make_psd=$3
ws="$work/ws"
mkdir -p "$ws"
run() { "$editor" --workspace "$ws" --user "$work/u" --port-data "$work/data" "$@"; }
mkdir -p "$work/data"

out=$(run --frames 8 --steps new-track:1,title 2>&1) || fail "New track: $out"
dir="$ws/tracks/road-1"
[ -f "$dir/project.ini" ] || fail "no track project: $(ls "$ws/tracks")"

# the masks panel is in the room's panels, and an own track with no masks says so
out=$(run --frames 6 "$dir" --steps print-panels 2>&1) || fail "print-panels: $out"
echo "$out" | grep -q 'Track masks' || fail "the track room does not list the Track masks panel: $out"
out=$(run --frames 6 "$dir" --steps masks-print 2>&1)
echo "$out" | grep -q '^masks: 0 ' || fail "a track with no masks does not say so: $out"

# two PSDs (one painted, one with lamps): the PNGs are made, sized by the project's art
mkdir -p "$dir/art-source/masks"
"$make_psd" "$dir/art-source/masks/slot_00.psd" paint
"$make_psd" "$dir/art-source/masks/slot_03.psd" lamps
out=$(run --frames 6 "$dir" --steps masks-print 2>&1) || fail "masks-print: $out"
echo "$out" | grep -q '^masks: 2 to_make 0' || fail "the masks are not made and counted: $out"
echo "$out" | grep -q '^mask slot_00: ok, 128x256' || fail "slot_00 is not ok at the art's size: $out"
echo "$out" | grep -q '^mask slot_03: ok, 128x256' || fail "slot_03 is not ok at the art's size: $out"
[ -s "$dir/art-generated/track_masks/slot_00.png" ] && [ -s "$dir/art-generated/track_masks/slot_03.png" ] || fail "the PNGs were not written: $(ls "$dir/art-generated/track_masks" 2>&1)"

# the shown mask is drawn over the project's texture
out=$(run --frames 6 "$dir" --steps mask-select:0 --shot "$work/mask.png" 2>&1) || fail "mask-select: $out"
[ -s "$work/mask.png" ] || fail "no picture of the Masks panel"

# a PSD with another mask: its PNG is made again on the next open
before=$(cksum < "$dir/art-generated/track_masks/slot_00.png")
sleep 1
"$make_psd" "$dir/art-source/masks/slot_00.psd" lamps
out=$(run --frames 6 "$dir" --steps masks-print 2>&1) || fail "masks-print after the change: $out"
[ "$(cksum < "$dir/art-generated/track_masks/slot_00.png")" != "$before" ] || fail "a PSD saved again did not make its PNG again: $out"

[ "$failures" = 0 ] || exit 1
