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
# no PROJECT: the start screen (nothing is made anywhere); a car's kit opens as a car, by its root or its coche/, and writes nothing; a card that is not a car is refused
"$editor" --frames 3 --shot "$work/start.png" >"$work/log" 2>&1 || fail "run with nothing to open: $(cat "$work/log")"
[ -s "$work/start.png" ] || fail "no PNG of the start screen"
"$editor" --ask opencar --frames 3 >"$work/log" 2>&1 || fail "run with Open car asked for on the start screen: $(cat "$work/log")"
mkdir -p "$work/kit/coche/SIMDATA/CARFAMS" "$work/kit/coche/car_masks/K1" "$work/kit/modelo/c1"
printf 'id=k1\nname=Kit One\nversion=1\ntype=car\nstem=K1\nsound_from=911\n' > "$work/kit/coche/mod.ini"
printf 'x' > "$work/kit/coche/SIMDATA/CARFAMS/K1.CFM"
printf 'x' > "$work/kit/coche/car_masks/K1/rsid_mask.png"
printf 'x' > "$work/kit/modelo/c1/0010_rsid.png"
"$editor" "$work/kit" --frames 3 --shot "$work/kit.png" >"$work/log" 2>&1 || fail "run on a car's kit: $(cat "$work/log")"
"$editor" "$work/kit/coche" --frames 3 --shot "$work/coche.png" >"$work/log" 2>&1 || fail "run on a car's coche folder: $(cat "$work/log")"
[ -s "$work/kit.png" ] && [ -s "$work/coche.png" ] || fail "no PNG of the car"
cmp -s "$work/kit.png" "$work/coche.png" || fail "the kit and its coche/ are not the same car on screen"
cmp -s "$work/kit.png" "$work/start.png" && fail "the car looks like the start screen"
[ ! -e "$work/kit/project.ini" ] && [ ! -e "$work/kit/coche/project.ini" ] || fail "opening a car made track files in it"
"$editor" "$work/kit" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a car"
grep -q "no Build or Install" "$work/log" || fail "Build on a car does not say so: $(cat "$work/log")"
mkdir "$work/notcar"
printf 'id=t\nname=T\nversion=1\ntype=track\nstem=TT\n' > "$work/notcar/mod.ini"
out=$("$editor" "$work/notcar" --frames 2 2>&1) && fail "a track's mod.ini was opened as a car"
echo "$out" | grep -q "type=track" || fail "no word about what the card is: $out"
[ ! -e "$work/notcar/project.ini" ] || fail "a card that is not a car was made a track project"

# ==== Card: edit and Save mod.ini, an invalid field, unsaved changes on Quit ====
cp "$work/kit/coche/mod.ini" "$work/card.before"
"$editor" "$work/kit" --frames 2 --save >"$work/log" 2>&1 || fail "Save with nothing changed failed: $(cat "$work/log")"
cmp -s "$work/card.before" "$work/kit/coche/mod.ini" || fail "Save with nothing changed touched mod.ini"
# a change is written to mod.ini and nothing else; an invalid field refuses Save, shows on its field, and leaves the file as it was
"$editor" "$work/kit" --frames 2 --set year=1990 --save >"$work/log" 2>&1 || fail "Save of a change failed: $(cat "$work/log")"
printf 'year=1990\n' >> "$work/card.before"
cmp -s "$work/card.before" "$work/kit/coche/mod.ini" || fail "Save changed more than the one line"
cp "$work/kit/coche/mod.ini" "$work/card.before"
"$editor" "$work/kit" --frames 3 --set year=12345678901234567 --save --shot "$work/invalid.png" >"$work/log" 2>&1 && fail "Save went with a field that has a problem"
grep -q "Year: 17 characters; the card keeps up to 16" "$work/log" || fail "no word about the field: $(cat "$work/log")"
cmp -s "$work/card.before" "$work/kit/coche/mod.ini" || fail "a refused Save changed mod.ini"
[ -s "$work/invalid.png" ] || fail "no PNG with an invalid field"
"$editor" "$work/kit" --frames 3 --set year=1991 --ask quit --shot "$work/carconfirm.png" >"$work/log" 2>&1 || fail "run with a changed card and a quit asked for: $(cat "$work/log")"
"$editor" "$work/kit" --frames 2 --set nokey=1 >"$work/log" 2>&1 && fail "--set accepted a key that is not editable"
rm -f "$work/kit/project.ini"

# ==== Rehacer ====
# Rehacer: a kit with no rehacer.command says so; unsaved card changes refuse it; the script runs with the kit as its folder after the kit is backed up, and its
# exit code is the run's
"$editor" "$work/kit" --frames 2 --rehacer >"$work/log" 2>&1 && fail "Rehacer went on a kit with no rehacer.command"
grep -q "no rehacer.command" "$work/log" || fail "no plain word about the missing script: $(cat "$work/log")"
printf '#!/bin/sh\necho "working in $(pwd)"\necho made > coche/proof.txt\nexit ${REHACER_EXIT:-0}\n' > "$work/kit/rehacer.command"
"$editor" "$work/kit" --frames 2 --set year=1992 --rehacer >"$work/log" 2>&1 && fail "Rehacer ran with unsaved card changes"
grep -q "not saved" "$work/log" || fail "no word about the unsaved changes: $(cat "$work/log")"
[ ! -e "$work/kit/coche/proof.txt" ] && [ ! -d "$work/kit/.studio-backup" ] || fail "a refused Rehacer ran or made a backup"
"$editor" "$work/kit" --frames 3 --rehacer --shot "$work/rehacer.png" >"$work/log" 2>&1 || fail "Rehacer did not run: $(cat "$work/log")"
grep -q "exit code 0" "$work/log" || fail "no exit code told: $(cat "$work/log")"
[ -f "$work/kit/coche/proof.txt" ] || fail "the script did not run with the kit as its folder"
set -- "$work"/kit/.studio-backup/*/coche/mod.ini
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
fi

# ==== Model ====
# the Model window: a car file it cannot draw says so and the run goes on; --orbit is YAW,PITCH[,DIST] or refused; with the CD's P911 two orbits are two pictures
"$editor" "$work/kit" --frames 3 --orbit 90,10,500 --shot "$work/stub.png" >"$work/log" 2>&1 || fail "run with an orbit on a car it cannot draw: $(cat "$work/log")"
"$editor" "$work/kit" --frames 2 --orbit north >"$work/log" 2>&1 && fail "a bad --orbit was accepted"
grep -q "YAW,PITCH" "$work/log" || fail "no word about --orbit: $(cat "$work/log")"
p911="$(cd "$(dirname "$0")/.." && pwd)/original/SIMDATA/CARFAMS/P911.CFM"
if [ -f "$p911" ]; then
    mkdir -p "$work/k911/coche/SIMDATA/CARFAMS" "$work/k911/coche/car_masks/P911"
    printf 'id=k911\nname=K911\nversion=1\ntype=car\nstem=P911\nsound_from=911\n' > "$work/k911/coche/mod.ini"
    cp "$p911" "$work/k911/coche/SIMDATA/CARFAMS/P911.CFM"
    printf 'x' > "$work/k911/coche/car_masks/P911/rsid_mask.png"
    "$editor" "$work/k911" --frames 4 --orbit 215,18 --shot "$work/m1.png" >"$work/log" 2>&1 || fail "run on the 911: $(cat "$work/log")"
    "$editor" "$work/k911" --frames 4 --orbit 90,18 --shot "$work/m2.png" >"$work/log" 2>&1 || fail "run on the 911, another orbit: $(cat "$work/log")"
    "$editor" "$work/k911" --frames 4 --orbit 215,18 --shot "$work/m3.png" >"$work/log" 2>&1 || fail "run on the 911 again: $(cat "$work/log")"
    cmp -s "$work/m1.png" "$work/m2.png" && fail "two orbits drew the same window"
    cmp -s "$work/m1.png" "$work/m3.png" || fail "the same orbit drew two different windows"
    # the body shine (M5): on by default, off is another picture of the same orbit; a bad --shine is refused
    "$editor" "$work/k911" --frames 4 --orbit 215,18 --shine off --shot "$work/m4.png" >"$work/log" 2>&1 || fail "run on the 911 without the shine: $(cat "$work/log")"
    cmp -s "$work/m1.png" "$work/m4.png" && fail "the shine off drew the same window as on"
    "$editor" "$work/k911" --frames 2 --shine maybe >"$work/log" 2>&1 && fail "a bad --shine was accepted"
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
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --night --race-png "$work/photo.png" --frames 4 --shot "$work/racepanel.png" >"$work/log" 2>&1 || fail "In-game photo with a fake game: $(cat "$work/log")"
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
[ "$failures" -eq 0 ] && echo "editor_headless_car: ok"
exit "$failures"
