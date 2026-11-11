#!/bin/sh
# Mods overlay against the real binary and the CD data (BYTES-0009 AC8): a track edited in a mod changes what the
# track viewer shows at the edited nodes and nowhere else, an invalid mod file stops the run naming the file and the
# reason, and a session never writes below the data root.
# usage: mods_overlay.sh TNFS STUDIO ORIGINAL_DIR
set -u
tnfs=$1
studio=$2
orig=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
# the player's folder is this run's own: the render depends on its settings (a profile, the baked light), and the default folder is shared with every other run on the machine (a gate, a game being played) -- a change in it between the four renders made "a node far from the edit changed"
user=$work/user
mkdir -p "$user"
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software

# Whether anything below the data root was written: any file or directory newer than this marker.
touch "$work/marker"
sleep 1

tri="$orig/SIMDATA/MISC/TR1.TRI"
[ -f "$tri" ] || { echo "skip: no TR1.TRI"; exit 0; }

# 1. the original, and the same track from a mod that moves node 21 sideways
"$tnfs" --data "$orig" --user "$user" --track TR1 --node 20 --screenshot "$work/orig20.ppm" >/dev/null 2>&1 || fail "the original track did not render"
"$tnfs" --data "$orig" --user "$user" --track TR1 --node 90 --screenshot "$work/orig90.ppm" >/dev/null 2>&1 || fail "the original track did not render at node 90"
"$studio" unpack "$tri" "$work/tri.d" >/dev/null 2>&1 || fail "unpack TR1.TRI"
awk -F'\t' 'BEGIN{OFS="\t"} $1=="21" { $8 = $8 + 60000 } { print }' "$work/tri.d/nodes.tsv" > "$work/nodes.new" && cp "$work/nodes.new" "$work/tri.d/nodes.tsv"
mkdir -p "$work/mod/SIMDATA/MISC"
"$studio" pack "$work/tri.d" "$work/mod/SIMDATA/MISC/TR1.TRI" --from "$tri" >/dev/null 2>&1 || fail "pack the edited TR1.TRI"
"$studio" validate "$work/mod" >/dev/null 2>&1 || fail "the edited track does not validate"
"$tnfs" --data "$orig" --user "$user" --mods "$work/mod" --track TR1 --node 20 --screenshot "$work/mod20.ppm" >/dev/null 2>"$work/err" || fail "the modded track did not render: $(cat "$work/err")"
"$tnfs" --data "$orig" --user "$user" --mods "$work/mod" --track TR1 --node 90 --screenshot "$work/mod90.ppm" >/dev/null 2>&1 || fail "the modded track did not render at node 90"
cmp -s "$work/orig20.ppm" "$work/mod20.ppm" && fail "the screenshot at the edited node is the same as the original's"
cmp -s "$work/orig90.ppm" "$work/mod90.ppm" || fail "a node far from the edit changed"

# 2. an invalid mod file stops the run: file and reason on stderr, a clean failure
cat "$tri" > "$work/mod/SIMDATA/MISC/TR1.TRI"
printf '\130\002' | dd of="$work/mod/SIMDATA/MISC/TR1.TRI" bs=1 seek=6 conv=notrunc 2>/dev/null     # 600 segments
"$tnfs" --data "$orig" --user "$user" --mods "$work/mod" --track TR1 --node 20 --screenshot "$work/bad.ppm" >"$work/out" 2>"$work/err"
status=$?
[ "$status" -ne 0 ] || fail "an invalid mod file was accepted"
[ "$status" -lt 128 ] || fail "an invalid mod file crashed the program (status $status)"
grep -q "SIMDATA/MISC/TR1.TRI" "$work/err" || fail "the invalid file is not named: $(cat "$work/err")"
grep -q "TRI.SEGMENTS" "$work/err" || fail "the reason is not given: $(cat "$work/err")"
[ -f "$work/bad.ppm" ] && fail "a frame was produced from an invalid mod file"
"$tnfs" --data "$orig" --user "$user" --mods "$work/nonexistent" --track TR1 --node 20 >/dev/null 2>"$work/err" && fail "a missing mods directory was accepted"
grep -q "does not exist" "$work/err" || fail "the missing mods directory is not explained"

# 3. nothing below the data root changed (and the player's files, if any, are beside the mod)
changed=$(find -L "$orig" -newer "$work/marker" | head -3)
[ -z "$changed" ] || fail "files below the data root changed: $changed"

[ "$failures" -eq 0 ] && echo "mods_overlay: all checks passed"
exit "$failures"
