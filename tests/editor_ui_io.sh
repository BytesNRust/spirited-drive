#!/bin/sh
# The UI thread does no file work per frame at rest (owner rule 2026-11-22: the workspace may be on a network or synced folder, where every open, stat and directory read is slow).
# The editor opens a kit with PSDs and masks and idles; tests/io_count.c (a dylib put in front of it with DYLD_INSERT_LIBRARIES, macOS) counts open / fopen, opendir and stat /
# lstat / access made by the main thread. A short run (20 frames) and a long one (120) differ by what 100 frames at rest cost: only the once-a-second watches may show.
# usage: editor_ui_io.sh EDITOR MAKE_TEST_PSD IO_COUNT_DYLIB
set -u
editor=$1 make_psd=$2 dylib=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
MAX_TOTAL=100 MAX_OPEN=8   # 100 frames at rest (about 8 s on the dummy driver): the watches (the card's files, the masks: every 2.5 s) and a manifest look; the card's files read the two banks' folders (2 directory reads a look, one look every 2.5 s: under a read a second); a look a second or a frame shows as more than the run's seconds + 4; no per-frame work, no hashing of PNGs
fail() { echo "FAIL: $*" >&2; exit 1; }
[ "$(uname)" = Darwin ] || { echo "skip: DYLD_INSERT_LIBRARIES is macOS"; exit 77; }

mkdir -p "$work/kit/model/near/bank" "$work/kit/art-generated/car_masks" "$work/kit/art-source/masks"
printf 'id=k1\nname=Kit One\nversion=1\ntype=car\nstem=K1\nsound_from=911\n' > "$work/kit/mod.ini"
for t in rsid frnt side; do
    "$make_psd" "$work/kit/art-source/masks/$t.psd" paint || fail "no PSD fixture"
    printf x > "$work/kit/art-generated/car_masks/${t}_mask.png"
    printf x > "$work/kit/model/near/bank/0010_$t.png"
done
count() {   # count FRAMES -> "open dir stat"
    DYLD_INSERT_LIBRARIES=$dylib TNFS_IO_COUNT_OUT=$work/out.$1 "$editor" "$work/kit" --user "$work/u" --frames "$1" >"$work/log.$1" 2>&1 || fail "the run of $1 frames: $(cat "$work/log.$1")"
    sed -n 's/^io: open \([0-9]*\) dir \([0-9]*\) stat \([0-9]*\)$/\1 \2 \3/p' "$work/out.$1"
}
t0=$(date +%s)
set -- $(count 20); a_open=$1 a_dir=$2 a_stat=$3
t1=$(date +%s)
set -- $(count 120); b_open=$1 b_dir=$2 b_stat=$3
t2=$(date +%s)
secs=$(( (t2 - t1) - (t1 - t0) ))   # what the 100 extra frames took: a slow machine takes longer, and the watches look by the clock
[ -n "${a_open:-}" ] && [ -n "${b_open:-}" ] || fail "the counter wrote nothing (is the dylib in front of the editor?)"
open=$((b_open - a_open)) dir=$((b_dir - a_dir)) stat=$((b_stat - a_stat))
total=$((open + dir + stat))
echo "100 frames at rest: open $open, opendir $dir, stat $stat (total $total; limits $MAX_TOTAL and $MAX_OPEN opens)"
[ "$total" -le "$MAX_TOTAL" ] && [ "$open" -le "$MAX_OPEN" ] && [ "$dir" -le $((secs + 4)) ] || fail "the UI thread did file work per frame at rest: open $open, opendir $dir, stat $stat in 100 frames"
echo "editor_ui_io: ok"
