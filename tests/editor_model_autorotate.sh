#!/bin/sh
# The Model window's Auto-rotate: off at a new start (a car opened with no layout file), turned on by hand it is kept with the layout (the line AutoRotate=1 of track-editor.ini, written when the
# mouse is let go) and the next start has it on; a new user folder starts with it off again. Needs the player's game files (original/): skipped without them.
# usage: editor_model_autorotate.sh EDITOR REPO
. "$(dirname "$0")/editor_headless_common.sh"
repo=$2
[ -f "$repo/original/SIMDATA/CARFAMS/DVIPER.CFM" ] || { echo "skip: no original/"; exit 0; }
ws="$work/ws"
mkdir -p "$ws"
run() { u=$1; shift; "$editor" --workspace "$ws" --user "$work/$u" --cd "$repo/original" --port-data "$repo/data" --window 1280x800 --frames 60 "$@" 2>&1; }
rotate() { echo "$out" | grep '^rotate: ' | sed -n "$1p"; }

out=$(run u1 --steps new-car:0,psd-wait:3,model-rotate,ui-click:model_auto_rotate,model-rotate) || fail "first start: $out"
[ "$(rotate 1)" = "rotate: 0" ] || fail "a new start has Auto-rotate on: $(rotate 1)"
[ "$(rotate 2)" = "rotate: 1" ] || fail "the box did not turn it on by hand: $(rotate 2)"
grep -q '^AutoRotate=1$' "$work/u1/track-editor.ini" 2>/dev/null || fail "the choice is not in the layout file: $(grep -c . "$work/u1/track-editor.ini" 2>/dev/null)"
kit=$(ls -d "$ws"/cars/* | head -n 1)
out=$(run u1 "$kit" --steps psd-wait:3,model-rotate) || fail "second start: $out"
[ "$(rotate 1)" = "rotate: 1" ] || fail "the next start did not keep Auto-rotate on: $(rotate 1)"
out=$(run u2 "$kit" --steps psd-wait:3,model-rotate) || fail "a new user folder: $out"
[ "$(rotate 1)" = "rotate: 0" ] || fail "a new user folder starts with Auto-rotate on: $(rotate 1)"
[ "$failures" -eq 0 ] && echo "editor_model_autorotate: ok"
exit "$failures"
