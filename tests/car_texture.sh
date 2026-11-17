#!/bin/sh
# dep71 follow-up 2: a bitmap of the car's own (tnfs-studio car texture add, studio/car_texture.h).
# 1. car new of the Viper, `car texture add PIECE badg PICTURE`: it says the material t<N>_badg; validate finds nothing; unpack lists badg
#    after the bank's last entry at the picture's size, a reference to it, and Blender's MTL a material t<N>_badg with its PNG.
# 2. Refused, nothing written: a name the bank has, one the race looks up, one of capitals, a label of the race's.
# 3. Through Blender: two faces put on t<N>_badg come back on it (raw --force), the bank kept; validate finds nothing. A far face on it
#    is refused naming --low; once added with --low, taken.
# 4. pack of its unpack with --from gives the CFM back; it races (Restomod, alone on TR1), with no sanitizer report.
# usage: car_texture.sh TNFS STUDIO ORIGINAL_DIR PORT_DATA_DIR BLENDER_LIKE_TOOL
set -u
tnfs=$1
studio=$2
orig=$3
port=$4
blender_like=$5
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

# OBJ MTL OUT: OBJ with its first two faces under the material MTL, the faces after them under theirs
two_faces_on() {
    awk -v m="$2" '/^usemtl / { last = $0; if (f == 1) next } /^f / && f == 0 { print "usemtl " m } { print }
        /^f / { f++; if (f == 2 && last != "") print last }' "$1" > "$3"
}

# the faces of MESH.obj under the material MTL
faces_on() { awk -v m="usemtl $2" '/^usemtl / { on = ($0 == m) } /^f / && on { n++ } END { print n + 0 }' "$1"; }

p=$work/piece
cfm=$p/SIMDATA/CARFAMS/TXCAR.CFM
"$studio" car new --template viper --id tx-car --stem TXCAR --out "$p" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
    fail "car new: $(cat "$work/log")"
"$studio" unpack "$cfm" "$work/d" >/dev/null 2>&1 || fail "unpack of the new piece"
pic=$(ls "$work/d/c1/"[0-9][0-9][0-9][0-9]_frnt.png)

# 1. added
"$studio" car texture add "$p" badg "$pic" >"$work/log" 2>&1 || fail "car texture add: $(cat "$work/log")"
clean "$work/log" || fail "car texture add: a sanitizer report"
mtl=$(sed -n 's/.*the material \(t[0-9]*_badg\).*/\1/p' "$work/log")
[ -n "$mtl" ] || fail "car texture add did not say the material: $(cat "$work/log")"
valid "$p"
"$studio" unpack "$cfm" "$work/n" >/dev/null 2>&1 || fail "unpack of the piece with badg"
last=$(tail -1 "$work/n/c1/entries.tsv" | cut -f3,6,7 | tr '\t' ' ')
[ "$last" = "badg 40 21" ] || fail "the bank's last entry is '$last', not badg 40 21"
[ "$(wc -l < "$work/n/c0/mesh.textures.tsv")" -eq $(($(wc -l < "$work/d/c0/mesh.textures.tsv") + 1)) ] || fail "no reference was added"
grep -q "^newmtl $mtl\$" "$work/n/c0/mesh.blender.mtl" || fail "Blender's MTL has no $mtl"
cp "$cfm" "$work/added.cfm"

# 2. refusals
for args in "badg $pic" "shad $pic" "Badg $pic" "logo $pic --label tire1"; do
    # shellcheck disable=SC2086
    "$studio" car texture add "$p" $args >"$work/log" 2>&1 && fail "car texture add $args was taken"
    clean "$work/log" || fail "car texture add $args: a sanitizer report"
done
cmp -s "$work/added.cfm" "$cfm" || fail "a refused add wrote the CFM"

# 3. through Blender: two faces on badg
two_faces_on "$work/n/c0/mesh.blender.obj" "$mtl" "$work/edit.obj"
"$blender_like" "$work/edit.obj" "$work/high.obj" && "$blender_like" "$work/n/c2/mesh.blender.obj" "$work/low.obj" || fail "blender_like_tool"
"$studio" car import "$p" --raw --obj "$work/high.obj" --tables "$work/n/c0" --low "$work/low.obj" --low-tables "$work/n/c2" --keep-pdn \
    --force --data "$orig" >"$work/log" 2>&1 || fail "car import with two faces on badg: $(cat "$work/log")"
clean "$work/log" || fail "car import with two faces on badg: a sanitizer report"
valid "$p"
"$studio" unpack "$cfm" "$work/b" >/dev/null 2>&1 || fail "unpack after Blender"
[ "$(faces_on "$work/b/c0/mesh.obj" "$mtl")" -eq 2 ] || fail "$(faces_on "$work/b/c0/mesh.obj" "$mtl") faces on $mtl, not 2"
cmp -s "$work/n/c1/entries.tsv" "$work/b/c1/entries.tsv" && cmp -s "$work/n/c1/0012_badg.png" "$work/b/c1/0012_badg.png" ||
    fail "the import did not keep the bank with badg"
# a far face on it: refused, naming --low; added with --low, taken
lowmtl=$mtl
two_faces_on "$work/b/c2/mesh.blender.obj" "$lowmtl" "$work/edit_low.obj"
"$blender_like" "$work/b/c0/mesh.blender.obj" "$work/high2.obj" && "$blender_like" "$work/edit_low.obj" "$work/low2.obj" || fail "blender_like_tool"
cp "$cfm" "$work/two.cfm"
"$studio" car import "$p" --raw --obj "$work/high2.obj" --tables "$work/b/c0" --low "$work/low2.obj" --low-tables "$work/b/c2" --keep-pdn \
    --force --data "$orig" >"$work/log" 2>&1 && fail "a far face on $lowmtl was taken with no badg in the far model"
grep -q -- "--low" "$work/log" || fail "the far refusal does not name --low: $(cat "$work/log")"
cmp -s "$work/two.cfm" "$cfm" || fail "a refused import wrote the CFM"
"$studio" car texture add "$p" badg "$pic" --low >"$work/log" 2>&1 || fail "car texture add --low: $(cat "$work/log")"
lowmtl=$(sed -n 's/.*the material \(t[0-9]*_badg\).*/\1/p' "$work/log")
"$studio" unpack "$cfm" "$work/f" >/dev/null 2>&1
two_faces_on "$work/f/c2/mesh.blender.obj" "$lowmtl" "$work/edit_low.obj"
"$blender_like" "$work/f/c0/mesh.blender.obj" "$work/high3.obj" && "$blender_like" "$work/edit_low.obj" "$work/low3.obj" || fail "blender_like_tool"
"$studio" car import "$p" --raw --obj "$work/high3.obj" --tables "$work/f/c0" --low "$work/low3.obj" --low-tables "$work/f/c2" --keep-pdn \
    --force --data "$orig" >"$work/log" 2>&1 || fail "a far face on badg added --low: $(cat "$work/log")"
"$studio" unpack "$cfm" "$work/g" >/dev/null 2>&1
[ "$(faces_on "$work/g/c2/mesh.obj" "$lowmtl")" -eq 2 ] || fail "$(faces_on "$work/g/c2/mesh.obj" "$lowmtl") far faces on $lowmtl, not 2"
[ "$(faces_on "$work/g/c0/mesh.obj" "$mtl")" -eq 2 ] || fail "the near faces on $mtl were lost"
valid "$p"

# 4. pack, race
"$studio" pack "$work/g" "$work/packed.cfm" --from "$cfm" >"$work/log" 2>&1 || fail "pack: $(cat "$work/log")"
cmp -s "$work/packed.cfm" "$cfm" || fail "pack of the unpacked piece did not give its CFM back"
mkdir -p "$work/user" && echo "profile=restomod" > "$work/user/port.cfg" && echo "extra_liveries=0" > "$work/user/restomod.cfg"
timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" --mods "$p" --drive TR1 --car tx-car --racers 1 --node 200 \
    --screenshot "$work/race.ppm" >/dev/null 2>"$work/err" || fail "the piece did not race: $(tail -3 "$work/err")"
clean "$work/err" || fail "the race: a sanitizer report: $(grep -m1 -E "ERROR|runtime error" "$work/err")"
[ -s "$work/race.ppm" ] || fail "the race: no frame"

[ "$failures" -eq 0 ] || exit 1
echo "car_texture: ok"
