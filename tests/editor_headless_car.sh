#!/bin/sh
# The editor on cars, headless, in the order a user goes: Export original car, open it (or a kit), edit and save its card, Rehacer,
# Edit mask and save it, Preview by day and night, the Model, In-game photo, Changes. The blocks that need the player's game files
# (original/) are skipped without them.
# usage: editor_headless_car.sh EDITOR
. "$(dirname "$0")/editor_headless_common.sh"

# ==== Export original car ====
# Export original car: a fixture game folder (a synthetic car file, masks that are PNG headers); a traffic car becomes a masks project, a refused car and a second
# export into the same folder say why and write nothing; the window of the choice draws
mkdir -p "$work/game/cd/SIMDATA/CARFAMS" "$work/game/data/car_masks/Wagon" "$work/exports"
printf 'cfm' > "$work/game/cd/SIMDATA/CARFAMS/WAGON.CFM"
printf 'cfm' > "$work/game/cd/SIMDATA/CARFAMS/PROBE.CFM"
python3 -c "
import sys, struct
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + struct.pack('>I4sIIBB', 13, b'IHDR', 100, 50, 8, 0) + bytes(20))
" "$work/game/data/car_masks/Wagon/side_mask.png"
export_opts="--cd $work/game/cd --port-data $work/game/data"
"$editor" --export-car Wagon --into "$work/exports" $export_opts --frames 3 --shot "$work/export.png" >"$work/log" 2>&1 || fail "export of a traffic car: $(cat "$work/log")"
grep -q "do not share" "$work/log" || fail "no word that the files are the game's: $(cat "$work/log")"
grep -q '^kind=masks$' "$work/exports/wagon-masks/project.ini" && grep -q '^redistributable=0$' "$work/exports/wagon-masks/project.ini" || fail "the masks project's project.ini"
cmp -s "$work/game/data/car_masks/Wagon/side_mask.png" "$work/exports/wagon-masks/car_masks/Wagon/side_mask.png" || fail "the exported mask is not the game's file"
[ -s "$work/export.png" ] || fail "no PNG of an exported masks project"
"$editor" --export-car Wagon --into "$work/exports" $export_opts --frames 2 >"$work/log" 2>&1 && fail "a second export into the same folder went"
grep -q "already there" "$work/log" || fail "no word about the folder that is there: $(cat "$work/log")"
"$editor" --export-car probe --into "$work/exports" $export_opts --frames 2 >"$work/log" 2>&1 && fail "a car with no masks was exported"
[ ! -e "$work/exports/probe-masks" ] && [ ! -e "$work/exports/probe-masks.exporting" ] || fail "a refused export left files"
"$editor" "$work/exports/wagon-masks" --frames 3 --shot "$work/masksproject.png" >"$work/log" 2>&1 || fail "a masks project did not open: $(cat "$work/log")"
[ ! -e "$work/exports/wagon-masks/trace.json" ] || fail "a masks project was made a track project"
"$editor" --ask export --pick Wagon $export_opts --frames 4 --shot "$work/exportdialog.png" >"$work/log" 2>&1 || fail "the export window did not draw: $(cat "$work/log")"
[ -s "$work/exportdialog.png" ] || fail "no PNG of the export window"
"$editor" --pick nosuchcar --frames 2 $export_opts >/dev/null 2>&1 && fail "--pick took a car that is not in the list"

# ==== Open: the start screen, a car's kit, a card that is not a car ====
# no PROJECT: the start screen (nothing is made anywhere); a car's kit (the current layout) opens as a car and writes nothing; an old coche/ + modelo/ kit and a card
# that is not a car are refused in words
"$editor" --frames 3 --shot "$work/start.png" >"$work/log" 2>&1 || fail "run with nothing to open: $(cat "$work/log")"
[ -s "$work/start.png" ] || fail "no PNG of the start screen"
"$editor" --ask opencar --frames 3 >"$work/log" 2>&1 || fail "run with Open car asked for on the start screen: $(cat "$work/log")"
mkdir -p "$work/kit/model/near/bank" "$work/kit/car_masks"
printf 'id=k1\nname=Kit One\nversion=1\ntype=car\nstem=K1\nsound_from=911\n' > "$work/kit/mod.ini"
printf 'x' > "$work/kit/car_masks/rsid_mask.png"
printf 'x' > "$work/kit/model/near/bank/0010_rsid.png"
"$editor" "$work/kit" --frames 3 --shot "$work/kit.png" >"$work/log" 2>&1 || fail "run on a car's kit: $(cat "$work/log")"
[ -s "$work/kit.png" ] || fail "no PNG of the car"
cmp -s "$work/kit.png" "$work/start.png" && fail "the car looks like the start screen"
[ ! -e "$work/kit/project.ini" ] || fail "opening a car made track files in it"
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

# ==== Rehacer ====
# Rehacer: a kit with no rehacer.command says so; unsaved card changes refuse it; the script runs with the kit as its folder after the kit is backed up, and its
# exit code is the run's
"$editor" "$work/kit" --frames 2 --rehacer >"$work/log" 2>&1 && fail "Rehacer went on a kit with no rehacer.command"
grep -q "no rehacer.command" "$work/log" || fail "no plain word about the missing script: $(cat "$work/log")"
printf '#!/bin/sh\necho "working in $(pwd)"\necho made > proof.txt\nexit ${REHACER_EXIT:-0}\n' > "$work/kit/rehacer.command"
"$editor" "$work/kit" --frames 2 --set year=1992 --rehacer >"$work/log" 2>&1 && fail "Rehacer ran with unsaved card changes"
grep -q "not saved" "$work/log" || fail "no word about the unsaved changes: $(cat "$work/log")"
[ ! -e "$work/kit/proof.txt" ] && [ ! -d "$work/kit/.studio-backup" ] || fail "a refused Rehacer ran or made a backup"
"$editor" "$work/kit" --frames 3 --rehacer --shot "$work/rehacer.png" >"$work/log" 2>&1 || fail "Rehacer did not run: $(cat "$work/log")"
grep -q "exit code 0" "$work/log" || fail "no exit code told: $(cat "$work/log")"
[ -f "$work/kit/proof.txt" ] || fail "the script did not run with the kit as its folder"
set -- "$work"/kit/.studio-backup/*/mod.ini
[ -f "$1" ] || fail "no backup of the kit before it ran"
[ -s "$work/rehacer.png" ] || fail "no PNG of a Rehacer run"
REHACER_EXIT=3 "$editor" "$work/kit" --frames 3 --rehacer >"$work/log" 2>&1 && fail "a script that failed was told as a success"
grep -q "exit code 3" "$work/log" || fail "the failing exit code is not told: $(cat "$work/log")"
rm -f "$work/kit/rehacer.command"

# ==== Edit mask and Save mask ====
# Edit mask: a car file with no textures offers none (the fixture's is not a real one); with the player's real game files a traffic car's mask is opened, painted, saved
# (a grey PNG of the same size) and drawn, and a texture the car has no mask for is refused
"$editor" "$work/exports/wagon-masks" --mask side --frames 2 >"$work/log" 2>&1 && fail "a mask was opened of a car file that has no textures"
grep -q "no texture side" "$work/log" || fail "no word that the car file has no such texture: $(cat "$work/log")"
repo=$(cd "$(dirname "$0")/.." && pwd)
if [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then
    mkdir "$work/real"
    "$editor" --export-car Wagon --into "$work/real" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 || fail "export of the real Wagon: $(cat "$work/log")"
    m="$work/real/wagon-masks/car_masks/Wagon/side_mask.png"
    "$editor" "$work/real/wagon-masks" --mask side --frames 8 --shot "$work/mask-before.png" >"$work/log" 2>&1 || fail "Edit mask of the real Wagon: $(cat "$work/log")"
    cmp -s "$m" "$repo/data/car_masks/Wagon/side_mask.png" || fail "opening a mask changed the file"
    "$editor" "$work/real/wagon-masks" --mask side --mask-rect 142,26,8,5,248 --mask-save --frames 8 --shot "$work/mask-after.png" >"$work/log" 2>&1 || fail "Edit mask with a painted rectangle: $(cat "$work/log")"
    grep -q "saved side_mask.png" "$work/log" || fail "no word that the mask was saved: $(cat "$work/log")"
    cmp -s "$m" "$repo/data/car_masks/Wagon/side_mask.png" && fail "the saved mask is the game's file: nothing was written"
    [ ! -e "$m.saving" ] || fail "a temporary file of the Save was left behind"
    cmp -s "$work/mask-before.png" "$work/mask-after.png" && fail "the edit did not change the picture"
    [ -s "$work/mask-after.png" ] || fail "no PNG of the Edit mask window"
    "$editor" "$work/real/wagon-masks" --mask nosuch --frames 2 >"$work/log" 2>&1 && fail "a texture the car has not was opened"
    "$editor" "$work/real/wagon-masks" --mask side --mask-rect 1,2,3 --frames 2 >"$work/log" 2>&1 && fail "a bad --mask-rect was taken"
fi

# ==== Preview: a paint, Day and Night ====
if [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then
    "$editor" --export-car P911 --into "$work/real" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 || fail "export of the real 911: $(cat "$work/log")"
    "$editor" "$work/real/p911-original" --frames 14 --shot "$work/preview-file.png" >"$work/log" 2>&1 || fail "Preview of the real 911: $(cat "$work/log")"
    "$editor" "$work/real/p911-original" --paint 3 --frames 14 --shot "$work/preview-paint.png" >"$work/log" 2>&1 || fail "Preview of the real 911 in a paint: $(cat "$work/log")"
    cmp -s "$work/preview-file.png" "$work/preview-paint.png" && fail "a paint did not change the Preview"
    ls "${TMPDIR:-/tmp}" | grep -q '^studio-preview-' && fail "a scratch folder of the Preview was left behind"
    # the night view: Day, Night with the lights off and Night with the headlights each show another picture of the front texture
    "$editor" "$work/real/p911-original" --view day --preview-tex frnt --frames 20 --shot "$work/night-day.png" >"$work/log" 2>&1 || fail "Preview by day: $(cat "$work/log")"
    "$editor" "$work/real/p911-original" --view night --lights none --preview-tex frnt --frames 20 --shot "$work/night-dark.png" >"$work/log" 2>&1 || fail "Preview at night, lights off: $(cat "$work/log")"
    "$editor" "$work/real/p911-original" --view night --lights head --preview-tex frnt --frames 20 --shot "$work/night-head.png" >"$work/log" 2>&1 || fail "Preview at night, headlights: $(cat "$work/log")"
    cmp -s "$work/night-day.png" "$work/night-dark.png" && fail "Night did not change the Preview"
    cmp -s "$work/night-dark.png" "$work/night-head.png" && fail "the headlights did not light"
    "$editor" "$work/real/p911-original" --view night --night-track nosuch --frames 2 >"$work/log" 2>&1
    grep -q "not a track the night view knows" "$work/log" || fail "no word about an unknown --night-track: $(cat "$work/log")"

    # the exported-original flow, as the owner works it (old shape: mod.ini + SIMDATA + car_masks/P911/): open the export, edit a mask and Save, then race it. Nothing of the
    # kit layout (build, model/) may touch it: no build/ appears, the mask is written in car_masks/P911/, and the race is given that folder
    p911="$work/real/p911-original"
    m="$p911/car_masks/P911/frnt_mask.png"
    [ -f "$m" ] || fail "the exported 911 has no car_masks/P911/frnt_mask.png"
    cp "$m" "$work/frnt-before.png"
    "$editor" "$p911" --mask frnt --mask-rect 4,4,6,6,200 --mask-save --frames 8 >"$work/log" 2>&1 || fail "Edit mask on the exported 911: $(cat "$work/log")"
    grep -q "saved frnt_mask.png" "$work/log" || fail "no word that the exported 911's mask was saved: $(cat "$work/log")"
    cmp -s "$m" "$work/frnt-before.png" && fail "the exported 911's mask was not written"
    [ ! -e "$p911/build" ] && [ ! -e "$p911/model" ] || fail "opening an exported original made a build/ or a model/ in it"
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

# ==== Sheets (a fake game): Day and Night of a car in one labelled picture, the traffic cars in another ====
# a fake game that draws the day red and the night green (the colour of a whole 1280x960 frame): Sheet takes both, in order, and the PNG written has them side by side
# (halved to 640x480 each, a label band under); the traffic sheet has a cell for every traffic car and the police car, the Wagon's done (its project is the one open)
# and the others empty with their reason; a game that fails leaves cells that say so, and the sheet still saves
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
"$editor" "$work/exports/wagon-masks" --game "$work/fake-sheet-tnfs" --cd "$work/game/cd" --frames 4 --steps sheet:0,print-sheet,sheet-save >"$work/log" 2>&1 || fail "the car sheet: $(cat "$work/log")"
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
"$editor" "$work/exports/wagon-masks" --game "$work/fake-sheet-tnfs" --cd "$work/game/cd" --frames 4 --steps sheet:1,print-sheet >"$work/log" 2>&1 || fail "the traffic sheet: $(cat "$work/log")"
grep -q "^sheet 13: .*Wagon=done" "$work/log" || fail "the traffic sheet has no Wagon photo among its 13 cells: $(grep '^sheet' "$work/log")"
[ "$(grep '^sheet 13:' "$work/log" | grep -o '=failed' | wc -l | tr -d ' ')" = 12 ] || fail "the traffic cars without a project are not 12 failed cells: $(grep '^sheet' "$work/log")"
FAKE_FAIL=1 "$editor" "$work/exports/wagon-masks" --game "$work/fake-sheet-tnfs" --cd "$work/game/cd" --frames 4 --steps sheet:0,print-sheet,sheet-save >"$work/log" 2>&1 || fail "a sheet of a game that fails: $(cat "$work/log")"
grep -q "^sheet 2: Day=failed Night=failed$" "$work/log" && grep -q "^sheet saved: " "$work/log" || fail "a game that fails is not two failed cells that still save: $(cat "$work/log")"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-sheet-tnfs" --cd "$work/game/cd" --frames 4 --steps sheet-save >"$work/log" 2>&1 && fail "Save sheet with no sheet taken wrote a file"
ls "${TMPDIR:-/tmp}" | grep -q '^studio-race-' && fail "a scratch folder of a sheet was left behind"

# ==== Changes from the game's masks ====
# Changes from the game's masks: the fixture's exported Wagon has the game's own file; with the real game files an edit shows as a changed mask with its texels
"$editor" "$work/exports/wagon-masks" --cd "$work/game/cd" --port-data "$work/game/data" --changes --frames 2 >"$work/log" 2>&1 || fail "--changes of the fixture's Wagon: $(cat "$work/log")"
grep -q "car_masks/Wagon: 0 changed, 0 new, 0 missing, 1 unchanged" "$work/log" || fail "an unchanged export is told as changed: $(cat "$work/log")"
if [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then
    "$editor" "$work/real/wagon-masks" --cd "$repo/original" --port-data "$repo/data" --changes --frames 2 >"$work/log" 2>&1 || fail "--changes of the real Wagon: $(cat "$work/log")"
    grep -q "side_mask.png" "$work/log" || fail "the edited mask is not in the changes: $(cat "$work/log")"
    grep -q "1 changed" "$work/log" || fail "no count of changes: $(cat "$work/log")"
    grep -q "headlight +40" "$work/log" || fail "no word of the texels' classes: $(cat "$work/log")"
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
# ==== Window menu: every panel a car's window draws is listed (a masks project's too) ====
for proj in "$work/kit" "$work/exports/wagon-masks"; do
    panels=$("$editor" "$proj" --window 1280x960 --frames 6 --steps print-panels 2>&1)
    echo "$panels" | grep -q '^unlisted:$' || fail "a panel of $(basename "$proj") is drawn that the Window menu does not list: $(echo "$panels" | grep '^unlisted')"
    [ "$(echo "$panels" | grep -c '^panel [0-9]*: .*: open, drawn')" -ge 5 ] || fail "the panels of $(basename "$proj") are not drawn: $panels"
done
[ "$failures" -eq 0 ] && echo "editor_headless_car: ok"
exit "$failures"
