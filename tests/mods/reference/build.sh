#!/bin/sh
# Builds the reference mod (BYTES-0009 AC11) from the original CD with tnfs-studio; nothing of EA's is stored.
#   track TR1: the road bends away from node 60 on (nodes move sideways, up to 300000 units), one more scenery
#              placement at node 100, and one road texture (04B0 of TR1_T01.FAM) in other colours
#   car DVIPER: power +20 %, the side texture (rsid) in other colours
# usage: build.sh STUDIO ORIGINAL_DIR OUT_DIR       (OUT_DIR must not exist or be empty; needs python3 for the textures)
set -eu
studio=$1
orig=$2
out=$3
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$out/SIMDATA/MISC" "$out/SIMDATA/NTRACKFM" "$out/SIMDATA/CARSPECS" "$out/SIMDATA/CARFAMS"

# --- track: a curve changed, an object added
"$studio" unpack "$orig/SIMDATA/MISC/TR1.TRI" "$work/tri" >/dev/null
awk -F'\t' 'BEGIN { OFS = "\t" }
    $1 ~ /^[0-9]+$/ && $1 >= 60 { shift = ($1 - 60) * 10000; if (shift > 300000) shift = 300000; $8 = $8 + shift }
    { print }' "$work/tri/nodes.tsv" > "$work/nodes.tsv"
cp "$work/nodes.tsv" "$work/tri/nodes.tsv"
awk -F'\t' 'BEGIN { OFS = "\t" }
    $1 ~ /^[0-9]+$/ { last = $0; count = $1 + 1 }
    { print }
    END { split(last, f, "\t"); f[1] = count; f[2] = 100; f[6] = -6000; print f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8] }' \
    "$work/tri/scenery.tsv" > "$work/scenery.tsv"
cp "$work/scenery.tsv" "$work/tri/scenery.tsv"
"$studio" pack "$work/tri" "$out/SIMDATA/MISC/TR1.TRI" --from "$orig/SIMDATA/MISC/TR1.TRI" >/dev/null

# --- track: a texture changed
"$studio" unpack "$orig/SIMDATA/NTRACKFM/TR1_T01.FAM" "$work/fam" >/dev/null
python3 -I "$here/png_shift.py" "$work/fam/c0/c0/0007_04B0.png" 32
"$studio" pack "$work/fam" "$out/SIMDATA/NTRACKFM/TR1_T01.FAM" --from "$orig/SIMDATA/NTRACKFM/TR1_T01.FAM" >/dev/null

# --- car: power +20 %
"$studio" unpack "$orig/SIMDATA/CARSPECS/DVIPER.PBS" "$work/pbs" >/dev/null
awk -F'\t' 'BEGIN { OFS = "\t" } $2 == "power" { $4 = int($4 * 12 / 10) } { print }' "$work/pbs/specs.tsv" > "$work/specs.tsv"
cp "$work/specs.tsv" "$work/pbs/specs.tsv"
"$studio" pack "$work/pbs" "$out/SIMDATA/CARSPECS/DVIPER.PBS" --from "$orig/SIMDATA/CARSPECS/DVIPER.PBS" >/dev/null

# --- car: a texture changed
"$studio" unpack "$orig/SIMDATA/CARFAMS/DVIPER.CFM" "$work/cfm" >/dev/null
python3 -I "$here/png_shift.py" "$work/cfm/c1/0011_rsid.png" 32
"$studio" pack "$work/cfm" "$out/SIMDATA/CARFAMS/DVIPER.CFM" --from "$orig/SIMDATA/CARFAMS/DVIPER.CFM" >/dev/null

"$studio" validate "$out" >/dev/null
echo "reference mod written to $out"
