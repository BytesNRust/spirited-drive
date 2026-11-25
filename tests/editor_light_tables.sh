#!/bin/sh
# The Track light panel's tables (Tunnels, Lamps) at 1280 px: no column is narrower than what its header and cells need (fit-report), a drag of a header's border changes a width, and the widths
# are still there after a restart (they are kept with the layout in the user folder, written when the mouse is let go). No CD data and no real game: a fake TRI and a fake game, as editor_light_window.sh.
# usage: editor_light_tables.sh EDITOR MAKE_FAKE_TRI
set -u
editor=$1 make_tri=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
mkdir -p "$work/light/track_light" "$work/cd/SIMDATA/MISC" "$work/tmp" "$work/u" "$work/u16"
"$make_tri" "$work/cd/SIMDATA/MISC/AL1.TRI" 30 || { echo "FAIL: no fake TRI" >&2; exit 1; }
export TMPDIR="$work/tmp/"
cat > "$work/light/track_light/AL1.txt" <<'TXT'
# tnfs track light 2
track AL1
tunnel 10 20
tunnel 60 71

time B
  sun 150 50 55
  tunnel_look 100
  tunnel_look 60 71 90 lamps 150
TXT
printf '#!/bin/sh\n' > "$work/fake_game" && chmod +x "$work/fake_game"
studio() { "$editor" --game "$work/fake_game" --cd "$work/cd" --light-dir "$work/light" --user "$work/${UDIR:-u}" --ask light --window "${WIN:-1280x800}" --frames 60 "$@" > "$work/log" 2>&1; }
width() { grep "^fit $1 $2 " "$work/log" | tail -n 1 | sed 's/.* width \([0-9.]*\) need.*/\1/'; }   # width TABLE COLUMN: the last report's

# 1. no header or cell narrower than it needs, in both tables (the lamps table has a lamp to draw)
studio --steps ui-click:time_B,ui-click:lamps_header,ui-click:add_lamp,fit-report || fail "fit-report: $(tail -3 "$work/log")"
[ "$(grep -c '^fit ##tunnels ' "$work/log")" -eq 9 ] || fail "the Tunnels table did not report its 9 columns: $(grep -c '^fit' "$work/log") lines"
[ "$(grep -c '^fit ##lamps ' "$work/log")" -eq 7 ] || fail "the Lamps table did not report its 7 columns: $(grep -c '^fit ##lamps' "$work/log") lines"
grep '^fit ' "$work/log" | awk '{ w = $(NF-2) + 0; n = $NF + 0; if (w + 0.01 < n) print $0 }' > "$work/narrow"
[ -s "$work/narrow" ] && fail "columns narrower than they need at 1280 px: $(cat "$work/narrow" | head -3)"

# 2. a drag of the border after From makes From wider (the others give way down to what they need: at 1280 px they have little to spare, so this is at 1600)
UDIR=u16 WIN=1600x900 studio --steps ui-click:time_B,fit-report,ui-span:##tunnels_border1,0.5,6.5,fit-report || fail "drag: $(tail -3 "$work/log")"
before=$(grep '^fit ##tunnels From ' "$work/log" | sed -n 1p | sed 's/.* width \([0-9.]*\) need.*/\1/')
after=$(grep '^fit ##tunnels From ' "$work/log" | sed -n 2p | sed 's/.* width \([0-9.]*\) need.*/\1/')
[ -n "$before" ] && [ -n "$after" ] && awk -v b="$before" -v a="$after" 'BEGIN { exit !(a >= b + 15) }' || fail "dragging the border after From 36 px did not widen it ($before -> $after)"

# 3. the width is kept: a new run with the same user folder starts with it
grep -q 'Table' "$work/u16/track-editor.ini" 2>/dev/null || fail "the layout file has no table settings after the drag: $(ls "$work/u")"
UDIR=u16 WIN=1600x900 studio --steps ui-click:time_B,fit-report || fail "restart: $(tail -3 "$work/log")"
again=$(width '##tunnels' From)
awk -v a="$after" -v n="$again" 'BEGIN { d = a - n; if (d < 0) d = -d; exit !(d <= 2) }' || fail "the dragged width did not survive a restart ($after -> $again)"
# 2b. a border dragged all the way (far to the right, then far to the left) leaves no column under its least: the neighbours give way only down to what they need
studio --steps ui-click:time_B,ui-span:##tunnels_border1,0.5,70,status,status,status,fit-report || fail "drag all the way right: $(tail -3 "$work/log")"
grep '^fit ##tunnels ' "$work/log" | awk '{ w = $(NF-2) + 0; n = $NF + 0; if (w + 0.01 < n) print $0 }' > "$work/narrow"
[ "$(grep -c '^fit ##tunnels ' "$work/log")" -eq 9 ] && [ ! -s "$work/narrow" ] || fail "a border dragged to the right left a column under its least: $(head -3 "$work/narrow")"
studio --steps ui-click:time_B,ui-span:##tunnels_border5,0.5,-70,status,status,status,fit-report || fail "drag all the way left: $(tail -3 "$work/log")"
grep '^fit ##tunnels ' "$work/log" | awk '{ w = $(NF-2) + 0; n = $NF + 0; if (w + 0.01 < n) print $0 }' > "$work/narrow"
[ "$(grep -c '^fit ##tunnels ' "$work/log")" -eq 9 ] && [ ! -s "$work/narrow" ] || fail "a border dragged to the left left a column under its least: $(head -3 "$work/narrow")"

[ "$failures" -eq 0 ] && echo "editor_light_tables: ok"
exit "$failures"
