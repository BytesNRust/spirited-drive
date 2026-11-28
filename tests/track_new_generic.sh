#!/bin/sh
# track new --generic: a piece from our own template and an art file (FAM), no file of the CD needed. A FAM without the generic road's
# banks, a missing FAM, a missing --piece and an existing output are refused.
# The art file is built here from a generated picture (python3 writes the PNG), the generic road's 11 banks with the fence's letter C last.
# usage: track_new_generic.sh STUDIO
set -u
studio=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

python3 - "$work/p.png" <<'PY' || { echo "skip: no python3"; exit 77; }
import sys, zlib, struct
w = h = 32
raw = b"".join(b"\x00" + b"".join(bytes((x * 8 % 256, y * 8 % 256, 90)) for x in range(w)) for y in range(h))
def chunk(t, d):
    c = struct.pack(">I", len(d)) + t + d
    return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[1], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
PY
{ i=0; while [ $i -lt 10 ]; do echo "terrain $i letters=A p.png"; i=$((i + 1)); done; echo "terrain 10 letters=C p.png"; echo "horizon p.png"; } > "$work/good.txt"
{ echo "terrain 0 letters=A p.png"; echo "terrain 1 letters=A p.png"; echo "horizon p.png"; } > "$work/few.txt"
{ i=0; while [ $i -lt 11 ]; do echo "terrain $i letters=A p.png"; i=$((i + 1)); done; echo "horizon p.png"; } > "$work/noc.txt"
fam="$work/good.fam"
"$studio" fam build "$work/good.txt" --out "$fam" >"$work/log" 2>&1 || { fail "fam build: $(cat "$work/log")"; exit 1; }
printf 'section 10\nsection 20 turn=90 grade=2\nsection 10\n' > "$work/t.trk"
out="$work/piece"
"$studio" track new "$work/t.trk" --generic "$fam" --piece --id gen-test --stem GENTEST --out "$out" >"$work/log" 2>&1 || fail "generic piece: $(cat "$work/log")"
[ -f "$out/SIMDATA/MISC/GENTEST1.TRI" ] || fail "no TRI in the piece"
[ -f "$out/SIMDATA/MISC/GENTEST1INFO" ] || fail "no INFO in the piece"
[ -f "$out/SIMDATA/ETRACKFM/GENTEST1_001.FAM" ] && [ -f "$out/SIMDATA/GTRACKFM/GENTEST1_001.FAM" ] || fail "the FAM is not in both folders"
cmp -s "$fam" "$out/SIMDATA/ETRACKFM/GENTEST1_001.FAM" || fail "the FAM was changed"
"$studio" track new "$work/t.trk" --generic "$fam" --piece --id gen-test --stem GENTEST --out "$out" >/dev/null 2>&1 && fail "a second run overwrote the output"
"$studio" track new "$work/t.trk" --generic "$fam" --piece --id gen-test --stem GENTEST --out "$out" --force >/dev/null 2>&1 || fail "--force was refused"
"$studio" track new "$work/t.trk" --generic "$fam" --id gen-test --stem GENTEST --out "$work/np" >/dev/null 2>&1 && fail "--generic without --piece was accepted"
"$studio" track new "$work/t.trk" --generic "$work/nothere.fam" --piece --id gen-test --stem GENTEST --out "$work/nf" >/dev/null 2>&1 && fail "a missing FAM was accepted"
for bad in few noc; do
    "$studio" fam build "$work/$bad.txt" --out "$work/$bad.fam" >"$work/log" 2>&1 || { fail "fam build $bad: $(cat "$work/log")"; continue; }
    "$studio" track new "$work/t.trk" --generic "$work/$bad.fam" --piece --id gen-test --stem GENTEST --out "$work/o_$bad" >/dev/null 2>"$work/log" && fail "the $bad FAM was accepted"
    [ -e "$work/o_$bad" ] && fail "the $bad FAM left an output"
done
head -c 5000 "$fam" > "$work/cut.fam"
"$studio" track new "$work/t.trk" --generic "$work/cut.fam" --piece --id gen-test --stem GENTEST --out "$work/cut" >/dev/null 2>&1 && fail "a cut FAM was accepted"
[ "$failures" -eq 0 ] && echo "track_new_generic: ok"
exit "$failures"
