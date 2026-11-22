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
echo "$out" | grep -q '^masks: 13 to_make 0' && [ "$(echo "$out" | grep -c ': no mask,')" = 13 ] || fail "a track with no masks lists its 13 textures, none with a mask: $out"

# two PSDs (one painted, one with lamps): the PNGs are made, sized by the project's art
mkdir -p "$dir/art-source/masks"
"$make_psd" "$dir/art-source/masks/slot_00.psd" paint
"$make_psd" "$dir/art-source/masks/slot_03.psd" lamps
out=$(run --frames 6 "$dir" --steps masks-print 2>&1) || fail "masks-print: $out"
echo "$out" | grep -q '^masks: 13 to_make 0' && [ "$(echo "$out" | grep -c ': no mask,')" = 11 ] || fail "the masks are not made and counted (13 textures, 11 with no mask): $out"
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

# Create PSD on a texture with no PSD: the script (a fake of tools/track_mask_psd.py in a scratch tools folder) makes its PSD in art-source/masks; a second one never overwrites it
mkdir -p "$work/tools"; : > "$work/tools/car_mask_from_psd.py"   # a tools folder counts when it has the car converter
cat > "$work/tools/track_mask_psd.py" <<'PY'
import os, sys
for stem in os.listdir(sys.argv[1]):
    for f in os.listdir(os.path.join(sys.argv[1], stem)):
        os.makedirs(os.path.join(sys.argv[2], stem), exist_ok=True)
        open(os.path.join(sys.argv[2], stem, f.replace(".rgb.png", ".psd")), "wb").write(b"8BPS fake")
PY
out=$(run --frames 6 "$dir" --steps masks-print 2>&1)
i=$(echo "$out" | grep '^mask ' | grep -n ': no mask,' | head -1 | cut -d: -f1)
name=$(echo "$out" | grep '^mask ' | sed -n "${i}p" | sed 's/^mask \([^:]*\):.*/\1/')
i=$((i - 1))
[ -n "$name" ] && [ ! -e "$dir/art-source/masks/$name.psd" ] || fail "no texture without a PSD to try Create PSD on: $out"
out=$(TNFS_TOOLS="$work/tools" run --frames 6 "$dir" --steps mask-psd:$i,status 2>&1) || fail "Create PSD: $out"
[ -s "$dir/art-source/masks/$name.psd" ] || fail "Create PSD did not make $name.psd: $out"
echo "$out" | grep -q "$name.psd made in art-source/masks" || fail "Create PSD says nothing: $out"
TNFS_TOOLS="$work/tools" run --frames 6 "$dir" --steps mask-psd:$i >/dev/null 2>&1 && fail "Create PSD went over a PSD that is there"

[ "$failures" = 0 ] || exit 1
