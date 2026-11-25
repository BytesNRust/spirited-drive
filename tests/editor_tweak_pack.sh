#!/bin/sh
# Edit tweak step 4: Pack zip and Check, on copies (a temp workspace and a temp copy of the repo's data/; never the owner's workspace). CY1: the sun changed -> Pack zip -> a zip with the light file only (its
# tunnel facts, the game's own, left out), which the game installs and reads; a tunnel fact changed is refused naming the line; a car mask with a texel that is no class is named by Check and Pack; a body color
# and a tweak with nothing changed make no zip; no studio/ in a zip. Needs the game's files (original/).
# usage: editor_tweak_pack.sh EDITOR ORIGINAL_DIR REPO TNFS STUDIO
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2
repo=$3
tnfs=$4
studio_bin=$5
here=$(dirname "$0")
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ] || { echo "skip: no game files in $orig"; exit 0; }
mkdir -p "$work/ws" "$work/art" || exit 1
cp -R "$repo/data" "$work/data"
studio() { "$editor" "$@" --port-data "$work/data" --cd "$orig" --art-dir "$work/art" --user "$work/u" --workspace "$work/ws" >"$work/log" 2>&1; }
names() { python3 -I -c "import sys, zipfile; print(' '.join(sorted(zipfile.ZipFile(sys.argv[1]).namelist())))" "$1"; }
member() { python3 -I -c "import sys, zipfile; sys.stdout.write(zipfile.ZipFile(sys.argv[1]).read(sys.argv[2]).decode())" "$1" "$2"; }
t=$work/ws/tweaks/cy1-tweak
zip=$t/build/cy1-tweak.zip

# the track: the sun changed, Check, Pack zip
studio --frames 90 --steps tweak-open-row:CY1,sun:120,30,70,save,ui-click:tweak_check,ui-click:tweak_pack,status || fail "CY1 pack: $(tail -3 "$work/log")"
grep -q '^status: pack zip: 1 file packed' "$work/log" || fail "Pack zip did not say what went in: $(grep status "$work/log")"
[ -f "$zip" ] || fail "no zip at $zip"
[ "$(names "$zip")" = "mod.ini track_light/CY1.txt" ] || fail "the zip holds: $(names "$zip")"
member "$zip" track_light/CY1.txt | grep -q '^  sun 120 30 70' || fail "the zip's light does not carry the new sun"
member "$zip" track_light/CY1.txt | grep -q '^tunnel ' && fail "the zip's light carries the game's own tunnel fact"
"$studio_bin" validate "$zip" >"$work/v" 2>&1 || fail "validate on the zip: $(cat "$work/v")"
[ -f "$t/studio/base.txt" ] && [ ! -d "$t/build/studio" ] || fail "studio/ state wrong"

# the game installs the zip and the race reads its light
if [ -n "$tnfs" ] && [ -x "$tnfs" ]; then
    U=$work/user
    mkdir -p "$U/editions" && echo profile=restomod > "$U/port.cfg"
    id=$(sed -n 's/^id=//p' "$t/mod.ini")
    "$tnfs" --user "$U" --install "$zip" >"$work/out" 2>"$work/err" || fail "--install of the zip: $(cat "$work/err")"
    printf 'name=Test\ncars=viper\ntracks=city\ntweaks=%s\n' "$id" > "$U/editions/pack.ini"
    echo "edition=pack" > "$U/restomod.cfg"
    SDL_RENDER_DRIVER=software timeout -k 5 120 "$tnfs" --data "$orig" --user "$U" --drive CY1 --node 200 --car 5 --screenshot "$work/f.ppm" >/dev/null 2>"$work/err" || fail "a race with the zip: $(tail -3 "$work/err")"
    grep -q "track light: .*mods/$id/track_light/CY1" "$U/spirited-drive.log" || fail "the race did not read the zip's light: $(grep 'track light' "$U/spirited-drive.log")"
fi

# a tunnel fact changed: refused, naming the line; no zip
rm -f "$zip"
light=$work/ws/tracks/CY1/light/track_light/CY1.txt
sed -i.bak 's/^tunnel 1064 1171$/tunnel 1000 1100/' "$light" && rm -f "$light.bak"
studio --frames 90 --steps tweak-open-row:CY1,ui-click:tweak_check,ui-click:tweak_pack,status || fail "fact: $(tail -3 "$work/log")"
grep -q '^status: pack zip: not made: .*tunnel 1000 1100' "$work/log" || fail "a changed tunnel fact was not refused by its line: $(grep status "$work/log")"
[ ! -e "$zip" ] || fail "a zip was made with a changed fact"

# a car mask with a texel that is no class: Check and Pack name it
studio --frames 60 --steps tweak-open-row:Wagon,tweak-status || fail "Wagon open: $(tail -3 "$work/log")"
kit=$(ls -d "$work"/ws/cars/* | head -n 1)
m=$(ls -S "$kit/art-generated/car_masks"/*_mask.png 2>/dev/null | head -n 1)
python3 -I "$here/mask_poke.py" "$m" 30 30 20 20 200
studio --frames 60 --steps tweak-open-row:Wagon,tweak-check   # the step fails: the run ends there
grep -q '^check: car_masks/Wagon/.*texel (30,30) is 200,200,200' "$work/log" || fail "Check did not name the texel: $(grep -E '^check|^status' "$work/log")"
studio --frames 60 --steps tweak-open-row:Wagon,ui-click:tweak_check,ui-click:tweak_pack,status
grep -q '^status: pack zip: not made: car_masks/Wagon/' "$work/log" || fail "Pack zip did not refuse the mask: $(grep status "$work/log")"
[ ! -e "$work/ws/tweaks/wagon-tweak/build/wagon-tweak.zip" ] || fail "a zip was made from a mask with a texel that is no class"
python3 -I "$here/mask_poke.py" "$m" 30 30 20 20 255
studio --frames 60 --steps tweak-open-row:Wagon,ui-click:tweak_check,ui-click:tweak_pack,status || fail "Wagon pack: $(tail -3 "$work/log")"
grep -q '^status: pack zip: 1 file packed' "$work/log" || fail "a class value was refused: $(grep status "$work/log")"
[ "$(names "$work/ws/tweaks/wagon-tweak/build/wagon-tweak.zip" | wc -w)" -eq 2 ] || fail "Wagon zip: $(names "$work/ws/tweaks/wagon-tweak/build/wagon-tweak.zip")"

# nothing changed: no zip
studio --frames 60 --steps tweak-open-row:Jetta,ui-click:tweak_check,ui-click:tweak_pack,status || fail "Jetta: $(tail -3 "$work/log")"
grep -q '^status: pack zip: not made: nothing differs from the game' "$work/log" || fail "an unchanged tweak: $(grep status "$work/log")"

# only a body color changed: the colors go to the game repo only, so there is no zip and the status says so
studio --frames 90 --steps tweak-open-row:LDiabl,paint-rgb:0,7,7,7,save,ui-click:tweak_check,ui-click:tweak_pack,status || fail "Diablo: $(tail -3 "$work/log")"
grep -q '^status: pack zip: not made: nothing differs from the game but the colors, which go to the game repo only' "$work/log" || fail "colors only: $(grep status "$work/log")"
[ ! -e "$work/ws/tweaks/ldiabl-tweak/build/ldiabl-tweak.zip" ] || fail "a zip was made for colors"

[ "$failures" -eq 0 ] && echo "editor_tweak_pack: ok"
exit "$failures"
