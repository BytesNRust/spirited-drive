#!/bin/sh
# A headless run's frames are about 16 ms apart. Not less: what the steps count in frames (a click's wait, a race's) would last less time by the clock, and a check could give up on a
# slow background program. Not much more: macOS stretches a plain 16 ms sleep to 50-90 ms on CI's runners, which was most of a Studio test's time. Studio's own TNFS_PERF report
# says how long the pause took, once a second.
# usage: editor_headless_pace.sh EDITOR
. "$(dirname "$0")/editor_headless_common.sh"
TNFS_PERF=1 "$editor" --frames 250 >"$work/log" 2>&1 || fail "the run: $(tail -3 "$work/log")"
awk '$1 == "perf:" { frames += $2; for (i = 3; i < NF; i++) if ($i == "pause") ms += $(i + 1) }
     END { if (frames == 0) { print "no perf line with a pause"; exit 1 }
           mean = ms / frames; printf "pause: %.1f ms a frame over %d frames\n", mean, frames; exit !(mean >= 15.0 && mean <= 35.0) }' "$work/log" >"$work/pace" \
    || fail "the pause between a headless run's frames is not about 16 ms: $(cat "$work/pace") | $(grep '^perf:' "$work/log" | head -2)"
cat "$work/pace"

[ $failures -eq 0 ] && echo "editor_headless_pace: ok"
exit $failures
