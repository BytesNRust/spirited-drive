#!/bin/sh
# Original tracks as workspace projects (dep103 phase A), headless: Add the 16 from a fake data/ and art folder, open one (the Track light window on its own folder, the track fixed),
# the File menu of that room, the publish preview and the copy back into data/. The photo goes through a fake game; no CD data.
# usage: editor_original_track.sh EDITOR REPO MAKE_TEST_PSD
set -u
editor=$1 repo=$2 make_psd=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

mkdir -p "$work/data/track_light" "$work/data/track" "$work/art/AL1" "$work/ws" "$work/cd" "$work/tmp"
export TMPDIR="$work/tmp/"
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do
    for l in A B C N; do printf '# tnfs track light 1\ntrack %s time %s nodes 10 flags 3ff\nrows 0 0 255 255\n' "$t" "$l" > "$work/data/track_light/$t$l.txt"; done
    printf '# tnfs track places 1\ntrack %s\n' "$t" > "$work/data/track/$t.txt"
done
"$make_psd" "$work/art/AL1/scenery_00.psd" paint; "$make_psd" "$work/art/AL1/slot_00.psd" lamps; "$make_psd" "$work/art/AL1/slot_01.psd" empty; printf psd > "$work/art/AL1/slot_00.before-x.psd"
cat > "$work/fake_game" <<'SH'
#!/bin/sh
shot=
while [ $# -gt 0 ]; do case $1 in --screenshot) shot=$2; shift;; esac; shift; done
printf 'P6\n1280 960\n255\n' > "$shot"
head -c 3686400 /dev/zero | LC_ALL=C tr '\000' '\100' >> "$shot"
SH
chmod +x "$work/fake_game"
run() { "$editor" --game "$work/fake_game" --cd "$work/cd" --user "$work/u" --workspace "$work/ws" --port-data "$work/data" --art-dir "$work/art" --window 1280x800 "$@"; }

# the hub: the workspace is the hub's; adding makes the sixteen
out=$(run --frames 4 --steps print-menu,add-originals,print-workspace 2>&1) || fail "add-originals: $out"
echo "$out" | grep -q '^file: Workspace | Quit$' || fail "the hub's File menu: $out"
echo "$out" | grep -q '^tracks: AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7$' || fail "the 16 originals are listed: $out"
for f in light/track_light/CL1B.txt light/track/CL1.txt original.ini; do [ -f "$work/ws/tracks/CL1/$f" ] || fail "CL1 has no $f"; done
[ -f "$work/ws/tracks/AL1/art-source/masks/scenery_00.psd" ] || fail "the PSD was not copied"
[ -f "$work/ws/tracks/AL1/art-source/masks/slot_00.before-x.psd" ] && fail "a PSD backup was copied"
[ -f "$work/art/AL1/scenery_00.psd" ] || fail "the original PSD is gone from the art folder"

# open one: the Tracks room, its own light folder, the track fixed; the publish item is there
out=$(run --frames 60 --steps open-original:3,publish-ask,print-original,print-menu 2>&1) || fail "open-original: $out"
echo "$out" | grep -q "^original: $work/ws/tracks/CL1$" || fail "the original track is open: $out"
echo "$out" | grep -q "^light: $work/ws/tracks/CL1/light$" || fail "the light folder is the track's own: $out"
echo "$out" | grep -q '^track: CL1 time: B$' || fail "CL1 at day: $out"
echo "$out" | grep -q '^file: New track... | Open track... | Open original track... | Open recent | Publish to the game... | Save | Main menu | Quit$' || fail "the room's File menu: $out"
echo "$out" | grep -q '^publish: CL1:.*5 unchanged' || fail "an untouched track has nothing to publish: $out"

# an edited light file: the preview names it, Publish writes it into data/ and nothing else
printf 'brightness 0 5 120\n' >> "$work/ws/tracks/CL1/light/track_light/CL1N.txt"
out=$(run --frames 60 --steps open-original:3,publish-ask,print-original,publish,publish-ask,print-original 2>&1) || fail "publish: $out"
echo "$out" | grep -q 'track_light/CL1N.txt changed (+1 -0 lines)' || fail "the preview names the edited file: $out"
echo "$out" | grep -q '4 unchanged' || fail "the other four are unchanged: $out"
grep -q '^brightness 0 5 120$' "$work/data/track_light/CL1N.txt" || fail "data/ did not get the edited file"
echo "$out" | grep -q '^status: published to the game: 1 file written' || fail "the status says what was written: $out"
[ "$(grep -c brightness "$work/data/track_light/CL1B.txt")" = 0 ] || fail "an untouched file was changed"

# the masks: PSDs make the PNGs when the track opens (never from an empty mask layer); the panel's rows say the state
out=$(run --frames 60 --steps open-original:0,masks-print 2>&1) || fail "masks-print: $out"
echo "$out" | grep -q '^masks: 3 to_make 0 edited 0$' || fail "three masks, all made: $out"
echo "$out" | grep -q '^mask scenery_00: ok, 4x1, 2 reflective, 0 lamps$' || fail "scenery_00 ok: $out"
echo "$out" | grep -q '^mask slot_00: ok, 4x1, 1 reflective, 1 lamps$' || fail "slot_00 has its lamp: $out"
echo "$out" | grep -q '^mask slot_01: no mask painted in the PSD, 4x1, 0 reflective, 0 lamps$' || fail "an empty mask layer is not a source: $out"
[ -f "$work/ws/tracks/AL1/art-generated/track_masks/scenery_00.png" ] || fail "scenery_00.png was not made"
[ -f "$work/ws/tracks/AL1/art-generated/track_masks/slot_01.png" ] && fail "a PNG was made from an empty mask layer"

# publish lists the PNGs, the copy puts them in data/track_masks/<TRACK>/ and a second one finds nothing to do
out=$(run --frames 60 --steps open-original:0,publish-ask,print-original,publish,publish-ask,print-original 2>&1) || fail "publish masks: $out"
echo "$out" | grep -q 'track_masks/AL1/scenery_00.png new' || fail "the preview lists the new PNG: $out"
[ -f "$work/data/track_masks/AL1/slot_00.png" ] && [ -f "$work/data/track_masks/AL1/scenery_00.png" ] || fail "the PNGs are not in data/track_masks/AL1"
echo "$out" | grep -q '^publish: AL1: *7 unchanged' || fail "after publishing nothing is left to publish: $out"
out=$(run --frames 4 --steps check-originals 2>&1) || fail "check-originals: $out"
echo "$out" | grep -q '^originals: 16 tracks, 2 masks: 2 equal, 0 differ, 0 new in the workspace$' || fail "the migration check: $out"

# a PNG changed by hand is kept and says so; Regenerate from PSD puts the PSD's mask back
python3 - "$work/ws/tracks/AL1/art-generated/track_masks/scenery_00.png" <<'PY'
import struct, sys, zlib
def chunk(kind, data): return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
row = b"\x00" + bytes([255, 255, 255, 255])
open(sys.argv[1], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 1, 8, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(row)) + chunk(b"IEND", b""))
PY
out=$(run --frames 60 --steps open-original:0,masks-print 2>&1) || fail "edited: $out"
echo "$out" | grep -q '^masks: 3 to_make 0 edited 1$' || fail "one PNG is edited by hand: $out"
echo "$out" | grep -q '^mask scenery_00: edited by hand, 4x1, 2 reflective' || fail "it is kept as it is: $out"
out=$(run --frames 60 --steps open-original:0,mask-select:2,mask-regen,masks-print 2>&1) || fail "regenerate: $out"
echo "$out" | grep -q '^masks: 3 to_make 0 edited 0$' || fail "Regenerate from PSD makes it again: $out"
echo "$out" | grep -q '^mask scenery_00: ok, 4x1, 2 reflective' || fail "the PSD's mask is back: $out"

# Main menu goes back to the hub and the plain light folder comes back
out=$(run --frames 60 --steps open-original:3,main-menu,print-menu,print-original 2>&1) || fail "main-menu: $out"
echo "$out" | grep -q '^room: hub$' || fail "Main menu returns to the hub: $out"
echo "$out" | grep -q '^original: none$' || fail "no original is open on the hub: $out"
echo "$out" | grep -q "^light: $work/data$" || fail "the light folder is the game's data/ again: $out"

[ $failures -eq 0 ] && echo "editor_original_track: ok"
exit $failures
