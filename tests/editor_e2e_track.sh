#!/bin/sh
# A track from the Studio to the game, the happy path a user takes: a new project, a point added and saved, an edit undone and redone with
# the window's unsaved mark following, Build, Install into the player's folder, and Test drive (the Drive window's button): the game runs on
# the piece from a scratch player folder and names it in its log. Needs the game's files (original/).
# usage: editor_e2e_track.sh EDITOR TNFS ORIGINAL_DIR
. "$(dirname "$0")/editor_headless_common.sh"
tnfs=$2
orig=$3
[ -f "$orig/SIMDATA/MISC/AL1.TRI" ] || { echo "skip: no game files in $orig"; exit 0; }
export SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software
player="$work/player"
mkdir -p "$player" && echo "profile=restomod" > "$player/port.cfg"   # the profile that loads mods
points() { python3 -c "import json, sys; print(len(json.load(open(sys.argv[1]))['points']))" "$work/loop/trace.json"; }

# 1. New project
"$editor" "$work/loop" --user "$player" --frames 2 --save >"$work/log" 2>&1 || fail "New project: $(cat "$work/log")"
[ -f "$work/loop/trace.json" ] || { fail "New project: no trace.json"; exit "$failures"; }
first=$(points)

# 2. A point added, then saved
"$editor" "$work/loop" --user "$player" --frames 2 --edit --save >"$work/log" 2>&1 || fail "a point added and saved: $(cat "$work/log")"
[ "$(points)" -eq $((first + 1)) ] || fail "the point added is not in trace.json ($first points before, $(points) after)"

# 2b. Edit, undo, redo, save: the window's title has the unsaved mark after an edit, not after its undo, again after the redo, not after
# Save, and again after an undo past the save; trace.json has what was saved
before=$(points)
"$editor" "$work/loop" --user "$player" --frames 2 --steps title,edit,title,undo,title,redo,title,save,title,undo,title >"$work/log" 2>&1 \
    || fail "edit, undo, redo, save: $(cat "$work/log")"
grep '^title: ' "$work/log" | sed 's/^title: Spirited Drive Studio .* loop/loop/' > "$work/titles"
printf 'loop\nloop *\nloop\nloop *\nloop\nloop *\n' | cmp -s - "$work/titles" || fail "the unsaved mark in the title: $(tr '\n' '|' < "$work/titles")"
[ "$(points)" -eq $((before + 1)) ] || fail "Save after a redo did not write the point redone ($before points before, $(points) after)"

# 2c. The window, as a user: N adds a point at the end; a click takes point 2 and a drag moves it along the road, one step to undo (the undo gives back the
# point exactly); Del removes it and an undo brings it back; a road key's width set as the Points window sets it (8 m at point 1) and a bank cell of the Road profile (5 degrees
# at point 3), saved
"$editor" "$work/loop" --user "$player" --frames 2 --steps print,key:N,print,click:2,drag:2,0,-5,print,undo,print,redo,print,key:Delete,print,undo,print,width:1,8,bank:3,5,save \
    >"$work/log" 2>&1 || fail "the window's edits: $(cat "$work/log")"
grep '^points: ' "$work/log" > "$work/prints"
line() { sed -n "${1}p" "$work/prints"; }
count() { line "$1" | cut -d' ' -f2; }
[ "$(sed -n '$=' "$work/prints")" = 7 ] || fail "not every print came: $(cat "$work/prints")"
[ "$(count 2)" -eq $(($(count 1) + 1)) ] || fail "N did not add a point: $(line 1) / $(line 2)"
[ "$(line 3)" != "$(line 2)" ] || fail "the drag did not move point 2: $(line 3)"
[ "$(line 4)" = "$(line 2)" ] || fail "one undo did not take the whole drag back: $(line 2) / $(line 4)"
[ "$(line 5)" = "$(line 3)" ] || fail "redo did not give the drag back: $(line 3) / $(line 5)"
[ "$(count 6)" -eq $(($(count 5) - 1)) ] || fail "Del did not remove the point taken: $(line 5) / $(line 6)"
[ "$(line 7)" = "$(line 5)" ] || fail "undo did not bring the removed point back: $(line 5) / $(line 7)"
python3 -c "import json, sys; k = json.load(open(sys.argv[1])).get('keys', []); sys.exit(0 if any(x.get('point') == 1 and x.get('width_m') == 8.0 for x in k) else 1)" \
    "$work/loop/trace.json" || fail "the width set on point 1 is not in trace.json: $(cat "$work/loop/trace.json")"
python3 -c "import json, sys; k = json.load(open(sys.argv[1])).get('keys', []); sys.exit(0 if any(x.get('point') == 3 and x.get('bank_deg') == 5.0 for x in k) else 1)" \
    "$work/loop/trace.json" || fail "the bank set on point 3 is not in trace.json: $(cat "$work/loop/trace.json")"

# 2c2. Heights (dep95): a height set on points 1 and 2 as the Points window's h cell and the profile set it (a tenth of a metre) is saved as trace.json version 5 with
# [x, z, h] points; put back to 0 the project is saved as it was before heights (no triples)
"$editor" "$work/hill" --user "$player" --frames 2 --save >"$work/log" 2>&1 || fail "New project (hill): $(cat "$work/log")"
"$editor" "$work/hill" --user "$player" --frames 2 --steps height:1,6,height:2,12.34,save >"$work/log" 2>&1 || fail "heights set in the window: $(cat "$work/log")"
python3 -c "
import json, sys
t = json.load(open(sys.argv[1])); p = t['points']
sys.exit(0 if t['version'] == 5 and len(p[1]) == 3 and p[1][2] == 6.0 and p[2][2] == 12.3 and len(p[0]) == 3 and p[0][2] == 0 else 1)" \
    "$work/hill/trace.json" || fail "the heights are not in trace.json as version 5 triples: $(cat "$work/hill/trace.json")"
"$editor" "$work/hill" --user "$player" --frames 2 --steps height:1,0,height:2,0,save >"$work/log" 2>&1 || fail "heights put back to 0: $(cat "$work/log")"
python3 -c "
import json, sys
t = json.load(open(sys.argv[1]))
sys.exit(0 if t['version'] < 5 and all(len(q) == 2 for q in t['points']) else 1)" \
    "$work/hill/trace.json" || fail "a flat project is not saved as before heights: $(cat "$work/hill/trace.json")"

# 2c3. The profile as a user drags it (dep95): the Distance strip's tab to the front, a press on point 2's mark, a move 24 pixels up, a release: the height has
# changed (up is higher), it is one step to undo (the undo gives the height back exactly) and a redo gives the drag back
"$editor" "$work/hill2" --user "$player" --frames 2 --save >"$work/log" 2>&1 || fail "New project (hill2): $(cat "$work/log")"
"$editor" "$work/hill2" --user "$player" --window 1280x800 --frames 2 --steps height:2,10,strip,print-height:2,profile-drag:2,-24,print-height:2,undo,print-height:2,redo,print-height:2 \
    >"$work/log" 2>&1 || fail "the profile's drag: $(cat "$work/log")"
grep '^height: 2 ' "$work/log" | cut -d' ' -f3 > "$work/heights"
[ "$(sed -n '$=' "$work/heights")" = 4 ] || fail "not every height came: $(cat "$work/heights")"
h() { sed -n "${1}p" "$work/heights"; }
[ "$(h 1)" = 10.0 ] || fail "the start height is not 10: $(h 1)"
awk -v a="$(h 1)" -v b="$(h 2)" 'BEGIN { exit !(b > a) }' || fail "dragging the mark up did not raise the height: $(h 1) -> $(h 2)"
[ "$(h 3)" = "$(h 1)" ] || fail "one undo did not give the height back: $(h 1) / $(h 3)"
[ "$(h 4)" = "$(h 2)" ] || fail "redo did not give the drag back: $(h 2) / $(h 4)"

# 2d. A point dragged too near the next (17 m before it, on a new project): the nodes bunch up with no red node; Problems says so in red in
# Build's words before Build, and Build refuses with those words
"$editor" "$work/near" --user "$player" --frames 2 --save >"$work/log" 2>&1 || fail "New project (near): $(cat "$work/log")"
"$editor" "$work/near" --user "$player" --frames 2 --steps problems,drag-near:2,3,17,problems,build >"$work/log" 2>&1 \
    || fail "the drag near the next point: $(cat "$work/log")"
[ "$(grep '^problems' "$work/log" | head -1)" = "problems: none" ] || fail "Problems was not clear before the drag: $(grep '^problems' "$work/log")"
refusal=$(sed -n 's/^problems: refusal //p' "$work/log")
case "$refusal" in *"units from the node before"*) ;; *) fail "Problems has no red line for the bunched nodes: $(grep '^problems' "$work/log")" ;; esac
grep -qxF "build: build refused: $refusal" "$work/log" || fail "Build refused with other words than Problems': $(grep '^build' "$work/log") / $refusal"

# 3. Build and 4. Install into the player's folder
"$editor" "$work/loop" --user "$player" --frames 2 --build --install --game "$tnfs" >"$work/log" 2>&1 || fail "Build and Install: $(cat "$work/log")"
grep -q "installed loop" "$work/log" || fail "Install does not say it installed the piece: $(cat "$work/log")"
[ -f "$player/mods/loop/mod.ini" ] || fail "the piece is not in the player's mods folder"
python3 - "$work/loop/build/loop.zip" <<'PY' || fail "the built TRI does not have the 8 m and the 5 degrees the keys set (the project's 10 m and no bank elsewhere)"
import struct, sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
d = z.read([n for n in z.namelist() if n.upper().endswith(".TRI")][0])
nodes = (0x1621C - 0x98C) // 36
verge = [d[0x98C + 36 * k] for k in range(nodes) if d[0x98C + 36 * k] != 0]   # byte 0 of each node: the road's half width in 1/8 m
assert min(verge) == 32 and max(verge) == 40, (min(verge), max(verge))
slant = [struct.unpack_from("<h", d, 0x98C + 36 * k + 0x16)[0] for k in range(nodes)]
bank = max(v for v in slant if v < 0x1000)   # +0x16 of each node: the slant, a 14-bit angle (0x4000 a turn); the 5 degrees at its peak
assert bank == round(5 * 0x4000 / 360), sorted(set(slant))
PY

# 5. Test drive: the editor builds, installs into its own scratch folder and runs the game on the piece; the game's log names it (the line is the editor's
# own launch's, printed by it), the game is stopped, and the player's folder is as it was
driver="$work/driver"
mkdir -p "$driver"
"$editor" "$work/loop" --user "$driver" --frames 2 --test-drive --game "$tnfs" --cd "$orig" >"$work/log" 2>&1 || fail "Test drive: $(cat "$work/log")"
grep -q "^track piece: loop is track [0-9]" "$work/log" || fail "Test drive does not print the game's line on the piece: $(cat "$work/log")"
grep -q "The game took loop as track" "$work/log" || fail "Test drive does not say the game took the piece: $(cat "$work/log")"
[ -d "$driver/mods" ] && fail "Test drive installed into the player's folder"
[ -f "$driver/spirited-drive.log" ] && fail "Test drive's game wrote its log in the player's folder"

[ "$failures" -eq 0 ] && echo "editor_e2e_track: ok"
exit "$failures"
