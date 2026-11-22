#!/bin/sh
# dep59 step 2: the installed mods against the real game. A car piece (tnfs-studio car new + mod pack) installed with --install is only
# made available (step 4: no edition changes, --mods-list says off); named by the edition in use it races with no --mods, is updated,
# and once uninstalled is no car any more while the originals race on (the edition's file left as it was); a tweak made by hand is
# installed and, named by the edition, its light is the one the race reads; a tweak's zip carrying a TRI is refused and writes nothing; --mods with a TRI is not served: the race is the CD's track
# and the log says a changed track is a track piece now; in Stock --install works and nothing of it is served; nothing below the data root is written.
# PART (one ctest each; the race a later part starts from is kept in FIXTURE, ctest's fixtures saying the order):
#   cars    1-4: the car piece installed, raced, updated, uninstalled
#   tweak   5-6: the tweak made by hand installed and read; the tweak's zip with a TRI refused
#   before  7, its first race: a race without --mods; keeps its player folder (race_records.dat, Replay.rpl) as FIXTURE/before
#   tri     7: from before's folder, a race with --mods holding a TRI, run to its finish: the TRI is not served and the log says why
#   stock   8: the car piece and the tweak installed, then in Stock: --install works, and nothing of them is served
# Every part checks at its end that nothing was written below the data root (9).
# usage: mod_install.sh TNFS STUDIO ORIGINAL_DIR PORT_DATA_DIR [PART FIXTURE] (no PART: all of them in turn, the fixture a scratch folder)
set -u
tnfs=$1
studio=$2
orig=$3
port=$4
parts=${5:-cars tweak before tri stock}
[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no CD in $orig"; exit 0; }
work=$(mktemp -d)
fixture=${6:-$work/fixture}
trap 'find "$work" -delete' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
U=$work/user
mkdir -p "$U" && echo profile=restomod > "$U/port.cfg"
touch "$work/marker"
sleep 1
cmd() { timeout -k 5 60 "$tnfs" --user "$U" "$@" >"$work/out" 2>"$work/err"; }
# step 4: editions/<id>.ini naming these cars and tweaks (and a track), in use in Restomod (restomod.cfg's edition=)
use_edition() {
    mkdir -p "$U/editions"
    printf 'name=Test\ncars=%s\ntracks=city\ntweaks=%s\n' "$2" "$3" > "$U/editions/$1.ini"
    echo "edition=$1" > "$U/restomod.cfg"
}
# Enter on each results screen in turn once it has faded in (its mark:results=NAME): the screens a race of its kind shows, no more. Headless
# the pointer is not over Done at first: on the first screen one Enter wakes it there and a second takes Done; on the next ones it is there
through() {
    first=1
    for name in "$@"; do
        printf ',0:WAIT:mark:results=%s:120,300:ENTER' "$name"
        [ $first -eq 1 ] && printf ',300:ENTER'
        first=0
    done
}
frame() { timeout -k 5 120 "$tnfs" --data "$orig" --user "$U" --drive TR1 --node 200 --screenshot "$work/f.ppm" "$@" >/dev/null 2>"$work/err"; }

# the car piece's zip, version 0.1 (1)
car_zip() {
    [ -e "$work/test-car.zip" ] && return   # made by an earlier part of this run
    "$studio" car new --template viper --id test-car --stem TESTCAR --out "$work/p" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
        fail "car new: $(cat "$work/log")"
    "$studio" mod pack "$work/p" "$work/test-car.zip" >"$work/log" 2>&1 || fail "mod pack: $(cat "$work/log")"
}
# the tweak made by hand, a track's light, installed (5)
hand_light() {
    t=$work/hand
    mkdir -p "$t/track_light"
    printf 'id=hand-light\nname=Hand Light\nversion=1\ntype=tweak\n' > "$t/mod.ini"
    cp "$port"/track_light/TR1.txt "$t/track_light/"
    [ -e "$work/hand.zip" ] || "$studio" mod pack "$t" "$work/hand.zip" >"$work/log" 2>&1 || fail "mod pack of the tweak: $(cat "$work/log")"
    cmd --install "$work/hand.zip" || fail "--install of the tweak: $(cat "$work/err")"
}

part_cars() {
    # 1. a car piece's zip installed: its line, a car, on
    car_zip
    cmd --install "$work/test-car.zip" || fail "--install: $(cat "$work/err")"
    [ "$(cat "$work/out")" = "installed test-car 0.1" ] || fail "--install said: $(cat "$work/out")"
    cmd --mods-list || fail "--mods-list: $(cat "$work/err")"
    grep -q "^test-car	0.1	car	off	" "$work/out" || fail "--mods-list, the car in no edition: $(cat "$work/out")"
    [ -e "$U/restomod.cfg" ] && fail "--install wrote restomod.cfg"
    frame --car test-car && fail "an installed car in no edition raced"

    # 2. in Restomod, with no --mods, the installed car named by the edition in use races
    use_edition cars "test-car,viper" ""
    cp "$U/editions/cars.ini" "$work/cars.ini"
    cmd --mods-list
    grep -q "^test-car	0.1	car	on	" "$work/out" || fail "--mods-list, the car in the edition: $(cat "$work/out")"
    frame --car test-car || fail "the installed car did not race: $(tail -3 "$work/err")"

    # 4. the zip again with another version: an update, the old one kept as a backup
    sed 's/^version=0.1$/version=0.2/' "$work/p/mod.ini" > "$work/mod.ini" && cp "$work/mod.ini" "$work/p/mod.ini"
    "$studio" mod pack "$work/p" "$work/test-car-2.zip" >"$work/log" 2>&1 || fail "mod pack 0.2: $(cat "$work/log")"
    cmd --install "$work/test-car-2.zip" || fail "--install 0.2: $(cat "$work/err")"
    [ "$(cat "$work/out")" = "updated test-car 0.1 -> 0.2" ] || fail "the update said: $(cat "$work/out")"
    ls "$U/mods_backup" 2>/dev/null | grep -q "^test-car-0.1-" || fail "no backup of 0.1"

    # 4b. the same version again: said as a reinstall, not an update; with other files, it says its version did not rise
    cmd --install "$work/test-car-2.zip" || fail "--install 0.2 again: $(cat "$work/err")"
    [ "$(cat "$work/out")" = "reinstalled test-car 0.2 (same version)" ] || fail "the same version again said: $(cat "$work/out")"
    echo "changed" > "$work/p/README.md" && "$studio" mod pack "$work/p" "$work/test-car-2b.zip" >"$work/log" 2>&1 || fail "mod pack 0.2 changed: $(cat "$work/log")"
    rm -f "$work/p/README.md"
    cmd --install "$work/test-car-2b.zip" || fail "--install 0.2 changed: $(cat "$work/err")"
    grep -q "^reinstalled test-car 0.2 (same version); its files changed but its version did not: raise version= in mod.ini$" "$work/out" ||
        fail "the same version with other files said: $(cat "$work/out")"

    # 3. uninstalled: not listed, no car any more; the originals race on (Review Focus 3)
    cmd --uninstall test-car || fail "--uninstall: $(cat "$work/err")"
    [ "$(cat "$work/out")" = "uninstalled test-car (backup kept)" ] || fail "--uninstall said: $(cat "$work/out")"
    cmd --mods-list
    grep -q "^test-car" "$work/out" && fail "--mods-list still has test-car"
    frame --car test-car && fail "an uninstalled car raced"
    grep -q "no car test-car" "$work/err" || fail "the uninstalled car is not said to be no car: $(tail -3 "$work/err")"
    frame --car 5 || fail "the Viper did not race after the uninstall: $(tail -3 "$work/err")"
    cmp -s "$U/editions/cars.ini" "$work/cars.ini" || fail "the uninstall changed the edition's file"
    grep -q "^edition=cars$" "$U/restomod.cfg" || fail "the uninstall changed restomod.cfg's edition"
    cmd --uninstall test-car && fail "a second --uninstall succeeded"
    grep -q "^tnfs: " "$work/err" || fail "a refused --uninstall does not say why"
}

part_tweak() {
    # 5. a tweak made by hand, a track's light: installed, and named by the edition in use the race reads its light
    hand_light
    use_edition light "viper" "hand-light"
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
}

# 7. --mods with a TRI: not served (a changed track is a track piece now), so the race runs the CD's track to its finish. A race first without it
# keeps the player folder (race_records.dat, Replay.rpl) as the second starts from
part_before() {
    script="0:WAIT:mark:spray_v4=1:120,8000:ESC,2000:UP,2000:ENTER,2000:Y$(through circuit)"
    TNFS_BANDS=0 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script \
        timeout -k 5 300 "$tnfs" --data "$orig" --user "$U" --drive TR1 --car 5 --type 0 >/dev/null 2>"$work/err" ||
        fail "the race without --mods ended badly: $(tail -3 "$work/err")"
    [ -e "$U/race_records.dat" ] || fail "no race_records.dat to compare"
    [ -e "$U/Replay.rpl" ] || fail "no Replay.rpl to compare"
    [ -d "$fixture/before" ] && find "$fixture/before" -delete
    mkdir -p "$fixture/before" && cp -R "$U" "$fixture/before/" || fail "the fixture before was not kept"
}

part_tri() {
    [ -d "$fixture/before/user" ] || { fail "no fixture before (its part runs first: ctest -R mod_install_game)"; return; }
    find "$U" -delete && cp -R "$fixture/before/user" "$U"
    mkdir -p "$work/tri_dev/SIMDATA/MISC" && cp "$orig/SIMDATA/MISC/TR1.TRI" "$work/tri_dev/SIMDATA/MISC/TR1.TRI"
    script="0:WAIT:mark:results=circuit:240,300:ENTER,300:ENTER,0:WAIT:mark:results=best_totals:120,300:ENTER"   # its results, the best totals
    TNFS_BANDS=0 TNFS_AUTOPILOT=1 TNFS_LAPS=1 TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT=$script \
        timeout -k 5 300 "$tnfs" --data "$orig" --user "$U" --mods "$work/tri_dev" --drive TR1 --car 5 --type 0 >/dev/null 2>"$work/err" ||
        fail "the race with --mods ended badly: $(tail -3 "$work/err")"
    grep -q "finish: 0=" "$U/spirited-drive.log" || fail "the race with --mods did not finish"
    grep -q "a changed track is a track piece now" "$U/spirited-drive.log" || fail "the log does not say a changed track is a track piece now"
}

part_stock() {
    # 8. Stock: --install works, and nothing of the installed mods is served (the tweak installed in Restomod first, as a player would)
    car_zip
    hand_light
    use_edition both "test-car,viper" "hand-light"
    echo profile=stock > "$U/port.cfg"
    cmd --install "$work/test-car.zip" || fail "--install in Stock: $(cat "$work/err")"
    frame --car 5 || fail "a race in Stock: $(tail -3 "$work/err")"
    grep -q "mods/hand-light" "$U/spirited-drive.log" && fail "Stock served the tweak's light"
}

for part in $parts; do
    case $part in
    cars|tweak|before|tri|stock) "part_$part" ;;
    *) echo "FAIL: no part $part" >&2; exit 2 ;;
    esac
done
# 9. nothing below the data root written
[ -z "$(find "$orig" -newer "$work/marker" | head -1)" ] || fail "something below the data root was written"
[ "$failures" -eq 0 ] && echo "mod_install ($parts): all checks passed"
exit "$failures"
