#!/bin/sh
# dep71 follow-up: a car piece's texture resized and its uvs following (tnfs-studio car resize, studio/car_resize.h).
# 1. car new of the Viper with a skin of rsid; `car resize PIECE rsid` to twice its size: validate finds nothing (the skin was scaled
#    with it), unpack shows rsid at its new size, the uvs of rsid's faces twice what they were and every other line of mesh.obj as it
#    was; resizing to the size it has gives the same CFM again (a rebuild may run it every time).
# 2. --image: a picture of the size asked is taken, one of another size refused with nothing written; --low resizes the far model's.
# 3. The the 3D editor round trip does not undo it: the Viper's export from before the resize, through the 3D editor and back with --raw --force
#    (the kit's command), gives the resized CFM byte for byte.
# 4. The resized piece races (Restomod, alone on TR1), with no sanitizer report.
# usage: car_resize.sh TNFS STUDIO ORIGINAL_DIR PORT_DATA_DIR MODELLER_SIM_TOOL
set -u
tnfs=$1
studio=$2
orig=$3
port=$4
modeller_sim=$5
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no DVIPER.CFM"; exit 0; }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0
clean() { ! grep -qE "ERROR: AddressSanitizer|runtime error:|ERROR: LeakSanitizer" "$1"; }

valid() {
    "$studio" validate "$1" >"$work/log" 2>&1 || fail "validate of $1: $(cat "$work/log")"
    grep -q "no findings" "$work/log" || fail "$1 has findings: $(cat "$work/log")"
}

# the size entries.tsv gives an entry: "WxH"
size_of() { awk -F'\t' -v n="$2" 'NR > 1 && $3 == n { print $6 "x" $7 }' "$1/entries.tsv"; }

# 1. the high bank's rsid at twice its size, a skin with it
p=$work/piece
"$studio" car new --template viper --id rs-car --stem RSCAR --out "$p" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
    fail "car new: $(cat "$work/log")"
"$studio" unpack "$p/SIMDATA/CARFAMS/RSCAR.CFM" "$work/d" >/dev/null 2>&1 || fail "unpack of the new piece"
was=$(size_of "$work/d/c1" rsid)
[ "$was" = "226x75" ] || fail "the Viper's rsid is '$was', not 226x75"
mkdir -p "$p/skins/red" && cp "$work/d/c1/"[0-9][0-9][0-9][0-9]_rsid.png "$p/skins/red/rsid.png"
valid "$p"
"$studio" car resize "$p" rsid 452x150 >"$work/log" 2>&1 || fail "car resize: $(cat "$work/log")"
clean "$work/log" || fail "car resize: a sanitizer report"
grep -q "rsid 226x75 -> 452x150" "$work/log" && grep -q "1 skin pictures scaled" "$work/log" || fail "car resize said: $(cat "$work/log")"
valid "$p"
"$studio" unpack "$p/SIMDATA/CARFAMS/RSCAR.CFM" "$work/n" >/dev/null 2>&1 || fail "unpack of the resized piece"
[ "$(size_of "$work/n/c1" rsid)" = "452x150" ] || fail "rsid is $(size_of "$work/n/c1" rsid) after the resize"
# mesh.obj: the vt lines of rsid's faces doubled, the rest as it was (the Viper's rsid shares no uv with another texture)
awk 'NR == FNR { if ($1 == "vt") old[++n] = $2 " " $3; next } $1 == "vt" { m++; split(old[m], o, " ");
    if ($2 != o[1] && $2 != 2 * o[1]) bad++; if ($3 != o[2] && $3 != 2 * o[2]) bad++; if ($2 != o[1] || $3 != o[2]) moved++ }
    END { if (m != n || bad || !moved) { print m, n, bad, moved; exit 1 } }' "$work/d/c0/mesh.obj" "$work/n/c0/mesh.obj" ||
    fail "the resized piece's uvs are not the Viper's, doubled on rsid"
[ "$(grep -v '^vt ' "$work/d/c0/mesh.obj" | cksum)" = "$(grep -v '^vt ' "$work/n/c0/mesh.obj" | cksum)" ] ||
    fail "the resize changed mesh.obj beyond its uvs"
for b in c2 c3; do
    for f in "$work/d/$b"/*; do cmp -s "$f" "$work/n/$b/${f##*/}" || fail "the resize changed the far model's $b/${f##*/}"; done
done
cp "$p/SIMDATA/CARFAMS/RSCAR.CFM" "$work/resized.cfm"
"$studio" car resize "$p" rsid 452x150 >"$work/log" 2>&1 || fail "car resize to the size it has: $(cat "$work/log")"
cmp -s "$work/resized.cfm" "$p/SIMDATA/CARFAMS/RSCAR.CFM" || fail "resizing to the size it has changed the CFM"

# 2. --image, refused at another size; --low
"$studio" car resize "$p" rsid 100x100 --image "$p/skins/red/rsid.png" >"$work/log" 2>&1 && fail "an image of another size was taken"
grep -q "not the 100x100 asked" "$work/log" || fail "the refusal said: $(cat "$work/log")"
cmp -s "$work/resized.cfm" "$p/SIMDATA/CARFAMS/RSCAR.CFM" || fail "a refused resize wrote the CFM"
"$studio" car resize "$p" rsid --image "$p/skins/red/rsid.png" >"$work/log" 2>&1 || fail "car resize --image: $(cat "$work/log")"
cmp -s "$work/resized.cfm" "$p/SIMDATA/CARFAMS/RSCAR.CFM" || fail "the bitmap's own picture given back changed the CFM"
"$studio" car resize "$p" side 248x58 --low >"$work/log" 2>&1 || fail "car resize --low: $(cat "$work/log")"
"$studio" unpack "$p/SIMDATA/CARFAMS/RSCAR.CFM" "$work/l" >/dev/null 2>&1
[ "$(size_of "$work/l/c3" side)" = "248x58" ] && [ "$(size_of "$work/l/c1" rsid)" = "452x150" ] ||
    fail "--low: side is $(size_of "$work/l/c3" side), rsid $(size_of "$work/l/c1" rsid)"
valid "$p"
cp "$p/SIMDATA/CARFAMS/RSCAR.CFM" "$work/resized.cfm"

# 3. the kit's the 3D editor round trip, from the export made before the resize
"$modeller_sim" "$work/d/c0/mesh.edit.obj" "$work/high.obj" && "$modeller_sim" "$work/d/c2/mesh.edit.obj" "$work/low.obj" ||
    fail "modeller_sim_tool"
"$studio" car import "$p" --raw --obj "$work/high.obj" --tables "$work/d/c0" --low "$work/low.obj" --low-tables "$work/d/c2" --keep-pdn \
    --force --data "$orig" >"$work/log" 2>&1 || fail "car import through the 3D editor: $(cat "$work/log")"
clean "$work/log" || fail "car import through the 3D editor: a sanitizer report"
cmp -s "$work/resized.cfm" "$p/SIMDATA/CARFAMS/RSCAR.CFM" || fail "the the 3D editor round trip did not give the resized CFM back"
valid "$p"

# 4. it races
mkdir -p "$work/user" && echo "profile=restomod" > "$work/user/port.cfg" && echo "extra_liveries=0" > "$work/user/restomod.cfg"
timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" --mods "$p" --drive TR1 --car rs-car --racers 1 --node 200 \
    --screenshot "$work/race.ppm" >/dev/null 2>"$work/err" || fail "the resized piece did not race: $(tail -3 "$work/err")"
clean "$work/err" || fail "the race: a sanitizer report: $(grep -m1 -E "ERROR|runtime error" "$work/err")"
[ -s "$work/race.ppm" ] || fail "the race: no frame"

[ "$failures" -eq 0 ] || exit 1
echo "car_resize: ok"
