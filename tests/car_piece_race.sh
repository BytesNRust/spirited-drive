#!/bin/sh
# dep59 step 1: an added car against the real binary and the CD data. A piece (mod.ini type=car and the Viper's files under the stem
# TESTCAR, with copies of its masks) races with --car test-car in Restomod and draws the same frame as the Viper itself; Stock, or
# Restomod without the piece, has no such car and says so; a race with it writes no record or replay and nothing below the data root.
# usage: car_piece_race.sh TNFS REPO ORIGINAL_DIR
set -u
tnfs=$1
repo=$2
orig=$3
[ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0

piece=$work/piece
mkdir -p "$piece/SIMDATA/CARFAMS" "$piece/SIMDATA/CARSPECS" "$piece/car_masks" "$piece/dash_masks"
printf 'id=test-car\nname=Test Car\nversion=1\ntype=car\nstem=TESTCAR\nshort_name=TEST\nsound_from=viper\nai_from=viper\n' > "$piece/mod.ini"
cp "$orig/SIMDATA/CARFAMS/DVIPER.CFM" "$piece/SIMDATA/CARFAMS/TESTCAR.CFM"
cp "$orig/SIMDATA/CARFAMS/DVIPER.PDN" "$piece/SIMDATA/CARFAMS/TESTCAR.PDN"
cp "$orig/SIMDATA/CARSPECS/DVIPER.PBS" "$piece/SIMDATA/CARSPECS/TESTCAR.PBS"
cp -R "$repo/data/car_masks/DVIPER" "$piece/car_masks/TESTCAR"
cp -R "$repo/data/dash_masks/DVIPER" "$piece/dash_masks/TESTCAR"

# a player folder in mode $1, fresh
user_in() {
    rm -rf "$work/user"
    mkdir -p "$work/user" && echo "profile=$1" > "$work/user/port.cfg"
}
run() { timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" "$@" >/dev/null 2>"$work/err"; }

touch "$work/marker"
sleep 1

# 1. Restomod with the piece: it is car 16 and races
user_in restomod
run --mods "$piece" --drive TR1 --car test-car --node 200 --screenshot "$work/a.ppm" || fail "the piece did not race: $(cat "$work/err")"
[ -s "$work/a.ppm" ] || fail "no frame of the piece"
grep -q "car piece: test-car is car 16" "$work/user/spirited-drive.log" || fail "the log does not say the piece is car 16"
# 5. it wrote no record or replay, and nothing below the data root (checked again after 7)
[ -e "$work/user/race_records.dat" ] && fail "a race with an added car wrote race_records.dat"
find "$work/user" -iname 'replay.rpl' | grep -q . && fail "a race with an added car wrote a replay"
[ -z "$(find "$orig" -newer "$work/marker" | head -1)" ] || fail "something below the data root was written"

# 2. the piece and the Viper itself draw the same frame (the piece is its copy, made from it: sound, pace, cockpit and lamps the Viper's).
# Alone on the road and with the mode's liveries off, as two things rightly differ otherwise: the paint catalogue is by stem (an added car
# has none until the menus come, so no fixed black), and a rival Viper takes another paint than the player's only when the player's is a Viper
user_in restomod
echo "extra_liveries=0" > "$work/user/restomod.cfg"
run --mods "$piece" --drive TR1 --car test-car --racers 1 --node 200 --screenshot "$work/a1.ppm" || fail "the piece alone did not race: $(cat "$work/err")"
user_in restomod
echo "extra_liveries=0" > "$work/user/restomod.cfg"
run --drive TR1 --car 5 --racers 1 --node 200 --screenshot "$work/b1.ppm" || fail "the Viper did not race: $(cat "$work/err")"
cmp -s "$work/a1.ppm" "$work/b1.ppm" || fail "the piece's frame is not the Viper's: something still reads the car by its number"

# 3. Stock: no added car, --mods ignored, a clean refusal
user_in stock
run --mods "$piece" --drive TR1 --car test-car --node 200 --screenshot "$work/c.ppm"
rc=$?
{ [ "$rc" -ne 0 ] && [ "$rc" -lt 128 ]; } || fail "Stock with --car test-car: status $rc, not a clean failure"
grep -q "no car test-car" "$work/err" || fail "Stock did not say there is no car test-car: $(cat "$work/err")"

# 4. Restomod without the piece: the same
user_in restomod
run --drive TR1 --car test-car --node 200 --screenshot "$work/d.ppm"
rc=$?
{ [ "$rc" -ne 0 ] && [ "$rc" -lt 128 ]; } || fail "Restomod without the piece: status $rc, not a clean failure"
grep -q "no car test-car" "$work/err" || fail "Restomod without the piece did not say there is no car test-car"

# 6. the piece loaded, the originals race as before
user_in restomod
run --mods "$piece" --drive TR1 --car 5 --racers 4 --node 200 --screenshot "$work/e.ppm" || fail "the originals with the piece loaded did not race: $(cat "$work/err")"

# 7. a whole race with it through to the results screens (they name a car out of a table of the twelve: car_registry_short_name), given up
# from the pause menu (Esc, Up to Quit, Enter, Y) once it runs, Enter through the results: a clean end (the gate runs it under ASan too)
user_in restomod
script="0:WAIT:mark:spray_v4=1:120,8000:ESC,2000:UP,2000:ENTER,2000:Y"
for i in 1 2 3 4 5 6 7 8; do script="$script,3000:ENTER"; done
TNFS_BANDS=0 TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script \
    timeout -k 5 300 "$tnfs" --data "$orig" --mods "$piece" --user "$work/user" --drive TR1 --car test-car --type 0 >/dev/null 2>"$work/err"
rc=$?
[ "$rc" -eq 0 ] || fail "a whole race with the piece ended with status $rc: $(tail -3 "$work/err")"
grep -q "finish: 0=TESTCAR" "$work/user/spirited-drive.log" || fail "the whole race with the piece did not finish"
find "$work/user" -iname 'replay.rpl' | grep -q . && fail "the whole race with the piece wrote a replay"

[ -z "$(find "$orig" -newer "$work/marker" | head -1)" ] || fail "something below the data root was written"
[ "$failures" -eq 0 ] && echo "car_piece_race: all checks passed"
exit "$failures"
