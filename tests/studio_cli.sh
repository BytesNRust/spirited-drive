#!/bin/sh
# tnfs-studio command line tests against the CD data: unpack then pack gives the original back, a mesh's MTL names the PNGs
# the unpack wrote, damaged
# files end with exit 1 and a message naming the file (never a crash), nothing is overwritten without
# --force, and an edit made in a table reaches the packed file.
# usage: studio_cli.sh STUDIO_BINARY ORIGINAL_DIR
set -u
studio=$1
orig=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

find_one() { find -L "$orig" -iname "$1" 2>/dev/null | head -1; }

# 1. a file of every kind goes out and comes back identical
for name in DVIPER.CFM TR1.TRI VIPERSW.BNK DVIPER.PBS DVIPER.PDN TR1_001.FAM CARPAL.FSH; do
    f=$(find_one "$name")
    [ -n "$f" ] || { echo "skip $name (not on this CD)"; continue; }
    d="$work/$name.d"
    "$studio" unpack "$f" "$d" >/dev/null 2>"$work/err" || { fail "unpack $name: $(cat "$work/err")"; continue; }
    # dep69: every texture an MTL names is a PNG the unpack wrote, so a 3D editor shows the model textured
    for mtl in $(find "$d" -name '*.mtl'); do
        for tex in $(sed -n 's/^map_Kd //p' "$mtl"); do
            [ -f "$(dirname "$mtl")/$tex" ] || fail "$name: ${mtl#$d/} names $tex, which the unpack did not write"
        done
    done
    "$studio" pack "$d" "$work/$name.out" --from "$f" >/dev/null 2>"$work/err" || { fail "pack $name: $(cat "$work/err")"; continue; }
    cmp -s "$f" "$work/$name.out" || fail "$name does not come back identical"
done
q=$(find_one '*.QFS')
if [ -n "$q" ]; then
    "$studio" unpack "$q" "$work/q.d" >/dev/null 2>&1 && "$studio" pack "$work/q.d" "$work/q.out" --from "$q" >/dev/null 2>&1
    cmp -s "$q" "$work/q.out" || fail "a QFS file does not come back identical"
fi

# 2. damaged inputs: exit 1, the file named, no crash
for name in TR1.TRI DVIPER.CFM VIPERSW.BNK DVIPER.PBS; do
    f=$(find_one "$name")
    [ -n "$f" ] || continue
    size=$(wc -c < "$f")
    for bytes in 0 7 100 3000 20000; do
        [ "$bytes" -lt "$size" ] || continue
        cut="$work/cut_$bytes/$name"
        mkdir -p "$work/cut_$bytes"
        if [ "$bytes" -eq 0 ]; then : > "$cut"; else head -c "$bytes" "$f" > "$cut"; fi   # BSD head refuses -c 0
        "$studio" unpack "$cut" "$work/cut_$bytes.d" >"$work/out" 2>"$work/err"
        status=$?
        if [ "$status" -eq 0 ]; then fail "$name cut to $bytes bytes was accepted"; fi
        if [ "$status" -ge 128 ]; then fail "$name cut to $bytes bytes crashed (status $status)"; fi
        grep -q "$name" "$work/err" || fail "$name cut to $bytes bytes: message does not name the file: $(cat "$work/err")"
        rm -rf "$work/cut_$bytes.d"
    done
done
q=$(find_one '*.QFS')
if [ -n "$q" ]; then
    head -c 1000 "$q" > "$work/cut.QFS"
    "$studio" unpack "$work/cut.QFS" "$work/cutq.d" >/dev/null 2>"$work/err"
    [ $? -eq 1 ] || fail "a cut QFS was not refused with status 1"
    grep -q "offset" "$work/err" || fail "a cut QFS message gives no offset: $(cat "$work/err")"
fi
# flipped bytes: whatever happens, no crash
f=$(find_one DVIPER.CFM)
if [ -n "$f" ]; then
    size=$(wc -c < "$f")
    for pos in 5 9 17 40 200 5000 20000 70000; do
        [ "$pos" -lt "$size" ] || continue
        cat "$f" > "$work/flip.CFM"
        printf '\377\377\377\377' | dd of="$work/flip.CFM" bs=1 seek="$pos" conv=notrunc 2>/dev/null
        "$studio" unpack "$work/flip.CFM" "$work/flip.d" >/dev/null 2>&1
        status=$?
        [ "$status" -lt 128 ] || fail "DVIPER.CFM with bytes flipped at $pos crashed (status $status)"
        rm -rf "$work/flip.d"
    done
fi

# 3. nothing is overwritten without --force
f=$(find_one TR1.TRI)
if [ -n "$f" ]; then
    "$studio" unpack "$f" "$work/o.d" >/dev/null 2>&1
    "$studio" unpack "$f" "$work/o.d" >/dev/null 2>"$work/err" && fail "unpack over an existing tree was accepted"
    "$studio" unpack "$f" "$work/o.d" --force >/dev/null 2>&1 || fail "unpack --force was refused"
    "$studio" pack "$work/o.d" "$work/o.out" --from "$f" >/dev/null 2>&1 || fail "first pack failed"
    "$studio" pack "$work/o.d" "$work/o.out" --from "$f" >/dev/null 2>"$work/err" && fail "pack over an existing file was accepted"
    "$studio" pack "$work/o.d" "$work/o.out" --from "$f" --force >/dev/null 2>&1 || fail "pack --force was refused"
    "$studio" pack "$work/o.d" "$work/o2.out" >/dev/null 2>"$work/err" && fail "pack without --from was accepted"
    "$studio" frobnicate >/dev/null 2>&1 && fail "an unknown command was accepted"
fi

# 4. an edit in specs.tsv reaches the packed PBS
f=$(find_one DVIPER.PBS)
if [ -n "$f" ]; then
    "$studio" unpack "$f" "$work/p.d" >/dev/null 2>&1
    sed 's/^\(0x03c\tpower\ti32\t\).*/\1424242/' "$work/p.d/specs.tsv" > "$work/specs.new" && cp "$work/specs.new" "$work/p.d/specs.tsv"
    "$studio" pack "$work/p.d" "$work/P.PBS" --from "$f" >/dev/null 2>&1 || fail "packing the edited PBS failed"
    "$studio" unpack "$work/P.PBS" "$work/p2.d" >/dev/null 2>&1
    grep -q "$(printf 'power\ti32\t424242')" "$work/p2.d/specs.tsv" || fail "the edited power did not reach the PBS"
    cmp -s "$f" "$work/P.PBS" && fail "the edited PBS is identical to the original"
    # a value that does not fit is refused with the line
    sed 's/^\(0x03c\tpower\ti32\t\).*/\1banana/' "$work/p.d/specs.tsv" > "$work/specs.bad" && cp "$work/specs.bad" "$work/p.d/specs.tsv"
    "$studio" pack "$work/p.d" "$work/p.bad" --from "$f" >/dev/null 2>"$work/err" && fail "a text value for an i32 was accepted"
    grep -q "specs.tsv:" "$work/err" || fail "the refusal does not give the line: $(cat "$work/err")"
fi

# 5. validate: the CD passes without findings; a file over a limit is refused in compatible mode only
"$studio" validate "$orig" >"$work/v.out" 2>&1 || fail "validate refused the original CD: $(head -3 "$work/v.out")"
grep -q "no findings" "$work/v.out" || fail "validate found something on the original CD: $(head -3 "$work/v.out")"
f=$(find_one TR1.TRI)
if [ -n "$f" ]; then
    cat "$f" > "$work/BIG.TRI"
    printf '\130\002' | dd of="$work/BIG.TRI" bs=1 seek=6 conv=notrunc 2>/dev/null     # 0x0258 = 600 segments
    "$studio" validate "$work/BIG.TRI" >"$work/v.out" 2>"$work/err"
    [ $? -eq 1 ] || fail "validate accepted a TRI with 600 segments"
    grep -q "TRI.SEGMENTS" "$work/v.out" || fail "the limit is not named: $(cat "$work/v.out")"
    grep -q "value 600, maximum 599" "$work/v.out" || fail "value and maximum are not shown: $(cat "$work/v.out")"
    grep -q "0043e4e0" "$work/v.out" || fail "the EXE address is not shown"
    grep -q "BIG.TRI" "$work/v.out" || fail "the file is not named"
    "$studio" validate --port-only "$work/BIG.TRI" >"$work/v.out" 2>&1 || fail "port-only validate did not exit 0"
    grep -q "^warning TRI.SEGMENTS" "$work/v.out" || fail "port-only validate did not report a warning: $(cat "$work/v.out")"
    "$studio" validate "$work/nonexistent.TRI" >/dev/null 2>&1 && fail "validate of a missing file exited 0"
fi
# the CARSPECS directory of an installation must not be writable
mkdir -p "$work/inst/SIMDATA/CARSPECS" "$work/inst/SIMDATA/MISC"
p=$(find_one DVIPER.PBS)
[ -n "$p" ] && cat "$p" > "$work/inst/SIMDATA/CARSPECS/DVIPER.PBS"
"$studio" validate --check-install "$work/inst" >"$work/v.out" 2>&1 && fail "a writable CARSPECS was accepted: $(cat "$work/v.out")"
grep -q "MOD.CARSPECS_WRITABLE" "$work/v.out" || fail "the writable CARSPECS is not reported"
"$studio" validate "$work/inst" >/dev/null 2>&1 || fail "without --check-install a writable CARSPECS must not matter"
chmod a-w "$work/inst/SIMDATA/CARSPECS"
"$studio" validate --check-install "$work/inst" >"$work/v.out" 2>&1 || fail "a read-only CARSPECS was refused: $(cat "$work/v.out")"
chmod u+w "$work/inst/SIMDATA/CARSPECS"

# 6. slots: replace a track in a mod directory, undo it, and the directory is as it was (AC7)
tri3=$(find_one TR3.TRI)
if [ -n "$tri3" ]; then
    treesum() { (cd "$1" && find . -type f | sort | while read -r f; do cksum "$f"; done); }
    "$studio" slot list track | grep -qx TR3 || fail "slot list does not show TR3"
    "$studio" slot list car | grep -qx DVIPER || fail "slot list does not show DVIPER"
    mkdir -p "$work/slot/src/SIMDATA/MISC" "$work/slot/mod/SIMDATA/MISC"
    "$studio" unpack "$tri3" "$work/slot/t" >/dev/null 2>&1
    awk -F'\t' 'BEGIN{OFS="\t"} $1=="30" { $8 = $8 + 5000 } { print }' "$work/slot/t/nodes.tsv" > "$work/slot/nodes" && cp "$work/slot/nodes" "$work/slot/t/nodes.tsv"
    "$studio" pack "$work/slot/t" "$work/slot/src/SIMDATA/MISC/MYTRACK.TRI" --from "$tri3" >/dev/null 2>&1 || fail "pack the new track"
    echo keep > "$work/slot/mod/SIMDATA/MISC/KEEP.TXT"
    cat "$tri3" > "$work/slot/mod/SIMDATA/MISC/TR3.TRI"          # a file the mod already had in that slot
    treesum "$work/slot/mod" > "$work/slot/before.sum"
    "$studio" slot replace track tr3 "$work/slot/src" --mods "$work/slot/mod" --data "$orig" >"$work/out" 2>"$work/err" && fail "a file named MYTRACK.TRI was accepted without --stem"
    grep -q "not a file of track slot TR3" "$work/err" || fail "the refusal does not say why: $(cat "$work/err")"
    "$studio" slot replace track tr3 "$work/slot/src" --mods "$work/slot/mod" --data "$orig" --stem MYTRACK >"$work/out" 2>"$work/err" || fail "slot replace failed: $(cat "$work/err")"
    cmp -s "$work/slot/src/SIMDATA/MISC/MYTRACK.TRI" "$work/slot/mod/SIMDATA/MISC/TR3.TRI" || fail "the mod's TR3.TRI is not the new track"
    [ -d "$work/slot/mod/.tnfs-studio" ] || fail "no journal was kept"
    "$studio" validate "$work/slot/mod" >/dev/null 2>&1 || fail "the replaced slot does not validate"
    "$studio" slot undo --mods "$work/slot/mod" >/dev/null 2>"$work/err" || fail "slot undo failed: $(cat "$work/err")"
    treesum "$work/slot/mod" > "$work/slot/after.sum"
    cmp -s "$work/slot/before.sum" "$work/slot/after.sum" || fail "undo did not restore the mod directory"
    [ -e "$work/slot/mod/.tnfs-studio" ] && fail "undo left its state directory behind"
    "$studio" slot undo --mods "$work/slot/mod" >/dev/null 2>&1 && fail "undo with nothing to undo exited 0"

    # a slot that is empty in the mod: the file is added, and undo removes it with the directories it made
    "$studio" slot replace track tr3 "$work/slot/src" --mods "$work/slot/fresh" --data "$orig" --stem MYTRACK >/dev/null 2>&1 || fail "replace into a new mod directory"
    [ -f "$work/slot/fresh/SIMDATA/MISC/TR3.TRI" ] || fail "the new mod directory has no TR3.TRI"
    "$studio" slot undo --mods "$work/slot/fresh" >/dev/null 2>&1 || fail "undo in the new mod directory"
    [ -z "$(find "$work/slot/fresh" -type f)" ] || fail "undo left files in the new mod directory"
    # a file edited after the replace is not silently overwritten
    "$studio" slot replace track tr3 "$work/slot/src" --mods "$work/slot/mod" --data "$orig" --stem MYTRACK >/dev/null 2>&1
    echo changed >> "$work/slot/mod/SIMDATA/MISC/TR3.TRI"
    "$studio" slot undo --mods "$work/slot/mod" >/dev/null 2>"$work/err" && fail "undo overwrote a file changed since the replace"
    grep -q "changed since" "$work/err" || fail "undo does not say the file changed"
    "$studio" slot undo --mods "$work/slot/mod" --force >/dev/null 2>&1 || fail "undo --force was refused"
    # refusals: unknown slot, a file the game does not have, an invalid file; nothing is written
    "$studio" slot replace track TR8 "$work/slot/src" --mods "$work/slot/none" --data "$orig" >/dev/null 2>"$work/err" && fail "an unknown slot was accepted"
    grep -q "slots are fixed" "$work/err" || fail "an unknown slot is not explained"
    mkdir -p "$work/slot/extra/SIMDATA/MISC"
    cat "$tri3" > "$work/slot/extra/SIMDATA/MISC/TR3.TRI"
    echo x > "$work/slot/extra/SIMDATA/MISC/TR3INFO"
    cat "$tri3" > "$work/slot/extra/SIMDATA/MISC/ZZZ.TXT"
    "$studio" slot replace track TR3 "$work/slot/extra" --mods "$work/slot/none" --data "$orig" --also SIMDATA/MISC/ZZZ.TXT >/dev/null 2>"$work/err" && fail "a file the data root lacks was accepted"
    grep -q "no such file" "$work/err" || fail "a file the data root lacks is not explained: $(cat "$work/err")"
    cat "$tri3" > "$work/slot/extra/SIMDATA/MISC/TR3.TRI"
    printf '\130\002' | dd of="$work/slot/extra/SIMDATA/MISC/TR3.TRI" bs=1 seek=6 conv=notrunc 2>/dev/null
    rm -f "$work/slot/extra/SIMDATA/MISC/ZZZ.TXT"
    "$studio" slot replace track TR3 "$work/slot/extra" --mods "$work/slot/none" --data "$orig" >/dev/null 2>"$work/err" && fail "an invalid track was accepted into a slot"
    grep -q "TRI.SEGMENTS" "$work/err" || fail "the invalid track is not explained: $(cat "$work/err")"
    [ -e "$work/slot/none" ] && fail "a refused replace still created the mod directory"
fi
# a car slot mentions what Level 1 cannot rename
pbs=$(find_one DVIPER.PBS)
if [ -n "$pbs" ]; then
    mkdir -p "$work/car/src/SIMDATA/CARSPECS"
    cat "$pbs" > "$work/car/src/SIMDATA/CARSPECS/DVIPER.PBS"
    "$studio" slot replace car dviper "$work/car/src" --mods "$work/car/mod" --data "$orig" >"$work/out" 2>&1 || fail "car slot replace failed: $(cat "$work/out")"
    grep -q "text inside the EXE" "$work/out" || fail "the car name limit is not mentioned"
fi

# 8. car import (dep59 tools B2): an OBJ on a piece of car new. No --obj is the usage; a folder with no mod.ini and an OBJ with no
# wheels are refused and explained; the test car (tests/car_import, tests/test_car_import.c's) gives a piece validate passes, the
# PDN's first three words the body's measured half sizes, and no high-detail masks left
fixture=$(dirname "$0")/car_import
port=$(dirname "$0")/../data
"$studio" --help | grep -q "tnfs-studio car import PIECE_DIR" || fail "--help does not list car import"
# 7b. --help (or -h) after any subcommand: rc 0, its usage, and every flag its parser takes
help_flags() {
    form=$1
    shift
    for flag in "$@"; do
        "$studio" $form --help >"$work/help.out" 2>&1 || fail "$form --help exited $?"
        grep -q -- "$flag" "$work/help.out" || fail "$form --help does not list $flag"
    done
}
help_flags "track new" --template --generic --out --stem --data --piece --id --name --road --edge --author --description --force
help_flags "car new" --template --id --stem --out --name --author --description --data --port-data --force
help_flags "car card" --generic --tnfs --data --track --node --time --force
help_flags "car import" --obj --low --textures --low-textures --raw --tables --low-tables --scale --steer --donor --keep-pdn --data --force
help_flags "car resize" --image --low
help_flags "car texture add" --low --label
help_flags "slot replace" --mods --data --stem --also
help_flags "slot undo" --mods --all --force
help_flags "slot list" track
help_flags "validate" --compat --port-only --check-install
help_flags "unpack" --palette --force
help_flags "pack" --from --palette --force
help_flags "mod pack" --force
help_flags "fam" --out --force
help_flags "fam build" --out --force
"$studio" track new --help | grep -q -- "--generic ART.FAM.*needs --piece" || fail "track new --help does not say that --generic needs --piece"
"$studio" car texture --help >/dev/null 2>&1 || fail "car texture --help exited non-zero"
"$studio" track --help | grep -q -- "--piece" || fail "track --help does not list --piece"
"$studio" car -h | grep -q "car new" || fail "car -h does not list the car forms"
"$studio" frobnicate --help >/dev/null 2>&1 && fail "an unknown command's --help was accepted"
grep -q "Geometry cannot be replaced yet" "$(dirname "$0")/../docs/modding.md" && fail "docs/modding.md still says geometry cannot be replaced"
grep -q "^#### Importing a model" "$(dirname "$0")/../docs/modding.md" || fail "docs/modding.md has no Importing a model section"
"$studio" car import "$work/ci" >/dev/null 2>"$work/err" && fail "car import with no --obj exited 0"
grep -q "usage: tnfs-studio car import" "$work/err" || fail "car import with no --obj does not print its usage: $(cat "$work/err")"
mkdir -p "$work/ci/empty"
"$studio" car import "$work/ci/empty" --obj "$fixture/car.obj" --data "$orig" >/dev/null 2>"$work/err" && fail "car import into a folder with no mod.ini exited 0"
grep -q "mod.ini" "$work/err" || fail "a folder with no mod.ini is not explained: $(cat "$work/err")"
if [ -n "$(find_one DVIPER.CFM)" ]; then
    "$studio" car new --template viper --id import-test --stem IMPTEST --out "$work/ci/p" --data "$orig" --port-data "$port" >/dev/null 2>"$work/err" ||
        fail "car new for car import: $(cat "$work/err")"
    sed '/^o wheel_fl/,$d' "$fixture/car.obj" > "$work/ci/nowheels.obj"
    cp "$fixture/car.mtl" "$fixture/paint.png" "$fixture/seat.png" "$work/ci/"
    cp "$work/ci/p/SIMDATA/CARFAMS/IMPTEST.CFM" "$work/ci/before.CFM"
    "$studio" car import "$work/ci/p" --obj "$work/ci/nowheels.obj" --data "$orig" >/dev/null 2>"$work/err" && fail "an OBJ with no wheels was imported"
    grep -q "wheel_fl" "$work/err" || fail "an OBJ with no wheels is not explained: $(cat "$work/err")"
    cmp -s "$work/ci/before.CFM" "$work/ci/p/SIMDATA/CARFAMS/IMPTEST.CFM" || fail "a refused import changed the CFM"
    "$studio" car import "$work/ci/p" --obj "$fixture/car.obj" --data "$orig" >"$work/out" 2>&1 || fail "car import of the test car: $(cat "$work/out")"
    grep -q "try it: tnfs --mods $work/ci/p --car import-test" "$work/out" || fail "car import does not say how to try it: $(cat "$work/out")"
    "$studio" validate "$work/ci/p" >"$work/v.out" 2>&1 || fail "validate refused the imported piece: $(cat "$work/v.out")"
    grep -q "no findings" "$work/v.out" || fail "validate found something in the imported piece: $(cat "$work/v.out")"
    "$studio" unpack "$work/ci/p/SIMDATA/CARFAMS/IMPTEST.PDN" "$work/ci/pdn" >/dev/null 2>&1 || fail "the imported piece's PDN does not unpack"
    grep -q "^0x000	unknown_000	i32	61440	33280	133120$" "$work/ci/pdn/pdn.tsv" ||
        fail "the PDN does not hold the body's half sizes: $(grep '^0x000' "$work/ci/pdn/pdn.tsv")"
    ls "$work/ci/p/car_masks/IMPTEST" 2>/dev/null | grep -q . && fail "the template's car masks are still in the piece"
    # a second import over an imported CFM needs --force, and gives the same file
    cp "$work/ci/p/SIMDATA/CARFAMS/IMPTEST.CFM" "$work/ci/first.CFM"
    "$studio" car import "$work/ci/p" --obj "$fixture/car.obj" --data "$orig" >/dev/null 2>"$work/err" && fail "car import overwrote an imported CFM without --force"
    grep -q -- "--force" "$work/err" || fail "the refusal does not mention --force: $(cat "$work/err")"
    "$studio" car import "$work/ci/p" --obj "$fixture/car.obj" --data "$orig" --force >/dev/null 2>"$work/err" || fail "car import --force: $(cat "$work/err")"
    cmp -s "$work/ci/first.CFM" "$work/ci/p/SIMDATA/CARFAMS/IMPTEST.CFM" || fail "importing twice does not give the same CFM"
    # raw: unpack's high mesh of the template back in; its banks and, without --low, its far model are the donor's, so its masks stay
    "$studio" car new --template viper --id import-raw --stem IMPRAW --out "$work/ci/r" --data "$orig" --port-data "$port" >/dev/null 2>&1
    "$studio" unpack "$(find_one DVIPER.CFM)" "$work/ci/u" >/dev/null 2>&1
    "$studio" car import "$work/ci/r" --obj "$work/ci/u/c0/mesh.obj" --raw --data "$orig" >"$work/out" 2>&1 || fail "car import --raw: $(cat "$work/out")"
    "$studio" validate "$work/ci/r" 2>&1 | grep -q "no findings" || fail "validate found something in the raw import"
    ls "$work/ci/r/car_masks/IMPRAW" | grep -q "_lo_mask.png" || fail "raw without --low removed the low masks of the donor's far model"
    ls "$work/ci/r/car_masks/IMPRAW" | grep -v "_lo_" | grep -q "_mask.png" || fail "raw removed the high masks of the donor's bank"
fi

[ "$failures" -eq 0 ] && echo "studio_cli: all checks passed"
exit "$failures"
