#!/bin/sh
# dep59 tools B1: a car with geometry and a texture the CD never had loads, draws and races. model_build_piece rewrites the CFM of a
# piece made with `car new` from the Viper through the builders (formats/mesh_build.h, shpi_build.h, cfm_build.h): `same` gives the
# Viper's bytes and frame back; `fin` adds a wing over the tail and the frame changes; `texture` paints the wing with a bitmap of its
# own and the frame changes again; `big` cuts the body to more than 300 polygons. Each validates with no finding and races in Restomod
# alone on the road (extra_liveries=0, as car_new.sh), with no sanitizer report when the build has one; the wing races 10 seconds
# against a rival.
# usage: model_build.sh TNFS STUDIO BUILDER ORIGINAL_DIR PORT_DATA_DIR
set -u
tnfs=$1
studio=$2
builder=$3
orig=$4
port=$5
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no DVIPER.CFM"; exit 0; }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0

# a game that takes an id in --car answers an unknown one with "no car <id>" (as car_new.sh)
restomod() {
    rm -rf "$work/user"
    mkdir -p "$work/user" && echo "profile=restomod" > "$work/user/port.cfg" && echo "extra_liveries=0" > "$work/user/restomod.cfg"
}
restomod
timeout -k 5 60 "$tnfs" --data "$orig" --user "$work/user" --drive TR1 --car no-such-car --node 1 --screenshot "$work/probe.ppm" \
    >/dev/null 2>"$work/err"
grep -q "no car no-such-car" "$work/err" || { echo "model_build: the game's --car takes no id: skipped"; exit 0; }

# a sanitizer's report in the game's errors
clean() { ! grep -qE "ERROR: AddressSanitizer|runtime error:|ERROR: LeakSanitizer" "$1"; }

# race PIECE OUT.ppm RACERS TICKS: the piece in Restomod, its frame at TICKS
race() {
    restomod
    timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" --mods "$1" --drive TR1 --car test-car --racers "$3" --node "$4" \
        --screenshot "$2" >/dev/null 2>"$work/err" || { fail "$1 did not race: $(tail -3 "$work/err")"; return 1; }
    clean "$work/err" || fail "$1: a sanitizer report: $(grep -m1 -E "ERROR|runtime error" "$work/err")"
    [ -s "$2" ] || fail "$1: no frame"
}

# piece MODE: a piece from the Viper, rewritten by the builder in MODE (fresh: none), validated
piece() {
    p="$work/$1"
    "$studio" car new --template viper --id test-car --stem TESTCAR --out "$p" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
        { fail "car new: $(cat "$work/log")"; return 1; }
    if [ "$1" != fresh ]; then
        "$builder" "$p" "$1" >"$work/log" 2>&1 || { fail "model_build_piece $1: $(cat "$work/log")"; return 1; }
        clean "$work/log" || fail "model_build_piece $1: a sanitizer report"
    fi
    "$studio" validate "$p" >"$work/log" 2>&1 || fail "validate of $1: $(cat "$work/log")"
    grep -q "no findings" "$work/log" || fail "$1 has findings: $(cat "$work/log")"
}

for mode in fresh same fin texture big; do
    piece "$mode" && race "$work/$mode" "$work/$mode.ppm" 1 200
done

# same: the Viper's bytes, the Viper's frame
cmp -s "$work/same/SIMDATA/CARFAMS/TESTCAR.CFM" "$orig/SIMDATA/CARFAMS/DVIPER.CFM" || fail "same does not give the Viper's CFM back"
cmp -s "$work/same.ppm" "$work/fresh.ppm" || fail "same draws another frame than the piece from car new"
# fin and texture: the frame changes each time; the PDN stays the Viper's
[ -s "$work/fin.ppm" ] && cmp -s "$work/fin.ppm" "$work/same.ppm" && fail "the wing does not show in the frame"
[ -s "$work/texture.ppm" ] && cmp -s "$work/texture.ppm" "$work/fin.ppm" && fail "the wing's own texture does not show in the frame"
for mode in same fin texture big; do
    cmp -s "$work/$mode/SIMDATA/CARFAMS/TESTCAR.PDN" "$orig/SIMDATA/CARFAMS/DVIPER.PDN" || fail "$mode changed the PDN"
done

# the wing races 10 seconds (500 ticks: the countdown, then about 30 a second) with a rival on the road. With the piece as the rival
# itself: the command line has no way to name the rival's car yet (the menus' step 3)
race "$work/fin" "$work/rival.ppm" 2 500
echo "model_build: the piece as the rival is skipped: no way to name the rival's car outside the menus yet"

[ "$failures" -eq 0 ] || exit 1
echo "model_build: ok"
