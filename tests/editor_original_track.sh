#!/bin/sh
# Original tracks as workspace projects (dep103 phase A), headless: Add the 16 from a fake data/ and art folder, open one (the Track light window on its own folder, the track fixed),
# the File menu of that room, the publish preview and the copy back into data/. The photo goes through a fake game; no CD data.
# usage: editor_original_track.sh EDITOR REPO MAKE_TEST_PSD MAKE_FAKE_TRI
set -u
editor=$1 repo=$2 make_psd=$3 make_tri=$4
part=${5:-all}   # a: hub, old workspace, light flow, copy; b: test drive, masks, polls, repo folder, main menu; all: both (two ctest tests run a and b: each well under the limit on a slow runner)
want() { [ "$part" = all ] || [ "$part" = "$1" ]; }
stamp() { echo "editor_original_track[$part]: $* at ${SECONDS}s" >&2; }
work=$(mktemp -d)
trap '[ -z "${poll_pid:-}" ] || kill -9 "$poll_pid" 2>/dev/null; rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

mkdir -p "$work/data/track_light" "$work/art/AL1" "$work/ws" "$work/cd/SIMDATA/MISC" "$work/tmp"
export TMPDIR="$work/tmp/"
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do
    printf '# tnfs track light 2\ntrack %s\n' "$t" > "$work/data/track_light/$t.txt"
    "$make_tri" "$work/cd/SIMDATA/MISC/$t.TRI" 200 || { echo "FAIL: no fake TRI for $t" >&2; exit 1; }   # the Studio reads a track's node count from its TRI
done
"$make_psd" "$work/art/AL1/scenery_00.psd" paint; "$make_psd" "$work/art/AL1/slot_00.psd" lamps; "$make_psd" "$work/art/AL1/slot_01.psd" empty; printf psd > "$work/art/AL1/slot_00.before-x.psd"
cat > "$work/fake_game" <<'SH'
#!/bin/sh
shot=
while [ $# -gt 0 ]; do case $1 in --screenshot) shot=$2; shift;; esac; shift; done
if [ -n "${FAKE_RECORD:-}" ]; then { echo "masks=${TNFS_TRACK_MASKS:-}"; for f in "${TNFS_TRACK_MASKS:-/nonexistent}"/*/*.png; do [ -f "$f" ] && echo "$(basename "$f") $(cksum < "$f")"; done; } > "$FAKE_RECORD"; fi
printf 'P6\n1280 960\n255\n' > "$shot"
head -c 3686400 /dev/zero | LC_ALL=C tr '\000' '\100' >> "$shot"
SH
chmod +x "$work/fake_game"
run() { "$editor" --game "$work/fake_game" --cd "$work/cd" --user "$work/u" --workspace "$work/ws" --port-data "$work/data" --art-dir "$work/art" --window 1280x800 "$@"; }
run_bg() { exec "$editor" --game "$work/fake_game" --cd "$work/cd" --user "$work/u" --workspace "$work/ws" --port-data "$work/data" --art-dir "$work/art" --window 1280x800 "$@"; }   # the same, started in the background: exec makes $! the editor itself, so a kill reaches it (killing the function's subshell left the editor running)

if want a; then
# the hub: the workspace is the hub's; adding makes the sixteen
out=$(run --frames 4 --steps print-menu,add-originals,print-workspace 2>&1) || fail "add-originals: $out"
echo "$out" | grep -q '^file: Workspace | Game repo folder... | Quit$' || fail "the hub's File menu: $out"
echo "$out" | grep -q '^tracks: AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7$' || fail "the 16 originals are listed: $out"
for f in light/track_light/CL1.txt original.ini; do [ -f "$work/ws/tracks/CL1/$f" ] || fail "CL1 has no $f"; done
[ -f "$work/ws/tracks/AL1/art-source/masks/scenery_00.psd" ] || fail "the PSD was not copied"
[ -f "$work/ws/tracks/AL1/art-source/masks/slot_00.before-x.psd" ] && fail "a PSD backup was copied"
[ -f "$work/art/AL1/scenery_00.psd" ] || fail "the original PSD is gone from the art folder"

# a workspace made before the new light format: its track folders hold the old light files (light/track/<T>.txt and light/track_light/<T>{A,B,C,N}.txt).
# Add originals on it puts the one new file in and takes the old ones away; Copy then finds the file and has nothing to publish
mkdir -p "$work/oldws/tracks/CL1/light/track" "$work/oldws/tracks/CL1/light/track_light"
printf 'old\n' > "$work/oldws/tracks/CL1/light/track/CL1.txt"
for t in A B C N; do printf 'old\n' > "$work/oldws/tracks/CL1/light/track_light/CL1$t.txt"; done
cp "$work/ws/tracks/CL1/original.ini" "$work/oldws/tracks/CL1/original.ini" || fail "no original.ini to copy for the old workspace"
out=$("$editor" --game "$work/fake_game" --cd "$work/cd" --user "$work/u2" --workspace "$work/oldws" --port-data "$work/data" --art-dir "$work/art" --window 1280x800 --frames 30 --steps ui-click:add_originals,status,print-workspace 2>&1) || fail "the Add originals button on an old workspace: $out"
echo "$out" | grep -q "^status: original tracks: 16 in the workspace" || fail "the button did not say what it did: $out"
[ -f "$work/oldws/tracks/CL1/light/track_light/CL1.txt" ] || fail "the old workspace did not get the one light file"
[ -f "$work/oldws/tracks/CL1/light/track/CL1.txt" ] && fail "the old light file light/track/CL1.txt is still there"
for t in A B C N; do [ -f "$work/oldws/tracks/CL1/light/track_light/CL1$t.txt" ] && fail "the old light file CL1$t.txt is still there"; done
out=$("$editor" --game "$work/fake_game" --cd "$work/cd" --user "$work/u2" --workspace "$work/oldws" --port-data "$work/data" --art-dir "$work/art" --window 1280x800 --frames 60 --steps open-original:3,repo-ask,print-original 2>&1) || fail "open on the old workspace: $out"
echo "$out" | grep -q 'the project has no track_light' && fail "Copy to game repo finds no light file in the old workspace after Add originals: $out"
echo "$out" | grep -q '^preview: CL1:.*1 unchanged' || fail "the old workspace's CL1 has something to publish after Add originals: $out"

# open one: the Tracks room, its own light folder, the track fixed; the copy item is there
out=$(run --frames 60 --steps open-original:3,repo-ask,print-original,print-menu 2>&1) || fail "open-original: $out"
echo "$out" | grep -q "^original: $work/ws/tracks/CL1$" || fail "the original track is open: $out"
echo "$out" | grep -q "^light: $work/ws/tracks/CL1/light$" || fail "the light folder is the track's own: $out"
echo "$out" | grep -q '^track: CL1 time: B$' || fail "CL1 at day: $out"
echo "$out" | grep -q '^file: New track... | Open track... | Open original track... | Export original track... | Open recent | Copy to game repo | Game repo folder... | Save | Main menu | Quit$' || fail "the room's File menu: $out"
echo "$out" | grep -q '^preview: CL1:.*1 unchanged' || fail "an untouched track has nothing to publish: $out"

# an edited light file: the preview names it, Copy writes it into data/ and nothing else
printf 'time N\nbrightness 0 5 120\n' >> "$work/ws/tracks/CL1/light/track_light/CL1.txt"
out=$(run --frames 60 --steps open-original:3,repo-ask,print-original,repo-copy,repo-ask,print-original 2>&1) || fail "publish: $out"
echo "$out" | grep -q 'track_light/CL1.txt changed (+2 -0 lines)' || fail "the preview names the edited file: $out"
grep -q '^brightness 0 5 120$' "$work/data/track_light/CL1.txt" || fail "data/ did not get the edited file"
echo "$out" | grep -q '^status: copied to the game repo: 1 file written' || fail "the status says what was written: $out"
[ "$(grep -c brightness "$work/data/track_light/AL1.txt")" = 0 ] || fail "an untouched file was changed"

# the whole light flow of a user, on CL1: open it, change the sun, a brightness line, a tone, a lamp and a tunnel's look, Save, open it again (the lines are there), take a tone away,
# Save, Copy to the game repo: data/ gets the edited file, line for line what the workspace has, and no other track's file changes
: > "$work/ws/tracks/CL1/light/track_light/CL1.txt"
printf '# tnfs track light 2\ntrack CL1\ntunnel 10 20\n\ntime B\n  sun 150 50 55\n  tunnel_look 100\n' > "$work/ws/tracks/CL1/light/track_light/CL1.txt"
before=$(cksum < "$work/ws/tracks/CL1/light/track_light/CL1.txt")
out=$(run --frames 60 --steps open-original:3,sun:120,30,70,light-bright:50,light-tone:64,light-lamp:150,tunnel-lamps:0,200,light-print,save,light-print 2>&1) || fail "user flow, edit: $out"
echo "$out" | grep -q '^light: CL1 nodes [0-9]* time B (unsaved) tunnels 1 looks 2 lamps 1 brightness 1 tones 1 .*sun 120 30 70$' || fail "the edits are not in the window's model: $(echo "$out" | grep '^light:')"
echo "$out" | grep '^light:' | tail -1 | grep -q '(unsaved)' && fail "Save left the unsaved mark: $(echo "$out" | grep '^light:')"
f=$work/ws/tracks/CL1/light/track_light/CL1.txt
for line in '  sun 120 30 70' '  lamp 0 150' '  tone 0 20 road 404040:60' '  tunnel_look 10 20 lamps 200' ; do grep -q "^$line" "$f" || fail "Save did not write '$line': $(cat "$f")"; done
grep -q '^  brightness 0 50 50' "$f" || fail "Save did not write the brightness line: $(cat "$f")"
[ "$(cksum < "$f")" != "$before" ] || fail "Save changed nothing"
out=$(run --frames 60 --steps open-original:3,light-print,light-remove:1,light-print,save,light-print 2>&1) || fail "user flow, reopen: $out"
echo "$out" | grep '^light:' | head -1 | grep -q 'lamps 1 brightness 1 tones 1 ' || fail "the saved lines were not read back when the track was opened again: $(echo "$out" | grep '^light:')"
echo "$out" | grep '^light:' | tail -1 | grep -q 'brightness 1 tones 0 ' || fail "Remove did not take the tone away: $(echo "$out" | grep '^light:')"
grep -q '^  tone ' "$f" && fail "the removed tone is still in the file: $(cat "$f")"
out=$(run --frames 60 --steps open-original:3,repo-ask,print-original,repo-copy 2>&1) || fail "user flow, copy: $out"
echo "$out" | grep -q 'track_light/CL1.txt changed' || fail "the preview does not name the edited file: $out"
cmp -s "$f" "$work/data/track_light/CL1.txt" || fail "data/ does not have the edited file, line for line: $(cat "$work/data/track_light/CL1.txt")"
[ "$(grep -c '^  sun\|lamp' "$work/data/track_light/AL1.txt")" = 0 ] || fail "another track's file was changed"
fi

if want b; then
[ "$part" = all ] || run --frames 4 --steps add-originals >/dev/null 2>&1 || fail "part b: add-originals"   # part b alone starts from the sixteen the hub makes
stamp "part b starts"
# the masks: PSDs make the PNGs when the track opens (never from an empty mask layer); the panel's rows say the state
# the test drive in the Track light window (light tuning design, piece 1): the game in a window beside Studio, the working copy in front of the files, weather and stop
cat > "$work/live_game" <<'SH'
#!/bin/sh
[ "$TNFS_LIGHT_EDIT" = 1 ] && echo "edit=$TNFS_LIGHT_EDIT time=$TNFS_LIGHT_TIME weather=$TNFS_WEATHER video=$SDL_VIDEO_DRIVER" > "$LIVE_RECORD"   # the drive, not the photos the window also takes
sleep 30
SH
chmod +x "$work/live_game"
export LIVE_RECORD="$work/live_record"
rm -f "$LIVE_RECORD"
out=$(run --game "$work/live_game" --frames 40 --steps open-original:0,light-weather:2,light-drive,poll-wait:8,print-drive,drive-stop,print-drive 2>&1) || fail "test drive: $out"
echo "$out" | grep -q '^drive: running AL1 B node 0 rain$' || fail "the drive did not run on AL1, day, node 0, rain: $(echo "$out" | grep '^drive:')"
echo "$out" | grep -q '^drive: failed AL1 B node 0 rain$' || fail "Stop did not end the drive: $(echo "$out" | grep '^drive:')"
echo "$out" | grep -q '^drive why: $' || fail "a Stop was reported as a failure: $(echo "$out" | grep '^drive why')"
grep -q 'edit=1 time=day weather=rain video=$' "$LIVE_RECORD" || fail "the game was not started with the light editor, the day and the rain (and a real window): $(cat "$LIVE_RECORD" 2>&1)"
out=$(run --game "$work/live_game" --frames 40 --steps open-original:0,light-drive,poll-wait:4,light-bright:40,poll-wait:4,print-drive,drive-stop 2>&1) || fail "test drive edit: $out"
echo "$out" | grep -q '^drive copy: brightness 0 50 40' || fail "an edit made while the game runs did not reach the drive's copy of the file: $(echo "$out" | grep '^drive')"
out=$(run --game /no/such/game --frames 10 --steps open-original:0,light-drive 2>&1) && fail "a drive started without a game: $out"
echo "$out" | grep -q "the game's program is not there" || fail "no game: the refusal does not say so: $out"

# the laptop's 854 px window, a fresh layout: the three panels share the width, none squeezed (the masks table's columns are checked in the captures)
out=$(run --window 854x560 --reset-layout --frames 30 --steps open-original:0,poll-wait:6,print-panels 2>&1) || fail "854 px layout: $out"
masks_w=$(echo "$out" | grep '^widths:' | sed -n 's/.*\[Track masks \([0-9]*\)\].*/\1/p')
[ -n "$masks_w" ] && [ "$masks_w" -ge 250 ] || fail "the Track masks panel is $masks_w px wide in an 854 px window (250 needed): $(echo "$out" | grep '^widths:')"
out=$(run --frames 60 --steps open-original:0,masks-print 2>&1) || fail "masks-print: $out"
echo "$out" | grep -q '^masks: 3 to_make 0 edited 0$' || fail "three masks, all made: $out"
echo "$out" | grep -q '^mask scenery_00: ok, 4x1, 2 reflective, 0 lamps$' || fail "scenery_00 ok: $out"
echo "$out" | grep -q '^mask slot_00: ok, 4x1, 1 reflective, 1 lamps$' || fail "slot_00 has its lamp: $out"
echo "$out" | grep -q '^mask slot_01: no mask painted in the PSD, 4x1, 0 reflective, 0 lamps$' || fail "an empty mask layer is not a source: $out"
[ -f "$work/ws/tracks/AL1/art-generated/track_masks/scenery_00.png" ] || fail "scenery_00.png was not made"
[ -f "$work/ws/tracks/AL1/art-generated/track_masks/slot_01.png" ] && fail "a PNG was made from an empty mask layer"

# copy to game repo lists the PNGs, the copy puts them in data/track_masks/<TRACK>/ and a second one finds nothing to do
out=$(run --frames 60 --steps open-original:0,repo-ask,print-original,repo-copy,repo-ask,print-original 2>&1) || fail "publish masks: $out"
echo "$out" | grep -q 'track_masks/AL1/scenery_00.png new' || fail "the preview lists the new PNG: $out"
[ -f "$work/data/track_masks/AL1/slot_00.png" ] && [ -f "$work/data/track_masks/AL1/scenery_00.png" ] || fail "the PNGs are not in data/track_masks/AL1"
echo "$out" | grep -q '^preview: AL1: *3 unchanged' || fail "after publishing nothing is left to publish: $out"
out=$(run --frames 4 --steps check-originals 2>&1) || fail "check-originals: $out"
echo "$out" | grep -q '^originals: 16 tracks, 2 masks: 2 equal, 0 differ, 0 new in the workspace; acceptance: 0 differences outside art-source/masks$' || fail "the migration check: $out"

# the photo shows the workspace's masks (TNFS_TRACK_MASKS), and a PSD retouched in Photoshop shows in the next photo
export FAKE_RECORD="$work/record"
out=$(run --frames 60 --steps open-original:0 2>&1) || fail "photo masks: $out"
grep -q '^masks=.' "$work/record" 2>/dev/null || fail "the photo was not given the workspace's masks: $(cat "$work/record" 2>/dev/null)"
want=$(cksum < "$work/ws/tracks/AL1/art-generated/track_masks/scenery_00.png")
grep -q "^scenery_00.png $want\$" "$work/record" || fail "the photo's scenery_00 mask is not the workspace's: $(cat "$work/record")"
before=$(grep '^scenery_00.png' "$work/record")
sleep 1
"$make_psd" "$work/ws/tracks/AL1/art-source/masks/scenery_00.psd" lamps   # retouched and saved: newer than its PNG
out=$(run --frames 60 --steps open-original:0 2>&1) || fail "photo after a retouch: $out"
after=$(grep '^scenery_00.png' "$work/record")
[ -n "$after" ] && [ "$after" != "$before" ] || fail "a retouched PSD did not change the mask in the photo: $before -> $after"
want=$(cksum < "$work/ws/tracks/AL1/art-generated/track_masks/scenery_00.png")
[ "$after" = "scenery_00.png $want" ] || fail "the photo's mask is not the regenerated PNG: $after"
"$make_psd" "$work/ws/tracks/AL1/art-source/masks/scenery_00.psd" paint   # back to the first mask for what follows
sleep 1
run --frames 4 --steps open-original:0,masks-print > /dev/null 2>&1
# the window open: a PSD saved while the editor runs (no new run) is seen by the panel's poll: its status says so, the PNG is made again and so is the photo
png="$work/ws/tracks/AL1/art-generated/track_masks/scenery_00.png"
before_png=$(cksum < "$png")
run_bg --frames 100000 --steps open-original:0,poll-wait:90 > "$work/poll.out" 2>&1 &
poll_pid=$!
for _ in $(seq 1 120); do grep -q '^poll: watching' "$work/poll.out" && break; sleep 0.5; done
grep -q '^poll: watching' "$work/poll.out" || { kill -9 "$poll_pid" 2>/dev/null; fail "the window did not start watching: $(cat "$work/poll.out")"; }
sleep 2
"$make_psd" "$work/ws/tracks/AL1/art-source/masks/scenery_00.psd" lamps   # saved again while the window is open
for _ in $(seq 1 120); do
    [ "$(cksum < "$png")" != "$before_png" ] && grep -q '^poll: .*a PSD was saved again' "$work/poll.out" && grep -q "^scenery_00.png $(cksum < "$png")\$" "$work/record" && break
    sleep 0.5
done
kill -9 "$poll_pid" 2>/dev/null; wait "$poll_pid" 2>/dev/null   # -9: the editor did not end on SIGTERM
grep -q '^poll: .*a PSD was saved again' "$work/poll.out" || fail "the open window did not notice the saved PSD: $(cat "$work/poll.out")"
[ "$(cksum < "$png")" != "$before_png" ] || fail "the open window did not make the PNG again"
grep -q "^scenery_00.png $(cksum < "$png")\$" "$work/record" || fail "the open window's photo did not show the new PNG: $(cat "$work/record")"
"$make_psd" "$work/ws/tracks/AL1/art-source/masks/scenery_00.psd" paint   # back to the first mask for what follows
sleep 1
run --frames 4 --steps open-original:0,masks-print > /dev/null 2>&1
unset FAKE_RECORD

# a PNG changed by hand is kept and says so; Regenerate from PSD puts the PSD's mask back
python3 - "$work/ws/tracks/AL1/art-generated/track_masks/scenery_00.png" <<'PY'
import struct, sys, zlib
def chunk(kind, data): return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
row = b"\x00" + bytes([255, 255, 255, 255])
open(sys.argv[1], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 1, 8, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(row)) + chunk(b"IEND", b""))
PY
out=$(run --frames 60 --steps open-original:0,masks-print 2>&1) || fail "edited: $out"
echo "$out" | grep -q '^masks: 3 to_make 0 edited 1$' || fail "one PNG is edited by hand: $out"
echo "$out" | grep -q '^mask scenery_00: edited by hand, 4x1, 4 reflective' || fail "it is kept as it is: $out"
out=$(run --frames 60 --steps "open-original:0,ui-focus:Track masks,ui-click:tmask2_row,ui-click:tmask_regen,masks-print" 2>&1) || fail "regenerate (the row and the Regenerate from PSD button clicked): $out"
echo "$out" | grep -q '^masks: 3 to_make 0 edited 0$' || fail "Regenerate from PSD makes it again: $out"
echo "$out" | grep -q '^mask scenery_00: ok, 4x1, 2 reflective' || fail "the PSD's mask is back: $out"

# the copy action is hidden unless the game repo's data/ is given (--port-data): without it the room's File menu has no Copy item and print-repo says none
out=$("$editor" --game "$work/fake_game" --cd "$work/cd" --user "$work/u" --workspace "$work/ws" --window 1280x800 --frames 20 --steps open-original:3,print-menu,print-repo 2>&1) || fail "no repo: $out"
echo "$out" | grep -qF 'Export original track... | Open recent | Copy to game repo (off: set the game repo folder first) | Game repo folder... | Save' || fail "Copy is shown off, with Game repo folder..., without the repo's data/: $out"
echo "$out" | grep -q '^repo data: none$' || fail "no repo data is given: $out"
# File > Game repo folder...: a folder without car_masks/ and track_light/ is refused, the repo's data/ (or the repo itself) is kept, and Copy turns on
mkdir -p "$work/notrepo" "$work/repo2/data/car_masks" "$work/repo2/data/track_light"
bare() { "$editor" --game "$work/fake_game" --cd "$work/cd" --user "$work/u" --workspace "$work/ws" --art-dir "$work/art" --window 1280x800 "$@"; }
out=$(TNFS_STUDIO_PICK="$work/notrepo" bare --frames 20 --steps open-original:3,repo-choose,status,print-repo 2>&1) || fail "repo refused: $out"
echo "$out" | grep -q 'is not the repo.s data/' || fail "a folder without car_masks/ and track_light/ is refused: $out"
echo "$out" | grep -q '^repo data: none$' || fail "nothing changed after a refusal: $out"
out=$(TNFS_STUDIO_PICK="$work/repo2" bare --frames 20 --steps open-original:3,repo-choose,status,print-repo,print-menu 2>&1) || fail "repo chosen: $out"
echo "$out" | grep -q "^repo data: $work/repo2/data$" || fail "the repo's data/ is kept when the repo is chosen: $out"
echo "$out" | grep -q '^file: .*| Copy to game repo | Game repo folder... |' || fail "Copy is on once the folder is set: $out"
out=$(run --frames 20 --steps print-repo 2>&1) || fail "repo: $out"
echo "$out" | grep -q "^repo data: $work/data$" || fail "--port-data is the repo data: $out"

# Main menu goes back to the hub and the plain light folder comes back
out=$(run --frames 60 --steps open-original:3,main-menu,print-menu,print-original 2>&1) || fail "main-menu: $out"
echo "$out" | grep -q '^room: hub$' || fail "Main menu returns to the hub: $out"
echo "$out" | grep -q '^original: none$' || fail "no original is open on the hub: $out"
echo "$out" | grep -q "^light: $work/data$" || fail "the light folder is the game's data/ again: $out"

# with the game's files: Track masks lists EVERY texture of the track, those with no mask too (a texture the PNG is not for); only the real-CD blocks skip in CI
if [ -r "$repo/original/SIMDATA/ETRACKFM/AL1_001.FAM" ]; then
    mkdir -p "$work/real/ws" "$work/real/u"
    real() { "$editor" --game /bin/true --cd "$repo/original" --user "$work/real/u" --workspace "$work/real/ws" --port-data "$repo/data" --window 1280x800 "$@"; }
    real --frames 6 --steps add-originals >/dev/null 2>&1 || fail "real add-originals"
    mkdir -p "$work/real/ws/tracks/AL1/art-generated/track_masks"
    cp "$repo/data/track_masks/AL1/scenery_00.png" "$work/real/ws/tracks/AL1/art-generated/track_masks/" || fail "no scenery_00.png in data/"
    out=$(real --frames 60 --steps open-original:0,masks-print,masks-only:1 --shot "$work/real/masks.png" 2>&1) || fail "real masks-print: $out"
    n=$(echo "$out" | sed -n 's/^masks: \([0-9]*\) .*/\1/p')
    [ "${n:-0}" -gt 20 ] || fail "AL1's textures are not all listed: $out"
    echo "$out" | grep -q '^mask scenery_00: PNG is the source, 53x127, [1-9][0-9]* reflective' || fail "the texture with a mask says so: $out"
    echo "$out" | grep -q '^mask slot_00: no mask, 128x256, 0 reflective, 0 lamps$' || fail "a texture with no mask is a row: $out"
    [ -s "$work/real/masks.png" ] || fail "no picture of the Masks panel with With mask only"
fi

fi

stamp "end"
[ $failures -eq 0 ] && echo "editor_original_track: ok"
exit $failures
