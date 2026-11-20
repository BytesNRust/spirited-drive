#!/bin/sh
# dep85 step 2: a car kit of flat sources (tnfs-studio car kit) and its build (tnfs-studio car build), the paths of core/formats/car_kit_paths.h.
# 1. car new of the Viper, then `car kit PIECE KIT`: the kit has README.txt, the near and far model (Blender OBJ and bank files), specs/
#    and the masks flat (no stem level).
# 2. `car build KIT`: build/piece/ holds the car's CFM byte for byte, its PBS and PDN with the same records (unpacked to the same TSVs),
#    the masks under the stem again; validate finds nothing; build/<id>.zip is made. The kit's sources are byte for byte as before, and a
#    second build gives the same bytes.
# 3. Refused, nothing written: `car kit` into a folder that has files; `car build` of a folder that is not a kit.
# usage: car_kit_cli.sh STUDIO ORIGINAL_DIR PORT_DATA_DIR
set -u
studio=$1
orig=$2
port=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no DVIPER.CFM"; exit 0; }

# the kit's sources, each file's hash, build/ left out
sources() { (cd "$1" && find . -type f ! -path './build/*' | LC_ALL=C sort | xargs shasum) }

p="$work/piece"
"$studio" car new --template viper --id kit-car --stem KITCAR --out "$p" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
    fail "car new: $(cat "$work/log")"
mkdir -p "$p/car_masks/KITCAR" && cp "$p/card.png" "$p/car_masks/KITCAR/note.png"   # a file under the stem, to see it come back there

# 1. the kit
k="$work/kit"
"$studio" car kit "$p" "$k" >"$work/log" 2>&1 || fail "car kit: $(cat "$work/log")"
for f in README.txt mod.ini card.png model/near/mesh/mesh.blender.obj model/near/bank/entries.tsv model/near/bank/bank.tsv \
         model/far/mesh/mesh.blender.obj model/far/bank/bank.tsv specs/specs.tsv specs/pdn.tsv car_masks/note.png; do
    [ -f "$k/$f" ] || fail "the kit has no $f"
done
[ ! -e "$k/car_masks/KITCAR" ] || fail "the kit's masks keep the stem level"
[ ! -e "$k/build" ] || fail "car kit made build/"
grep -q "disposable" "$k/README.txt" || fail "README.txt does not say build/ is disposable"

# 2. the build
before=$(sources "$k")
"$studio" car build "$k" >"$work/log" 2>&1 || fail "car build: $(cat "$work/log")"
b="$k/build/piece"
cmp -s "$p/SIMDATA/CARFAMS/KITCAR.CFM" "$b/SIMDATA/CARFAMS/KITCAR.CFM" || fail "the built CFM is not the piece's"
for f in SIMDATA/CARSPECS/KITCAR.PBS SIMDATA/CARFAMS/KITCAR.PDN; do
    "$studio" unpack "$p/$f" "$work/was-$(basename "$f")" >/dev/null 2>&1 && "$studio" unpack "$b/$f" "$work/now-$(basename "$f")" >/dev/null 2>&1 ||
        fail "unpack of $f"
    diff -r "$work/was-$(basename "$f")" "$work/now-$(basename "$f")" >/dev/null || fail "$f's record is not the piece's"
done
[ -f "$b/car_masks/KITCAR/note.png" ] || fail "the build has no car_masks/KITCAR/note.png"
cmp -s "$p/card.png" "$b/card.png" && cmp -s "$p/mod.ini" "$b/mod.ini" || fail "the card is not copied as it is"
ls "$k"/build/*.zip >/dev/null 2>&1 || fail "no zip in build/"
"$studio" validate "$b" >"$work/log" 2>&1 && grep -q "no findings" "$work/log" || fail "validate of the build: $(cat "$work/log")"
[ "$(sources "$k")" = "$before" ] || fail "car build changed the kit's sources"
cp "$b/SIMDATA/CARFAMS/KITCAR.CFM" "$work/first.CFM"
"$studio" car build "$k" >"$work/log" 2>&1 || fail "car build again: $(cat "$work/log")"
cmp -s "$work/first.CFM" "$b/SIMDATA/CARFAMS/KITCAR.CFM" || fail "a second build gives other bytes"

# 3. refusals
"$studio" car kit "$p" "$k" >"$work/log" 2>&1 && fail "car kit into a folder with files went on"
[ "$(sources "$k")" = "$before" ] || fail "a refused car kit wrote into the kit"
"$studio" car build "$p" >"$work/log" 2>&1 && fail "car build of a piece (not a kit) went on"
[ ! -e "$p/build" ] || fail "a refused car build made build/ in the piece"

[ "$failures" -eq 0 ] && echo "car_kit_cli: ok"
exit "$failures"
