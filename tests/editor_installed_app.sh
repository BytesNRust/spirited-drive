#!/bin/sh
# Studio as the installed app runs it, with no source tree behind it (TNFS_STUDIO_NO_SOURCE_TREE=1: nothing compiled in, as a TNFS_RELEASE build; HOME is an empty folder; the settings and Documents are this run's):
#  1. the game beside Studio (a path with ../), no repo: the 16 tracks are usable and a tweak is made;
#  2. no game and no repo: the tracks say what to do (File > Settings...), not a bare "(none)";
# Needs the game's files. usage: editor_installed_app.sh EDITOR ORIGINAL_DIR REPO
. "$(dirname "$0")/editor_headless_common.sh"
orig=$(cd "$2" && pwd)
repo=$3
[ -r "$orig/SIMDATA/CARFAMS/WAGON.CFM" ] && [ -d "$repo/data/track_light" ] || { echo "skip: no game files in $orig"; exit 0; }
mkdir -p "$work/home" "$work/ws" "$work/art" "$work/prefs" "$work/docs" "$work/Studio.app/Contents/Resources" "$work/Game.app/Contents/MacOS" "$work/Game.app/Contents/Resources" || exit 1
cp -R "$repo/data" "$work/Game.app/Contents/Resources/data"
printf '#!/bin/sh\n' > "$work/Game.app/Contents/MacOS/tnfs" && chmod +x "$work/Game.app/Contents/MacOS/tnfs"
shape="$work/Studio.app/Contents/Resources/../../../Game.app/Contents/MacOS/tnfs"
mkdir -p "$work/cwd"
exe=$(cd "$(dirname "$editor")" && pwd)/$(basename "$editor")
run() { cd "$work/cwd" && HOME="$work/home" TNFS_STUDIO_NO_SOURCE_TREE=1 TNFS_STUDIO_GAME_SEARCH=off TNFS_STUDIO_PREFS="$work/prefs" TNFS_STUDIO_DOCUMENTS="$work/docs" "$exe" --cd "$orig" --art-dir "$work/art" --user "$work/u" --window 1280x800 --frames 60 "$@" >"$work/log" 2>&1; rc=$?; cd - >/dev/null; return $rc; }

# 1
run --game "$shape" --workspace "$work/ws" --steps tweak-list || fail "1 list: $(tail -3 "$work/log")"
[ "$(grep -c '^tweak row: .* | track | ' "$work/log")" -eq 16 ] || fail "1 the list has not the 16 tracks: $(grep -c 'row:' "$work/log") rows"
grep '| track |' "$work/log" | grep -q "not found" && fail "1 a track says the game's data/ is not found: $(grep 'not found' "$work/log" | head -1)"
run --game "$shape" --workspace "$work/ws" --steps tweak-open-row:TR2,tweak-status || fail "1 TR2 did not open: $(tail -3 "$work/log")"
[ -f "$work/ws/tweaks/tr2-tweak/mod.ini" ] || fail "1 no tweak made for TR2"

# 2
run --game "$work/nogame" --workspace "$work/ws" --steps tweak-list
grep '| track |' "$work/log" | head -1 | grep -q "File > Settings" || fail "2 the track row does not say what to do: $(grep '| track |' "$work/log" | head -1)"
grep '| track |' "$work/log" | grep -q "(none)" && fail "2 a bare (none) is back"

[ "$failures" -eq 0 ] && echo "editor_installed_app: ok"
exit "$failures"
