#!/bin/sh
# dep85 step 2: a car kit of flat sources (tnfs-studio car kit) and its build (tnfs-studio car build), the paths of core/formats/car_kit_paths.h.
# 1. car new of the Viper, then `car kit PIECE KIT`: the kit has README.txt, the near and far model (Blender OBJ and bank files), specs/
#    and the masks flat (no stem level).
# 2. `car build KIT`: build/piece/ holds the car's CFM byte for byte, its PBS and PDN with the same records (unpacked to the same TSVs),
#    the masks under the stem again; validate finds nothing; build/<id>.zip is made. The kit's sources are byte for byte as before, and a
#    second build gives the same bytes.
# 4. `car check KIT`: the kit has no finding; with no near OBJ it says the model does not build and fails; a mask with no texture and masks that paint nothing are warnings.
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
[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no DVIPER.CFM"; exit 77; }

# the kit's sources, each file's hash, build/ left out
sources() { (cd "$1" && find . -type f ! -path './build/*' | LC_ALL=C sort | xargs shasum) }

p="$work/piece"
"$studio" car new --template viper --id kit-car --stem KITCAR --out "$p" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
    fail "car new: $(cat "$work/log")"
mkdir -p "$p/car_masks/KITCAR" "$p/dash_masks/KITCAR" && cp "$p/card.png" "$p/car_masks/KITCAR/note.png" && cp "$p/card.png" "$p/dash_masks/KITCAR/dash_mask.png"   # a file under the stem, to see it come back there

# 1. the kit
k="$work/kit"
"$studio" car kit "$p" "$k" >"$work/log" 2>&1 || fail "car kit: $(cat "$work/log")"
for f in README.txt mod.ini card.png art-source/mesh/near.obj art-source/mesh/near.mtl art-generated/model/near/mesh/mesh.header.tsv model/near/bank/entries.tsv model/near/bank/bank.tsv \
         art-source/mesh/far.obj art-generated/model/far/mesh/mesh.polygons.tsv model/far/bank/bank.tsv specs/specs.tsv specs/pdn.tsv art-generated/car_masks/note.png art-generated/dash_masks/dash_mask.png; do
    [ -f "$k/$f" ] || fail "the kit has no $f"
done
[ ! -e "$k/art-generated/car_masks/KITCAR" ] || fail "the kit's masks keep the stem level"
[ ! -e "$k/car_masks" ] && [ ! -e "$k/dash_masks" ] || fail "the kit has masks at its top: they are generated, under art-generated/"
[ ! -e "$k/model/near/mesh" ] && [ ! -e "$k/model/far/mesh" ] || fail "the kit has a mesh folder under model/: the OBJ is art-source/mesh/, the tables art-generated/model/"
grep -q '^mtllib near.mtl$' "$k/art-source/mesh/near.obj" && grep -q '^mtllib far.mtl$' "$k/art-source/mesh/far.obj" || fail "an OBJ does not name its own MTL"
grep -q 'map_Kd \.\./\.\./model/near/bank/' "$k/art-source/mesh/near.mtl" || fail "near.mtl's textures are not at ../../model/near/bank/"
grep -q 'map_Kd far\.[a-z]*\.png' "$k/art-source/mesh/far.mtl" && [ -f "$k/art-source/mesh/far.frnt.png" ] || fail "far.mtl's see-through copies are not beside it, named far.<label>.png"   # the far MTL's other materials name no bank file
[ ! -e "$k/art-source/mesh/mesh.blender.obj" ] || fail "the OBJ keeps its old name"
[ ! -e "$k/build" ] || fail "car kit made build/"
grep -q "disposable" "$k/README.txt" || fail "README.txt does not say build/ is disposable"
grep -q "model/near/bank" "$k/README.txt" && grep -q "not tested" "$k/README.txt" || fail "README.txt does not say where the textures are and that the MTL path is untested"

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
[ -f "$b/car_masks/KITCAR/note.png" ] && [ -f "$b/dash_masks/KITCAR/dash_mask.png" ] || fail "the build has no car_masks/KITCAR/note.png and dash_masks/KITCAR/dash_mask.png"
[ ! -e "$b/art-generated" ] && [ ! -e "$b/art-source" ] || fail "the build carries the kit's art folders"
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

# an old-layout kit (the OBJ and tables in model/near/mesh) is refused in words, and nothing is built or moved
old="$work/oldkit"
mkdir -p "$old/model/near/mesh" "$old/model/near/bank" && cp "$k/mod.ini" "$old/" && cp "$k/model/near/bank/bank.tsv" "$old/model/near/bank/" &&
    cp "$k/art-source/mesh/near.obj" "$old/model/near/mesh/mesh.blender.obj"
"$studio" car build "$old" >"$work/log" 2>&1 && fail "car build of an old-layout kit went on"
grep -q "old layout" "$work/log" && grep -q "art-source/mesh" "$work/log" && grep -q "car kit" "$work/log" || fail "the old-layout refusal does not say so: $(cat "$work/log")"
[ ! -e "$old/build" ] || fail "a refused car build made build/ in an old-layout kit"
# 4. car check
"$studio" car check "$k" >"$work/log" 2>&1 || fail "car check of the kit: $(cat "$work/log")"
grep -q "^0 errors, 0 warnings" "$work/log" || fail "car check of the kit found something: $(cat "$work/log")"
cp -R "$k" "$work/nomesh" && rm "$work/nomesh/art-source/mesh/near.obj"
"$studio" car check "$work/nomesh" >"$work/log" 2>&1 && fail "car check of a kit with no near OBJ went on"
grep -q "^error: the model does not build" "$work/log" || fail "car check does not say the model does not build: $(cat "$work/log")"
cp -R "$k" "$work/masks"
m=$(ls "$work/masks/art-generated/car_masks" | head -1)
cp "$work/masks/art-generated/car_masks/$m" "$work/masks/art-generated/car_masks/zzzz_mask.png"
"$studio" car check "$work/masks" >"$work/log" 2>&1 || fail "car check of a kit with a mask of no texture is a warning, not an error: $(cat "$work/log")"
grep -q "^warning: zzzz_mask.png has no texture zzzz" "$work/log" || fail "car check does not say the mask has no texture: $(cat "$work/log")"
python3 -I - "$work/masks/art-generated/car_masks" <<'PY'
import os, struct, sys, zlib
def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress((b"\x00" + b"\xff" * 4) * 4)) + chunk(b"IEND", b"")
for n in os.listdir(sys.argv[1]):
    open(os.path.join(sys.argv[1], n), "wb").write(png)
PY
"$studio" car check "$work/masks" >"$work/log" 2>&1 || fail "car check of a kit whose masks paint nothing is a warning: $(cat "$work/log")"
grep -q "^warning: no mask has a texel that takes the car's paint" "$work/log" || fail "car check does not say no mask paints: $(cat "$work/log")"

[ "$failures" -eq 0 ] && echo "car_kit_cli: ok"
exit "$failures"
