#!/bin/sh
# dep59 tools B2: a car modelled outside the game, and the Viper exported and imported back, validate and race.
# 1. tests/car_import/box.obj (a body, a glass cabin clear in the middle, a seat, four wheels; nothing of the CD) on a piece of
#    `car new --template viper`: validate finds nothing, it races in Restomod alone on the road (extra_liveries=0, as model_build.sh),
#    its frame is not the template's, and `mod pack` gives the same zip twice.
# 2. The low-detail model seen from far is skipped: no camera of the command line puts the player's car past car_draw_far_z (the 14
#    view kinds, TNFS_VIEW, measured with a low body three times taller: none draws it).
# 3. The Viper unpacked and imported back with --raw (both meshes, both banks): validate finds nothing, it races; unpacked again,
#    its meshes' OBJ and tables are the Viper's, each bitmap and palette has the Viper's bytes (raw keeps the template's banks as they
#    are), its MTLs differ only in those file numbers, and the PDN's measured half sizes are within a tenth of the Viper's.
# 4. dep71: every race car of the CD through Blender and back: a piece of it, unpacked, both meshes' mesh.blender.obj rewritten as
#    Blender's exporter writes them (blender_like_tool: tests/blender_like.h; half the cars with their faces in material blocks), taken
#    back with --raw --tables --keep-pdn; unpacked again, its meshes' OBJs and tables are the car's, and the CFM is byte-identical
#    (fix/raw-import-banks: raw keeps the banks as they are). A vertex moved a metre up comes back 130 units up; that import has no
#    --low, and the far model, its bank and the piece's _lo_mask.png stay. The owner's kit's command (car new of the 911, the
#    mesh.blender.obj files copied unchanged, --low, --force) gives the CFM back byte for byte.
# With a sanitizer build, no report.
# usage: car_import.sh TNFS STUDIO FIXTURE_DIR ORIGINAL_DIR PORT_DATA_DIR BLENDER_LIKE_TOOL
set -u
tnfs=$1
studio=$2
fixture=$3
orig=$4
port=$5
blender_like=$6
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

[ -f "$orig/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no DVIPER.CFM"; exit 77; }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0

restomod() {
    rm -rf "$work/user"
    mkdir -p "$work/user" && echo "profile=restomod" > "$work/user/port.cfg" && echo "extra_liveries=0" > "$work/user/restomod.cfg"
}
restomod
timeout -k 5 60 "$tnfs" --data "$orig" --user "$work/user" --drive TR1 --car no-such-car --node 1 --screenshot "$work/probe.ppm" \
    >/dev/null 2>"$work/err"
grep -q "no car no-such-car" "$work/err" || { echo "car_import: the game's --car takes no id: skipped"; exit 77; }

clean() { ! grep -qE "ERROR: AddressSanitizer|runtime error:|ERROR: LeakSanitizer" "$1"; }

# race PIECE ID OUT.ppm: the piece in Restomod alone on TR1, its frame at node 200
race() {
    restomod
    timeout -k 5 120 "$tnfs" --data "$orig" --user "$work/user" --mods "$1" --drive TR1 --car "$2" --racers 1 --node 200 \
        --screenshot "$3" >/dev/null 2>"$work/err" || { fail "$1 did not race: $(tail -3 "$work/err")"; return 1; }
    clean "$work/err" || fail "$1: a sanitizer report: $(grep -m1 -E "ERROR|runtime error" "$work/err")"
    [ -s "$3" ] || fail "$1: no frame"
}

# new ID STEM DIR: a piece of the Viper
new() {
    "$studio" car new --template viper --id "$1" --stem "$2" --out "$3" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
        fail "car new $1: $(cat "$work/log")"
}

valid() {
    "$studio" validate "$1" >"$work/log" 2>&1 || fail "validate of $1: $(cat "$work/log")"
    grep -q "no findings" "$work/log" || fail "$1 has findings: $(cat "$work/log")"
}

# 1. the box car
new box-car BOXCAR "$work/fresh"
race "$work/fresh" box-car "$work/fresh.ppm"
new box-car BOXCAR "$work/box"
"$studio" car import "$work/box" --obj "$fixture/box.obj" --data "$orig" >"$work/log" 2>&1 || fail "car import of box.obj: $(cat "$work/log")"
clean "$work/log" || fail "car import of box.obj: a sanitizer report"
valid "$work/box"
race "$work/box" box-car "$work/box.ppm"
[ -s "$work/box.ppm" ] && cmp -s "$work/box.ppm" "$work/fresh.ppm" && fail "the box car draws the template's frame"
"$studio" mod pack "$work/box" "$work/box1.zip" >"$work/log" 2>&1 || fail "mod pack: $(cat "$work/log")"
"$studio" mod pack "$work/box" "$work/box2.zip" >/dev/null 2>&1
cmp -s "$work/box1.zip" "$work/box2.zip" || fail "mod pack of the box car gives two zips"

# 2. the low-detail model from far
echo "SKIPPED-PART: car_import: the low-detail model from far is skipped: no camera of the command line goes past car_draw_far_z"

# 3. the Viper back
"$studio" unpack "$orig/SIMDATA/CARFAMS/DVIPER.CFM" "$work/d" >/dev/null 2>&1 || fail "unpack of DVIPER.CFM"
new rt-car RTCAR "$work/rt"
"$studio" car import "$work/rt" --raw --obj "$work/d/c0/mesh.obj" --textures "$work/d/c1" --low "$work/d/c2/mesh.obj" \
    --low-textures "$work/d/c3" --data "$orig" >"$work/log" 2>&1 || fail "car import --raw of the Viper: $(cat "$work/log")"
valid "$work/rt"
race "$work/rt" rt-car "$work/rt.ppm"
"$studio" unpack "$work/rt/SIMDATA/CARFAMS/RTCAR.CFM" "$work/n" >/dev/null 2>&1 || fail "unpack of the imported Viper"
for mesh in c0 c2; do
    for f in "$work/d/$mesh"/*.obj "$work/d/$mesh"/*.tsv; do
        cmp -s "$f" "$work/n/$mesh/${f##*/}" || fail "the imported Viper's $mesh/${f##*/} is not the Viper's"
    done
    sed 's|/[0-9][0-9][0-9][0-9]_|/|' "$work/d/$mesh/mesh.mtl" > "$work/mtl_d"
    sed 's|/[0-9][0-9][0-9][0-9]_|/|' "$work/n/$mesh/mesh.mtl" > "$work/mtl_n"
    cmp -s "$work/mtl_d" "$work/mtl_n" || fail "the imported Viper's $mesh/mesh.mtl names other textures"
done
for bank in c1 c3; do
    count=0
    for f in "$work/d/$bank"/[0-9][0-9][0-9][0-9]_*; do
        label=${f##*/[0-9][0-9][0-9][0-9]_}
        set -- "$work/n/$bank"/[0-9][0-9][0-9][0-9]_"$label"
        cmp -s "$f" "$1" || fail "the imported Viper's $bank/$label is not the Viper's"
        count=$((count + 1))
    done
    [ "$(ls "$work/n/$bank" | grep -c '^[0-9][0-9][0-9][0-9]_')" -eq "$count" ] || fail "the imported Viper's $bank has other entries"
done
"$studio" unpack "$orig/SIMDATA/CARFAMS/DVIPER.PDN" "$work/pv" >/dev/null 2>&1
"$studio" unpack "$work/rt/SIMDATA/CARFAMS/RTCAR.PDN" "$work/pn" >/dev/null 2>&1
paste "$work/pv/pdn.tsv" "$work/pn/pdn.tsv" | awk -F'\t' '$1 == "0x000" { for (k = 4; k <= 6; k++) { d = $k - $(k + 6); if (d < 0) d = -d;
    if (d * 10 > $(k + 6)) bad = 1 } found = 1 } END { exit !(found && !bad) }' ||
    fail "the imported Viper's half sizes are not within a tenth of the Viper's: $(grep '^0x000' "$work/pn/pdn.tsv")"

# 4. every race car through Blender and back, half of them with their faces in material blocks (as Blender's exporter may write them)
for car in rx7 nsx supra 911 zr1 viper 512tr diablo; do
    b=$work/bl_$car
    case $car in rx7 | supra | zr1 | 512tr) grouped=--by-material ;; *) grouped= ;; esac
    "$studio" car new --template "$car" --id "bl-$car" --stem BLCAR --out "$b/piece" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
        { fail "car new of $car: $(cat "$work/log")"; continue; }
    "$studio" unpack "$b/piece/SIMDATA/CARFAMS/BLCAR.CFM" "$b/d" >/dev/null 2>&1 || { fail "unpack of $car"; continue; }
    "$blender_like" $grouped "$b/d/c0/mesh.blender.obj" "$b/high.obj" && "$blender_like" $grouped "$b/d/c2/mesh.blender.obj" "$b/low.obj" ||
        { fail "$car: blender_like_tool"; continue; }
    cp "$b/piece/SIMDATA/CARFAMS/BLCAR.CFM" "$b/before.cfm"
    "$studio" car import "$b/piece" --raw --obj "$b/high.obj" --tables "$b/d/c0" --low "$b/low.obj" --low-tables "$b/d/c2" --keep-pdn \
        --data "$orig" --force >"$work/log" 2>&1 || { fail "car import of $car's Blender OBJs: $(cat "$work/log")"; continue; }
    cmp -s "$b/before.cfm" "$b/piece/SIMDATA/CARFAMS/BLCAR.CFM" || fail "$car through Blender unchanged: the CFM is not the same bytes"
    clean "$work/log" || fail "car import of $car's Blender OBJs: a sanitizer report"
    "$studio" unpack "$b/piece/SIMDATA/CARFAMS/BLCAR.CFM" "$b/n" >/dev/null 2>&1 || { fail "unpack of $car back"; continue; }
    for mesh in c0 c2; do
        for f in "$b/d/$mesh"/*.obj "$b/d/$mesh"/*.tsv; do
            cmp -s "$f" "$b/n/$mesh/${f##*/}" || fail "$car through Blender: its $mesh/${f##*/} is not the car's"
        done
    done
done
# the import does write: the Viper's first vertex a metre up in Blender comes back 130 units up, the only line of mesh.obj that differs
b=$work/bl_viper
awk '!done && /^v / { $3 = sprintf("%.6f", $3 + 1); done = 1 } { print }' "$b/high.obj" > "$b/moved.obj" && cp "$b/high.mtl" "$b/moved.mtl"
lo_masks=$(find "$b/piece" -name '*_lo_mask.png' | sort)
"$studio" car import "$b/piece" --raw --obj "$b/moved.obj" --tables "$b/d/c0" --keep-pdn --data "$orig" --force >"$work/log" 2>&1 ||
    fail "car import of the Viper with a vertex moved: $(cat "$work/log")"
"$studio" unpack "$b/piece/SIMDATA/CARFAMS/BLCAR.CFM" "$b/m" >/dev/null 2>&1 || fail "unpack of the moved Viper"
was=$(grep -m1 '^v ' "$b/d/c0/mesh.obj") now=$(grep -m1 '^v ' "$b/m/c0/mesh.obj")
[ "$(diff "$b/d/c0/mesh.obj" "$b/m/c0/mesh.obj" | grep -c '^[<>]')" -eq 2 ] &&
    [ "$(echo "$now" | awk '{ print $3 }')" -eq $(( $(echo "$was" | awk '{ print $3 }') + 130 )) ] ||
    fail "the Viper's first vertex moved a metre up in Blender is '$now' (was '$was')"
# without --low the far model, its bank and its masks are the car's
for f in "$b/d/c2"/* "$b/d/c3"/*; do
    cmp -s "$f" "$b/m/${f#"$b/d/"}" || fail "the Viper imported without --low: its far ${f#"$b/d/"} changed"
done
[ -n "$lo_masks" ] || fail "the Viper's piece has no _lo_mask.png to keep"
[ "$(find "$b/piece" -name '*_lo_mask.png' | sort)" = "$lo_masks" ] || fail "the Viper imported without --low: its _lo_mask.png were removed"

# the owner's kit's command, on a car new piece of the 911: the mesh.blender.obj files as unpack wrote them
k=$work/kit
"$studio" car new --template 911 --id carrera-84 --stem C84 --out "$k/P" --data "$orig" --port-data "$port" >"$work/log" 2>&1 ||
    fail "car new of the kit's 911: $(cat "$work/log")"
cp "$k/P/SIMDATA/CARFAMS/C84.CFM" "$k/before.cfm"
"$studio" unpack "$k/P/SIMDATA/CARFAMS/C84.CFM" "$k/M" >/dev/null 2>&1 || fail "unpack of the kit's 911"
cp "$k/M/c0/mesh.blender.obj" "$k/M/c0/x.obj"
"$studio" car import "$k/P" --raw --obj "$k/M/c0/x.obj" --tables "$k/M/c0" --low "$k/M/c2/mesh.blender.obj" --low-tables "$k/M/c2" \
    --keep-pdn --force --data "$orig" >"$work/log" 2>&1 || fail "car import of the kit's 911: $(cat "$work/log")"
clean "$work/log" || fail "car import of the kit's 911: a sanitizer report"
cmp -s "$k/before.cfm" "$k/P/SIMDATA/CARFAMS/C84.CFM" || fail "the kit's 911 through Blender unchanged: the CFM is not the same bytes"

[ "$failures" -eq 0 ] || exit 1
echo "car_import: ok"
