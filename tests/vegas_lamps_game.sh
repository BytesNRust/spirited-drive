#!/bin/sh
# Lost Vegas (TR4) is always night but its world has no tint (the night is in the art), so the lamps were put out whenever the ratio at the car stayed at 80 % or more for 12 frames, and lit again under a
# street lamp: the player's lamps flipped by themselves. With the light effects (Restomod) a night keeps them on: one lap of Vegas has no "lamps: slot 0 ... OFF" line; the day (TR1 by day) still turns lamps
# off and on as before (a tunnel's dark lights them, the day's light puts them out: its lines are unchanged by the night's rule).
# PART=night or PART=day (environment) runs one half (the gate runs them as two tests, side by side); with no PART both, as ever. 3 checks: 1 at night, 2 by day.
# Only the log's lamps lines are read, so the frames are drawn at render scale 1 (the same lines, half the time); every tick still draws its frame (the
# lamps' rule counts frames).
# A run that does not end by itself fails the half: what it printed until it was killed proves nothing (no OFF line yet is not "no OFF line"). The
# bound is only against a hang: a half takes under 30 s in release and under 3 min with the sanitizers on the gate, several times that on a slow machine.
# usage: tests/vegas_lamps_game.sh <tnfs binary> <original/ dir> <repo root>    Exit 77 (skipped) without the CD's data.
set -u
bin=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
original=$(cd "$2" 2> /dev/null && pwd) || original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_vegas.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
LT=   # LT: the time of day asked for (empty: the track's own)
run() {   # TRACK TICKS [PROFILE]: the log's lines in $work/run.out; the run's status
  printf 'profile=%s\npsx_dir=\n' "${3:-restomod}" > "$work/user/port.cfg"
  (cd "$work" && env ${LT:+TNFS_LIGHT_TIME=$LT} TNFS_RENDER_SCALE=1 TNFS_AUTOPILOT=1 TNFS_FRAMES="$2" SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_PREF_DIR="$work/pref" timeout -k 5 900 "$bin" --data "$original" --user "$work/user" --drive "$1" --node "$2" --screenshot "$work/s.ppm" --verbose > "$work/run.out" 2>&1)
}
status=0
part=${PART:-all}
vegas=
if [ "$part" = all ] || [ "$part" = night ]; then
  run TR4 3000 || { echo "FAIL: the night's run did not end by itself (status $?): $(tail -2 "$work/run.out")"; status=1; }
  vegas=$(grep '^lamps: slot 0 ' "$work/run.out" || true)
fi
echo "$vegas" | grep -q ' OFF:' && { echo "FAIL: Vegas' lamps went out at night: $(echo "$vegas" | grep ' OFF:' | head -2)"; status=1; }
# the day keeps its rule (1200 ticks: both an ON and an OFF are there by tick 1000): TR3 by day puts its lamps on in the dark stretches and out again (10 lines, 5 OFF, before the night's rule), and the night's rule does not touch them
day=
if [ "$part" = all ] || [ "$part" = day ]; then
LT=day
run TR3 1200 || { echo "FAIL: the day's run did not end by itself (status $?): $(tail -2 "$work/run.out")"; status=1; }
day=$(grep '^lamps: slot 0 ' "$work/run.out" || true)
echo "$day" | grep -q ' OFF:' || { echo "FAIL: the day's lamps no longer go out: $day"; status=1; }
echo "$day" | grep -q ' ON:' || { echo "FAIL: the day's lamps no longer light in the dark: $day"; status=1; }
fi
[ $status -eq 0 ] && echo "vegas_lamps_game: ok"
find "$work" -delete
exit $status
