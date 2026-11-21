#!/bin/sh
# dep95 step 8, flow 9: a hill drawn in the editor is raced. The project's heights (0 .. 30 m .. 0 over 15 points) are set the way the Points window sets them (--steps height:),
# Build + Install put the piece in a scratch player folder, and the game, headless, puts the player's car at three nodes (TNFS_START_NODE: the foot, the crest, the way down) and
# logs the camera's height (the log's "eye node N (x, y, z)" of the last tick, kind 3: 3.2 m over the road): the crest is about 30 m over the foot and the way down about 5 m, and a
# steeper hill (up to 90 m) is higher still. Needs the game's files (original/): the test is only registered when it is there.
# usage: editor_hill_race.sh EDITOR TNFS ORIGINAL_DIR
set -u
editor=$1 tnfs=$2 original=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
player="$work/player"
mkdir -p "$player"
printf 'profile=restomod\n' > "$player/port.cfg"

# the heights of the 15 points of a new project, as the Points window's h cell takes them
steps_for() { i=0; out=; for h in "$@"; do out="$out${out:+,}height:$i,$h"; i=$((i + 1)); done; echo "$out"; }

make_hill() {   # NAME heights...
    name=$1; shift
    "$editor" "$work/$name" --user "$player" --frames 2 --save >"$work/log" 2>&1 || { fail "New project $name: $(cat "$work/log")"; return 1; }
    "$editor" "$work/$name" --user "$player" --frames 2 --steps "$(steps_for "$@"),save,problems" >"$work/log" 2>&1 || { fail "heights of $name: $(cat "$work/log")"; return 1; }
    grep -q "^problems: none" "$work/log" || { fail "$name has problems: $(grep '^problems' "$work/log")"; return 1; }
    "$editor" "$work/$name" --user "$player" --frames 2 --build --install --game "$tnfs" >"$work/log" 2>&1 || { fail "Build and Install of $name: $(cat "$work/log")"; return 1; }
    [ -f "$player/mods/$name/mod.ini" ] || { fail "$name is not in the player's mods folder"; return 1; }
}

eye_y() {   # NAME NODE: the camera's height at the node, or nothing
    env TNFS_AUTOPILOT=1 TNFS_FRAMES=1 TNFS_WEATHER=clear TNFS_VIEW=3 TNFS_START_NODE="$2" \
        "$tnfs" --data "$original" --user "$player" --mods "$player/mods/$1" --drive "$1" --car 5 --racers 1 --node 60 --screenshot "$work/$1-$2.ppm" >"$work/race.log" 2>&1 ||
        { fail "the race on $1 at node $2 did not run: $(tail -3 "$work/race.log")"; return 1; }
    [ -s "$work/$1-$2.ppm" ] || { fail "no picture of $1 at node $2"; return 1; }
    grep '^tick ' "$work/race.log" | tail -1 | sed -n 's/.*eye node [0-9]* ([^,]*, \([-0-9.]*\), .*/\1/p'
}

make_hill hill 0 0 5 12 20 26 30 30 26 20 12 5 0 0 0 || exit "$failures"
make_hill steep 0 0 15 35 60 80 90 90 80 60 35 15 0 0 0 || exit "$failures"
foot=$(eye_y hill 20) crest=$(eye_y hill 95) down=$(eye_y hill 170)
mid=$(eye_y steep 20) top=$(eye_y steep 95)
[ -n "$foot" ] && [ -n "$crest" ] && [ -n "$down" ] && [ -n "$mid" ] && [ -n "$top" ] || { fail "no camera height in the game's log (foot '$foot', crest '$crest', way down '$down', steep '$mid' '$top')"; exit "$failures"; }
awk -v f="$foot" -v c="$crest" 'BEGIN { exit !(c - f >= 25 && c - f <= 35) }' || fail "the crest is not about 30 m over the foot: foot $foot, crest $crest"
awk -v f="$foot" -v d="$down" 'BEGIN { exit !(d - f >= 2 && d - f <= 9) }' || fail "the way down is not about 5 m over the foot: foot $foot, down $down"
awk -v f="$mid" -v t="$top" 'BEGIN { exit !(t - f >= 80 && t - f <= 100) }' || fail "the steep hill's crest is not about 90 m over its foot: foot $mid, crest $top"
[ "$failures" = 0 ] && echo "ok"
exit "$failures"
