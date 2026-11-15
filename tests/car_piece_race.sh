#!/bin/sh
# dep59 step 1: an added car against the real binary and the CD data. A piece (mod.ini type=car and the Viper's files under the stem
# TESTCAR, with copies of its masks) races with --car test-car in Restomod and draws the same frame as the Viper itself; Stock, or
# Restomod without the piece, has no such car and says so; nothing is written below the data root. Plan 1b: a race with it keeps its
# record in race_records_added.dat and its replay as Replay.rpx, never in the original's files; the replay plays back only with the
# piece as it was. Step 3: the replay keeps the skin and paint the added car wore (TNFSLOK1) and plays them whatever is picked now.
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
printf 'id=test-car\nname=Test Car\nversion=1\ntype=car\nstem=TESTCAR\nshort_name=TEST\nsound_from=viper\nai_from=viper\npaints=Blue:1040c0\n' > "$piece/mod.ini"
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
# 5. a --node run wrote no record and no Replay.rpl (its replay, as an original car's run keeps Replay.rpl, is Replay.rpx), and nothing
# below the data root (checked again at the end)
[ -e "$work/user/race_records.dat" ] && fail "a race with an added car wrote race_records.dat"
find "$work/user" -iname 'replay.rpl' | grep -q . && fail "a --node run with an added car wrote Replay.rpl"
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
# from the pause menu (Esc, Up to Quit, Enter, Y) once it runs, Enter through the results: a clean end (the gate runs it under ASan too).
# Its replay is Replay.rpx; Replay.rpl is never written
user_in restomod
give_up() {   # the mark the race logs once it runs ($1), then the pause menu's Quit and Enter through the results
    script="0:WAIT:mark:$1:120,8000:ESC,2000:UP,2000:ENTER,2000:Y"
    for i in 1 2 3 4 5 6 7 8; do script="$script,3000:ENTER"; done
    echo "$script"
}
TNFS_BANDS=0 TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$(give_up spray_v4=1) \
    timeout -k 5 300 "$tnfs" --data "$orig" --mods "$piece" --user "$work/user" --drive TR1 --car test-car --type 0 >/dev/null 2>"$work/err"
rc=$?
[ "$rc" -eq 0 ] || fail "a whole race with the piece ended with status $rc: $(tail -3 "$work/err")"
grep -q "finish: 0=TESTCAR" "$work/user/spirited-drive.log" || fail "the whole race with the piece did not finish"
find "$work/user" -iname 'replay.rpl' | grep -q . && fail "the whole race with the piece wrote Replay.rpl"
[ -s "$work/user/Replay.rpx" ] || fail "the whole race with the piece kept no Replay.rpx"

# 8. a race run to its finish (the harness's autopilot, one lap): its record is in race_records_added.dat under test-car, and the
# original's files are as they were before it (race_records.dat, made at the start, the same bytes; no config.dat or Replay.rpl)
cp "$work/user/race_records.dat" "$work/records.before" 2>/dev/null || fail "the given-up race made no race_records.dat to compare"
[ -e "$work/user/config.dat" ] && fail "a race with the piece wrote config.dat"
echo "look.test-car=-,Blue" >> "$work/user/restomod.cfg"   # step 3: the paint picked in its gallery (the skin as it comes has masks)
script="0:WAIT:mark:race_over:240"
for i in 1 2 3 4 5 6 7 8; do script="$script,3000:ENTER"; done
TNFS_BANDS=0 TNFS_VERBOSE=1 TNFS_AUTOPILOT=1 TNFS_LAPS=1 TNFS_TRACE_PLAYER=30 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script \
    timeout -k 5 300 "$tnfs" --data "$orig" --mods "$piece" --user "$work/user" --drive TR1 --car test-car --type 0 >"$work/race.out" 2>"$work/err"
rc=$?
[ "$rc" -eq 0 ] || fail "the finished race with the piece ended with status $rc: $(tail -3 "$work/err")"
grep -q "test-car" "$work/user/race_records_added.dat" 2>/dev/null || fail "race_records_added.dat does not hold test-car's record"
cmp -s "$work/records.before" "$work/user/race_records.dat" || fail "the finished race with the piece changed race_records.dat"
[ -e "$work/user/config.dat" ] && fail "the finished race with the piece wrote config.dat"
find "$work/user" -iname 'replay.rpl' | grep -q . && fail "the finished race with the piece wrote Replay.rpl"
grep -q "car_paint: slot 0 TESTCAR painted 16,64,192" "$work/user/spirited-drive.log" || fail "the race did not paint the piece the Blue picked"
[ "$(tail -c 344 "$work/user/Replay.rpx" | head -c 8)" = "TNFSLOK1" ] || fail "Replay.rpx does not end with the looks' block"
cp "$work/user/Replay.rpx" "$work/kept.rpx"

# 9. its replay with the piece: it plays the race it recorded (the trace of every tick both logged is the same), then Esc and Enter end it
replay() {   # the replay file $1, the mods folder $2 (none: no --mods): the status
    script="0:WAIT:mark:spray_v4=1:120,30000:ESC,3000:ENTER,3000:ENTER,3000:ENTER,3000:ESC"
    file=$1
    if [ -n "$2" ]; then set -- --mods "$2"; else set --; fi
    TNFS_BANDS=0 TNFS_VERBOSE=1 TNFS_TRACE_PLAYER=30 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script \
        timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" "$@" --replay "$file" >"$work/replay.out" 2>"$work/err"
}
sed 's/^look\.test-car=.*/look.test-car=-,-/' "$work/user/restomod.cfg" > "$work/cfg" && cp "$work/cfg" "$work/user/restomod.cfg"   # its own colour picked now
rm -f "$work/user/spirited-drive.log"
replay "$work/user/Replay.rpx" "$piece"
rc=$?
[ "$rc" -eq 0 ] || fail "Replay.rpx with the piece ended with status $rc: $(tail -3 "$work/err")"
grep -q "car_paint: slot 0 TESTCAR painted 16,64,192" "$work/user/spirited-drive.log" || fail "the replay did not paint the piece the Blue it raced in"
python3 "$repo/tools/replay_sync.py" race "$work/race.out" "$work/replay.out" >"$work/sync" || fail "Replay.rpx is not in sync: $(cat "$work/sync")"

# 10. without the piece it does not play, and says which mod it needs
replay "$work/user/Replay.rpx" ""
rc=$?
{ [ "$rc" -ne 0 ] && [ "$rc" -lt 128 ]; } || fail "Replay.rpx without the piece: status $rc, not a clean failure"
grep -q "replay: needs the mod test-car" "$work/err" || fail "Replay.rpx without the piece did not name it: $(tail -3 "$work/err")"

# 11. with the piece's physics changed (another car's PBS, valid), it does not play, and says so
changed=$work/changed
cp -R "$piece" "$changed"
chmod u+w "$changed/SIMDATA/CARSPECS/TESTCAR.PBS"
cp "$orig/SIMDATA/CARSPECS/CZR1.PBS" "$changed/SIMDATA/CARSPECS/TESTCAR.PBS"
replay "$work/user/Replay.rpx" "$changed"
rc=$?
{ [ "$rc" -ne 0 ] && [ "$rc" -lt 128 ]; } || fail "Replay.rpx with the piece changed: status $rc, not a clean failure"
grep -q "replay: the mod test-car has changed since this replay was made" "$work/err" || fail "Replay.rpx with the piece changed did not say so"

# 12. the same file named as the original's: the setup's car number is no car of the twelve, so not a replay
cp "$work/user/Replay.rpx" "$work/X.RPL"
replay "$work/X.RPL" "$piece"
rc=$?
{ [ "$rc" -ne 0 ] && [ "$rc" -lt 128 ]; } || fail "Replay.rpx named X.RPL: status $rc, not a clean failure"
grep -q "track_viewer: not a replay" "$work/err" || fail "Replay.rpx named X.RPL was not refused as not a replay"

# 13. a race in Stock leaves the new files alone (their bytes and their dates)
touch "$work/stock.marker"
sleep 1
echo profile=stock > "$work/user/port.cfg"
TNFS_BANDS=0 TNFS_VERBOSE=1 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$(give_up spray_v4=0) \
    timeout -k 5 300 "$tnfs" --data "$orig" --mods "$piece" --user "$work/user" --drive TR1 --car 5 --type 0 >/dev/null 2>"$work/err"
rc=$?
[ "$rc" -eq 0 ] || fail "a Stock race ended with status $rc: $(tail -3 "$work/err")"
cmp -s "$work/kept.rpx" "$work/user/Replay.rpx" || fail "a Stock race changed Replay.rpx"
[ -n "$(find "$work/user" -name 'Replay.rpx' -newer "$work/stock.marker")" ] && fail "a Stock race wrote Replay.rpx"
[ -n "$(find "$work/user" -name 'race_records_added.dat' -newer "$work/stock.marker")" ] && fail "a Stock race wrote race_records_added.dat"

[ -z "$(find "$orig" -newer "$work/marker" | head -1)" ] || fail "something below the data root was written"
[ "$failures" -eq 0 ] && echo "car_piece_race: all checks passed"
exit "$failures"
