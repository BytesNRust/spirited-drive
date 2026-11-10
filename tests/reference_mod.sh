#!/bin/sh
# The reference mod (BYTES-0009 AC11): tests/mods/reference/build.sh makes it from the CD, then
#  - the track viewer shows other pictures than the original at nodes the mod changed (a curve, an object, a texture),
#  - the drive view shows the edited car (power and texture, each alone),
#  - the port's loader reads the edited car values through the overlay (test_reference_mod),
#  - nothing below the data root was written.
# usage: reference_mod.sh TNFS STUDIO ORIGINAL_DIR TEST_REFERENCE_MOD
set -u
tnfs=$1
studio=$2
orig=$3
loader_test=$4
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
# the player's folder is this run's own: a tnfs with no --user would write its log, settings and replay in the real one
export TNFS_PREF_DIR="$work/pref"
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software

[ -f "$orig/SIMDATA/MISC/TR1.TRI" ] && [ -f "$orig/SIMDATA/CARSPECS/DVIPER.PBS" ] || { echo "skip: no CD data"; exit 0; }
command -v python3 >/dev/null 2>&1 || { echo "skip: no python3 for the texture edits"; exit 0; }
touch "$work/marker"
sleep 1

sh "$here/mods/reference/build.sh" "$studio" "$orig" "$work/mod" >"$work/log" 2>&1 || { fail "build.sh: $(tail -5 "$work/log")"; exit 1; }
for f in MISC/TR1.TRI NTRACKFM/TR1_T01.FAM CARSPECS/DVIPER.PBS CARFAMS/DVIPER.CFM; do
    [ -f "$work/mod/SIMDATA/$f" ] || fail "the mod has no $f"
done

# the track: every affected node looks different (the curve starts at node 60, the object stands at node 100, the texture is on the first segments)
view() { "$tnfs" --data "$orig" ${3:+--mods "$3"} --track TR1 --node "$1" --screenshot "$2" >/dev/null 2>&1; }
for node in 20 90 140 200; do
    view "$node" "$work/orig$node.ppm" || fail "the original track did not render at node $node"
    view "$node" "$work/mod$node.ppm" "$work/mod" || fail "the mod's track did not render at node $node"
    cmp -s "$work/orig$node.ppm" "$work/mod$node.ppm" && fail "node $node looks the same in the mod and the original"
done

# the curve (with the object) alone and the texture alone each show too
mkdir -p "$work/curve/SIMDATA/MISC" "$work/texture/SIMDATA/NTRACKFM"
cp "$work/mod/SIMDATA/MISC/TR1.TRI" "$work/curve/SIMDATA/MISC/"
cp "$work/mod/SIMDATA/NTRACKFM/TR1_T01.FAM" "$work/texture/SIMDATA/NTRACKFM/"
view 200 "$work/curve200.ppm" "$work/curve" || fail "the curve-only mod did not render"
cmp -s "$work/orig200.ppm" "$work/curve200.ppm" && fail "the changed curve (and the added object) do not show at node 200"
view 20 "$work/texture20.ppm" "$work/texture" || fail "the texture-only mod did not render"
cmp -s "$work/orig20.ppm" "$work/texture20.ppm" && fail "the changed texture does not show at node 20"

# the car: the drive view with each car file of the mod on its own
drive() { "$tnfs" --data "$orig" ${3:+--mods "$3"} --drive TR1 --car 5 --node "$1" --screenshot "$2" >/dev/null 2>&1; }
mkdir -p "$work/power/SIMDATA" "$work/paint/SIMDATA"
cp -R "$work/mod/SIMDATA/CARSPECS" "$work/power/SIMDATA/"
cp -R "$work/mod/SIMDATA/CARFAMS" "$work/paint/SIMDATA/"
drive 400 "$work/drive_orig.ppm" || fail "the original drive view did not render"
drive 400 "$work/drive_power.ppm" "$work/power" || fail "the drive view with the edited specs did not render"
drive 400 "$work/drive_paint.ppm" "$work/paint" || fail "the drive view with the edited CFM did not render"
cmp -s "$work/drive_orig.ppm" "$work/drive_power.ppm" && fail "+20 % power does not change the drive view"
cmp -s "$work/drive_orig.ppm" "$work/drive_paint.ppm" && fail "the changed car texture does not show in the drive view"

# the loader of the port reads the edited values
"$loader_test" "$orig" "$work/mod" >"$work/log" 2>&1 || fail "the car loader test failed: $(tail -5 "$work/log")"

# nothing below the data root was written
[ -z "$(find "$orig" -newer "$work/marker" 2>/dev/null | head -1)" ] || fail "something below the data root was written"

[ "$failures" -eq 0 ] && echo "reference_mod: ok"
exit $((failures != 0))
