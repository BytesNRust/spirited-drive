#!/bin/sh
# dep100 item 6: the originals as kits (tools/car_export_kits.c, editor/car_export_kit.c). Every car Export original lists with masks (20) is made a kit in the
# current layout, and each kit builds the original's CFM byte for byte (the exporter says so); the masks in art-generated are the game's data/ bytes (but the
# stray data/car_masks/<CAR>/dash_mask.png, which no kit carries); a traffic car's kit has the minimal card and no specs; a drivable car's has its specs.
# `car build` of an original's kit makes its car file only (a preview build: build/piece/SIMDATA/CARFAMS/<STEM>.CFM, the original's bytes) and no package: a
# traffic car has no PBS and an original's stem is not allowed for a piece, so there is nothing to validate or install.
# usage: car_export_kits.sh CAR_EXPORT_KITS ORIGINAL_DIR DATA_DIR TNFS_STUDIO
set -u
exporter=$1
orig=$2
data=$3
studio=$4
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no DVIPER.CFM"; exit 77; }

"$exporter" "$orig" "$data" "$work/out" >"$work/log" 2>&1 || fail "the export: $(tail -3 "$work/log")"
grep -q '^20 kits made, 0 failed$' "$work/log" || fail "not 20 kits: $(tail -2 "$work/log")"
[ "$(grep -c '^cfm .*byte for byte$' "$work/log")" -eq 20 ] || fail "not every kit builds its original's CFM"
for stem in p911 dviper wagon copmust axxess; do
    k="$work/out/$stem"
    for f in mod.ini README.txt art-source/mesh/near.obj art-source/mesh/far.obj art-generated/model/near/mesh/mesh.header.tsv model/near/bank/bank.tsv model/far/bank/bank.tsv; do
        [ -f "$k/$f" ] || fail "$stem has no $f"
    done
    [ ! -e "$k/model/near/mesh" ] && [ ! -e "$k/car_masks" ] || fail "$stem has an old-layout folder"
    [ ! -e "$k/art-generated/car_masks/dash_mask.png" ] || fail "$stem carries the stray car_masks dash_mask.png"
done
for f in "$work"/out/p911/art-generated/car_masks/*_mask.png; do
    cmp -s "$f" "$data/car_masks/P911/${f##*/}" || fail "p911's ${f##*/} is not data/'s"
done
cmp -s "$work/out/p911/art-generated/dash_masks/dash_mask.png" "$data/dash_masks/P911/dash_mask.png" || fail "p911's dash mask is not data/'s"
[ -f "$work/out/p911/specs/specs.tsv" ] && [ ! -e "$work/out/wagon/specs/specs.tsv" ] || fail "specs: the 911 has them, the wagon has none"
grep -q '^stem=WAGON$' "$work/out/wagon/mod.ini" && grep -q '^redistributable=0$' "$work/out/wagon/mod.ini" || fail "the wagon's minimal card"
for stem in wagon copmust; do   # no engine of the eight to take the sound from: the game's traffic sound, so the card shows no problem
    grep -q '^sound_from=traffic$' "$work/out/$stem/mod.ini" || fail "the $stem card has no sound_from=traffic"
done
for stem in p911 wagon copmust; do
    up=$(echo "$stem" | tr a-z A-Z)
    "$studio" car build "$work/out/$stem" >"$work/log3" 2>&1 || fail "car build of the original $stem: $(cat "$work/log3")"
    cmp -s "$orig/SIMDATA/CARFAMS/$up.CFM" "$work/out/$stem/build/piece/SIMDATA/CARFAMS/$up.CFM" || fail "$stem's preview CFM is not the original's"
    ls "$work/out/$stem"/build/*.zip >/dev/null 2>&1 && fail "$stem was packaged: an original has no package"
done
"$exporter" "$orig" "$data" "$work/out" >"$work/log2" 2>&1 && fail "a second export into the same folder went on"
[ "$failures" -eq 0 ] && echo "car_export_kits: ok"
exit "$failures"
