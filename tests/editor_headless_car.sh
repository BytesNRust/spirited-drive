#!/bin/sh
# The editor on cars, headless, in the order a user goes: Export original car, open it (or a kit), edit and save its card,
# The mask view, Preview by day and night, the Model, In-game photo, Changes. The blocks that need the player's game files
# (original/) are skipped without them.
# usage: editor_headless_car.sh EDITOR
. "$(dirname "$0")/editor_headless_common.sh"
part=${2:-all}   # a: export .. mask view; b: Preview .. Window menu (both start with the Export block, which the rest uses); all: both. CMake runs a and b as two tests: one test of ~100 editor runs passed the 450 s limit on a loaded machine
want() { [ "$part" = all ] || [ "$part" = "$1" ]; }
repo_src="$(cd "$(dirname "$0")/.." && pwd)"
repo=$(cd "$(dirname "$0")/.." && pwd)

# ==== Export original car ====
# Export original car makes a kit (the real-original blocks below). With a fixture game folder (a synthetic car file, masks that are PNG headers) it is refused in
# words and writes nothing: the car file is not a car model. A car with no masks is refused too. The masks project the exporter made before kits is still opened,
# raced and sheeted (old exports are there): the fixture one is written here by hand, in the shape car_export made it.
mkdir -p "$work/game/cd/SIMDATA/CARFAMS" "$work/game/data/car_masks/Wagon" "$work/exports"
printf 'cfm' > "$work/game/cd/SIMDATA/CARFAMS/WAGON.CFM"
printf 'cfm' > "$work/game/cd/SIMDATA/CARFAMS/PROBE.CFM"
python3 -c "
import sys, struct
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + struct.pack('>I4sIIBB', 13, b'IHDR', 100, 50, 8, 0) + bytes(20))
" "$work/game/data/car_masks/Wagon/side_mask.png"
export_opts="--cd $work/game/cd --port-data $work/game/data"
"$editor" --export-car Wagon --into "$work/exports" $export_opts --frames 3 >"$work/log" 2>&1 && fail "a car file that is not a car model was exported"
grep -q "not a car model" "$work/log" || fail "no word why the export was refused: $(cat "$work/log")"
[ -z "$(ls "$work/exports")" ] || fail "a refused export left files: $(ls "$work/exports")"
"$editor" --export-car probe --into "$work/exports" $export_opts --frames 2 >"$work/log" 2>&1 && fail "a car with no masks was exported"
[ -z "$(ls "$work/exports")" ] || fail "a refused export left files: $(ls "$work/exports")"
mkdir -p "$work/exports/wagon-masks/original" "$work/exports/wagon-masks/car_masks/Wagon"
cp "$work/game/cd/SIMDATA/CARFAMS/WAGON.CFM" "$work/exports/wagon-masks/original/WAGON.CFM"
cp "$work/game/data/car_masks/Wagon/side_mask.png" "$work/exports/wagon-masks/car_masks/Wagon/side_mask.png"
printf '# made by Export original car\nid=wagon\nname=Wagon masks\nkind=masks\nmasks=Wagon\ncfm=original/WAGON.CFM\nexport_sizes=side_mask.png:100x50\nredistributable=0\n' > "$work/exports/wagon-masks/project.ini"
"$editor" --ask export --pick Wagon $export_opts --frames 4 --shot "$work/exportdialog.png" >"$work/log" 2>&1 || fail "the export window did not draw: $(cat "$work/log")"
[ -s "$work/exportdialog.png" ] || fail "no PNG of the export window"
"$editor" --pick nosuchcar --frames 2 $export_opts >/dev/null 2>&1 && fail "--pick took a car that is not in the list"
"$editor" "$work/exports/wagon-masks" --frames 3 --shot "$work/masksproject.png" >"$work/log" 2>&1 || fail "a masks project did not open: $(cat "$work/log")"
[ ! -e "$work/exports/wagon-masks/trace.json" ] || fail "a masks project was made a track project"

if want a; then
# poll-wait on a car (the clock was the track room's alone, so on a car the step never ended): 1 s of frames, then it is over (--frames 3: the step alone keeps the run going); python bounds it, CI has no timeout command
python3 -I -c "
import subprocess, sys
try:
    r = subprocess.run(sys.argv[2:], stdout=open(sys.argv[1], 'w'), stderr=subprocess.STDOUT, timeout=60)
    sys.exit(r.returncode)
except subprocess.TimeoutExpired:
    sys.exit(124)
" "$work/log" "$editor" "$work/exports/wagon-masks" --frames 3 --steps poll-wait:1 || fail "poll-wait on a car did not end (rc $?): $(cat "$work/log")"
grep -q "poll: watching" "$work/log" || fail "poll-wait on a car did not start: $(cat "$work/log")"

# ==== Open: the start screen, a car's kit, a card that is not a car ====
# no PROJECT: the start screen (nothing is made anywhere); a car's kit (the current layout) opens as a car and writes nothing; an old coche/ + modelo/ kit and a card
# that is not a car are refused in words
"$editor" --frames 3 --shot "$work/start.png" >"$work/log" 2>&1 || fail "run with nothing to open: $(cat "$work/log")"
[ -s "$work/start.png" ] || fail "no PNG of the start screen"
"$editor" --ask opencar --frames 3 >"$work/log" 2>&1 || fail "run with Open car asked for on the start screen: $(cat "$work/log")"
mkdir -p "$work/kit/model/near/bank" "$work/kit/art-generated/car_masks"
printf 'id=k1\nname=Kit One\nversion=1\ntype=car\nstem=K1\nsound_from=911\n' > "$work/kit/mod.ini"
printf 'x' > "$work/kit/art-generated/car_masks/rsid_mask.png"
printf 'x' > "$work/kit/model/near/bank/0010_rsid.png"
"$editor" "$work/kit" --frames 3 --shot "$work/kit.png" >"$work/log" 2>&1 || fail "run on a car's kit: $(cat "$work/log")"
[ -s "$work/kit.png" ] || fail "no PNG of the car"
cmp -s "$work/kit.png" "$work/start.png" && fail "the car looks like the start screen"
[ ! -e "$work/kit/project.ini" ] || fail "opening a car made track files in it"
# the Cars room's Build and Install are on for a kit and off for one of the game's own cars (stem P911): the panel's test, not a message after the click
"$editor" "$work/kit" --frames 4 --steps car-buttons >"$work/log" 2>&1 || fail "car-buttons on a kit: $(cat "$work/log")"
grep -qx "car buttons: on" "$work/log" || fail "Build and Install are not on for a kit: $(cat "$work/log")"
mkdir -p "$work/okit/model/near/bank"
printf 'id=orig\nname=Orig\nversion=1\ntype=car\nstem=P911\n' > "$work/okit/mod.ini"
"$editor" "$work/okit" --frames 4 --steps car-buttons >"$work/log" 2>&1 || fail "car-buttons on an original: $(cat "$work/log")"
grep -qx "car buttons: off (original)" "$work/log" || fail "Build and Install are on for an original car: $(cat "$work/log")"
# Copy to game repo for an original: its card's Colors are written into its row of TABLE in the repo's core/formats/car_paint.c (the data/ folder sits in the repo), and the window lists the
# change as was/now. The card as the export makes it is the table's own list, so there is nothing to copy; a color changed and saved is listed, written, and said as nothing once the row has it
repo_src="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$work/okit/art-generated/car_masks" "$work/orepo/data/car_masks/P911" "$work/orepo/core/formats"
cp "$repo_src/core/formats/car_paint.c" "$work/orepo/core/formats/car_paint.c"
table="$work/orepo/core/formats/car_paint.c"
printf 'id=orig\nname=Orig\nversion=1\ntype=car\nstem=P911\nsound_from=911\npaints=Grand Prix White:f1f1ee,Black:0e0e10,Guards Red:b3121e,Speed Yellow:f2c500\n' > "$work/okit/mod.ini"
"$editor" "$work/okit" --port-data "$work/orepo/data" --frames 2 --steps repo-ask,status >"$work/log" 2>&1 && fail "Copy to game repo with the game's own colors went on: $(cat "$work/log")"
grep -q "every mask and color is the game repo's own" "$work/log" || fail "no word that the colors are the game's own: $(cat "$work/log")"
"$editor" "$work/okit" --port-data "$work/orepo/data" --frames 2 --steps paint-rgb:2,1,2,3,save,repo-ask,print-publish >"$work/log" 2>&1 || fail "Copy to game repo, a changed color: $(cat "$work/log")"
grep -q "core/formats/car_paint.c, the row of P911" "$work/log" && grep -q "was: Grand Prix White #f1f1ee, Black #0e0e10, Guards Red #b3121e, Speed Yellow #f2c500" "$work/log" \
    && grep -q "now: Grand Prix White #f1f1ee, Black #0e0e10, Guards Red #010203, Speed Yellow #f2c500" "$work/log" || fail "the window does not list the colors was/now: $(cat "$work/log")"
cmp -s "$table" "$repo_src/core/formats/car_paint.c" || fail "the preview wrote into car_paint.c"
"$editor" "$work/okit" --port-data "$work/orepo/data" --frames 2 --steps repo-ask,repo-copy,status >"$work/log" 2>&1 || fail "Copy to game repo, the copy: $(cat "$work/log")"
grep -q "^status: copied to the game repo: .*the colors into $work/orepo/core/formats/car_paint.c" "$work/log" || fail "Copy did not say what it wrote: $(cat "$work/log")"
grep -q '{ "Guards Red", { 1, 2, 3 } }' "$table" || fail "the row of the 911 does not have the new color"
[ "$(diff "$table" "$repo_src/core/formats/car_paint.c" | grep -c '^[<>]')" = 2 ] || fail "Copy changed more than the one line of the 911's paints: $(diff "$table" "$repo_src/core/formats/car_paint.c")"
[ ! -e "$table.new" ] || fail "Copy left a scratch file"
"$editor" "$work/okit" --port-data "$work/orepo/data" --frames 2 --steps repo-ask,status >"$work/log" 2>&1 && fail "Copy to game repo with nothing changed went on"
grep -q "every mask and color is the game repo's own" "$work/log" || fail "the second ask does not say nothing is left: $(cat "$work/log")"
"$editor" "$work/okit" --port-data "$work/orepo/data" --frames 2 --steps paint-rgb:0,9,9,9,save,repo-ask,print-publish >"$work/log" 2>&1 || fail "Copy to game repo, a color changed again: $(cat "$work/log")"
grep -q "was: Grand Prix White #f1f1ee, Black #0e0e10, Guards Red #010203" "$work/log" && grep -q "now: Grand Prix White #090909" "$work/log" || fail "a color changed after the row was written is not listed before and after: $(cat "$work/log")"
# a car_paint.c the writer does not know: refused in words, nothing written; no car_paint.c at all: refused too (the colors are not the game's)
cp "$table" "$work/table.keep"
sed 's/{ "P911", 4, { 16, 56, 72 },/{ "P911", 4, { 16, 56 },/' "$work/table.keep" > "$table"
cp "$table" "$work/table.odd"
"$editor" "$work/okit" --port-data "$work/orepo/data" --frames 2 --steps repo-ask,status >"$work/log" 2>&1 && fail "Copy to game repo into a table it does not know went on"
grep -q "row of P911 is not laid out" "$work/log" || fail "no word why the table was refused: $(cat "$work/log")"
cmp -s "$table" "$work/table.odd" || fail "the refused copy touched car_paint.c"
rm "$table"
"$editor" "$work/okit" --port-data "$work/orepo/data" --frames 2 --steps repo-ask,status >"$work/log" 2>&1 && fail "Copy to game repo with no car_paint.c went on"
grep -q "car_paint.c is not there" "$work/log" || fail "no word that car_paint.c is not there: $(cat "$work/log")"
cp "$work/table.keep" "$table"
# unsaved card changes: the copy asks for Save first
"$editor" "$work/okit" --port-data "$work/orepo/data" --frames 2 --steps paint-rgb:1,5,5,5,repo-ask,status >"$work/log" 2>&1 && fail "Copy to game repo with an unsaved card went on"
grep -q "save the card first" "$work/log" || fail "no word to save the card first: $(cat "$work/log")"
# the Preview's Night light and tint (no track selector): the defaults are the old look (100 %, grey 0.72), the steps set what the slider and the picker set, the two Reset buttons
# put each back, the editor keeps both in its ini for the next run, and the command line comes after the ini
nightu="$work/night-user"
"$editor" "$work/kit" --user "$nightu" --frames 14 --steps print-night,night-light:60,night-tint:0.2,0.4,0.6,print-night,night-reset:0,print-night,night-reset:1,print-night >"$work/log" 2>&1 || fail "the night steps: $(cat "$work/log")"
[ "$(grep '^night: ' "$work/log" | tr '\n' '|')" = "night: light=100 tint=0.720,0.720,0.720|night: light=60 tint=0.200,0.400,0.600|night: light=100 tint=0.200,0.400,0.600|night: light=100 tint=0.720,0.720,0.720|" ] \
    || fail "the Night light and tint, set and reset: $(grep '^night' "$work/log")"
"$editor" "$work/kit" --user "$nightu" --frames 14 --steps night-light:55,night-tint:0.1,0.2,0.3 >"$work/log" 2>&1 || fail "the night light and tint to keep: $(cat "$work/log")"
grep -q '^NightLight=55$' "$nightu/track-editor.ini" && grep -q '^NightTint=0.100,0.200,0.300$' "$nightu/track-editor.ini" || fail "the ini does not keep the Night light and tint: $(grep -i night "$nightu/track-editor.ini")"
"$editor" "$work/kit" --user "$nightu" --frames 3 --steps print-night >"$work/log" 2>&1 || fail "the night values after a restart: $(cat "$work/log")"
grep -qx "night: light=55 tint=0.100,0.200,0.300" "$work/log" || fail "the Night light and tint were not remembered: $(grep '^night' "$work/log")"
"$editor" "$work/kit" --user "$nightu" --night-light 70 --night-tint 0.5,0.5,0.5 --frames 3 --steps print-night >"$work/log" 2>&1 || fail "the night options: $(cat "$work/log")"
grep -qx "night: light=70 tint=0.500,0.500,0.500" "$work/log" || fail "the command line did not win over the ini: $(grep '^night' "$work/log")"
"$editor" "$work/kit" --user "$work/night-user2" --night-tint 2,0,0 --frames 3 >"$work/log" 2>&1
grep -q "R,G,B, each 0 to 1 expected" "$work/log" || fail "no word about a --night-tint out of range: $(cat "$work/log")"
mkdir -p "$work/oldkit/coche" "$work/oldkit/modelo"
printf 'id=k1\nname=Kit One\nversion=1\ntype=car\nstem=K1\n' > "$work/oldkit/coche/mod.ini"
out=$("$editor" "$work/oldkit" --frames 2 2>&1) && fail "an old coche/ + modelo/ kit was opened"
echo "$out" | grep -q "not a kit in the current layout" || fail "no word that the layout is the old one: $out"
out=$("$editor" "$work/oldkit/coche" --frames 2 2>&1) && fail "an old coche/ folder was opened"
echo "$out" | grep -q "not a kit in the current layout" || fail "no word that coche/ is the old layout: $out"
[ ! -e "$work/oldkit/project.ini" ] && [ ! -e "$work/oldkit/coche/project.ini" ] || fail "an old kit was made a track project"
"$editor" "$work/kit" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a kit that has no Blender OBJ"
grep -q "could not be built" "$work/log" || fail "Build on a kit that cannot be built does not say why: $(cat "$work/log")"
"$editor" "$work/exports/wagon-masks" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a masks project"
grep -q "for a car kit" "$work/log" || fail "Build on a masks project does not say it is for a kit: $(cat "$work/log")"
mkdir -p "$work/notcar/model"
printf 'id=t\nname=T\nversion=1\ntype=track\nstem=TT\n' > "$work/notcar/mod.ini"
out=$("$editor" "$work/notcar" --frames 2 2>&1) && fail "a track's mod.ini was opened as a car"
echo "$out" | grep -q "type=track" || fail "no word about what the card is: $out"
[ ! -e "$work/notcar/project.ini" ] || fail "a card that is not a car was made a track project"

# ==== Card: edit and Save mod.ini, an invalid field, unsaved changes on Quit ====
cp "$work/kit/mod.ini" "$work/card.before"
"$editor" "$work/kit" --frames 2 --save >"$work/log" 2>&1 || fail "Save with nothing changed failed: $(cat "$work/log")"
cmp -s "$work/card.before" "$work/kit/mod.ini" || fail "Save with nothing changed touched mod.ini"
# a change is written to mod.ini and nothing else; an invalid field refuses Save, shows on its field, and leaves the file as it was
"$editor" "$work/kit" --frames 2 --set year=1990 --save >"$work/log" 2>&1 || fail "Save of a change failed: $(cat "$work/log")"
printf 'year=1990\n' >> "$work/card.before"
cmp -s "$work/card.before" "$work/kit/mod.ini" || fail "Save changed more than the one line"
cp "$work/kit/mod.ini" "$work/card.before"
"$editor" "$work/kit" --frames 3 --set year=12345678901234567 --save --shot "$work/invalid.png" >"$work/log" 2>&1 && fail "Save went with a field that has a problem"
grep -q "Year: 17 characters; the card keeps up to 16" "$work/log" || fail "no word about the field: $(cat "$work/log")"
cmp -s "$work/card.before" "$work/kit/mod.ini" || fail "a refused Save changed mod.ini"
[ -s "$work/invalid.png" ] || fail "no PNG with an invalid field"
"$editor" "$work/kit" --frames 3 --set year=1991 --ask quit --shot "$work/carconfirm.png" >"$work/log" 2>&1 || fail "run with a changed card and a quit asked for: $(cat "$work/log")"
"$editor" "$work/kit" --frames 2 --set nokey=1 >"$work/log" 2>&1 && fail "--set accepted a key that is not editable"
rm -f "$work/kit/project.ini"

# ==== Showcase photos: Add photo, Save, refusals ====
# a PNG is copied into the car's folder under a safe name and named on the card (photos=); Save writes the line; a picture of a size the showcase cannot use is refused with
# its size and nothing is copied; a copy made by an earlier run is the author's: a shorter list is saved and the file stays (the sweep itself is test_car_photos)
mkdir -p "$work/shots-in"
png() {   # png FILE W H
    python3 -c "
import struct, sys, zlib
w, h = int(sys.argv[2]), int(sys.argv[3])
raw = b''.join(b'\\x00' + b'\\x30\\x60\\xc0\\xff' * w for _ in range(h))
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[1], 'wb').write(b'\\x89PNG\\r\\n\\x1a\\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
" "$@"
}
png "$work/shots-in/Side View.PNG" 450 254 || fail "could not write a showcase picture"
png "$work/shots-in/tiny.png" 50 30 || fail "could not write a tiny picture"
cp "$work/kit/mod.ini" "$work/card.before"
"$editor" "$work/kit" --frames 3 --add-photo "$work/shots-in/Side View.PNG" --add-photo "$work/shots-in/Side View.PNG" --save --shot "$work/photos.png" >"$work/log" 2>&1 || fail "Add photo and Save: $(cat "$work/log")"
grep -q '^photos=side-view.png,side-view-2.png$' "$work/kit/mod.ini" || fail "photos= is not the two safe names: $(cat "$work/kit/mod.ini")"
[ -s "$work/kit/photos/side-view.png" ] && [ -s "$work/kit/photos/side-view-2.png" ] || fail "the photos were not copied into the car's folder"
cmp -s "$work/shots-in/Side View.PNG" "$work/kit/photos/side-view.png" || fail "the copy is not the picture"
[ -s "$work/photos.png" ] || fail "no PNG of the Showcase photos"
ls "$work/kit/photos" | grep -q '\.tmp$' && fail "a staged file was left behind"
# (that the lists' rows share no ID is editor_card_ids')
"$editor" "$work/kit" --frames 2 --add-photo "$work/shots-in/tiny.png" >"$work/log" 2>&1 && fail "a 50x30 picture was accepted as a showcase photo"
grep -q "50x30" "$work/log" || fail "no word about the size: $(cat "$work/log")"
[ -e "$work/kit/photos/tiny.png" ] && fail "a refused picture was copied"
"$editor" "$work/kit" --frames 2 --add-photo "$work/shots-in/nothing.png" >"$work/log" 2>&1 && fail "a picture that is not there was accepted"
cp "$work/kit/mod.ini" "$work/card.after"
# a photo added and not saved waits as name.new: no PNG of a photo's name, which the piece's check would count, and the card is not changed
"$editor" "$work/kit" --frames 2 --add-photo "$work/shots-in/Side View.PNG" >"$work/log" 2>&1 || fail "Add photo without Save: $(cat "$work/log")"
[ -s "$work/kit/photos/side-view-3.png.new" ] || fail "an unsaved photo is not staged"
[ -e "$work/kit/photos/side-view-3.png" ] && fail "an unsaved photo is a PNG of a photo's name"
cmp -s "$work/kit/mod.ini" "$work/card.after" || fail "an unsaved photo changed the card"
find "$work/kit/photos" -name '*.new' -delete
# a card listing a photo the folder has not: Save refuses with the name
"$editor" "$work/kit" --frames 2 --set photos=ghost.png --save >"$work/log" 2>&1 && fail "Save went with a listed photo that is not in the folder"
grep -q "ghost.png" "$work/log" || fail "no word about the missing photo: $(cat "$work/log")"
# the copies are the author's now (a later run did not make them): a shorter list is written and the files stay
"$editor" "$work/kit" --frames 2 --set photos=side-view.png --save >"$work/log" 2>&1 || fail "Save of a shorter list: $(cat "$work/log")"
grep -q '^photos=side-view.png$' "$work/kit/mod.ini" || fail "the shorter list was not written"
[ -s "$work/kit/photos/side-view-2.png" ] || fail "Save removed a file this run did not make"
"$editor" "$work/kit" --frames 2 --set photos=side-view.png,side-view.png --save >"$work/log" 2>&1 && fail "Save went with a photo named twice"
grep -q "Showcase photos" "$work/log" || fail "no word about the photos: $(cat "$work/log")"
grep -q '^photos=side-view.png$' "$work/kit/mod.ini" || fail "a refused Save changed the card"
cp "$work/card.before" "$work/kit/mod.ini"
rm -f "$work/kit/photos/side-view.png" "$work/kit/photos/side-view-2.png"

# ==== Rehacer is gone ====
# Build replaces the kit script: --rehacer is refused with the usage, and no run makes a backup folder in the kit
"$editor" "$work/kit" --frames 2 --rehacer >"$work/log" 2>&1 && fail "--rehacer was taken: Rehacer is gone"
grep -q "^usage: " "$work/log" || fail "--rehacer was not refused with the usage: $(cat "$work/log")"
[ ! -d "$work/kit/.studio-backup" ] || fail "a run made a backup folder in the kit"

# ==== The ImGui error guard ====
# A panel that breaks ImGui's rules (an EndDisabled with no BeginDisabled...) makes a headless run fail, with the error on stderr; a clean run does not
"$editor" "$work/kit" --frames 2 --steps ui-fault >"$work/log" 2>&1 && fail "a deliberate ImGui error did not fail the run"
grep -q "^imgui error: " "$work/log" || fail "the ImGui error was not printed: $(cat "$work/log")"
grep -q "ImGui reported 1 error" "$work/log" || fail "the run did not say it failed on an ImGui error: $(cat "$work/log")"
"$editor" "$work/kit" --frames 2 >"$work/log" 2>&1 || fail "a clean run failed: $(cat "$work/log")"
grep -q "imgui error" "$work/log" && fail "a clean run reported an ImGui error: $(cat "$work/log")"

# ==== The mask view (read-only) ====
# The mask view (read-only): a car file with no textures offers none (the fixture's is not a real one); with the player's real game files a traffic car's mask is opened and
# drawn, a change made to its file from outside (an image editor) shows in the picture, Studio never writes it, and a texture the car has no mask for is refused
"$editor" "$work/exports/wagon-masks" --mask side --frames 2 >"$work/log" 2>&1 && fail "a mask was opened of a car file that has no textures"
grep -q "no texture side" "$work/log" || fail "no word that the car file has no such texture: $(cat "$work/log")"
repo=$(cd "$(dirname "$0")/.." && pwd)
if [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then
    mkdir "$work/real"
    "$editor" --export-car Wagon --into "$work/real" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 || fail "export of the real Wagon: $(cat "$work/log")"
    [ -f "$work/real/wagon/mod.ini" ] && [ -f "$work/real/wagon/art-generated/car_masks/side_mask.png" ] || fail "the real Wagon's export is not a kit: $(ls "$work/real/wagon")"
    grep -q "do not share" "$work/log" || fail "no word that the files are the game's: $(cat "$work/log")"
    [ ! -e "$work/real/wagon-masks" ] && [ ! -e "$work/real/wagon.exporting" ] && [ ! -e "$work/real/wagon.project" ] || fail "the export left a masks project or a scratch folder: $(ls "$work/real")"
    "$editor" --export-car Wagon --into "$work/real" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 && fail "a second export into the same folder went"
    grep -q "already there" "$work/log" || fail "no word about the folder that is there: $(cat "$work/log")"
    m="$work/real/wagon/art-generated/car_masks/side_mask.png"
    "$editor" "$work/real/wagon" --mask side --view night --lights head --preview-tex side --frames 14 --shot "$work/mask-before.png" >"$work/log" 2>&1 || fail "mask view of the real Wagon: $(cat "$work/log")"
    cmp -s "$m" "$repo/data/car_masks/Wagon/side_mask.png" || fail "opening a mask changed the file"
    python3 -I "$(dirname "$0")/mask_poke.py" "$m" 142 26 8 5 248 || fail "the outside change of the mask"
    cp "$m" "$work/mask-poked.png"
    "$editor" "$work/real/wagon" --mask side --view night --lights head --preview-tex side --frames 14 --shot "$work/mask-after.png" >"$work/log" 2>&1 || fail "mask view of the changed mask: $(cat "$work/log")"
    cmp -s "$m" "$work/mask-poked.png" || fail "the mask view wrote the mask file"
    ls "$work/real/wagon/art-generated/car_masks" | grep -q "saving" && fail "a temporary file of a save was left behind"
    cmp -s "$work/mask-before.png" "$work/mask-after.png" && fail "the change made from outside did not change the picture"
    [ -s "$work/mask-after.png" ] || fail "no PNG of the mask view"
    "$editor" "$work/real/wagon" --mask nosuch --frames 2 >"$work/log" 2>&1 && fail "a texture the car has not was opened"
    "$editor" "$work/real/wagon" --mask side --mask-rect 1,2,3,4,5 --frames 2 >"$work/log" 2>&1 && fail "--mask-rect was taken: masks are no longer painted in Studio"
    grep -q "^usage: " "$work/log" || fail "an option that is gone was not refused with the usage: $(cat "$work/log")"
    "$editor" "$work/real/wagon" --mask side --mask-save --frames 2 >"$work/log" 2>&1 && fail "--mask-save was taken: masks are no longer saved by Studio"
    grep -q "^usage: " "$work/log" || fail "an option that is gone was not refused with the usage: $(cat "$work/log")"
fi

fi

if want b; then
if [ "$part" = b ] && [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then   # what part a leaves for the blocks below: the real Wagon's kit, its side mask changed from outside
    mkdir "$work/real"
    "$editor" --export-car Wagon --into "$work/real" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 || fail "export of the real Wagon: $(cat "$work/log")"
    python3 -I "$(dirname "$0")/mask_poke.py" "$work/real/wagon/art-generated/car_masks/side_mask.png" 142 26 8 5 248 || fail "the outside change of the mask"
fi
# ==== Preview: a paint, Day and Night ====
if [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then
    "$editor" --export-car P911 --into "$work/real" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 || fail "export of the real 911: $(cat "$work/log")"
    "$editor" "$work/real/p911" --frames 14 --shot "$work/preview-file.png" >"$work/log" 2>&1 || fail "Preview of the real 911: $(cat "$work/log")"
    "$editor" "$work/real/p911" --paint 3 --frames 14 --shot "$work/preview-paint.png" >"$work/log" 2>&1 || fail "Preview of the real 911 in a paint: $(cat "$work/log")"
    cmp -s "$work/preview-file.png" "$work/preview-paint.png" && fail "a paint did not change the Preview"
    ls "${TMPDIR:-/tmp}" | grep -q '^studio-preview-' && fail "a scratch folder of the Preview was left behind"
    # «Use as card photo» (the Model window): card.png of 309 x 166, opaque, in the project's piece; the step is the button's own code
    "$editor" "$work/real/p911" --paint 3 --steps card-photo,status --frames 14 >"$work/log" 2>&1 || fail "Use as card photo: $(cat "$work/log")"
    grep -q "status: card.png made" "$work/log" || fail "no word that card.png was made: $(cat "$work/log")"
    card=$(find "$work/real/p911" -name card.png | head -1)
    [ -n "$card" ] || fail "no card.png in the project after Use as card photo"
    [ ! -e "$card.tmp" ] || fail "a staged card.png.tmp was left behind"
    python3 -c "
import sys
d = open(sys.argv[1], 'rb').read()
assert d[:8] == b'\x89PNG\r\n\x1a\n' and int.from_bytes(d[16:20], 'big') == 309 and int.from_bytes(d[20:24], 'big') == 166 and d[25] == 6
" "$card" || fail "card.png is not a 309x166 RGBA PNG"
    # the Model window's Card view and «As on the card» (Central's palette, from the game's files): three other pictures of the same car
    "$editor" "$work/real/p911" --cd "$repo/original" --paint 3 --frames 14 --shot "$work/model-plain.png" >"$work/log" 2>&1 || fail "the Model window: $(cat "$work/log")"
    "$editor" "$work/real/p911" --cd "$repo/original" --paint 3 --steps card-view:1,0 --frames 14 --shot "$work/model-card.png" >"$work/log" 2>&1 || fail "Card view: $(cat "$work/log")"
    "$editor" "$work/real/p911" --cd "$repo/original" --paint 3 --steps card-view:1,1 --frames 14 --shot "$work/model-card-central.png" >"$work/log" 2>&1 || fail "As on the card: $(cat "$work/log")"
    cmp -s "$work/model-plain.png" "$work/model-card.png" && fail "Card view did not change the Model window"
    cmp -s "$work/model-card.png" "$work/model-card-central.png" && fail "As on the card did not change the Card view"
    # the card photo's backdrop: a plain colour, a gradient and the person's own picture each give another Card view, and Use as card photo saves the one on screen
    python3 -c "
import struct, sys, zlib
w, h = 40, 20
raw = b''.join(b'\\x00' + b'\\x10\\xc0\\x30\\xff' * w for _ in range(h))
def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[1], 'wb').write(b'\\x89PNG\\r\\n\\x1a\\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
" "$work/backdrop.png" || fail "could not write the backdrop picture"
    for kind in "plain:card-backdrop:0,200,40,40" "gradient:card-backdrop:1,200,40,40,card-bd-bottom:20,20,200"; do
        "$editor" "$work/real/p911" --cd "$repo/original" --paint 3 --steps "card-view:1,0,${kind#*:}" --frames 14 --shot "$work/model-${kind%%:*}.png" >"$work/log" 2>&1 || fail "backdrop ${kind%%:*}: $(cat "$work/log")"
    done
    "$editor" "$work/real/p911" --cd "$repo/original" --paint 3 --card-backdrop "$work/backdrop.png" --steps card-view:1,0 --frames 14 --shot "$work/model-picture.png" >"$work/log" 2>&1 || fail "backdrop picture: $(cat "$work/log")"
    cmp -s "$work/model-card.png" "$work/model-plain.png" && fail "a backdrop did not change the Card view"
    cmp -s "$work/model-card.png" "$work/model-gradient.png" && fail "a gradient did not change the Card view"
    cmp -s "$work/model-gradient.png" "$work/model-picture.png" && fail "a backdrop picture did not change the Card view"
    cp "$card" "$work/card-before.png"
    "$editor" "$work/real/p911" --cd "$repo/original" --paint 3 --steps card-backdrop:0,200,40,40,card-photo,status --frames 14 >"$work/log" 2>&1 || fail "Use as card photo on a backdrop: $(cat "$work/log")"
    cmp -s "$card" "$work/card-before.png" && fail "the backdrop did not reach card.png"
    # the night view: Day, Night with the lights off and Night with the headlights each show another picture of the front texture
    "$editor" "$work/real/p911" --view day --preview-tex frnt --frames 20 --shot "$work/night-day.png" >"$work/log" 2>&1 || fail "Preview by day: $(cat "$work/log")"
    "$editor" "$work/real/p911" --view night --lights none --preview-tex frnt --frames 20 --shot "$work/night-dark.png" >"$work/log" 2>&1 || fail "Preview at night, lights off: $(cat "$work/log")"
    "$editor" "$work/real/p911" --view night --lights head --preview-tex frnt --frames 20 --shot "$work/night-head.png" >"$work/log" 2>&1 || fail "Preview at night, headlights: $(cat "$work/log")"
    cmp -s "$work/night-day.png" "$work/night-dark.png" && fail "Night did not change the Preview"
    cmp -s "$work/night-dark.png" "$work/night-head.png" && fail "the headlights did not light"
    "$editor" "$work/real/p911" --view night --lights head --night-tint 0.9,0.3,0.2 --preview-tex frnt --frames 20 --shot "$work/night-tint.png" >"$work/log" 2>&1 || fail "Preview at night with a tint: $(cat "$work/log")"
    cmp -s "$work/night-head.png" "$work/night-tint.png" && fail "the tint did not change the night picture"

    # the exported-original flow, as the owner works it (a kit): open the export, change a mask from outside, then race it with the game's own car
    p911="$work/real/p911"
    m="$p911/art-generated/car_masks/frnt_mask.png"
    [ -f "$m" ] || fail "the exported 911 has no art-generated/car_masks/frnt_mask.png"
    cp "$m" "$work/frnt-before.png"
    python3 -I "$(dirname "$0")/mask_poke.py" "$m" 4 4 6 6 200 || fail "the outside change of the exported 911's mask"
    "$editor" "$p911" --mask frnt --frames 8 >"$work/log" 2>&1 || fail "mask view on the exported 911: $(cat "$work/log")"
    cmp -s "$m" "$work/frnt-before.png" && fail "the outside change of the exported 911's mask was not written"
    cat > "$work/fake-tnfs-911" <<'FAKE911'
#!/bin/sh
user=; shot=
while [ $# -gt 0 ]; do case "$1" in --user) user=$2; shift;; --screenshot) shot=$2; shift;; esac; shift; done
printf 'race car: slot 0 is P911\n' > "$user/spirited-drive.log"
printf '%s\n' "$TNFS_CAR_MASKS" > "$user/masks-given"
printf 'P6\n640 480\n255\n' > "$shot"; head -c 921600 /dev/zero >> "$shot"
FAKE911
    chmod +x "$work/fake-tnfs-911"
    "$editor" "$p911" --game "$work/fake-tnfs-911" --cd "$repo/original" --race-photo --race-png "$work/photo911.png" --frames 4 >"$work/log" 2>&1 || fail "Race photo of the exported 911: $(cat "$work/log")"
    [ -s "$work/photo911.png" ] || fail "no race photo of the exported 911"
    grep -q "^Photo: Porsche 911 Carrera" "$work/log" || fail "the race of the exported 911 said nothing: $(cat "$work/log")"
fi

# ==== Model ====
# the Model window: a car file it cannot draw says so and the run goes on; --orbit is YAW,PITCH[,DIST] or refused; with the CD's P911 two orbits are two pictures
"$editor" "$work/kit" --frames 3 --orbit 90,10,500 --shot "$work/stub.png" >"$work/log" 2>&1 || fail "run with an orbit on a car it cannot draw: $(cat "$work/log")"
"$editor" "$work/kit" --frames 2 --orbit north >"$work/log" 2>&1 && fail "a bad --orbit was accepted"
grep -q "YAW,PITCH" "$work/log" || fail "no word about --orbit: $(cat "$work/log")"
p911="$(cd "$(dirname "$0")/.." && pwd)/original/SIMDATA/CARFAMS/P911.CFM"
if [ -f "$p911" ]; then
    k911="$work/k911"   # a piece folder: mod.ini, the CD's car file, masks under car_masks/P911/ (the shape Export original makes)
    mkdir -p "$k911/SIMDATA/CARFAMS" "$k911/car_masks/P911"
    printf 'id=k911\nname=K911\nversion=1\ntype=car\nstem=P911\nsound_from=911\n' > "$k911/mod.ini"
    cp "$p911" "$k911/SIMDATA/CARFAMS/P911.CFM"
    printf 'x' > "$k911/car_masks/P911/rsid_mask.png"
    "$editor" "$k911" --frames 4 --orbit 215,18 --shot "$work/m1.png" >"$work/log" 2>&1 || fail "run on the 911: $(cat "$work/log")"
    "$editor" "$k911" --frames 4 --orbit 90,18 --shot "$work/m2.png" >"$work/log" 2>&1 || fail "run on the 911, another orbit: $(cat "$work/log")"
    "$editor" "$k911" --frames 4 --orbit 215,18 --shot "$work/m3.png" >"$work/log" 2>&1 || fail "run on the 911 again: $(cat "$work/log")"
    cmp -s "$work/m1.png" "$work/m2.png" && fail "two orbits drew the same window"
    cmp -s "$work/m1.png" "$work/m3.png" || fail "the same orbit drew two different windows"
    # the body shine (M5): on by default, off is another picture of the same orbit; a bad --shine is refused
    "$editor" "$k911" --frames 4 --orbit 215,18 --shine off --shot "$work/m4.png" >"$work/log" 2>&1 || fail "run on the 911 without the shine: $(cat "$work/log")"
    cmp -s "$work/m1.png" "$work/m4.png" && fail "the shine off drew the same window as on"
    "$editor" "$k911" --frames 2 --shine maybe >"$work/log" 2>&1 && fail "a bad --shine was accepted"
    grep -q "on or off" "$work/log" || fail "no word about --shine: $(cat "$work/log")"
    # the Wheels (Still / Slow / Fast): three pictures, each the same every run (Slow shows its first frame headless); a bad --wheels is refused
    for w in still slow fast; do
        "$editor" "$work/k911" --frames 4 --orbit 270,0 --wheels $w --shot "$work/w_$w.png" >"$work/log" 2>&1 || fail "run on the 911 with the wheels $w: $(cat "$work/log")"
    done
    "$editor" "$work/k911" --frames 4 --orbit 270,0 --wheels slow --shot "$work/w_slow2.png" >"$work/log" 2>&1 || fail "run on the 911 with slow wheels again"
    cmp -s "$work/w_still.png" "$work/w_slow.png" && fail "Wheels Still and Slow drew the same window"
    cmp -s "$work/w_slow.png" "$work/w_fast.png" && fail "Wheels Slow and Fast drew the same window"
    cmp -s "$work/w_still.png" "$work/w_fast.png" && fail "Wheels Still and Fast drew the same window"
    cmp -s "$work/w_slow.png" "$work/w_slow2.png" || fail "headless Wheels Slow drew two different windows"
    # at night with the headlights on the wheel layer follows the frame too (the lamp layer is made again when the wheels change)
    "$editor" "$work/k911" --frames 4 --orbit 270,0 --view night --lights head,side --wheels still --shot "$work/wn_still.png" >"$work/log" 2>&1 || fail "run at night with still wheels: $(cat "$work/log")"
    "$editor" "$work/k911" --frames 4 --orbit 270,0 --view night --lights head,side --wheels fast --shot "$work/wn_fast.png" >"$work/log" 2>&1 || fail "run at night with fast wheels: $(cat "$work/log")"
    cmp -s "$work/wn_still.png" "$work/wn_fast.png" && fail "Wheels Still and Fast drew the same window at night"
    "$editor" "$work/k911" --frames 2 --wheels spinning >"$work/log" 2>&1 && fail "a bad --wheels was accepted"
    cmp -s "$work/m1.png" "$work/stub.png" && fail "the 911 looks like a car that cannot be drawn"
fi

# ==== In-game photo (a fake game) ====
# In-game photo with a fake game (a script that writes what the real one does): a masks project of a traffic car finds the car in the draw, follows its slot and shows
# the picture; the picture is written as a PNG; a game that fails is told, with a non-zero status; a car of the game's that is not one asks for an export first
cat > "$work/fake-tnfs" <<'FAKE'
#!/bin/sh
user=; shot=
while [ $# -gt 0 ]; do case "$1" in --user) user=$2; shift;; --screenshot) shot=$2; shift;; esac; shift; done
[ "${FAKE_FAIL:-}" = 1 ] && { echo "error: the fake game fails" >&2; exit 3; }
printf 'race car: slot 0 is TSupra\nrace car: slot 3 is Wagon\n' > "$user/spirited-drive.log"
if grep -q '^render_scale=2$' "$user/restomod.cfg"; then   # the size the game draws at: render scale 2, twice each way
    printf 'P6\n1280 960\n255\n' > "$shot"; head -c 3686400 /dev/zero >> "$shot"
else
    printf 'P6\n640 480\n255\n' > "$shot"; head -c 921600 /dev/zero >> "$shot"
fi
FAKE
chmod +x "$work/fake-tnfs"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --photo-view --night --race-png "$work/photo.png" --frames 4 --shot "$work/racepanel.png" >"$work/log" 2>&1 || fail "In-game photo with a fake game: $(cat "$work/log")"
grep -q "Photo: the camera follows the Wagon (slot 3" "$work/log" || fail "no word about the slot followed: $(cat "$work/log")"
python3 -c "
import sys
d = open(sys.argv[1], 'rb').read()
assert d[:8] == b'\x89PNG\r\n\x1a\n' and int.from_bytes(d[16:20], 'big') == 1280 and int.from_bytes(d[20:24], 'big') == 960
" "$work/photo.png" || fail "the in-game photo is not a 1280x960 PNG (render scale 2, the default)"
# the Race window's switches (T6): render scale 1 is 640x480, and what is off is said in the summary
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --race-scale 1 --race-off reflections --race-png "$work/photo1.png" --frames 2 >"$work/log" 2>&1 || fail "In-game photo at scale 1: $(cat "$work/log")"
grep -q "(Reflections off, 640x480)" "$work/log" || fail "the summary does not say what was off: $(cat "$work/log")"
python3 -c "
import sys
d = open(sys.argv[1], 'rb').read()
assert int.from_bytes(d[16:20], 'big') == 640 and int.from_bytes(d[20:24], 'big') == 480
" "$work/photo1.png" || fail "the in-game photo at render scale 1 is not 640x480"
[ -s "$work/racepanel.png" ] || fail "no PNG of the Race window"
# a switch that is not one, a scale that is not 1 or 2, or the look without a In-game photo: refused, never a photo with the whole look
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --race-off reflection --frames 2 >"$work/log" 2>&1 && fail "--race-off with an unknown switch was taken"
grep -q "reflections, car_colours" "$work/log" || fail "an unknown --race-off word is not named: $(cat "$work/log")"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --race-scale 3 --frames 2 >"$work/log" 2>&1 && fail "--race-scale 3 was taken"
"$editor" "$work/exports/wagon-masks" --race-scale 1 --frames 2 >"$work/log" 2>&1 && fail "--race-scale without --race-photo was taken"
FAKE_FAIL=1 "$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --frames 2 >"$work/log" 2>&1 && fail "a game that failed was told as a success"
grep -q "exit code 3" "$work/log" || fail "the game's exit code is not told: $(cat "$work/log")"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --track TR5 --frames 2 >"$work/log" 2>&1 && fail "a traffic car was raced on a circuit"
grep -q "no traffic" "$work/log" || fail "no word about the circuit: $(cat "$work/log")"
ls "${TMPDIR:-/tmp}" | grep -q '^studio-race-' && fail "a scratch folder of a race was left behind"

# ==== Sheets (a fake game): Day and Night of a car in one labelled picture ====
# a fake game that draws the day red and the night green (the colour of a whole 1280x960 frame): Sheet takes both, in order, and the PNG written has them side by side
# (halved to 640x480 each, a label band under); a game that fails leaves cells that say so, and the sheet still saves
cat > "$work/fake-sheet-tnfs" <<'FAKE'
#!/bin/sh
user=; shot=
while [ $# -gt 0 ]; do case "$1" in --user) user=$2; shift;; --screenshot) shot=$2; shift;; esac; shift; done
[ "${FAKE_FAIL:-}" = 1 ] && { echo "error: the fake game fails" >&2; exit 3; }
printf 'race car: slot 0 is TSupra\nrace car: slot 3 is Wagon\n' > "$user/spirited-drive.log"
if grep -q '^light_time=night$' "$user/restomod.cfg"; then rgb='0,200,0'; else rgb='200,0,0'; fi
python3 -c "
import sys
r, g, b = map(int, sys.argv[2].split(','))
open(sys.argv[1], 'wb').write(b'P6\n1280 960\n255\n' + bytes([r, g, b]) * (1280 * 960))
" "$shot" "$rgb"
FAKE
chmod +x "$work/fake-sheet-tnfs"
rm -rf "$work/exports/wagon-masks/sheets"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-sheet-tnfs" --cd "$work/game/cd" --frames 4 --steps sheet,print-sheet,sheet-save >"$work/log" 2>&1 || fail "the car sheet: $(cat "$work/log")"
grep -q "^sheet 2: Day=done Night=done$" "$work/log" || fail "the sheet did not take Day and Night, in order: $(grep '^sheet\|--steps' "$work/log")"
sheet=$(sed -n 's/^sheet saved: //p' "$work/log")
[ -s "$sheet" ] && [ "$(basename "$(dirname "$sheet")")" = sheets ] && [ -s "$work/exports/wagon-masks/sheets/$(basename "$sheet")" ] || fail "Save sheet did not write a PNG in the project's sheets folder: $(grep '^sheet' "$work/log")"
python3 - "$sheet" <<'PY' || fail "the saved sheet is not Day (red) and Night (green) side by side, 1280x500"
import struct, sys, zlib
d = open(sys.argv[1], 'rb').read()
assert d[:8] == b'\x89PNG\r\n\x1a\n'
w, h = struct.unpack('>II', d[16:24])
assert (w, h) == (1280, 500), (w, h)
pos, idat = 8, b''
while pos < len(d):
    n, kind = struct.unpack('>I4s', d[pos:pos + 8])
    if kind == b'IDAT': idat += d[pos + 8:pos + 8 + n]
    pos += 12 + n
raw, stride = zlib.decompress(idat), w * 4 + 1
rows, prev = [], bytearray(w * 4)
for y in range(h):
    f, line = raw[y * stride], bytearray(raw[y * stride + 1:(y + 1) * stride])
    for i in range(len(line)):
        a = line[i - 4] if i >= 4 else 0
        b = prev[i]
        c = prev[i - 4] if i >= 4 else 0
        if f == 1: line[i] = (line[i] + a) & 255
        elif f == 2: line[i] = (line[i] + b) & 255
        elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
        elif f == 4:
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
    rows.append(line); prev = line
px = lambda x, y: tuple(rows[y][x * 4:x * 4 + 3])
assert px(100, 100) == (200, 0, 0), ('Day cell', px(100, 100))
assert px(640 + 100, 100) == (0, 200, 0), ('Night cell', px(740, 100))
assert any(px(x, 490) != px(639, 499) for x in range(0, 200)), 'the label band has no text'
PY
FAKE_FAIL=1 "$editor" "$work/exports/wagon-masks" --game "$work/fake-sheet-tnfs" --cd "$work/game/cd" --frames 4 --steps sheet,print-sheet,sheet-save >"$work/log" 2>&1 || fail "a sheet of a game that fails: $(cat "$work/log")"
grep -q "^sheet 2: Day=failed Night=failed$" "$work/log" && grep -q "^sheet saved: " "$work/log" || fail "a game that fails is not two failed cells that still save: $(cat "$work/log")"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-sheet-tnfs" --cd "$work/game/cd" --frames 4 --steps sheet-save >"$work/log" 2>&1 && fail "Save sheet with no sheet taken wrote a file"
ls "${TMPDIR:-/tmp}" | grep -q '^studio-race-' && fail "a scratch folder of a sheet was left behind"

# ==== Changes from the game's masks ====
# Changes from the game's masks: the fixture's exported Wagon has the game's own file; with the real game files an edit shows as a changed mask with its texels
"$editor" "$work/exports/wagon-masks" --cd "$work/game/cd" --port-data "$work/game/data" --changes --frames 2 >"$work/log" 2>&1 || fail "--changes of the fixture's Wagon: $(cat "$work/log")"
grep -q "car_masks/Wagon: 0 changed, 0 new, 0 missing, 1 unchanged" "$work/log" || fail "an unchanged export is told as changed: $(cat "$work/log")"
if [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then
    "$editor" "$work/real/wagon" --cd "$repo/original" --port-data "$repo/data" --changes --frames 2 >"$work/log" 2>&1 || fail "--changes of the real Wagon: $(cat "$work/log")"
    grep -q "side_mask.png" "$work/log" || fail "the edited mask is not in the changes: $(cat "$work/log")"
    grep -q "1 changed" "$work/log" || fail "no count of changes: $(cat "$work/log")"
    grep -q "headlight +40" "$work/log" || fail "no word of the texels' classes: $(cat "$work/log")"
fi
# an untouched export of a drivable car (the 911) is none of the changes: its masks are the game's own and the dash mask is not among car_masks'
if [ -r "$repo/original/SIMDATA/CARFAMS/P911.CFM" ] && [ -d "$repo/data/car_masks/P911" ]; then
    mkdir "$work/real911"
    "$editor" --export-car P911 --into "$work/real911" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 || fail "export of the real 911: $(cat "$work/log")"
    kit911=$(ls -d "$work"/real911/*/ | head -1)   # its folder is named after the car (p911 here)
    "$editor" "$kit911" --cd "$repo/original" --port-data "$repo/data" --changes --frames 2 >"$work/log" 2>&1 || fail "--changes of the real 911: $(cat "$work/log")"
    grep -q "car_masks/P911: 0 changed, 0 new, 0 missing, 7 unchanged" "$work/log" || fail "an untouched 911 export shows changes: $(cat "$work/log")"
fi
# ==== Workspace: one folder with cars/ and tracks/, listed on the start screen, where the dialogs start ====
ws="$work/workspace"
mkdir -p "$ws"
"$editor" "$work/emptyws" --workspace "$work/nothing-here" --frames 2 >"$work/log" 2>&1
grep -q "must be a folder that exists" "$work/log" || fail "a workspace folder that does not exist was taken: $(cat "$work/log")"
"$editor" --user "$work/u-ws" --frames 6 --steps print-workspace,dialog-dir:0 >"$work/log" 2>&1 || fail "no workspace: $(cat "$work/log")"
[ "$(grep -E '^(workspace|offer|dialog):' "$work/log" | tr '\n' '|')" = "workspace: none|offer: 1|dialog: |" ] || fail "with none, the start screen offers one and the dialogs start where the system puts them: $(cat "$work/log")"
mkdir -p "$ws/cars/zed/model/near/bank" "$ws/cars/Alpha/model/near/bank" "$ws/cars/not-a-car" "$ws/tracks/not-a-track"
for k in zed Alpha; do printf 'id=%s\nname=K\nversion=1\ntype=car\nstem=K1\nsound_from=911\n' "$(echo "$k" | tr 'A-Z' 'a-z')" > "$ws/cars/$k/mod.ini"; done
"$editor" "$ws/tracks/loop" --frames 2 --save >"$work/log" 2>&1 || fail "a track made in the workspace: $(cat "$work/log")"
"$editor" --user "$work/u-ws" --workspace "$ws" --frames 6 --steps print-workspace,dialog-dir:0,dialog-dir:1,dialog-dir:2,no-workspace,print-workspace,dialog-dir:0 >"$work/log" 2>&1 || fail "a workspace: $(cat "$work/log")"
got=$(grep -E '^(workspace|offer|cars|tracks|dialog):' "$work/log" | sed "s|$ws|WS|g" | tr '\n' '|')
want="workspace: WS|offer: 0|cars: Alpha zed|tracks: loop|dialog: WS/tracks|dialog: WS/cars|dialog: WS|workspace: none|offer: 0|cars:|tracks:|dialog: |"
[ "$got" = "$want" ] || fail "the workspace lists its cars and tracks, the dialogs start in its cars/ and tracks/, and No workspace puts it aside: $got"
[ -d "$ws/cars" ] && [ -d "$ws/tracks" ] || fail "the workspace has no cars/ and tracks/"
for pick in "0,0" "0,1" "1,0"; do
    "$editor" --user "$work/u-ws" --workspace "$ws" --frames 6 --steps "open-ws:$pick,status" >"$work/log" 2>&1 || fail "open-ws:$pick: $(cat "$work/log")"
done
"$editor" --user "$work/u-ws" --workspace "$ws" --frames 6 --steps open-ws:1,7 >"$work/log" 2>&1 && fail "a track the workspace does not list was opened"
"$editor" --user "$work/u-ws" --workspace "$ws" --window 960x600 --frames 6 --shot "$work/ws-start.png" >"$work/log" 2>&1 || fail "the start screen of a workspace: $(cat "$work/log")"
"$editor" --user "$work/u-ws2" --window 960x600 --frames 6 --shot "$work/ws-offer.png" >"$work/log" 2>&1 || fail "the start screen with the offer: $(cat "$work/log")"
[ -s "$work/ws-start.png" ] && [ -s "$work/ws-offer.png" ] && ! cmp -s "$work/ws-start.png" "$work/ws-offer.png" || fail "the start screen does not change with a workspace"
# ==== Two rooms: the main menu, Cars, Tracks; each room's File menu holds its own actions only, New before Open ====
rooms=$("$editor" --user "$work/u-ws" --workspace "$ws" --frames 6 --steps print-menu,open-ws:1,0,print-menu,main-menu,print-menu,open-ws:0,0,print-menu,main-menu,print-menu 2>&1 | grep -E '^(room|file):' | tr '\n' '|')
want="room: hub|file: Workspace | Settings... | Quit|room: tracks|file: New track... | Open track... | New track from an original... | Open recent | Settings... | Save | Main menu | Quit|room: hub|file: Workspace | Settings... | Quit|room: cars|file: New car... | Open car... | Open recent | Settings... | Save | Main menu | Quit|room: hub|file: Workspace | Settings... | Quit|"
[ "$rooms" = "$want" ] || fail "hub, room, Main menu, other room: the File menu of each holds its own actions: $rooms"
# New track with a workspace asks only for the name: the id comes from it, the folder is made in tracks/, a folder that exists is refused and nothing is written
"$editor" --user "$work/u-ws" --workspace "$ws" --frames 6 --steps new-track:7,print-menu >"$work/log" 2>&1 || fail "New track in a workspace: $(cat "$work/log")"
grep -q '^id=road-7$' "$ws/tracks/road-7/project.ini" && grep -q '^name=Road 7$' "$ws/tracks/road-7/project.ini" || fail "the new track's id and name: $(cat "$ws/tracks/road-7/project.ini")"
before=$(cksum < "$ws/tracks/road-7/project.ini")
"$editor" --user "$work/u-ws" --workspace "$ws" --frames 6 --steps new-track:7 >"$work/log" 2>&1 && fail "a track whose folder exists was made again"
grep -q 'already exists' "$work/log" || fail "the refusal does not say the folder exists: $(cat "$work/log")"
[ "$before" = "$(cksum < "$ws/tracks/road-7/project.ini")" ] || fail "a refused New track wrote into the existing project"
# A track just made is not "modified": nothing was changed since its folder was written, so the title has no unsaved mark (and Cmd+W / Quit ask no question)
"$editor" --user "$work/u-ws" --workspace "$ws" --frames 6 --steps new-track:8,title >"$work/log" 2>&1 || fail "New track (road-8): $(cat "$work/log")"
[ "$(grep '^title: ' "$work/log")" = "title: Spirited Drive Studio — road-8" ] || fail "a track just made shows as modified: $(grep '^title: ' "$work/log")"
# ==== Window menu: every panel a car's window draws is listed (a masks project's too) ====
for proj in "$work/kit" "$work/exports/wagon-masks"; do
    panels=$("$editor" "$proj" --window 1280x960 --frames 6 --steps print-panels 2>&1)
    echo "$panels" | grep -q '^unlisted:$' || fail "a panel of $(basename "$proj") is drawn that the Window menu does not list: $(echo "$panels" | grep '^unlisted')"
    [ "$(echo "$panels" | grep -c '^panel [0-9]*: .*: open, drawn')" -ge 5 ] || fail "the panels of $(basename "$proj") are not drawn: $panels"
done
fi
[ "$failures" -eq 0 ] && echo "editor_headless_car: ok"
exit "$failures"
