#!/bin/sh
# A track from the Studio to the game, the happy path a user takes: a new project, a point added and saved, Build, Install into the
# player's folder, and Test drive (the Drive window's button): the game runs on the piece from a scratch player folder and names it in its log.
# Needs the game's files (original/).
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

# 3. Build and 4. Install into the player's folder
"$editor" "$work/loop" --user "$player" --frames 2 --build --install --game "$tnfs" >"$work/log" 2>&1 || fail "Build and Install: $(cat "$work/log")"
grep -q "installed loop" "$work/log" || fail "Install does not say it installed the piece: $(cat "$work/log")"
[ -f "$player/mods/loop/mod.ini" ] || fail "the piece is not in the player's mods folder"

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
