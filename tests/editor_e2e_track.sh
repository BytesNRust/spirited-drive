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
