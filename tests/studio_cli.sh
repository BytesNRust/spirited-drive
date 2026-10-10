#!/bin/sh
# tnfs-studio command line tests against the CD data: unpack then pack gives the original back, damaged
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
        head -c "$bytes" "$f" > "$cut"
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

[ "$failures" -eq 0 ] && echo "studio_cli: all checks passed"
exit "$failures"
