#!/bin/sh
# The editor on track projects, headless, in the order a user goes: a new project, its points and limits, the road profile keys and
# sides, Build, Install; then the other ways in (window size, player folder, --changes, an unknown option).
# The track editor runs headless (Dear ImGui over the SDL software renderer): a project folder is made, N frames are drawn and the last one
# is written as a PNG that has the window's size, the road's line and the control points drawn in the viewport panel; --save writes the
# project files; --user makes its folder and keeps the layout in it; an unknown option fails. (Without a PROJECT it shows the start screen and makes nothing; a normal run reopens the last project from the recent list, a headless one does not read it.)
# usage: editor_headless_track.sh EDITOR
. "$(dirname "$0")/editor_headless_common.sh"

# ==== New project: made, saved and drawn ====
"$editor" "$work/proj" --frames 6 --save --shot "$work/shot.png" >"$work/log" 2>&1 || fail "run: $(cat "$work/log")"
[ -f "$work/proj/project.ini" ] && [ -f "$work/proj/trace.json" ] || fail "no project files"
[ -f "$work/shot.png" ] || { fail "no PNG"; exit 1; }
png_check "$work/shot.png" good

# ==== Points and limits: mild, amber and red curves, a point selected, unsaved changes on Quit ====
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

# ==== Road profile keys: widths, shoulder surfaces, sides (each built and read back from the TRI) ====
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

# ==== Build: under the limits, over an amber one, refused over a red one ====
# Build: a curve under the limits is built, one over them is refused with the limit named
"$editor" "$work/mild" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a curve under the limits: $(cat "$work/log")"
[ -s "$work/mild/build/mild.zip" ] || fail "no zip of the mild S"
"$editor" "$work/warn" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a curve that is only over an amber limit: $(cat "$work/log")"
[ -s "$work/warn/build/warn.zip" ] || fail "no zip of the amber curve"
"$editor" "$work/bad" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a curve over a red limit"
grep -q "tighter than 20.9 m\|more than 16.5 degrees" "$work/log" || fail "the refusal does not name the red limit: $(cat "$work/log")"

# ==== Install: the game not where it is said or not found ====
# the game is not where --game says: Install says so, and what to do, without running anything
out=$("$editor" "$work/proj" --frames 2 --install --game "$work/no-such-game" --user "$work/u2" 2>&1) && fail "install with a game that is not there went"
echo "$out" | grep -q "copy Spirited Drive and Spirited Drive Studio to Applications" || fail "no hint about the game's place: $out"
# and with no --game at all, from a folder with no ./tnfs (skipped where the game really is in /Applications)
if [ ! -x "/Applications/Spirited Drive.app/Contents/MacOS/tnfs" ] && [ ! -x "$(dirname "$editor")/../../../Spirited Drive.app/Contents/MacOS/tnfs" ]; then
    out=$(cd "$work" && "$editor" "$work/proj" --frames 2 --install 2>&1) && fail "install with no game found went"
    echo "$out" | grep -q "Applications" || fail "no hint with no game found: $out"
fi

# ==== Window, player folder, and what a track project refuses ====
# --window WxH: the picture is that size (a layout at another window size); a size out of range is refused by name
"$editor" "$work/surface" --window 1440x900 --frames 6 --select 2 --shot "$work/wide.png" >"$work/log" 2>&1 || fail "run with --window: $(cat "$work/log")"
python3 -c "import struct,sys; d=open(sys.argv[1],'rb').read(24); sys.exit(0 if struct.unpack('>II', d[16:24]) == (1440, 900) else 1)" "$work/wide.png" || fail "--window 1440x900 did not make a 1440x900 picture"
"$editor" "$work/surface" --window 100x100 --frames 2 >"$work/log" 2>&1 && fail "--window 100x100 was taken"
grep -q "\-\-window wants WxH" "$work/log" || fail "the refusal of --window does not say what it wants: $(cat "$work/log")"
"$editor" "$work/proj" --frames 2 --user "$work/user/sub" >/dev/null 2>&1 || fail "run with --user"
[ -f "$work/user/sub/track-editor.ini" ] || fail "the layout was not kept in the player folder (made when missing)"
"$editor" "$work/proj" --changes --frames 2 >"$work/log" 2>&1 && fail "--changes of a track project went"
"$editor" --no-such-option >/dev/null 2>&1 && fail "an unknown option was accepted"
[ "$failures" -eq 0 ] && echo "editor_headless_track: ok"
exit "$failures"
