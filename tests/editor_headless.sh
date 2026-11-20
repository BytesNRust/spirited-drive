#!/bin/sh
# The track editor runs headless (Dear ImGui over the SDL software renderer): a project folder is made, N frames are drawn and the last one
# is written as a PNG that has the window's size, the road's line and the control points drawn in the viewport panel; --save writes the
# project files; --user makes its folder and keeps the layout in it; an unknown option fails. (Without a PROJECT it shows the start screen and makes nothing; a normal run reopens the last project from the recent list, a headless one does not read it.)
# usage: editor_headless.sh EDITOR
set -u
editor=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

"$editor" "$work/proj" --frames 6 --save --shot "$work/shot.png" >"$work/log" 2>&1 || fail "run: $(cat "$work/log")"
[ -f "$work/proj/project.ini" ] && [ -f "$work/proj/trace.json" ] || fail "no project files"
[ -f "$work/shot.png" ] || { fail "no PNG"; exit 1; }
cat > "$work/png_check.py" <<'PY'
import sys, struct, zlib
d = open(sys.argv[1], "rb").read()
mode = sys.argv[2]
assert d[:8] == b"\x89PNG\r\n\x1a\n"
w, h, depth, ctype = struct.unpack(">IIBB", d[16:26])
assert (w, h) == (960, 600), (w, h)
assert ctype in (2, 6) and depth == 8
bpp = 3 if ctype == 2 else 4
i, idat = 8, b""
while i < len(d):
    n, t = struct.unpack(">I4s", d[i:i + 8])
    if t == b"IDAT": idat += d[i + 8:i + 8 + n]
    i += 12 + n
raw = zlib.decompress(idat)
stride = w * bpp
rows, prev = [], bytearray(stride)
for y in range(h):
    f = raw[y * (stride + 1)]
    line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
    for x in range(stride):
        a = line[x - bpp] if x >= bpp else 0
        b = prev[x]
        c = prev[x - bpp] if x >= bpp else 0
        if f == 1: line[x] = (line[x] + a) & 255
        elif f == 2: line[x] = (line[x] + b) & 255
        elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
        elif f == 4:
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
    rows.append(line)
    prev = line
# the road is light grey and the control points blue (viewport.cpp): near those colours, so many pixels, and nothing else is
def count(rgb, tol=12):
    n = 0
    for row in rows:
        n += sum(1 for r, g, b in zip(row[0::bpp], row[1::bpp], row[2::bpp])
                 if abs(r - rgb[0]) <= tol and abs(g - rgb[1]) <= tol and abs(b - rgb[2]) <= tol)
    return n
road, points, red, amber = count((200, 204, 212)), count((90, 160, 255)), count((255, 70, 70)), count((255, 176, 32))
if mode == "good":   # the road was made and no limit breaks: no red, no amber
    assert road > 400 and points > 100 and red == 0 and amber == 0, (road, points, red, amber)
elif mode == "warn":   # a curve over an amber limit and under the red ones: amber nodes, no red
    assert road > 400 and points > 100 and amber > 20 and red == 0, (road, points, red, amber)
else:                # a corner too tight: the road is made (grey), and its nodes at the corner are red
    assert road > 400 and points > 100 and red > 50, (road, points, red)
PY
png_check() { python3 "$work/png_check.py" "$1" "$2" || fail "$1 is not a 960x600 picture with the $2 project's road and points in it"; }
png_check "$work/shot.png" good
mkdir "$work/mild"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 40.0], [15.0, 80.0], [15.0, 120.0], [0.0, 160.0], [0.0, 200.0]]\n}\n' > "$work/mild/trace.json"   # an S of 6.5 degrees a node at most: nothing is marked (the old limit was 6)
"$editor" "$work/mild" --frames 6 --shot "$work/mild.png" >"$work/log" 2>&1 || fail "run on a mild S: $(cat "$work/log")"
if [ -f "$work/mild.png" ]; then png_check "$work/mild.png" good; else fail "no PNG of the mild S"; fi
mkdir "$work/warn"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, -40.0], [0.0, 0.0], [20.0, 40.0], [20.0, 80.0], [0.0, 120.0], [0.0, 160.0]]\n}\n' > "$work/warn/trace.json"   # an S that turns about 9 degrees a node: amber
"$editor" "$work/warn" --frames 6 --shot "$work/warn.png" >"$work/log" 2>&1 || fail "run on a curve over an amber limit: $(cat "$work/log")"
if [ -f "$work/warn.png" ]; then png_check "$work/warn.png" warn; else fail "no PNG of the curve over an amber limit"; fi
"$editor" "$work/warn" --select 3 --frames 6 --shot "$work/select.png" >"$work/log" 2>&1 || fail "run with a point selected (the Node window): $(cat "$work/log")"
[ -s "$work/select.png" ] || fail "no PNG with a point selected"
"$editor" "$work/warn" --edit --ask quit --frames 6 --shot "$work/confirm.png" >"$work/log" 2>&1 || fail "run with unsaved changes and a quit asked for: $(cat "$work/log")"
[ -s "$work/confirm.png" ] || fail "no PNG with the unsaved-changes question"
"$editor" "$work/warn" --ask quit --frames 6 >"$work/log" 2>&1 || fail "run with a quit asked for and nothing unsaved: $(cat "$work/log")"
mkdir "$work/bad"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 80.0], [20.0, 90.0], [0.0, 100.0], [0.0, 180.0]]\n}\n' > "$work/bad/trace.json"
"$editor" "$work/bad" --frames 6 --shot "$work/bad.png" >"$work/log" 2>&1 || fail "run on a project that breaks limits: $(cat "$work/log")"
if [ -f "$work/bad.png" ]; then png_check "$work/bad.png" bad; else fail "no PNG of the project that breaks limits"; fi
# Build: a curve under the limits is built, one over them is refused with the limit named
"$editor" "$work/mild" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a curve under the limits: $(cat "$work/log")"
[ -s "$work/mild/build/mild.zip" ] || fail "no zip of the mild S"
"$editor" "$work/warn" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a curve that is only over an amber limit: $(cat "$work/log")"
[ -s "$work/warn/build/warn.zip" ] || fail "no zip of the amber curve"
"$editor" "$work/bad" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a curve over a red limit"
grep -q "tighter than 20.9 m\|more than 16.5 degrees" "$work/log" || fail "the refusal does not name the red limit: $(cat "$work/log")"
# a new project is born a stage the game races: a straight of the shortest stage's length, built with nothing too short
mkdir "$work/fresh"
"$editor" "$work/fresh" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a new project: $(cat "$work/log")"
[ -s "$work/fresh/build/fresh.zip" ] || fail "no zip of a new project"
grep -q "too short" "$work/log" && fail "a new project is too short for a stage: $(cat "$work/log")"
python3 -c "
import json, sys
t = json.load(open(sys.argv[1]))
p = t['points']
assert len(p) == 15 and p[0] == [0.0, 0.0] and p[-1] == [0.0, 1308.0], p
" "$work/fresh/trace.json" || fail "a new project is not the 1308 m straight of 15 points"
# road keys (trace.json version 2): a ramp from the project's 10 m to 8 m and 14 m builds a TRI whose verge bytes are the widths the keys say; a ramp too sudden is
# amber (built, drawn amber), and a file of a newer version is refused with its reason
mkdir "$work/keyed"
printf '{\n  "version": 2,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 1, "width_m": 8.0}, {"point": 4, "width_m": 14.0}]\n}\n' > "$work/keyed/trace.json"
"$editor" "$work/keyed" --frames 6 --select 2 --shot "$work/keyed.png" >"$work/log" 2>&1 || fail "run on a project with road keys: $(cat "$work/log")"
png_check "$work/keyed.png" good
"$editor" "$work/keyed" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a project with road keys: $(cat "$work/log")"
cat > "$work/tri_widths.py" <<'PY'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
names = [n for n in z.namelist() if n.upper().endswith(".TRI")]
assert len(names) == 1, names
d = z.read(names[0])
first, last = d[0x98C], d[0x98C + 36 * 48]   # byte 0 of node 0, byte 0 of node 48 (a straight of 300 m: 12 segments of 4 nodes)
mid = d[0x98C + 36 * 24]
assert first == 40 and last == 56 and 40 < mid < 56, (first, mid, last)
step = max(abs(d[0x98C + 36 * (i + 1)] - d[0x98C + 36 * i]) for i in range(48))
assert 1 <= step <= 5, step
PY
python3 "$work/tri_widths.py" "$work/keyed/build/keyed.zip" || fail "the TRI of the keyed project does not have the widths of its keys"
mkdir "$work/sudden"
printf '{\n  "version": 2,\n  "width": 40.0,\n  "points": [[0.0,  0.0], [0.0,  15.0], [0.0,  30.0], [0.0,  45.0], [0.0,  60.0], [0.0,  75.0], [0.0,  90.0], [0.0,  105.0], [0.0,  120.0], [0.0,  135.0], [0.0,  150.0], [0.0,  165.0]],\n  "keys": [{"point": 2, "width_m": 10.0}, {"point": 3, "width_m": 14.0}]\n}\n' > "$work/sudden/trace.json"
"$editor" "$work/sudden" --frames 6 --shot "$work/sudden.png" >"$work/log" 2>&1 || fail "run on a ramp that is too sudden: $(cat "$work/log")"
if [ -f "$work/sudden.png" ]; then png_check "$work/sudden.png" warn; else fail "no PNG of the sudden ramp"; fi
"$editor" "$work/sudden" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a ramp that is only over an amber limit: $(cat "$work/log")"
mkdir "$work/newer"
printf '{\n  "version": 4,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 100.0], [0.0, 200.0]]\n}\n' > "$work/newer/trace.json"
"$editor" "$work/newer" --frames 2 --build >"$work/log" 2>&1 && fail "a trace.json of a newer version was read"
grep -q "version 4" "$work/log" || fail "the refusal does not name the version: $(cat "$work/log")"
# shoulder surfaces (trace.json version 3, step 2b): a tarmac shoulder on the left from point 1, then the right one from point 3 (the left back to the verge), builds a TRI
# whose nodes' flags[2] (node +6, high nibble left, low right) clear that side: 0x22 before, 0x02, then 0x20; those segments draw that shoulder in asphalt
mkdir "$work/surface"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 1, "shoulder_l_surface": "tarmac"}, {"point": 3, "shoulder_l_surface": "verge", "shoulder_r_surface": "tarmac"}]\n}\n' > "$work/surface/trace.json"
"$editor" "$work/surface" --frames 6 --select 3 --shot "$work/surface.png" >"$work/log" 2>&1 || fail "run on a project with shoulder surfaces: $(cat "$work/log")"
png_check "$work/surface.png" good
"$editor" "$work/surface" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a project with shoulder surfaces: $(cat "$work/log")"
cat > "$work/tri_surfaces.py" <<'PY'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
names = [n for n in z.namelist() if n.upper().endswith(".TRI")]
assert len(names) == 1, names
d = z.read(names[0])
got = [d[0x98C + 36 * k + 6] for k in range(49)]   # 300 m: 49 nodes, 6.25 m apart; points at 0, 60 .. 300 m
start = lambda k: (k - k % 4) * 6.25                # a change takes effect where a segment starts
want = [0x22 if start(k) < 60 else 0x02 if start(k) < 180 else 0x20 for k in range(49)]
assert got == want, [hex(b) for b in got]
terrain = 0x1621C + int.from_bytes(d[0x1621C + 4:0x1621C + 8], "little")   # after the SJBO chunk (core/formats/tri.c), each segment at its offset in the table at 0x2c
for seg in range(12):                               # the shoulder band: texture[1] right, texture[6] left; 12 is the asphalt, 3 the gravel
    at = terrain + int.from_bytes(d[0x2c + 4 * seg:0x2c + 4 * seg + 4], "little")
    assert d[at:at + 4] == b"TRKD", (seg, d[at:at + 4])
    tex = d[at + 0xe:at + 0x18]
    s0 = seg * 4 * 6.25
    assert tex[6] == (12 if 60 <= s0 < 180 else 3) and tex[1] == (12 if s0 >= 180 else 3), (seg, list(tex))
PY
python3 "$work/tri_surfaces.py" "$work/surface/build/surface.zip" || fail "the TRI of the surface project does not have the shoulders of its keys"
# the road's sides (step 3): the right wall 8 m past the road edge from point 0, a soft left wall from point 1 then a hard one from point 3, no right
# fence from point 2. The TRI's nodes have the right wall byte 64 eighths past the right edge (+3 minus +1), flags[1]'s high nibble (left) 1 then 0 where
# the template's was before, and the segments (4 nodes, 25 m) from 125 m on lose the right fence bit (0x40), every other bit the template's
mkdir "$work/sides"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 0, "barrier_r_m": 8.0}, {"point": 1, "wall_l": "soft"}, {"point": 2, "fence_r": false}, {"point": 3, "wall_l": "hard"}]\n}\n' > "$work/sides/trace.json"
"$editor" "$work/sides" --frames 6 --select 1 --shot "$work/sides.png" >"$work/log" 2>&1 || fail "run on a project with side keys: $(cat "$work/log")"
png_check "$work/sides.png" good
"$editor" "$work/sides" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a project with side keys: $(cat "$work/log")"
cat > "$work/tri_sides.py" <<'PY'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
names = [n for n in z.namelist() if n.upper().endswith(".TRI")]
assert len(names) == 1, names
d = z.read(names[0])
node = lambda k, b: d[0x98C + 36 * k + b]
walls = [node(k, 3) - node(k, 1) for k in range(49)]
assert walls == [64] * 49, walls
first = node(0, 5) >> 4   # the template's left wall kind, before point 1
kinds = [node(k, 5) >> 4 for k in range(49)]
want = [first if k * 6.25 < 60 else 1 if k * 6.25 < 180 else 0 for k in range(49)]
assert kinds == want, (kinds, want)
assert all(node(k, 5) & 0xF == node(0, 5) & 0xF for k in range(49)), "the right wall's kind moved"
at = d.find(b"TRKD")
fences = [d[at + 288 * s + 0xD] for s in range(12)]
assert all(d[at + 288 * s:at + 288 * s + 4] == b"TRKD" for s in range(12))
assert fences[0] & 0x40, "the template has no right fence: the check proves nothing"
assert [f & 0x40 for f in fences] == [0x40 if 25 * s < 120 else 0 for s in range(12)], [hex(f) for f in fences]
assert all(f & ~0x40 == fences[0] & ~0x40 for f in fences), [hex(f) for f in fences]
PY
python3 "$work/tri_sides.py" "$work/sides/build/sides.zip" || fail "the TRI of the sides project does not have the sides of its keys"
# --window WxH: the picture is that size (a layout at another window size); a size out of range is refused by name
"$editor" "$work/surface" --window 1440x900 --frames 6 --select 2 --shot "$work/wide.png" >"$work/log" 2>&1 || fail "run with --window: $(cat "$work/log")"
python3 -c "import struct,sys; d=open(sys.argv[1],'rb').read(24); sys.exit(0 if struct.unpack('>II', d[16:24]) == (1440, 900) else 1)" "$work/wide.png" || fail "--window 1440x900 did not make a 1440x900 picture"
"$editor" "$work/surface" --window 100x100 --frames 2 >"$work/log" 2>&1 && fail "--window 100x100 was taken"
grep -q "\-\-window wants WxH" "$work/log" || fail "the refusal of --window does not say what it wants: $(cat "$work/log")"
"$editor" "$work/proj" --frames 2 --user "$work/user/sub" >/dev/null 2>&1 || fail "run with --user"
[ -f "$work/user/sub/track-editor.ini" ] || fail "the layout was not kept in the player folder (made when missing)"
# the game is not where --game says: Install says so, and what to do, without running anything
out=$("$editor" "$work/proj" --frames 2 --install --game "$work/no-such-game" --user "$work/u2" 2>&1) && fail "install with a game that is not there went"
echo "$out" | grep -q "copy Spirited Drive and Spirited Drive Studio to Applications" || fail "no hint about the game's place: $out"
# and with no --game at all, from a folder with no ./tnfs (skipped where the game really is in /Applications)
if [ ! -x "/Applications/Spirited Drive.app/Contents/MacOS/tnfs" ] && [ ! -x "$(dirname "$editor")/../../../Spirited Drive.app/Contents/MacOS/tnfs" ]; then
    out=$(cd "$work" && "$editor" "$work/proj" --frames 2 --install 2>&1) && fail "install with no game found went"
    echo "$out" | grep -q "Applications" || fail "no hint with no game found: $out"
fi
# no PROJECT: the start screen (nothing is made anywhere); a car's kit opens as a car, by its root or its coche/, and writes nothing; a card that is not a car is refused
"$editor" --frames 3 --shot "$work/start.png" >"$work/log" 2>&1 || fail "run with nothing to open: $(cat "$work/log")"
[ -s "$work/start.png" ] || fail "no PNG of the start screen"
"$editor" --ask opencar --frames 3 >"$work/log" 2>&1 || fail "run with Open car asked for on the start screen: $(cat "$work/log")"
mkdir -p "$work/kit/coche/SIMDATA/CARFAMS" "$work/kit/coche/car_masks/K1" "$work/kit/modelo/c1"
printf 'id=k1\nname=Kit One\nversion=1\ntype=car\nstem=K1\nsound_from=911\n' > "$work/kit/coche/mod.ini"
printf 'x' > "$work/kit/coche/SIMDATA/CARFAMS/K1.CFM"
printf 'x' > "$work/kit/coche/car_masks/K1/rsid_mask.png"
printf 'x' > "$work/kit/modelo/c1/0010_rsid.png"
"$editor" "$work/kit" --frames 3 --shot "$work/kit.png" >"$work/log" 2>&1 || fail "run on a car's kit: $(cat "$work/log")"
"$editor" "$work/kit/coche" --frames 3 --shot "$work/coche.png" >"$work/log" 2>&1 || fail "run on a car's coche folder: $(cat "$work/log")"
[ -s "$work/kit.png" ] && [ -s "$work/coche.png" ] || fail "no PNG of the car"
cmp -s "$work/kit.png" "$work/coche.png" || fail "the kit and its coche/ are not the same car on screen"
cmp -s "$work/kit.png" "$work/start.png" && fail "the car looks like the start screen"
[ ! -e "$work/kit/project.ini" ] && [ ! -e "$work/kit/coche/project.ini" ] || fail "opening a car made track files in it"
"$editor" "$work/kit" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a car"
grep -q "no Build or Install" "$work/log" || fail "Build on a car does not say so: $(cat "$work/log")"
cp "$work/kit/coche/mod.ini" "$work/card.before"
"$editor" "$work/kit" --frames 2 --save >"$work/log" 2>&1 || fail "Save with nothing changed failed: $(cat "$work/log")"
cmp -s "$work/card.before" "$work/kit/coche/mod.ini" || fail "Save with nothing changed touched mod.ini"
# a change is written to mod.ini and nothing else; an invalid field refuses Save, shows on its field, and leaves the file as it was
"$editor" "$work/kit" --frames 2 --set year=1990 --save >"$work/log" 2>&1 || fail "Save of a change failed: $(cat "$work/log")"
printf 'year=1990\n' >> "$work/card.before"
cmp -s "$work/card.before" "$work/kit/coche/mod.ini" || fail "Save changed more than the one line"
cp "$work/kit/coche/mod.ini" "$work/card.before"
"$editor" "$work/kit" --frames 3 --set year=12345678901234567 --save --shot "$work/invalid.png" >"$work/log" 2>&1 && fail "Save went with a field that has a problem"
grep -q "Year: 17 characters; the card keeps up to 16" "$work/log" || fail "no word about the field: $(cat "$work/log")"
cmp -s "$work/card.before" "$work/kit/coche/mod.ini" || fail "a refused Save changed mod.ini"
[ -s "$work/invalid.png" ] || fail "no PNG with an invalid field"
"$editor" "$work/kit" --frames 3 --set year=1991 --ask quit --shot "$work/carconfirm.png" >"$work/log" 2>&1 || fail "run with a changed card and a quit asked for: $(cat "$work/log")"
"$editor" "$work/kit" --frames 2 --set nokey=1 >"$work/log" 2>&1 && fail "--set accepted a key that is not editable"
rm -f "$work/kit/project.ini"
# Rehacer: a kit with no rehacer.command says so; unsaved card changes refuse it; the script runs with the kit as its folder after the kit is backed up, and its
# exit code is the run's
"$editor" "$work/kit" --frames 2 --rehacer >"$work/log" 2>&1 && fail "Rehacer went on a kit with no rehacer.command"
grep -q "no rehacer.command" "$work/log" || fail "no plain word about the missing script: $(cat "$work/log")"
printf '#!/bin/sh\necho "working in $(pwd)"\necho made > coche/proof.txt\nexit ${REHACER_EXIT:-0}\n' > "$work/kit/rehacer.command"
"$editor" "$work/kit" --frames 2 --set year=1992 --rehacer >"$work/log" 2>&1 && fail "Rehacer ran with unsaved card changes"
grep -q "not saved" "$work/log" || fail "no word about the unsaved changes: $(cat "$work/log")"
[ ! -e "$work/kit/coche/proof.txt" ] && [ ! -d "$work/kit/.studio-backup" ] || fail "a refused Rehacer ran or made a backup"
"$editor" "$work/kit" --frames 3 --rehacer --shot "$work/rehacer.png" >"$work/log" 2>&1 || fail "Rehacer did not run: $(cat "$work/log")"
grep -q "exit code 0" "$work/log" || fail "no exit code told: $(cat "$work/log")"
[ -f "$work/kit/coche/proof.txt" ] || fail "the script did not run with the kit as its folder"
set -- "$work"/kit/.studio-backup/*/coche/mod.ini
[ -f "$1" ] || fail "no backup of the kit before it ran"
[ -s "$work/rehacer.png" ] || fail "no PNG of a Rehacer run"
REHACER_EXIT=3 "$editor" "$work/kit" --frames 3 --rehacer >"$work/log" 2>&1 && fail "a script that failed was told as a success"
grep -q "exit code 3" "$work/log" || fail "the failing exit code is not told: $(cat "$work/log")"
rm -f "$work/kit/rehacer.command"
mkdir "$work/notcar"
printf 'id=t\nname=T\nversion=1\ntype=track\nstem=TT\n' > "$work/notcar/mod.ini"
out=$("$editor" "$work/notcar" --frames 2 2>&1) && fail "a track's mod.ini was opened as a car"
echo "$out" | grep -q "type=track" || fail "no word about what the card is: $out"
[ ! -e "$work/notcar/project.ini" ] || fail "a card that is not a car was made a track project"
# Export original car: a fixture game folder (a synthetic car file, masks that are PNG headers); a traffic car becomes a masks project, a refused car and a second
# export into the same folder say why and write nothing; the window of the choice draws
mkdir -p "$work/game/cd/SIMDATA/CARFAMS" "$work/game/data/car_masks/Wagon" "$work/exports"
printf 'cfm' > "$work/game/cd/SIMDATA/CARFAMS/WAGON.CFM"
printf 'cfm' > "$work/game/cd/SIMDATA/CARFAMS/PROBE.CFM"
python3 -c "
import sys, struct
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + struct.pack('>I4sIIBB', 13, b'IHDR', 100, 50, 8, 0) + bytes(20))
" "$work/game/data/car_masks/Wagon/side_mask.png"
export_opts="--cd $work/game/cd --port-data $work/game/data"
"$editor" --export-car Wagon --into "$work/exports" $export_opts --frames 3 --shot "$work/export.png" >"$work/log" 2>&1 || fail "export of a traffic car: $(cat "$work/log")"
grep -q "do not share" "$work/log" || fail "no word that the files are the game's: $(cat "$work/log")"
grep -q '^kind=masks$' "$work/exports/wagon-masks/project.ini" && grep -q '^redistributable=0$' "$work/exports/wagon-masks/project.ini" || fail "the masks project's project.ini"
cmp -s "$work/game/data/car_masks/Wagon/side_mask.png" "$work/exports/wagon-masks/car_masks/Wagon/side_mask.png" || fail "the exported mask is not the game's file"
[ -s "$work/export.png" ] || fail "no PNG of an exported masks project"
"$editor" --export-car Wagon --into "$work/exports" $export_opts --frames 2 >"$work/log" 2>&1 && fail "a second export into the same folder went"
grep -q "already there" "$work/log" || fail "no word about the folder that is there: $(cat "$work/log")"
"$editor" --export-car probe --into "$work/exports" $export_opts --frames 2 >"$work/log" 2>&1 && fail "a car with no masks was exported"
[ ! -e "$work/exports/probe-masks" ] && [ ! -e "$work/exports/probe-masks.exporting" ] || fail "a refused export left files"
"$editor" "$work/exports/wagon-masks" --frames 3 --shot "$work/masksproject.png" >"$work/log" 2>&1 || fail "a masks project did not open: $(cat "$work/log")"
[ ! -e "$work/exports/wagon-masks/trace.json" ] || fail "a masks project was made a track project"
"$editor" --ask export --pick Wagon $export_opts --frames 4 --shot "$work/exportdialog.png" >"$work/log" 2>&1 || fail "the export window did not draw: $(cat "$work/log")"
[ -s "$work/exportdialog.png" ] || fail "no PNG of the export window"
"$editor" --pick nosuchcar --frames 2 $export_opts >/dev/null 2>&1 && fail "--pick took a car that is not in the list"
# Race photo with a fake game (a script that writes what the real one does): a masks project of a traffic car finds the car in the draw, follows its slot and shows
# the picture; the picture is written as a PNG; a game that fails is told, with a non-zero status; a car of the game's that is not one asks for an export first
cat > "$work/fake-tnfs" <<'FAKE'
#!/bin/sh
user=; shot=
while [ $# -gt 0 ]; do case "$1" in --user) user=$2; shift;; --screenshot) shot=$2; shift;; esac; shift; done
[ "${FAKE_FAIL:-}" = 1 ] && { echo "error: the fake game fails" >&2; exit 3; }
printf 'race car: slot 0 is TSupra\nrace car: slot 3 is Wagon\n' > "$user/spirited-drive.log"
if grep -q '^render_scale=2$' "$user/restomod.cfg"; then   # the size the game draws at: render scale 2, twice each way
    printf 'P6\n1280 960\n255\n' > "$shot"; head -c 3686400 /dev/zero >> "$shot"
else
    printf 'P6\n640 480\n255\n' > "$shot"; head -c 921600 /dev/zero >> "$shot"
fi
FAKE
chmod +x "$work/fake-tnfs"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --night --race-png "$work/photo.png" --frames 4 --shot "$work/racepanel.png" >"$work/log" 2>&1 || fail "Race photo with a fake game: $(cat "$work/log")"
grep -q "Photo: the camera follows the Wagon (slot 3" "$work/log" || fail "no word about the slot followed: $(cat "$work/log")"
python3 -c "
import sys
d = open(sys.argv[1], 'rb').read()
assert d[:8] == b'\x89PNG\r\n\x1a\n' and int.from_bytes(d[16:20], 'big') == 1280 and int.from_bytes(d[20:24], 'big') == 960
" "$work/photo.png" || fail "the race photo is not a 1280x960 PNG (render scale 2, the default)"
# the Race window's switches (T6): render scale 1 is 640x480, and what is off is said in the summary
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --race-scale 1 --race-off reflections --race-png "$work/photo1.png" --frames 2 >"$work/log" 2>&1 || fail "Race photo at scale 1: $(cat "$work/log")"
grep -q "(Reflections off, 640x480)" "$work/log" || fail "the summary does not say what was off: $(cat "$work/log")"
python3 -c "
import sys
d = open(sys.argv[1], 'rb').read()
assert int.from_bytes(d[16:20], 'big') == 640 and int.from_bytes(d[20:24], 'big') == 480
" "$work/photo1.png" || fail "the race photo at render scale 1 is not 640x480"
[ -s "$work/racepanel.png" ] || fail "no PNG of the Race window"
# a switch that is not one, a scale that is not 1 or 2, or the look without a Race photo: refused, never a photo with the whole look
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --race-off reflection --frames 2 >"$work/log" 2>&1 && fail "--race-off with an unknown switch was taken"
grep -q "reflections, car_colours" "$work/log" || fail "an unknown --race-off word is not named: $(cat "$work/log")"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --race-scale 3 --frames 2 >"$work/log" 2>&1 && fail "--race-scale 3 was taken"
"$editor" "$work/exports/wagon-masks" --race-scale 1 --frames 2 >"$work/log" 2>&1 && fail "--race-scale without --race-photo was taken"
FAKE_FAIL=1 "$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --frames 2 >"$work/log" 2>&1 && fail "a game that failed was told as a success"
grep -q "exit code 3" "$work/log" || fail "the game's exit code is not told: $(cat "$work/log")"
"$editor" "$work/exports/wagon-masks" --game "$work/fake-tnfs" --cd "$work/game/cd" --race-photo --track TR5 --frames 2 >"$work/log" 2>&1 && fail "a traffic car was raced on a circuit"
grep -q "no traffic" "$work/log" || fail "no word about the circuit: $(cat "$work/log")"
ls "${TMPDIR:-/tmp}" | grep -q '^studio-race-' && fail "a scratch folder of a race was left behind"
# the Model window: a car file it cannot draw says so and the run goes on; --orbit is YAW,PITCH[,DIST] or refused; with the CD's P911 two orbits are two pictures
"$editor" "$work/kit" --frames 3 --orbit 90,10,500 --shot "$work/stub.png" >"$work/log" 2>&1 || fail "run with an orbit on a car it cannot draw: $(cat "$work/log")"
"$editor" "$work/kit" --frames 2 --orbit north >"$work/log" 2>&1 && fail "a bad --orbit was accepted"
grep -q "YAW,PITCH" "$work/log" || fail "no word about --orbit: $(cat "$work/log")"
p911="$(cd "$(dirname "$0")/.." && pwd)/original/SIMDATA/CARFAMS/P911.CFM"
if [ -f "$p911" ]; then
    mkdir -p "$work/k911/coche/SIMDATA/CARFAMS" "$work/k911/coche/car_masks/P911"
    printf 'id=k911\nname=K911\nversion=1\ntype=car\nstem=P911\nsound_from=911\n' > "$work/k911/coche/mod.ini"
    cp "$p911" "$work/k911/coche/SIMDATA/CARFAMS/P911.CFM"
    printf 'x' > "$work/k911/coche/car_masks/P911/rsid_mask.png"
    "$editor" "$work/k911" --frames 4 --orbit 215,18 --shot "$work/m1.png" >"$work/log" 2>&1 || fail "run on the 911: $(cat "$work/log")"
    "$editor" "$work/k911" --frames 4 --orbit 90,18 --shot "$work/m2.png" >"$work/log" 2>&1 || fail "run on the 911, another orbit: $(cat "$work/log")"
    "$editor" "$work/k911" --frames 4 --orbit 215,18 --shot "$work/m3.png" >"$work/log" 2>&1 || fail "run on the 911 again: $(cat "$work/log")"
    cmp -s "$work/m1.png" "$work/m2.png" && fail "two orbits drew the same window"
    cmp -s "$work/m1.png" "$work/m3.png" || fail "the same orbit drew two different windows"
    # the body shine (M5): on by default, off is another picture of the same orbit; a bad --shine is refused
    "$editor" "$work/k911" --frames 4 --orbit 215,18 --shine off --shot "$work/m4.png" >"$work/log" 2>&1 || fail "run on the 911 without the shine: $(cat "$work/log")"
    cmp -s "$work/m1.png" "$work/m4.png" && fail "the shine off drew the same window as on"
    "$editor" "$work/k911" --frames 2 --shine maybe >"$work/log" 2>&1 && fail "a bad --shine was accepted"
    grep -q "on or off" "$work/log" || fail "no word about --shine: $(cat "$work/log")"
    cmp -s "$work/m1.png" "$work/stub.png" && fail "the 911 looks like a car that cannot be drawn"
fi
# Edit mask: a car file with no textures offers none (the fixture's is not a real one); with the player's real game files a traffic car's mask is opened, painted, saved
# (a grey PNG of the same size) and drawn, and a texture the car has no mask for is refused
"$editor" "$work/exports/wagon-masks" --mask side --frames 2 >"$work/log" 2>&1 && fail "a mask was opened of a car file that has no textures"
grep -q "no texture side" "$work/log" || fail "no word that the car file has no such texture: $(cat "$work/log")"
repo=$(cd "$(dirname "$0")/.." && pwd)
if [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then
    mkdir "$work/real"
    "$editor" --export-car Wagon --into "$work/real" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 || fail "export of the real Wagon: $(cat "$work/log")"
    m="$work/real/wagon-masks/car_masks/Wagon/side_mask.png"
    "$editor" "$work/real/wagon-masks" --mask side --frames 8 --shot "$work/mask-before.png" >"$work/log" 2>&1 || fail "Edit mask of the real Wagon: $(cat "$work/log")"
    cmp -s "$m" "$repo/data/car_masks/Wagon/side_mask.png" || fail "opening a mask changed the file"
    "$editor" "$work/real/wagon-masks" --mask side --mask-rect 142,26,8,5,248 --mask-save --frames 8 --shot "$work/mask-after.png" >"$work/log" 2>&1 || fail "Edit mask with a painted rectangle: $(cat "$work/log")"
    grep -q "saved side_mask.png" "$work/log" || fail "no word that the mask was saved: $(cat "$work/log")"
    cmp -s "$m" "$repo/data/car_masks/Wagon/side_mask.png" && fail "the saved mask is the game's file: nothing was written"
    [ ! -e "$m.saving" ] || fail "a temporary file of the Save was left behind"
    cmp -s "$work/mask-before.png" "$work/mask-after.png" && fail "the edit did not change the picture"
    [ -s "$work/mask-after.png" ] || fail "no PNG of the Edit mask window"
    "$editor" --export-car P911 --into "$work/real" --cd "$repo/original" --port-data "$repo/data" --frames 1 >"$work/log" 2>&1 || fail "export of the real 911: $(cat "$work/log")"
    "$editor" "$work/real/p911-original" --frames 14 --shot "$work/preview-file.png" >"$work/log" 2>&1 || fail "Preview of the real 911: $(cat "$work/log")"
    "$editor" "$work/real/p911-original" --paint 3 --frames 14 --shot "$work/preview-paint.png" >"$work/log" 2>&1 || fail "Preview of the real 911 in a paint: $(cat "$work/log")"
    cmp -s "$work/preview-file.png" "$work/preview-paint.png" && fail "a paint did not change the Preview"
    ls "${TMPDIR:-/tmp}" | grep -q '^studio-preview-' && fail "a scratch folder of the Preview was left behind"
    # the night view: Day, Night with the lights off and Night with the headlights each show another picture of the front texture
    "$editor" "$work/real/p911-original" --view day --preview-tex frnt --frames 20 --shot "$work/night-day.png" >"$work/log" 2>&1 || fail "Preview by day: $(cat "$work/log")"
    "$editor" "$work/real/p911-original" --view night --lights none --preview-tex frnt --frames 20 --shot "$work/night-dark.png" >"$work/log" 2>&1 || fail "Preview at night, lights off: $(cat "$work/log")"
    "$editor" "$work/real/p911-original" --view night --lights head --preview-tex frnt --frames 20 --shot "$work/night-head.png" >"$work/log" 2>&1 || fail "Preview at night, headlights: $(cat "$work/log")"
    cmp -s "$work/night-day.png" "$work/night-dark.png" && fail "Night did not change the Preview"
    cmp -s "$work/night-dark.png" "$work/night-head.png" && fail "the headlights did not light"
    "$editor" "$work/real/p911-original" --view night --night-track nosuch --frames 2 >"$work/log" 2>&1
    grep -q "not a track the night view knows" "$work/log" || fail "no word about an unknown --night-track: $(cat "$work/log")"
    "$editor" "$work/real/wagon-masks" --mask nosuch --frames 2 >"$work/log" 2>&1 && fail "a texture the car has not was opened"
    "$editor" "$work/real/wagon-masks" --mask side --mask-rect 1,2,3 --frames 2 >"$work/log" 2>&1 && fail "a bad --mask-rect was taken"
fi
# Changes from the game's masks: the fixture's exported Wagon has the game's own file; with the real game files an edit shows as a changed mask with its texels
"$editor" "$work/exports/wagon-masks" --cd "$work/game/cd" --port-data "$work/game/data" --changes --frames 2 >"$work/log" 2>&1 || fail "--changes of the fixture's Wagon: $(cat "$work/log")"
grep -q "car_masks/Wagon: 0 changed, 0 new, 0 missing, 1 unchanged" "$work/log" || fail "an unchanged export is told as changed: $(cat "$work/log")"
if [ -r "$repo/original/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/car_masks/Wagon" ]; then
    "$editor" "$work/real/wagon-masks" --cd "$repo/original" --port-data "$repo/data" --changes --frames 2 >"$work/log" 2>&1 || fail "--changes of the real Wagon: $(cat "$work/log")"
    grep -q "side_mask.png" "$work/log" || fail "the edited mask is not in the changes: $(cat "$work/log")"
    grep -q "1 changed" "$work/log" || fail "no count of changes: $(cat "$work/log")"
    grep -q "headlight +40" "$work/log" || fail "no word of the texels' classes: $(cat "$work/log")"
fi
"$editor" "$work/proj" --changes --frames 2 >"$work/log" 2>&1 && fail "--changes of a track project went"
"$editor" --no-such-option >/dev/null 2>&1 && fail "an unknown option was accepted"
[ "$failures" -eq 0 ] && echo "editor_headless: ok"
exit "$failures"
