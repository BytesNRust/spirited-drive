#!/bin/sh
# The first start, end to end (notes/primer-uso-studio-2026-11-24.md, point 6), on empty preferences and a temp workspace (never the owner's): the game's folder is asked once (Not now ends the question
# and it does not come back), Edit tweak -> Car -> the Viper -> Open, a mask changed from outside, Check, Pack zip, the zip installed in the game and the race reads the changed mask. Needs the game's files.
# usage: editor_first_start_flow.sh EDITOR ORIGINAL_DIR REPO TNFS
. "$(dirname "$0")/editor_headless_common.sh"
orig=$2
repo=$3
tnfs=$4
here=$(dirname "$0")
[ -r "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] && [ -d "$repo/data/car_masks/DVIPER" ] || { echo "skip: no game files in $orig"; exit 0; }
mkdir -p "$work/ws" "$work/art" "$work/home" || exit 1
cp -R "$repo/data" "$work/data"
printf '#!/bin/sh\n' >"$work/none" && chmod +x "$work/none"
# empty preferences: a user folder nobody wrote in; the editor's own settings live there
run() { "$editor" --user "$work/u" --cd "$orig" --art-dir "$work/art" --port-data "$work/data" --workspace "$work/ws" --window 1280x800 "$@" >"$work/log" 2>&1; }
row='ui-click:door_tweak,ui-click:tweak_tab_car,ui-click:tweak_row_Dodge Viper RT/10,ui-click:tweak_open'

# 1. the game is not found: the question is on the main menu; Not now answers it, and it is gone (a second click has nothing to click)
run --game "$work/gone" --frames 12 --steps ui-click:locate_not_now || fail "the first start did not ask for the game's folder: $(tail -3 "$work/log")"
run --game "$work/gone" --frames 12 --steps ui-click:locate_not_now,ui-click:locate_not_now && fail "the question was still there after Not now"

# 1b. no --game: the answer is kept in the Documents folder, which is empty on a clean machine; a second session on the same one does not ask (the search for the game is off, whatever is installed here)
docs=$work/docs
mkdir -p "$docs" "$work/cwd"
ask() { exe=$(cd "$(dirname "$editor")" && pwd)/$(basename "$editor"); (cd "$work/cwd" && TNFS_STUDIO_GAME_SEARCH=off TNFS_STUDIO_DOCUMENTS="$docs" "$exe" --user "$work/u2" --cd "$orig" --art-dir "$work/art" --workspace "$work/ws" --window 1280x800 --frames 12 "$@" >"$work/log" 2>&1); }
ask --steps ui-click:locate_not_now || fail "the first session did not ask for the game's folder: $(tail -3 "$work/log")"
[ -f "$docs/Spirited Drive Studio/game.txt.asked" ] || fail "Not now left no mark in the Documents folder: $(find "$docs" | head -5)"
ask --steps ui-click:locate_not_now && fail "the second session asked for the game's folder again"

# 2. Edit tweak -> Car -> the Viper -> Open: the tweak is made in the workspace, the game is not changed
run --game "$work/none" --frames 90 --steps "$row,tweak-status" || fail "Open the Viper by the doors: $(tail -3 "$work/log")"
t=$work/ws/tweaks/dviper-tweak
[ -f "$t/mod.ini" ] && [ -d "$t/car_masks/DVIPER" ] || fail "no tweak for the Viper in the workspace: $(ls "$work/ws" "$work/ws/tweaks" 2>&1 | head -5)"
grep -q 'same as the game' "$work/log" || fail "a tweak not changed yet is not the game's: $(grep '^tweak' "$work/log")"
diff -rq "$repo/data" "$work/data" >/dev/null || fail "Open changed the game's data"

# 3. a mask changed from outside, as the picture editor would; Check, then Pack zip
kit=$(ls -d "$work"/ws/cars/* | head -n 1)
m=$(ls -S "$kit/art-generated/car_masks"/*_mask.png 2>/dev/null | head -n 1)
[ -n "$m" ] || fail "the Viper's kit has no mask"
python3 -I "$here/mask_poke.py" "$m" 30 30 20 20 255
run --game "$work/none" --frames 90 --steps "$row,ui-click:tweak_check,tweak-check" || fail "Check after the change: $(grep -E '^check|^tweak' "$work/log")"
grep -q '^check: ok' "$work/log" || fail "Check: $(grep '^check' "$work/log")"
grep -q 'car_masks/DVIPER/.*changed' "$work/log" || fail "the changed mask is not listed: $(grep -A5 '^tweak dviper' "$work/log")"
run --game "$work/none" --frames 90 --steps "$row,ui-click:tweak_check,ui-click:tweak_pack,status" || fail "Pack zip: $(tail -3 "$work/log")"
grep -q '^status: pack zip: [0-9]* files\? packed' "$work/log" || fail "Pack zip did not say what went in: $(grep status "$work/log")"
zip=$t/build/dviper-tweak.zip
[ -f "$zip" ] || fail "no zip"
names=$(python3 -I -c "import sys, zipfile; print(' '.join(sorted(zipfile.ZipFile(sys.argv[1]).namelist())))" "$zip")
case "$names" in *car_masks/DVIPER/*_mask.png*mod.ini*|*mod.ini*car_masks/DVIPER/*) ;; *) fail "the zip holds: $names" ;; esac
case "$names" in *studio/*|*.psd*) fail "the zip carries what it must not: $names" ;; esac

# 4. the game installs the zip and the race reads the changed mask
if [ -n "$tnfs" ] && [ -x "$tnfs" ]; then
    export SDL_RENDER_DRIVER=software
    U=$work/user
    mkdir -p "$U/editions" && echo profile=restomod > "$U/port.cfg"
    id=$(sed -n 's/^id=//p' "$t/mod.ini")
    "$tnfs" --user "$U" --install "$zip" >"$work/out" 2>"$work/err" || fail "--install of the zip: $(cat "$work/err")"
    printf 'name=Test\ncars=viper\ntracks=city\ntweaks=%s\n' "$id" > "$U/editions/first.ini"
    echo "edition=first" > "$U/restomod.cfg"
    timeout -k 5 120 "$tnfs" --data "$orig" --user "$U" --drive TR1 --node 200 --car 5 --screenshot "$work/f.ppm" >/dev/null 2>"$work/err" || fail "a race with the zip: $(tail -3 "$work/err")"
    grep -q "car mask: .*mods/$id/car_masks/DVIPER/" "$U/spirited-drive.log" || fail "the race did not read the changed mask: $(grep 'car mask' "$U/spirited-drive.log" | head -3)"
fi
[ "$failures" -eq 0 ] && echo "editor_first_start_flow: ok"
exit "$failures"
