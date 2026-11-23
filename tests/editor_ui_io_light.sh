#!/bin/sh
# The UI thread does no file work per frame at rest in the Track light window either (owner rule 2026-11-22: the workspace may be on a network or synced folder): the window is open on a
# track with the four-times comparison on, and idles; tests/io_count.c counts open / fopen, opendir and stat made by the main thread. A short run (20 frames) and a long one (120) differ
# by what 100 frames at rest cost. The game is a stub that makes no picture: the photo and the comparison try once and settle.
# usage: editor_ui_io_light.sh EDITOR MAKE_FAKE_TRI IO_COUNT_DYLIB
set -u
editor=$1 make_tri=$2 dylib=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
MAX_TOTAL=30 MAX_OPEN=8
fail() { echo "FAIL: $*" >&2; exit 1; }
[ "$(uname)" = Darwin ] || { echo "skip: DYLD_INSERT_LIBRARIES is macOS"; exit 0; }
mkdir -p "$work/light/track_light" "$work/cd/SIMDATA/MISC" "$work/user"
"$make_tri" "$work/cd/SIMDATA/MISC/AL1.TRI" 30 || fail "no fake TRI"
cat > "$work/light/track_light/AL1.txt" <<'TXT'
# tnfs track light 2
track AL1
tunnel 10 20

time B
  sun 150 50 55
  tunnel_look 100
  lamp 12 100 ff2000 8
TXT
printf '#!/bin/sh\nexit 0\n' > "$work/game"
chmod +x "$work/game"
count() {   # count FRAMES -> "open dir stat"
    DYLD_INSERT_LIBRARIES=$dylib TNFS_IO_COUNT_OUT=$work/out.$1 "$editor" --game "$work/game" --cd "$work/cd" --light-dir "$work/light" --ask light --compare four --user "$work/user" --frames "$1" >"$work/log.$1" 2>&1 || fail "the run of $1 frames: $(tail -3 "$work/log.$1")"
    sed -n 's/^io: open \([0-9]*\) dir \([0-9]*\) stat \([0-9]*\)$/\1 \2 \3/p' "$work/out.$1"
}
set -- $(count 20); a_open=$1 a_dir=$2 a_stat=$3
set -- $(count 120); b_open=$1 b_dir=$2 b_stat=$3
[ -n "${a_open:-}" ] && [ -n "${b_open:-}" ] || fail "the counter wrote nothing (is the dylib in front of the editor?)"
open=$((b_open - a_open)) dir=$((b_dir - a_dir)) stat=$((b_stat - a_stat))
total=$((open + dir + stat))
echo "100 frames at rest: open $open, opendir $dir, stat $stat (total $total; limits $MAX_TOTAL and $MAX_OPEN opens)"
[ "$total" -le "$MAX_TOTAL" ] && [ "$open" -le "$MAX_OPEN" ] || fail "the Track light window did file work per frame at rest: open $open, opendir $dir, stat $stat in 100 frames"
echo "editor_ui_io_light: ok"
