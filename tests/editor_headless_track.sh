#!/bin/sh
# The editor on track projects, headless, in the order a user goes: a new project, its points and limits, the road profile keys and
# sides, Build, Install; then the other ways in (window size, player folder, --changes, an unknown option).
# The track editor runs headless (Dear ImGui over the SDL software renderer): a project folder is made, N frames are drawn and the last one
# is written as a PNG that has the window's size, the road's line and the control points drawn in the viewport panel; --save writes the
# project files; --user makes its folder and keeps the layout in it; an unknown option fails. (Without a PROJECT it shows the start screen and makes nothing; a normal run reopens the last project from the recent list, a headless one does not read it.)
# usage: editor_headless_track.sh EDITOR
. "$(dirname "$0")/editor_headless_common.sh"

# ==== New project: made, saved and drawn ====
"$editor" "$work/proj" --window 960x1200 --frames 6 --save --shot "$work/shot.png" >"$work/log" 2>&1 || fail "run: $(cat "$work/log")"
[ -f "$work/proj/project.ini" ] && [ -f "$work/proj/trace.json" ] || fail "no project files"
[ -f "$work/shot.png" ] || { fail "no PNG"; exit 1; }
png_check "$work/shot.png" good

# ==== Points and limits: mild, amber and red curves, a point selected, unsaved changes on Quit ====
mkdir "$work/mild"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 40.0], [15.0, 80.0], [15.0, 120.0], [0.0, 160.0], [0.0, 200.0]]\n}\n' > "$work/mild/trace.json"   # an S of 6.5 degrees a node at most: nothing is marked (the old limit was 6)
"$editor" "$work/mild" --window 960x1200 --frames 6 --shot "$work/mild.png" >"$work/log" 2>&1 || fail "run on a mild S: $(cat "$work/log")"
if [ -f "$work/mild.png" ]; then png_check "$work/mild.png" good; else fail "no PNG of the mild S"; fi
mkdir "$work/warn"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, -40.0], [0.0, 0.0], [20.0, 40.0], [20.0, 80.0], [0.0, 120.0], [0.0, 160.0]]\n}\n' > "$work/warn/trace.json"   # an S that turns about 9 degrees a node: amber
"$editor" "$work/warn" --window 960x1200 --frames 6 --shot "$work/warn.png" >"$work/log" 2>&1 || fail "run on a curve over an amber limit: $(cat "$work/log")"
if [ -f "$work/warn.png" ]; then png_check "$work/warn.png" warn; else fail "no PNG of the curve over an amber limit"; fi
"$editor" "$work/warn" --window 960x1200 --select 3 --frames 6 --shot "$work/select.png" >"$work/log" 2>&1 || fail "run with a point selected (the Node window): $(cat "$work/log")"
[ -s "$work/select.png" ] || fail "no PNG with a point selected"
"$editor" "$work/warn" --window 960x1200 --edit --ask quit --frames 6 --shot "$work/confirm.png" >"$work/log" 2>&1 || fail "run with unsaved changes and a quit asked for: $(cat "$work/log")"
[ -s "$work/confirm.png" ] || fail "no PNG with the unsaved-changes question"
"$editor" "$work/warn" --ask quit --frames 6 >"$work/log" 2>&1 || fail "run with a quit asked for and nothing unsaved: $(cat "$work/log")"
# Escape never quits Studio; a quit is asked every time, with nothing unsaved too (Quit / Cancel), and Escape on that question is Cancel
"$editor" "$work/warn" --frames 6 --steps key:Escape,print-confirm >"$work/log" 2>&1 || fail "Escape on a clean track: $(cat "$work/log")"
grep -q "^confirm: none$" "$work/log" && grep -q "^go: none$" "$work/log" || fail "Escape asked to quit or quit: $(cat "$work/log")"
"$editor" "$work/warn" --frames 6 --ask quit --steps print-confirm >"$work/log" 2>&1 || fail "a clean quit asked for: $(cat "$work/log")"
grep -q "^confirm: quit$" "$work/log" && grep -q "^go: none$" "$work/log" || fail "a quit with nothing unsaved was not asked: $(cat "$work/log")"
"$editor" "$work/warn" --frames 6 --ask quit --steps key:Escape,print-confirm >"$work/log" 2>&1 || fail "Escape on the quit question: $(cat "$work/log")"
grep -q "^confirm: none$" "$work/log" && grep -q "^go: none$" "$work/log" || fail "Escape on the quit question did not cancel it: $(cat "$work/log")"
mkdir "$work/bad"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 80.0], [20.0, 90.0], [0.0, 100.0], [0.0, 180.0]]\n}\n' > "$work/bad/trace.json"
"$editor" "$work/bad" --window 960x1200 --frames 6 --shot "$work/bad.png" >"$work/log" 2>&1 || fail "run on a project that breaks limits: $(cat "$work/log")"
if [ -f "$work/bad.png" ]; then png_check "$work/bad.png" bad; else fail "no PNG of the project that breaks limits"; fi

# ==== Road profile keys: widths, shoulder surfaces, sides (each built and read back from the TRI) ====
# road keys (trace.json version 2): a ramp from the project's 10 m to 8 m and 14 m builds a TRI whose verge bytes are the widths the keys say; a ramp too sudden is
# amber (built, drawn amber), and a file of a newer version is refused with its reason
mkdir "$work/keyed"
printf '{\n  "version": 2,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 1, "width_m": 8.0}, {"point": 4, "width_m": 14.0}]\n}\n' > "$work/keyed/trace.json"
"$editor" "$work/keyed" --window 960x1200 --frames 6 --select 2 --shot "$work/keyed.png" >"$work/log" 2>&1 || fail "run on a project with road keys: $(cat "$work/log")"
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
"$editor" "$work/sudden" --window 960x1200 --frames 6 --shot "$work/sudden.png" >"$work/log" 2>&1 || fail "run on a ramp that is too sudden: $(cat "$work/log")"
if [ -f "$work/sudden.png" ]; then png_check "$work/sudden.png" warn; else fail "no PNG of the sudden ramp"; fi
"$editor" "$work/sudden" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a ramp that is only over an amber limit: $(cat "$work/log")"
mkdir "$work/newer"
printf '{\n  "version": 6,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 100.0], [0.0, 200.0]]\n}\n' > "$work/newer/trace.json"
"$editor" "$work/newer" --frames 2 --build >"$work/log" 2>&1 && fail "a trace.json of a newer version was read"
grep -q "version 6" "$work/log" || fail "the refusal does not name the version: $(cat "$work/log")"
# shoulder surfaces (trace.json version 3, step 2b): a tarmac shoulder on the left from point 1, then the right one from point 3 (the left back to the verge), builds a TRI
# whose nodes' flags[2] (node +6, high nibble left, low right) clear that side: 0x22 before, 0x02, then 0x20; those segments draw that shoulder in asphalt
mkdir "$work/surface"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 1, "shoulder_l_surface": "tarmac"}, {"point": 3, "shoulder_l_surface": "verge", "shoulder_r_surface": "tarmac"}]\n}\n' > "$work/surface/trace.json"
"$editor" "$work/surface" --window 960x1200 --frames 6 --select 3 --shot "$work/surface.png" >"$work/log" 2>&1 || fail "run on a project with shoulder surfaces: $(cat "$work/log")"
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
"$editor" "$work/sides" --window 960x1200 --frames 6 --select 1 --shot "$work/sides.png" >"$work/log" 2>&1 || fail "run on a project with side keys: $(cat "$work/log")"
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
# lanes (step 4): 1+2 from point 1, 1+1 again from point 3. A segment (4 nodes, 25 m) takes the lanes of the last point at or before its start: nodes
# 12..31 have flags[0] 0x12, the rest the template's. The left half stays 40 eighths (5 m); the right one grows 5 a node to 80 (two lanes) and back from node 33;
# the shoulder and barrier move out with the edge (barrier - verge the template's on every node, each side); a segment whose right half is two whole lanes
# draws its first right strip with the two-lane picture (slot 18), every other strip the template's
mkdir "$work/lanes"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 1, "lanes": "1+2"}, {"point": 3, "lanes": "1+1"}]\n}\n' > "$work/lanes/trace.json"
"$editor" "$work/lanes" --window 960x1200 --frames 6 --select 2 --shot "$work/lanes.png" >"$work/log" 2>&1 || fail "run on a project with lanes keys: $(cat "$work/log")"
png_check "$work/lanes.png" good
"$editor" "$work/lanes" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a project with lanes keys: $(cat "$work/log")"
cat > "$work/tri_lanes.py" <<'PY'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
names = [n for n in z.namelist() if n.upper().endswith(".TRI")]
assert len(names) == 1, names
d = z.read(names[0])
node = lambda k, b: d[0x98C + 36 * k + b]
stretch = lambda k: 12 <= k < 32
lanes = [node(k, 4) for k in range(49)]
assert lanes == [0x12 if stretch(k) else node(0, 4) for k in range(49)], [hex(v) for v in lanes]
assert all(node(k, 0) == 40 for k in range(49)), [node(k, 0) for k in range(49)]
right = [node(k, 1) for k in range(49)]
assert max(right) == 80 and right[0] == 40 and right[48] == 40, right
assert all(abs(right[k] - right[k - 1]) <= 5 for k in range(1, 49)), right
assert all(right[k] == 80 for k in range(20, 33)) and right[33] == 75, right   # node 32 too: segment 7 draws its picture to it
for side in (0, 1):
    assert all(node(k, 2 + side) - node(k, side) == node(0, 2 + side) - node(0, side) for k in range(49)), side
at = d.find(b"TRKD")
tex = [d[at + 288 * s + 0xE:at + 288 * s + 0x18] for s in range(12)]
full = [s for s in range(12) if stretch(4 * s) and right[4 * s] >= 80]
assert full == [5, 6, 7], full
for s in range(12):
    want = bytes([18]) + tex[0][1:] if s in full else tex[0]
    assert tex[s] == want, (s, list(tex[s]), list(want))
PY
python3 "$work/tri_lanes.py" "$work/lanes/build/lanes.zip" || fail "the TRI of the lanes project does not have the lanes of its keys"
# the fence step (trace.json version 4): tall from point 1 (60 m), the rail picture from point 3 (180 m), and a 6 m left shoulder from point 2 with no
# Custom. A segment (4 nodes, 25 m) takes its first node's: the fence byte's low bit (0x20) clear from segment 3 (75 m), its picture bits 3 (the rail, slot
# 35) from segment 8 (200 m), the side bits the template's; the left barrier follows the shoulder (barrier - verge = shoulder), the right is the template's
mkdir "$work/fence"
printf '{\n  "version": 4,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 1, "fence_height": "tall"}, {"point": 2, "shoulder_l_m": 6.0}, {"point": 3, "fence_art": "rail"}]\n}\n' > "$work/fence/trace.json"
"$editor" "$work/fence" --window 960x1200 --frames 6 --select 3 --shot "$work/fence.png" >"$work/log" 2>&1 || fail "run on a project with fence keys: $(cat "$work/log")"
png_check "$work/fence.png" good
"$editor" "$work/fence" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a project with fence keys: $(cat "$work/log")"
cat > "$work/tri_fence.py" <<'PY'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
names = [n for n in z.namelist() if n.upper().endswith(".TRI")]
assert len(names) == 1, names
d = z.read(names[0])
node = lambda k, b: d[0x98C + 36 * k + b]
at = d.find(b"TRKD")
fences = [d[at + 288 * s + 0xD] for s in range(12)]
assert fences[0] == 0xE0, hex(fences[0])   # the template's: both sides, low, planks
want = [(0xE0 & ~(0x20 if 25 * s >= 60 else 0) & ~0x1F) | (3 if 25 * s >= 180 else 0) for s in range(12)]
assert fences == want, ([hex(f) for f in fences], [hex(w) for w in want])
left = [node(k, 2) - node(k, 0) for k in range(49)]   # barrier - verge, 1/8 m
assert left[0] == 24 and left[48] == 48, left
assert all(left[k] <= left[k + 1] for k in range(48)), left
assert all(node(k, 3) - node(k, 1) == 24 for k in range(49)), "the right barrier moved"
PY
python3 "$work/tri_fence.py" "$work/fence/build/fence.zip" || fail "the TRI of the fence project does not have the fence of its keys"

# ==== Several points selected (dep87): gestures, group edits, one undo each, the guard on removing, the profile Mixed ====
# a straight of 8 points 40 m apart, the right shoulder tarmac from point 3. In the viewport: a click, Shift+click a range, Cmd+click one in or
# out, Esc clears, a box drag the points inside it; the Points rows take the same clicks. The Road profile shows a value the points share, else Mixed
mkdir "$work/multi"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 40.0], [0.0, 80.0], [0.0, 120.0], [0.0, 160.0], [0.0, 200.0], [0.0, 240.0], [0.0, 280.0]],\n  "keys": [{"point": 3, "shoulder_r_surface": "tarmac"}]\n}\n' > "$work/multi/trace.json"
"$editor" "$work/multi" --window 960x1200 --user "$work/mu" --frames 2 --steps click:1,print-sel,shift-click:4,print-sel,cmd-click:6,print-sel,cmd-click:2,print-sel,key:Escape,print-sel,box:-10,110,10,210,print-sel,row:2,shift-row:5,cmd-row:7,print-sel,print-profile >"$work/log" 2>&1 \
    || fail "selecting several points: $(cat "$work/log")"
[ "$(grep '^selected:' "$work/log" | tr '\n' '/')" = "selected: 1/selected: 1-4/selected: 1-4, 6/selected: 1, 3-4, 6/selected: none/selected: 3-5/selected: 2-5, 7/" ] \
    || fail "click, Shift, Cmd, Esc, box and rows did not select what they say: $(grep '^selected:' "$work/log")"
grep -q "^profile: Off-road|Mixed 3.00|3.00" "$work/log" || fail "the Road profile of points 2-5, 7 does not show the right surface Mixed: $(grep '^profile:' "$work/log")"
# dragging one of 2-4 moves the three, one undo puts them back; an arrow nudges the three; rotate 90 about point 2 folds the road (Problems shows
# it red at once), undo straightens it; Delete removes the three, undo brings them back; removing all 8 is refused with the minimum named
"$editor" "$work/multi" --window 960x1200 --user "$work/mu" --frames 2 --steps click:2,shift-click:4,drag:3,20,0,print,undo,print,key:Right,print,undo,key:],print,undo,rotate:90,print,problems,undo,problems,key:Delete,print,undo,print,box:-100,-10,100,300,key:Delete,status,print >"$work/log" 2>&1 \
    || fail "editing several points: $(cat "$work/log")"
python3 -c "
import sys
rows = [[tuple(map(float, q.split(','))) for q in l.split()[2:]] for l in open(sys.argv[1]) if l.startswith('points: ')]
straight = [(0.0, 40.0 * i) for i in range(8)]
assert len(rows) == 8, len(rows)
d = rows[0]
assert d[2][0] > 10 and d[2][0] == d[3][0] == d[4][0] and [p[1] for p in d] == [p[1] for p in straight], ('drag', d)
assert all(d[i] == straight[i] for i in (0, 1, 5, 6, 7)), ('drag moved others', d)
assert rows[1] == straight, ('undo of the drag', rows[1])
assert [p[0] for p in rows[2]] == [0, 0, 1, 1, 1, 0, 0, 0], ('arrow', rows[2])
r = rows[3]
assert r[2] == (0, 80) and abs(r[3][0] - 40 * 0.0174524) < 1e-3 and abs(r[4][0] - 80 * 0.0174524) < 1e-3, (']', r)
assert rows[4][2:5] == [(0, 80), (40, 80), (80, 80)], ('rotate 90', rows[4])
assert len(rows[5]) == 5 and rows[5] == [straight[i] for i in (0, 1, 5, 6, 7)], ('Delete', rows[5])
assert rows[6] == straight and rows[7] == straight, ('undo of Delete, refusal', rows[6:])
" "$work/log" || fail "the group's drag, arrow, ], rotate and Delete (each one undo) did not do what they say: $(grep '^points:' "$work/log")"
[ "$(grep '^problems:' "$work/log" | tr '\n' '/')" = "problems: red node 13/problems: none/" ] || fail "Problems did not show the folded road red at once, then clear on undo: $(grep '^problems:' "$work/log")"
grep -q "^status: a track keeps at least 2 points: 8 of 8 are selected" "$work/log" || fail "removing all the points was not refused with the minimum: $(grep '^status:' "$work/log")"

# ==== Copy, Paste, Mirror, Reverse on a run of points (dep87 1b): half a circuit closes; the profile's sides follow; each is one undo; the end snaps ====
# half a circuit from (0, 0) to (0, 300), its ends square to the axis x = 0, the right shoulder asphalt from point 2. Cmd+C on all of it, Cmd+V
# after its last point, Mirror L/R and Reverse on the pasted run: the second half is the first's mirror image and ends on the start. Point 10
# (old 4's mirror) has the asphalt on the left after Mirror, back on the right after Reverse (a circuit's outside both ways). Three undos.
mkdir "$work/half"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [60.0, 0.0], [140.0, 30.0], [160.0, 120.0], [110.0, 180.0], [150.0, 260.0], [60.0, 300.0], [0.0, 300.0]],\n  "keys": [{"point": 2, "shoulder_r_surface": "tarmac"}]\n}\n' > "$work/half/trace.json"
"$editor" "$work/half" --window 960x1200 --user "$work/hu" --frames 2 --steps row:0,shift-row:7,copy,status,paste,status,print-sel,mirror-lr,status,row:10,print-profile,row:7,shift-row:14,reverse,status,print,row:10,print-profile,undo,undo,undo,print >"$work/log" 2>&1 \
    || fail "copy, paste, mirror and reverse on half a circuit: $(cat "$work/log")"
[ "$(grep '^status:' "$work/log" | tr '\n' '/')" = "status: 8 points copied (0-7)/status: 7 points pasted after point 7/status: points 7-14 mirrored left/right/status: points 7-14 reversed/" ] \
    || fail "copy, paste, mirror, reverse did not say what they did: $(grep '^status:' "$work/log")"
grep -q "^selected: 7-14$" "$work/log" || fail "the pasted run (from the point it went after) is not the selection: $(grep '^selected:' "$work/log")"
grep -q "^points: 15 0.000,0.000 60.000,0.000 140.000,30.000 160.000,120.000 110.000,180.000 150.000,260.000 60.000,300.000 0.000,300.000 -60.000,300.000 -150.000,260.000 -110.000,180.000 -160.000,120.000 -140.000,30.000 -60.000,0.000 0.000,0.000$" "$work/log" \
    || fail "half a circuit square to its axis did not close as its mirror image: $(grep '^points:' "$work/log")"
[ "$(grep '^profile:' "$work/log" | cut -d' ' -f2 | tr '\n' '/')" = "Asphalt|Off-road/Off-road|Asphalt/" ] || fail "the shoulder surface did not change sides with Mirror and back with Reverse: $(grep '^profile:' "$work/log")"
grep -q "^points: 8 0.000,0.000 60.000,0.000 140.000,30.000 160.000,120.000 110.000,180.000 150.000,260.000 60.000,300.000 0.000,300.000$" "$work/log" \
    || fail "three undos did not bring the half back (each operation one step): $(grep '^points:' "$work/log" | tail -1)"
# a selection with a gap, or Paste before any Copy, is refused with the reason
"$editor" "$work/half" --window 960x1200 --user "$work/hu" --frames 2 --steps row:1,cmd-row:3,copy >"$work/log" 2>&1 && fail "Copy of points with a gap went"
grep -q "Copy wants a run of points without gaps" "$work/log" || fail "Copy's refusal of a gap does not say why: $(cat "$work/log")"
"$editor" "$work/half" --window 960x1200 --user "$work/hu" --frames 2 --steps paste >"$work/log" 2>&1 && fail "Paste with nothing copied went"
grep -q "copy a run of points first" "$work/log" || fail "Paste's refusal does not say why: $(cat "$work/log")"
# the last point dragged 2 m from the start lands on it; 40 m away it stays where it is let go
"$editor" "$work/half" --window 960x1200 --user "$work/hu" --frames 2 --steps drag-near:7,0,2,print,undo,drag-near:7,0,40,print >"$work/log" 2>&1 || fail "dragging the end near the start: $(cat "$work/log")"
[ "$(grep '^points:' "$work/log" | head -1 | awk '{print $NF}')" = "0.000,0.000" ] || fail "the end dragged 2 m from the start did not land on it: $(grep '^points:' "$work/log")"
[ "$(grep '^points:' "$work/log" | tail -1 | awk '{print $NF}')" != "0.000,0.000" ] || fail "the end 40 m from the start was pulled onto it"

# ==== Add curve and Add straight (dep88): after the last point or the selected one, going on the road's way; one undo; red under Build's radius ====
# a straight north of 4 points 100 m apart. A quarter right of radius 60 m after the last point ends 60 m on and 60 m east (6 points, the
# default: 15 degrees each); after point 1 the road beyond turns with it and runs east; radius 15 m is red in Problems at once, in Build's words
mkdir "$work/curve"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 100.0], [0.0, 200.0], [0.0, 300.0]]\n}\n' > "$work/curve/trace.json"
"$editor" "$work/curve" --window 960x1200 --user "$work/cu" --frames 2 --steps curve:1,90,60,0,status,print-sel,print,undo,print,row:1,curve:1,90,60,6,print,problems,undo,curve:1,90,15,6,problems,status,undo,straight:250,0,print >"$work/log" 2>&1 \
    || fail "Add curve and Add straight: $(cat "$work/log")"
grep -q "^status: curve added: 94 m, 6 points after point 3$" "$work/log" || fail "Add curve did not say what it added: $(grep '^status:' "$work/log")"
grep -q "^selected: 3-9$" "$work/log" || fail "the new curve (from the point it went after) is not the selection: $(grep '^selected:' "$work/log")"
python3 -c "
import sys
rows = [[tuple(map(float, q.split(','))) for q in l.split()[2:]] for l in open(sys.argv[1]) if l.startswith('points: ')]
assert len(rows) == 4, len(rows)
end = rows[0]
assert len(end) == 10 and end[-1] == (60.0, 360.0) and all(abs(((x - 60) ** 2 + (z - 300) ** 2) ** 0.5 - 60) < 0.01 for x, z in end[3:]), ('at the end', end)
assert rows[1] == [(0.0, 0.0), (0.0, 100.0), (0.0, 200.0), (0.0, 300.0)], ('one undo', rows[1])
mid = rows[2]
assert mid[7] == (60.0, 160.0) and mid[8] == (160.0, 160.0) and mid[9] == (260.0, 160.0), ('in the middle: the road after turns east', mid)
assert rows[3][-1] == (0.0, 550.0) and len(rows[3]) == 7, ('a straight of 250 m in 3', rows[3])
" "$work/log" || fail "the curve or the straight did not go where they say: $(grep '^points:' "$work/log")"
grep -q "^problems: red" "$work/log" && grep -q "radius 15 m (under 20.9 m): not built" "$work/log" || fail "a curve of radius 15 m was not red at once, in Build's words: $(grep '^problems:\|^status:' "$work/log")"

# the rows are a folding block: folded to begin with when the Points table would be left under 4 rows (the default layout, at any window size);
# a click on its header (curve-fold) unfolds it and the choice stays; the fields are there to Accept either way
"$editor" "$work/curve" --user "$work/cu2" --window 960x600 --frames 12 --steps curve-block,curve-fold:1,curve-block,curve-fold:0,curve-block >"$work/log" 2>&1 || fail "the Add curve block: $(cat "$work/log")"
[ "$(grep '^curve block' "$work/log" | tr '\n' '|')" = "curve block: folded|curve block: open|curve block: folded|" ] || fail "the Add curve block does not start folded and follow the header: $(grep '^curve block' "$work/log")"
"$editor" "$work/curve" --user "$work/cu3" --window 960x600 --frames 12 --steps curve-block,curve:1,90,60,0,status >"$work/log" 2>&1 || fail "Add curve with the block folded: $(cat "$work/log")"
grep -q "^status: curve added" "$work/log" || fail "Add curve does not work with the block folded: $(grep '^status:' "$work/log")"

# ==== Colour by (dep95): the road coloured by what each stretch is; a double-click on the road selects its stretch ====
# a road that goes straight 300 m, turns right round a U and comes back south: curve mode finds straight, (the corner's wobble to the left), right, straight
mkdir "$work/cs"
printf '{\n  "version": 1,\n  "width": 40.0,\n  "points": [[0,0],[0,100],[0,200],[0,300],[30,390],[100,430],[190,420],[260,360],[280,260],[280,160],[280,60]]\n}\n' > "$work/cs/trace.json"
# a double-click on a mark in the strip's profile takes the top view to that point (point 4 of this road is at 30, 390) and zooms in, never out; a click alone moves nothing
"$editor" "$work/cs" --user "$work/cs-p" --window 960x1200 --frames 26 --steps strip,strip-size:370,print-view,strip-dblclick:4,print-view >"$work/log" 2>&1 || fail "strip double-click: $(cat "$work/log")"
python3 - "$work/log" <<'PY' || fail "the strip's double-click did not centre the top view on point 4 at a working scale: $(grep '^view' "$work/log")"
import sys
v = [l.split()[1:] for l in open(sys.argv[1]) if l.startswith("view:")]
assert len(v) == 2, v
before, after = [[float(x) for x in r] for r in v]
assert before != after
assert abs(after[0] - 30) < 3 and abs(after[1] - 390) < 3 and after[2] >= 1.0, after   # a mark is a few px wide: the place under the press is near the point
PY
# the slope tool's preview: the run it is for is a teal band over the road in the Viewport (nothing without the preview), and the profile draws the plan (blue)
"$editor" "$work/cs" --user "$work/cs-p" --window 960x1200 --frames 26 --steps row:2,shift-row:6,strip,strip-size:370,slope-preview:40 --shot "$work/slope-run.png" >"$work/log" 2>&1 || fail "slope preview: $(cat "$work/log")"
png_check "$work/slope-run.png" slope-run
"$editor" "$work/cs" --user "$work/cs-p" --window 960x1200 --frames 26 --steps row:2,shift-row:6,strip,strip-size:370 --shot "$work/slope-none.png" >"$work/log" 2>&1 || fail "no slope preview: $(cat "$work/log")"
png_check "$work/slope-none.png" slope-none
# the Distance strip's profile line is coloured by the same classes (the right turn orange); in Limits it is not (their own --user: the saved window layout of a 960x1200 run would move the 1280x960 runs' clicks)
"$editor" "$work/cs" --user "$work/cs-p" --window 960x1200 --frames 26 --steps strip,strip-size:370,colour-by:1 --shot "$work/profile-curve.png" >"$work/log" 2>&1 || fail "profile in Colour by curve: $(cat "$work/log")"
png_check "$work/profile-curve.png" profile-curve
"$editor" "$work/cs" --user "$work/cs-p" --window 960x1200 --frames 26 --steps strip,strip-size:370 --shot "$work/profile-limits.png" >"$work/log" 2>&1 || fail "profile in Limits: $(cat "$work/log")"
png_check "$work/profile-limits.png" profile-limits
"$editor" "$work/cs" --user "$work/cs-u" --window 1280x960 --frames 12 --steps colour-by:1,print-classes,dblclick-road:280,200,print-sel,status >"$work/log" 2>&1 || fail "Colour by curve: $(cat "$work/log")"
python3 - "$work/log" <<'PY' || fail "curve mode did not find a straight, the corner, the right turn and a straight: $(grep '^classes' "$work/log")"
import sys, re
line = [l for l in open(sys.argv[1]) if l.startswith('classes 1:')][0]
runs = [tuple(map(int, re.split('[@-]', t))) for t in line.split(':')[1].split()]
assert [r[0] for r in runs] == [0, 1, 2, 0], runs
assert runs[0][1] == 0 and all(a[2] + 1 == b[1] for a, b in zip(runs, runs[1:])), ('runs are not in order and whole', runs)
assert runs[2][2] - runs[2][1] > 80, ('the right turn is the long run', runs)
PY
grep -q "^selected: 3-9$" "$work/log" || fail "a double-click on the road after the turn did not select its stretch (points 3 to 9): $(grep '^selected' "$work/log")"
grep -q "^status: stretch selected: points 3 to 9$" "$work/log" || fail "the stretch does not say which points: $(grep '^status' "$work/log")"
# Shift or Cmd on the double-click adds the stretch to the selection, which stays; a plain one replaces it
cs_run() { "$editor" "$work/cs" --user "$work/cs-u" --window 1280x960 --frames 12 --steps "colour-by:1,$1" 2>&1 | grep -E '^(selected|status):' | tr '\n' '|'; }
got=$(cs_run "dblclick-road:0,150,print-sel,shift-dblclick:280,200,print-sel,status")
[ "$got" = "selected: 0-1|selected: 0-1, 3-9|status: stretch added: points 3 to 9|" ] || fail "a Shift double-click on a second stretch did not add it to the first: $got"
got=$(cs_run "dblclick-road:280,200,print-sel,cmd-dblclick:0,150,print-sel,status")
[ "$got" = "selected: 3-9|selected: 0-1, 3-9|status: stretch added: points 0 to 1|" ] || fail "a Cmd double-click on a second stretch did not add it to the first: $got"
got=$(cs_run "click:5,dblclick-road:280,200,print-sel")
[ "$got" = "selected: 3-9|" ] || fail "a plain double-click on a stretch did not replace the selection with it: $got"
# in Limits mode a double-click on the road selects nothing (the first click clears); a click on a point still picks it
"$editor" "$work/cs" --user "$work/cs-u" --window 1280x960 --frames 12 --steps colour-by:0,dblclick-road:280,200,print-sel,click:9,print-sel >"$work/log" 2>&1 || fail "Limits mode: $(cat "$work/log")"
[ "$(grep '^selected:' "$work/log" | tr '\n' '|')" = "selected: none|selected: 9|" ] || fail "Limits mode: a double-click on the road must select nothing, a click a point: $(grep '^selected' "$work/log")"
# the bank key's stretch (30 degrees is the top class), and a mode with nothing keyed is one run
mkdir "$work/cbank"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 2, "bank_deg": 30.0}, {"point": 4, "bank_deg": 0.0}]\n}\n' > "$work/cbank/trace.json"
"$editor" "$work/cbank" --user "$work/cs-u" --window 1280x960 --frames 12 --steps colour-by:2,print-classes,dblclick-road:0,150,print-sel,colour-by:3,print-classes,colour-by:5,print-classes >"$work/log" 2>&1 || fail "Colour by bank: $(cat "$work/log")"
grep -q "^classes 2: 0@0-2 1@3-7 2@8-11 3@12-26 2@27-30 1@31-36 0@37-48$" "$work/log" || fail "the bank ramp is not up and down the classes: $(grep '^classes 2' "$work/log")"
grep -q "^selected: 2$" "$work/log" || fail "a double-click on the banked stretch did not select its point: $(grep '^selected' "$work/log")"
grep -q "^classes 3: 1@0-48$" "$work/log" && grep -q "^classes 5: 0@0-48$" "$work/log" || fail "the width and shoulder modes of a plain road are not one run each: $(grep '^classes' "$work/log")"
# Colour by grade (dep95 step 5): a hill of 15 m over 60 m is steep up and steep down (3 and 6), level before, between and after
mkdir "$work/cgrade"
printf '{\n  "version": 5,\n  "width": 40.0,\n  "points": [[0.0, 0.0, 0.0], [0.0, 60.0, 0.0], [0.0, 120.0, 15.0], [0.0, 180.0, 15.0], [0.0, 240.0, 0.0], [0.0, 300.0, 0.0]]\n}\n' > "$work/cgrade/trace.json"
"$editor" "$work/cgrade" --user "$work/cs-u" --window 1280x960 --frames 12 --steps colour-by:7,print-classes >"$work/log" 2>&1 || fail "Colour by grade: $(cat "$work/log")"
grep -q "^classes 7: 0@0-9 2@10-10 3@11-17 2@18-18 0@19-28 5@29-29 6@30-36 5@37-37 0@38-48$" "$work/log" || fail "the hill is not up and down the grade classes: $(grep '^classes 7' "$work/log")"
# a mode out of range is refused
"$editor" "$work/cs" --window 960x1200 --user "$work/cs-u" --frames 2 --steps colour-by:99 >"$work/log" 2>&1 && fail "colour-by:99 was taken"

# ==== Build: under the limits, over an amber one, refused over a red one ====
# Build: a curve under the limits is built, one over them is refused with the limit named
"$editor" "$work/mild" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a curve under the limits: $(cat "$work/log")"
[ -s "$work/mild/build/mild.zip" ] || fail "no zip of the mild S"
"$editor" "$work/warn" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a curve that is only over an amber limit: $(cat "$work/log")"
[ -s "$work/warn/build/warn.zip" ] || fail "no zip of the amber curve"
"$editor" "$work/bad" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a curve over a red limit"
grep -q "tighter than 20.9 m\|more than 16.5 degrees" "$work/log" || fail "the refusal does not name the red limit: $(cat "$work/log")"
# the bank (version 3 keys): 30 degrees is amber and builds; 40 is red, refused with the limit named, as a red curve is
mkdir "$work/banked" "$work/overbanked"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 60.0], [0.0, 120.0], [0.0, 180.0], [0.0, 240.0], [0.0, 300.0]],\n  "keys": [{"point": 2, "bank_deg": 30.0}, {"point": 4, "bank_deg": 0.0}]\n}\n' > "$work/banked/trace.json"
sed 's/"bank_deg": 30.0/"bank_deg": 40.0/' "$work/banked/trace.json" > "$work/overbanked/trace.json"
"$editor" "$work/banked" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a road banked 30 degrees (amber): $(cat "$work/log")"
[ -s "$work/banked/build/banked.zip" ] || fail "no zip of the road banked 30 degrees"
"$editor" "$work/overbanked" --frames 2 --build >"$work/log" 2>&1 && fail "Build went on a road banked 40 degrees"
grep -q "banks more than 35 degrees" "$work/log" || fail "the bank's refusal does not name the limit: $(cat "$work/log")"
# a new project is born a stage the game races: a straight of the shortest stage's length, built with nothing too short
mkdir "$work/fresh"
"$editor" "$work/fresh" --frames 2 --build >"$work/log" 2>&1 || fail "Build refused a new project: $(cat "$work/log")"
[ -s "$work/fresh/build/fresh.zip" ] || fail "no zip of a new project"
grep -q "too short" "$work/log" && fail "a new project is too short for a stage: $(cat "$work/log")"
python3 -c "
import json, sys
p = json.load(open(sys.argv[1]))['points']
gaps = [b[1] - a[1] for a, b in zip(p, p[1:])]
assert p[0] == [0.0, 0.0] and all(q[0] == 0.0 for q in p) and max(gaps) <= 100 and min(gaps) > 0 and p[-1][1] > 1290, p
" "$work/fresh/trace.json" || fail "a new project is not a straight of over 1.29 km with a point every 100 m at most"

# ==== Install: in the background (a slow installer), the game not where it is said or not found ====
# the game's installer runs while the window goes on; --install waits for it, says where it went and prints the installer's own line
printf '#!/bin/sh\nuser=; zip=\nwhile [ $# -gt 0 ]; do case "$1" in --user) user=$2; shift;; --install) zip=$2; shift;; esac; shift; done\nsleep 1\nmkdir -p "$user/mods/$(basename "$zip" .zip)" && echo "installed $(basename "$zip" .zip) 0.1"\n' > "$work/slow-tnfs"
chmod +x "$work/slow-tnfs"
"$editor" "$work/fresh" --frames 2 --build --install --game "$work/slow-tnfs" --user "$work/u-slow" >"$work/log" 2>&1 || fail "install with a slow installer: $(cat "$work/log")"
grep -q "installed fresh 0.1" "$work/log" && grep -q "installed into" "$work/log" || fail "the slow install does not say it installed: $(cat "$work/log")"
[ -d "$work/u-slow/mods/fresh" ] || fail "the slow install made no mods/fresh"
# the game is not where --game says: Install says so, and what to do, without running anything
out=$("$editor" "$work/proj" --frames 2 --install --game "$work/no-such-game" --user "$work/u2" 2>&1) && fail "install with a game that is not there went"
echo "$out" | grep -q "copy Spirited Drive and Spirited Drive Studio to Applications" || fail "no hint about the game's place: $out"
# and with no --game at all, from a folder with no ./tnfs (skipped where the game really is in /Applications)
if [ ! -x "/Applications/Spirited Drive.app/Contents/MacOS/tnfs" ] && [ ! -x "$(dirname "$editor")/../../../Spirited Drive.app/Contents/MacOS/tnfs" ]; then
    out=$(cd "$work" && "$editor" "$work/proj" --frames 2 --install 2>&1) && fail "install with no game found went"
    echo "$out" | grep -q "Applications" || fail "no hint with no game found: $out"
fi

# ==== Test drive: refused with the Problems' text, never into the player's folder (the game's own run is editor_e2e_track's) ====
"$editor" "$work/bad" --frames 2 --test-drive --game "$work/no-such-game" --user "$work/td" >"$work/log" 2>&1 && fail "Test drive went on a curve over a red limit"
grep -q "fix the red problems first" "$work/log" && grep -q "under 20.9 m\|more than 16.5" "$work/log" || fail "Test drive's refusal does not name the red limit: $(cat "$work/log")"
"$editor" "$work/mild" --frames 2 --test-drive --game "$work/no-such-game" --user "$work/td" >"$work/log" 2>&1 && fail "Test drive went on a road too short for a stage"
grep -q "too short for a stage" "$work/log" || fail "Test drive's refusal does not say the road is too short: $(cat "$work/log")"
"$editor" "$work/fresh" --frames 2 --test-drive --race-off reflections --race-scale 1 --game "$work/no-such-game" --user "$work/td" >"$work/log" 2>&1 && fail "Test drive went with no game"
grep -q "the game was not found" "$work/log" || fail "Test drive's refusal does not say the game is missing: $(cat "$work/log")"
[ -d "$work/td/mods" ] && fail "Test drive put something in the player's mods folder"
"$editor" "$work/fresh" --frames 2 --test-drive --race-scale 3 >/dev/null 2>&1 && fail "--race-scale 3 was taken with --test-drive"
"$editor" "$work/fresh" --frames 2 --race-off reflections >/dev/null 2>&1 && fail "--race-off was taken with neither --race-photo nor --test-drive"
"$editor" "$work/fresh" --frames 6 --shot "$work/drive.png" >"$work/log" 2>&1 || fail "run with the Drive window: $(cat "$work/log")"
[ -s "$work/drive.png" ] || fail "no PNG with the Drive window"

# ==== dep87 part 2: an edit changes only what is selected ====
# a straight of 10 points 40 m apart with a 2+1 lanes key on point 7; print-road prints what the keys make at every point (lanes 17 = 1+1, 18 = 1+2, 33 = 2+1)
mkdir "$work/scope"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 40.0], [0.0, 80.0], [0.0, 120.0], [0.0, 160.0], [0.0, 200.0], [0.0, 240.0], [0.0, 280.0], [0.0, 320.0], [0.0, 360.0]],\n  "keys": [{"point": 7, "lanes": "2+1"}]\n}\n' > "$work/scope/trace.json"
road_lanes() { grep '^road ' "$work/log" | sed -e 's/^road \([0-9]*\):.* lanes \([0-9]*\) shoulder.*/\1=\2/' | tr '\n' ' '; }
run_scope() { "$editor" "$work/scope" --user "$work/scope-u" --frames 2 --steps "$1" >"$work/log" 2>&1 || fail "scope $1: $(cat "$work/log")"; road_lanes; }
got=$(run_scope "row:3,lanes:3,1,print-road")
[ "$got" = "0=17 1=17 2=17 3=18 4=17 5=17 6=17 7=33 8=33 9=33 " ] || fail "a lanes edit on one point runs on (got: $got)"
got=$(run_scope "row:3,shift-row:5,lanes:4,2,print-road")
[ "$got" = "0=17 1=17 2=17 3=33 4=33 5=33 6=17 7=33 8=33 9=33 " ] || fail "a lanes edit on points 3-5 is not on 3-5 alone (got: $got)"
got=$(run_scope "row:1,cmd-row:4,cmd-row:8,lanes:4,1,print-road")
[ "$got" = "0=17 1=18 2=17 3=17 4=18 5=17 6=17 7=33 8=18 9=33 " ] || fail "a lanes edit on points 1, 4 and 8 is not on each of them alone (got: $got)"
got=$(run_scope "lanes:3,1,extend:3,2,print-road")
[ "$got" = "0=17 1=17 2=17 3=18 4=18 5=18 6=18 7=33 8=33 9=33 " ] || fail "Extend to next change does not run the value on to the next key (got: $got)"
got=$(run_scope "lanes:3,1,extend-end:3,2,print-road")
[ "$got" = "0=17 1=17 2=17 3=18 4=18 5=18 6=18 7=18 8=18 9=18 " ] || fail "Extend to the end does not run the value on to the last point (got: $got)"
got=$(run_scope "lanes:3,1,undo,print-road")
[ "$got" = "0=17 1=17 2=17 3=17 4=17 5=17 6=17 7=33 8=33 9=33 " ] || fail "one undo does not take a scoped edit back whole (got: $got)"
# Copy profile / Paste profile: point 2's lanes (1+2) and left shoulder (6 m) put on points 6-7; points 4 and 8 give back what held before (1+1, 3 m), one undo
mkdir "$work/scope2"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 40.0], [0.0, 80.0], [0.0, 120.0], [0.0, 160.0], [0.0, 200.0], [0.0, 240.0], [0.0, 280.0], [0.0, 320.0], [0.0, 360.0]],\n  "keys": [{"point": 2, "lanes": "1+2", "shoulder_l_m": 6.0}, {"point": 4, "lanes": "1+1", "shoulder_l_m": 3.0}]\n}\n' > "$work/scope2/trace.json"
"$editor" "$work/scope2" --user "$work/scope-u" --frames 2 --steps row:2,copy-profile,status,row:6,shift-row:7,paste-profile,status,print-road,undo,print-road >"$work/log" 2>&1 || fail "copy and paste profile: $(cat "$work/log")"
grep -q "status: profile of point 2 copied" "$work/log" || fail "Copy profile did not say what it copied: $(cat "$work/log")"
grep -q "status: profile pasted on points 6-7" "$work/log" || fail "Paste profile did not say where it pasted: $(cat "$work/log")"
python3 - "$work/log" <<'PY' || fail "Paste profile did not put the profile on points 6-7 alone, or one undo did not take it back"
import re, sys
rows = [re.match(r"road (\d+): width [\d.]+ bank [-\d.]+ lanes (\d+) shoulder ([\d.]+)\|", l) for l in open(sys.argv[1]) if l.startswith("road ")]
lanes = [int(m.group(2)) for m in rows]
shoulder = [float(m.group(3)) for m in rows]
after, undone = (lanes[:10], shoulder[:10]), (lanes[10:], shoulder[10:])
assert after[0] == [17, 17, 18, 18, 17, 17, 18, 18, 17, 17], after
assert shoulder[:10][5] == 3.0 and shoulder[:10][6] == 6.0 and shoulder[:10][7] == 6.0 and shoulder[:10][8] == 3.0, shoulder[:10]
assert undone[0] == [17, 17, 18, 18, 17, 17, 17, 17, 17, 17], undone
assert shoulder[10:][6] != 6.0, shoulder[10:]
PY
"$editor" "$work/scope2" --user "$work/scope-u" --frames 2 --steps row:6,paste-profile >"$work/log" 2>&1 && fail "Paste profile with nothing copied went"
grep -q "no profile copied" "$work/log" || fail "Paste profile before any copy did not say nothing was copied: $(cat "$work/log")"
# The Road profile of several selected points: a value they do not share is a dash («Mixed» in print-mixed), one they share is shown; setting one value sets only
# it on all of them. Points 3-5 have left shoulders of 3, 6 and 9 m and widths of 10, 12 and 14 m; the bank they share (4 degrees)
mkdir "$work/mixed"
printf '{\n  "version": 3,\n  "width": 40.0,\n  "points": [[0.0, 0.0], [0.0, 40.0], [0.0, 80.0], [0.0, 120.0], [0.0, 160.0], [0.0, 200.0], [0.0, 240.0], [0.0, 280.0]],\n  "keys": [{"point": 3, "width_m": 10.0, "shoulder_l_m": 3.0, "bank_deg": 4.0}, {"point": 4, "width_m": 12.0, "shoulder_l_m": 6.0, "bank_deg": 4.0}, {"point": 5, "width_m": 14.0, "shoulder_l_m": 9.0, "bank_deg": 4.0}, {"point": 6, "width_m": 10.0, "shoulder_l_m": 3.0, "bank_deg": 4.0}]\n}\n' > "$work/mixed/trace.json"
"$editor" "$work/mixed" --user "$work/scope-u" --frames 2 --steps row:3,shift-row:5,print-mixed,side:3,0,0,5,print-mixed,width:4,11,print-mixed,print-road >"$work/log" 2>&1 || fail "mixed profile: $(cat "$work/log")"
[ "$(grep '^mixed:' "$work/log" | sed -n 1p)" = "mixed: width shoulder-l" ] || fail "points with different widths and left shoulders do not report both as mixed: $(grep '^mixed:' "$work/log")"
[ "$(grep '^mixed:' "$work/log" | sed -n 2p)" = "mixed: width" ] || fail "after the shoulder is set on all three it is still mixed: $(grep '^mixed:' "$work/log")"
[ "$(grep '^mixed:' "$work/log" | sed -n 3p)" = "mixed:" ] || fail "after the width is set on all three it is still mixed: $(grep '^mixed:' "$work/log")"
python3 - "$work/log" <<'PY' || fail "setting one value on several points did not set it on all of them and nothing else"
import re, sys
rows = [re.match(r"road (\d+): width ([\d.]+) bank ([-\d.]+) lanes (\d+) shoulder ([\d.]+)\|", l) for l in open(sys.argv[1]) if l.startswith("road ")]
width = [float(m.group(2)) for m in rows]
bank = [float(m.group(3)) for m in rows]
shoulder = [float(m.group(5)) for m in rows]
assert shoulder[3:6] == [5.0, 5.0, 5.0], shoulder
assert width[3:6] == [11.0, 11.0, 11.0], width
assert bank[3:6] == [4.0, 4.0, 4.0], bank   # the bank was not touched
PY
# a ramped value: a width on one point rises to it and falls after it; on a range it holds over the range
"$editor" "$work/scope" --user "$work/scope-u" --frames 2 --steps print-road,width:5,14,print-road >"$work/log" 2>&1 || fail "scope width: $(cat "$work/log")"
python3 - "$work/log" <<'PY' || fail "a width edit on one point is not on that point alone"
import re, sys
rows = [re.match(r"road (\d+): width ([\d.]+)", l) for l in open(sys.argv[1]) if l.startswith("road ")]
w = [float(m.group(2)) for m in rows]
before, after = w[:10], w[10:]
assert after[5] == 14.0, after
assert [i for i in range(10) if after[i] != before[i]] == [5], (before, after)
PY
"$editor" "$work/scope" --user "$work/scope-u" --frames 2 --steps row:3,shift-row:6,width:3,12,print-road >"$work/log" 2>&1 || fail "scope width range: $(cat "$work/log")"
python3 - "$work/log" <<'PY' || fail "a width edit on points 3-6 is not on 3-6 alone"
import re, sys
w = [float(re.match(r"road (\d+): width ([\d.]+)", l).group(2)) for l in open(sys.argv[1]) if l.startswith("road ")]
assert w[3:7] == [12.0] * 4, w
assert w[2] != 12.0 and w[7] != 12.0 and w[0] == w[2] == w[7] == w[9], w
PY

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
# ==== Window menu: every panel the frame draws is listed, each item closes and reopens it, the closed ones are kept in the ini ====
panels=$("$editor" "$work/warn" --user "$work/u-win" --window 1280x960 --frames 6 --steps print-panels,panel:3,print-panels,panel:3,print-panels,panel:9 2>&1)
[ "$(echo "$panels" | grep -c '^panel [0-9]*: .*: open, drawn, docked$')" = 32 ] || fail "the track's panels are not 11 open and drawn, 10 after closing one, 11 after opening it: $panels"
[ "$(echo "$panels" | grep -c '^unlisted:$')" = 3 ] || fail "a panel is drawn that the Window menu does not list: $(echo "$panels" | grep '^unlisted')"
echo "$panels" | grep -q '^panel 3: Points: closed, not drawn, docked$' || fail "Window > Points did not close the panel: $panels"
echo "$panels" | grep -q '^panel 9: Drive: ' || fail "the Drive panel is not in the Window menu: $panels"
grep -q '^Closed=Drive$' "$work/u-win/track-editor.ini" || fail "a closed panel is not kept in the ini: $(cat "$work/u-win/track-editor.ini")"
panels=$("$editor" "$work/warn" --user "$work/u-win" --window 1280x960 --frames 6 --steps print-panels,panel:9,print-panels 2>&1)
echo "$panels" | grep -q '^panel 9: Drive: closed, not drawn' || fail "the panel closed in the last run is open again: $panels"
echo "$panels" | tail -11 | grep -q '^panel 9: Drive: open, drawn, docked$' || fail "Window > Drive did not open the panel again, docked: $panels"
[ "$failures" -eq 0 ] && echo "editor_headless_track: ok"
exit "$failures"
