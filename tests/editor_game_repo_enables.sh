#!/bin/sh
# A Studio with no game data folder of its own (an installed app: no --port-data, no source tree) is given the game repo with File > Game repo folder... (or the one workspace.txt keeps):
# Add originals and Open original track... are greyed until then (clicked by ui_probe name, nothing happens), and enabled and working after: the repo feeds the data folder and the light folder.
# usage: editor_game_repo_enables.sh EDITOR
set -u
editor=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
mkdir -p "$work/repo/data/track_light" "$work/repo/data/car_masks" "$work/ws" "$work/ws3" "$work/ws4" "$work/tmp"
export TMPDIR="$work/tmp/"
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do printf '# tnfs track light 2\ntrack %s\n' "$t" > "$work/repo/data/track_light/$t.txt"; done
bare() { "$editor" --user "$work/u" --workspace "$work/ws" --port-data "" --light-dir "" --window 1280x800 "$@"; }   # nothing of Studio's own: as an installed app starts

# 1. no repo: the button is grey, a click does nothing
out=$(bare --frames 8 --steps ui-click:add_originals,print-workspace,status 2>&1) || fail "no repo: $out"
echo "$out" | grep -q '^tracks: ' && echo "$out" | grep -q '^tracks: AL1' && fail "Add originals worked with no game data folder: $out"
# 2. File > Game repo folder... then the button is enabled and works (the repo is the data folder)
out=$(TNFS_STUDIO_PICK="$work/repo/data" bare --frames 12 --steps repo-choose,ui-click:add_originals,print-workspace,status 2>&1) || fail "repo chosen: $out"
echo "$out" | grep -q '^tracks: AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7$' || fail "Add originals did not add the 16 once the repo was chosen: $out"
[ -f "$work/ws/tracks/CL1/light/track_light/CL1.txt" ] || fail "no light file copied from the chosen repo"
# 3. launched with a --port-data that is not a folder (another Mac's path): the button is grey, and the status says which path failed when Add originals is asked for (a headless step ignores the grey)
out=$("$editor" --user "$work/u3" --workspace "$work/ws3" --port-data "$work/not/there/data" --light-dir "" --frames 8 --steps ui-click:add_originals,status,add-originals,status 2>&1)
echo "$out" | grep -q "game data folder not found: $work/not/there/data" || fail "the missing --port-data path is not named: $out"
# 4. launched with a good --port-data, another repo chosen in the menu: that one is the data folder from then on (Add originals reads it, no 'no game data folder')
mkdir -p "$work/repo2/data/track_light" "$work/repo2/data/car_masks"
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do printf '# tnfs track light 2\ntrack %s\n# second repo\n' "$t" > "$work/repo2/data/track_light/$t.txt"; done
out=$(TNFS_STUDIO_PICK="$work/repo2/data" "$editor" --user "$work/u4" --workspace "$work/ws4" --port-data "$work/repo/data" --light-dir "" --frames 12 --steps repo-choose,ui-click:add_originals,print-workspace,status 2>&1) || fail "port-data then repo: $out"
echo "$out" | grep -q '^tracks: AL1 AL2 AL3 CL1' || fail "Add originals failed after choosing another repo over --port-data: $out"
grep -q '# second repo' "$work/ws4/tracks/CL1/light/track_light/CL1.txt" || fail "Add originals did not read the repo chosen in the menu"
[ "$failures" -eq 0 ] && echo "editor_game_repo_enables: ok"
exit "$failures"
