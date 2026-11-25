#!/bin/sh
# A Studio with no game data folder of its own (an installed app: no --port-data, no source tree) is given the game repo with File > Settings... (or the one workspace.txt keeps):
# the Edit tweak list's track rows are greyed until then (clicked by ui_probe name, nothing happens), and enabled and working after: the repo feeds the data folder and the light folder.
# usage: editor_game_repo_enables.sh EDITOR MAKE_FAKE_TRI
set -u
editor=$1 make_tri=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
mkdir -p "$work/cd/SIMDATA/MISC" "$work/repo/data/track_light" "$work/repo/data/car_masks" "$work/ws" "$work/ws3" "$work/ws4" "$work/tmp"
export TMPDIR="$work/tmp/"
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do printf '# tnfs track light 2\ntrack %s\n' "$t" > "$work/repo/data/track_light/$t.txt"; "$make_tri" "$work/cd/SIMDATA/MISC/$t.TRI" 200 || echo "FAIL: no fake TRI for $t" >&2; done
bare() { "$editor" --game "$work/none/tnfs" --user "$work/u" --workspace "$work/ws" --port-data "" --light-dir "" --cd "$work/cd" --window 1280x800 "$@"; }   # nothing of Studio's own: as an installed app starts

# 1. no repo: the rows are grey, a click does nothing
out=$(bare --frames 8 --steps ui-click:door_tweak,ui-click:tweak_tab_track,ui-click:tweak_row_AL1,ui-click:tweak_open,print-workspace,status 2>&1) || fail "no repo: $out"
echo "$out" | grep -q '^tracks: ' && echo "$out" | grep -q '^tracks: AL1' && fail "the tweak list opened a track with no game data folder: $out"
# 2. File > Settings... then the rows are enabled and work (the repo is the data folder): AL1 is added to the workspace, from the repo
out=$(TNFS_STUDIO_PICK="$work/repo/data" bare --frames 12 --steps repo-choose,ui-click:door_tweak,ui-click:tweak_tab_track,ui-click:tweak_row_AL1,ui-click:tweak_open,print-workspace,status 2>&1) || fail "repo chosen: $out"
[ "$(ls "$work/ws/tweaks" | tr '\n' ' ')" = "al1-tweak " ] || fail "Open on AL1 did not add it alone once the repo was chosen: $out"
[ -f "$work/ws/tweaks/al1-tweak/studio/project/light/track_light/AL1.txt" ] || fail "no light file copied from the chosen repo"
# 2a. the others come with Add originals (kept for the tests and the command line): 16 in the workspace, and it says why no PSD was copied
out=$(TNFS_STUDIO_PICK="$work/repo/data" bare --frames 12 --steps repo-choose,add-originals,print-workspace,status 2>&1) || fail "add-originals: $out"
[ "$(ls "$work/ws/tweaks" | wc -l | tr -d ' ')" = 16 ] || fail "Add originals did not add the 16 once the repo was chosen: $out"
echo "$out" | grep -q 'status: original tracks: 16 in the workspace; 15 light files copied, 0 mask PSDs copied: no PSD folder (--art-dir); the workspace has none yet' || fail "Add originals does not say why no PSD was copied: $(echo "$out" | grep '^status')"
# 2b. the Tweak panel of an original track open: it lists the saved light file as changed
out=$(TNFS_STUDIO_PICK="$work/repo/data" bare --frames 30 --steps repo-choose,tweak-open-row:CL1,sun:120,30,70,save,tweak-status 2>&1) || fail "tweak panel: $out"
echo "$out" | grep -q 'track_light/CL1.txt: changed' || fail "the Tweak panel did not list the saved light file as changed: $out"
# 2c. a workspace that has its own PSDs and no PSD folder: kept, and it says so
mkdir -p "$work/ws/tweaks/cl1-tweak/studio/project/art-source/masks" && printf psd > "$work/ws/tweaks/cl1-tweak/studio/project/art-source/masks/scenery_00.psd"
out=$(TNFS_STUDIO_PICK="$work/repo/data" bare --frames 12 --steps repo-choose,add-originals,status 2>&1) || fail "add again: $out"
echo "$out" | grep -q "no PSD folder (--art-dir); the workspace's own PSDs are kept" || fail "a workspace with its own PSDs is not told they are kept: $(echo "$out" | grep '^status')"
# 3. launched with a --port-data that is not a folder (another Mac's path): the rows are grey, and the status says which path failed when Add originals is asked for (a headless step ignores the grey)
out=$("$editor" --user "$work/u3" --workspace "$work/ws3" --port-data "$work/not/there/data" --light-dir "" --frames 8 --steps add-originals,status 2>&1)
echo "$out" | grep -q "game data folder not found: $work/not/there/data" || fail "the missing --port-data path is not named: $out"
# 4. launched with a good --port-data, another repo chosen in the menu: that one is the data folder from then on (Open reads it, no 'no game data folder')
mkdir -p "$work/repo2/data/track_light" "$work/repo2/data/car_masks"
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do printf '# tnfs track light 2\ntrack %s\n# second repo\n' "$t" > "$work/repo2/data/track_light/$t.txt"; done
out=$(TNFS_STUDIO_PICK="$work/repo2/data" "$editor" --cd "$work/cd" --user "$work/u4" --workspace "$work/ws4" --port-data "$work/repo/data" --light-dir "" --frames 12 --steps repo-choose,ui-click:door_tweak,ui-click:tweak_tab_track,ui-click:tweak_row_CL1,ui-click:tweak_open,print-workspace,status 2>&1) || fail "port-data then repo: $out"
echo "$out" | [ -f "$work/ws4/tweaks/cl1-tweak/studio/project/light/track_light/CL1.txt" ] || fail "Open on CL1 failed after choosing another repo over --port-data: $out"
grep -q '# second repo' "$work/ws4/tweaks/cl1-tweak/studio/project/light/track_light/CL1.txt" || fail "Open did not read the repo chosen in the menu"
[ "$failures" -eq 0 ] && echo "editor_game_repo_enables: ok"
exit "$failures"
