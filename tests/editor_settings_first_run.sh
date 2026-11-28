#!/bin/sh
# Edit tweak, step 3: the main menu has three doors (Edit car mod, Edit track mod, Edit tweak); the first start asks where the game is, once (Locate game... or Not now), and File > Settings holds
# the game, the workspace and the game repo's folder afterwards. No game files needed.
# usage: editor_settings_first_run.sh EDITOR
. "$(dirname "$0")/editor_headless_common.sh"
mkdir -p "$work/ws" "$work/repo/data/car_masks" "$work/repo/data/track_light" || exit 1
printf '#!/bin/sh\n' >"$work/fake_game" && chmod +x "$work/fake_game"
run() { "$editor" --user "$work/u" --workspace "$work/ws" --window 1280x800 --frames 12 "$@" >"$work/log" 2>&1; }

# 1. the game is not found: the question is on the main menu; Not now answers it, and then it is gone
run --game "$work/none" --steps ui-click:locate_not_now || fail "Not now was not on the first start: $(tail -3 "$work/log")"
run --game "$work/none" --steps ui-click:locate_not_now,patience:60,ui-click:locate_not_now && fail "the question was still there after Not now"
grep -q "locate_not_now: no such widget drawn" "$work/log" || fail "the second Not now failed for another reason: $(tail -3 "$work/log")"
# 2. the game is found: the question is never shown
run --game "$work/fake_game" --steps patience:60,ui-click:locate_not_now && fail "the game is found and the question was asked"
grep -q "locate_not_now: no such widget drawn" "$work/log" || fail "the click on the question not asked failed for another reason: $(tail -3 "$work/log")"
# 3. the three doors, each a button; Edit tweak opens the list
run --game "$work/fake_game" --steps ui-click:door_tweak,ui-click:tweak_tab_car,ui-click:tweak_main || fail "Edit tweak, Car, Main menu: $(tail -3 "$work/log")"
# 4. Settings: the game, the workspace and the game repo; Clear takes the repo away
run --game "$work/fake_game" --port-data "$work/repo/data" --steps settings-open,ui-click:settings_repo,ui-click:settings_close || fail "Settings: $(tail -3 "$work/log")"
run --game "$work/fake_game" --steps settings-open,ui-click:settings_game,ui-click:settings_workspace,ui-click:settings_repo,ui-click:settings_close || fail "Settings buttons: $(tail -3 "$work/log")"
# 5. the File menu of every room has Settings and none of the old items; the hub has Workspace too
run --game "$work/fake_game" --steps print-menu
grep -q '^file: Workspace | Settings... | Quit$' "$work/log" || fail "the hub's File menu: $(grep '^file' "$work/log")"
for gone in "Game repo folder" "Export original car" "Open original track" "Copy to game repo"; do grep -q "$gone" "$work/log" && fail "the File menu still has $gone"; done
[ "$failures" -eq 0 ] && echo "editor_settings_first_run: ok"
exit "$failures"
