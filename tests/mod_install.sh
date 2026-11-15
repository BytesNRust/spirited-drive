#!/bin/sh
# dep59 step 2: the installed mods against the real game. A car piece (tnfs-studio car new + mod pack) installed with --install races
# with no --mods, is updated, and once uninstalled is no car any more while the originals race on; a tweak made by hand is installed and
# its light is the one the race reads; a tweak's zip carrying a TRI is refused and writes nothing; --mods with a TRI races and keeps no
# record nor replay (temporary until step 5); in Stock --install works and nothing of it is served; nothing below the data root is written.
# usage: mod_install.sh TNFS STUDIO ORIGINAL_DIR PORT_DATA_DIR
set -u
tnfs=$1
studio=$2
orig=$3
port=$4
[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
U=$work/user
mkdir -p "$U" && echo profile=restomod > "$U/port.cfg"
touch "$work/marker"
sleep 1
cmd() { timeout -k 5 60 "$tnfs" --user "$U" "$@" >"$work/out" 2>"$work/err"; }
frame() { timeout -k 5 120 "$tnfs" --data "$orig" --user "$U" --drive TR1 --node 200 --screenshot "$work/f.ppm" "$@" >/dev/null 2>"$work/err"; }

# 1. a car piece's zip installed: its line, a car, on
"$studio" car new --template viper --id test-car --stem TESTCAR --out "$work/p" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
    fail "car new: $(cat "$work/log")"
"$studio" mod pack "$work/p" "$work/test-car.zip" >"$work/log" 2>&1 || fail "mod pack: $(cat "$work/log")"
cmd --install "$work/test-car.zip" || fail "--install: $(cat "$work/err")"
[ "$(cat "$work/out")" = "installed test-car 0.1" ] || fail "--install said: $(cat "$work/out")"
cmd --mods-list || fail "--mods-list: $(cat "$work/err")"
grep -q "^test-car	0.1	car	on	" "$work/out" || fail "--mods-list: $(cat "$work/out")"

# 2. in Restomod, with no --mods, the installed car races
frame --car test-car || fail "the installed car did not race: $(tail -3 "$work/err")"

# 4. the zip again with another version: an update, the old one kept as a backup
sed 's/^version=0.1$/version=0.2/' "$work/p/mod.ini" > "$work/mod.ini" && cp "$work/mod.ini" "$work/p/mod.ini"
"$studio" mod pack "$work/p" "$work/test-car-2.zip" >"$work/log" 2>&1 || fail "mod pack 0.2: $(cat "$work/log")"
cmd --install "$work/test-car-2.zip" || fail "--install 0.2: $(cat "$work/err")"
[ "$(cat "$work/out")" = "updated test-car 0.1 -> 0.2" ] || fail "the update said: $(cat "$work/out")"
ls "$U/mods_backup" 2>/dev/null | grep -q "^test-car-0.1-" || fail "no backup of 0.1"

# 3. uninstalled: not listed, no car any more; the originals race on (Review Focus 3)
cmd --uninstall test-car || fail "--uninstall: $(cat "$work/err")"
[ "$(cat "$work/out")" = "uninstalled test-car (backup kept)" ] || fail "--uninstall said: $(cat "$work/out")"
cmd --mods-list
grep -q "^test-car" "$work/out" && fail "--mods-list still has test-car"
frame --car test-car && fail "an uninstalled car raced"
grep -q "no car test-car" "$work/err" || fail "the uninstalled car is not said to be no car: $(tail -3 "$work/err")"
frame --car 5 || fail "the Viper did not race after the uninstall: $(tail -3 "$work/err")"
cmd --uninstall test-car && fail "a second --uninstall succeeded"
grep -q "^tnfs: " "$work/err" || fail "a refused --uninstall does not say why"

# 5. a tweak made by hand, a track's light: installed, the race reads its light
t=$work/hand
mkdir -p "$t/track_light"
printf 'id=hand-light\nname=Hand Light\nversion=1\ntype=tweak\n' > "$t/mod.ini"
cp "$port"/track_light/TR1?.txt "$t/track_light/"
"$studio" mod pack "$t" "$work/hand.zip" >"$work/log" 2>&1 || fail "mod pack of the tweak: $(cat "$work/log")"
cmd --install "$work/hand.zip" || fail "--install of the tweak: $(cat "$work/err")"
frame --car 5 || fail "a race with the tweak: $(tail -3 "$work/err")"
grep -q "track light: .*mods/hand-light/track_light/TR1" "$U/spirited-drive.log" || fail "the race does not read the tweak's light"

# 6. a tweak's zip with a TRI (made by hand: mod pack refuses it): refused, the file named, nothing new in mods/
mkdir -p "$work/tri/SIMDATA/MISC"
printf 'id=tri-tweak\nname=TRI Tweak\nversion=1\n' > "$work/tri/mod.ini"
cp "$orig/SIMDATA/MISC/TR1.TRI" "$work/tri/SIMDATA/MISC/TR1.TRI"
(cd "$work/tri" && python3 -c 'import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as z:
    z.write("mod.ini"); z.write("SIMDATA/MISC/TR1.TRI")' "$work/tri.zip") || fail "the TRI zip was not made"
ls -a "$U/mods" > "$work/mods.before"
cmd --install "$work/tri.zip" && fail "a tweak with a TRI was installed"
grep -q "TR1.TRI" "$work/err" || fail "the refusal does not name TR1.TRI: $(cat "$work/err")"
ls -a "$U/mods" | cmp -s - "$work/mods.before" || fail "a refused install changed mods/"

# 7. --mods with a TRI: a race run to its finish keeps no record and no replay (temporary until step 5). A race first without it
# makes race_records.dat and Replay.rpl, which must come out of the second the same bytes
script="0:WAIT:mark:spray_v4=1:120,8000:ESC,2000:UP,2000:ENTER,2000:Y"
for i in 1 2 3 4 5 6 7 8; do script="$script,3000:ENTER"; done
TNFS_BANDS=0 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script \
    timeout -k 5 300 "$tnfs" --data "$orig" --user "$U" --drive TR1 --car 5 --type 0 >/dev/null 2>"$work/err" ||
    fail "the race without --mods ended badly: $(tail -3 "$work/err")"
cp "$U/race_records.dat" "$work/records.before" 2>/dev/null || fail "no race_records.dat to compare"
cp "$U/Replay.rpl" "$work/replay.before" 2>/dev/null || fail "no Replay.rpl to compare"
mkdir -p "$work/tri_dev/SIMDATA/MISC" && cp "$orig/SIMDATA/MISC/TR1.TRI" "$work/tri_dev/SIMDATA/MISC/TR1.TRI"
script="0:WAIT:mark:race_over:240"
for i in 1 2 3 4 5 6 7 8; do script="$script,3000:ENTER"; done
TNFS_BANDS=0 TNFS_AUTOPILOT=1 TNFS_LAPS=1 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script \
    timeout -k 5 300 "$tnfs" --data "$orig" --user "$U" --mods "$work/tri_dev" --drive TR1 --car 5 --type 0 >/dev/null 2>"$work/err" ||
    fail "the race with --mods ended badly: $(tail -3 "$work/err")"
grep -q "finish: 0=" "$U/spirited-drive.log" || fail "the race with --mods did not finish"
grep -q "this run keeps no records and no replays" "$U/spirited-drive.log" || fail "the log does not say the run keeps nothing"
cmp -s "$work/records.before" "$U/race_records.dat" || fail "the race with --mods changed race_records.dat"
cmp -s "$work/replay.before" "$U/Replay.rpl" || fail "the race with --mods changed Replay.rpl"

# 8. Stock: --install works, and nothing of the installed mods is served
echo profile=stock > "$U/port.cfg"
cmd --install "$work/test-car.zip" || fail "--install in Stock: $(cat "$work/err")"
frame --car 5 || fail "a race in Stock: $(tail -3 "$work/err")"
grep -q "mods/hand-light" "$U/spirited-drive.log" && fail "Stock served the tweak's light"

# 9. nothing below the data root written
[ -z "$(find "$orig" -newer "$work/marker" | head -1)" ] || fail "something below the data root was written"
[ "$failures" -eq 0 ] && echo "mod_install: all checks passed"
exit "$failures"
