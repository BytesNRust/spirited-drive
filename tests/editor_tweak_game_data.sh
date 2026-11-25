#!/bin/sh
# Edit tweak > Track on an installed app: the 16 tracks open with no game repo set (the game's own data/ is found beside the game, as Export original car finds it, through a game path with ../ as the owner's
# has), and with the repo kept in the settings file (workspace.txt, read when the editor starts) the repo's data/ is the one. Never the real settings: TNFS_STUDIO_PREFS is this run's folder. Needs the game's files.
# usage: editor_tweak_game_data.sh EDITOR ORIGINAL_DIR REPO
. "$(dirname "$0")/editor_headless_common.sh"
orig=$(cd "$2" && pwd)
repo=$3
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/track_light" ] || { echo "skip: no game files in $orig"; exit 0; }
mkdir -p "$work/ws" "$work/art" "$work/prefs" "$work/Studio.app/Contents/Resources" "$work/Game.app/Contents/MacOS" "$work/Game.app/Contents/Resources" "$work/norepo" || exit 1
cp -R "$repo/data" "$work/Game.app/Contents/Resources/data"
printf '#!/bin/sh\n' > "$work/Game.app/Contents/MacOS/tnfs" && chmod +x "$work/Game.app/Contents/MacOS/tnfs"
shape="$work/Studio.app/Contents/Resources/../../../Game.app/Contents/MacOS/tnfs"   # the owner's: the game beside Studio, reached through ../
none="$work/nowhere"
run() { TNFS_STUDIO_GAME_SEARCH=off "$editor" --cd "$orig" --art-dir "$work/art" --user "$work/u" --light-dir "$none" --window 1280x800 --frames 60 "$@" >"$work/log" 2>&1; }

# 1. no repo, no --port-data: the game's own data/ feeds the tracks
run --game "$shape" --workspace "$work/ws" --steps tweak-open-row:CY1,tweak-status || fail "no repo, the game found: CY1 did not open: $(tail -3 "$work/log")"
grep -q '^tweak cy1-tweak' "$work/log" || fail "no tweak for CY1 with the game found and no repo: $(tail -3 "$work/log")"
[ -f "$work/ws/tweaks/cy1-tweak/mod.ini" ] || fail "the tweak folder was not made"
run --game "$shape" --workspace "$work/ws" --steps tweak-list
grep '^tweak row: [A-Z][A-Z][0-9] | track' "$work/log" | grep -q 'not found' && fail "a track row says the game's data/ is not found: $(grep 'not found' "$work/log" | head -2)"

# 2. the repo kept in the settings file, a game with no data/ of its own: the repo is the data
rm -rf "$work/ws2" "$work/Game2.app"; mkdir -p "$work/ws2" "$work/Game2.app/Contents/MacOS"
printf '#!/bin/sh\n' > "$work/Game2.app/Contents/MacOS/tnfs" && chmod +x "$work/Game2.app/Contents/MacOS/tnfs"
cp -R "$repo/data" "$work/repo_data"
printf 'dir\t%s\nrepo data\t%s\n' "$work/ws2" "$work/repo_data" > "$work/prefs/workspace.txt"
TNFS_STUDIO_PREFS="$work/prefs" run --game "$work/Game2.app/Contents/MacOS/tnfs" --steps tweak-open-row:CY1,tweak-status || fail "the repo in the settings file: CY1 did not open: $(tail -3 "$work/log")"
[ -f "$work/ws2/tweaks/cy1-tweak/mod.ini" ] || fail "the repo in the settings file: no tweak folder (the workspace.txt was not read, or its repo not applied)"

[ "$failures" -eq 0 ] && echo "editor_tweak_game_data: ok"
exit "$failures"
