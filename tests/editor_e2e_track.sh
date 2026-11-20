#!/bin/sh
# A track from the Studio to the game, the happy path a user takes: a new project, a point added and saved, an edit undone and redone with
# the window's unsaved mark following, Build, Install into the player's folder, and the game races the installed piece (no window) and
# names it in its log. Needs the game's files (original/).
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

# 3. Build and 4. Install into the player's folder
"$editor" "$work/loop" --user "$player" --frames 2 --build --install --game "$tnfs" >"$work/log" 2>&1 || fail "Build and Install: $(cat "$work/log")"
grep -q "installed loop" "$work/log" || fail "Install does not say it installed the piece: $(cat "$work/log")"
[ -f "$player/mods/loop/mod.ini" ] || fail "the piece is not in the player's mods folder"

# 5. The game races the installed piece and names it
"$tnfs" --data "$orig" --user "$player" --mods "$player/mods/loop" --drive loop --car 5 --racers 1 --node 40 --screenshot "$work/race.ppm" >"$work/log" 2>&1 \
    || fail "the game did not race the installed piece: $(tail -3 "$work/log")"
[ -s "$work/race.ppm" ] || fail "no frame of the race"
grep -q "track piece: loop is track" "$player/spirited-drive.log" || fail "the game's log does not name the piece: $(tail -5 "$player/spirited-drive.log")"

[ "$failures" -eq 0 ] && echo "editor_e2e_track: ok"
exit "$failures"
