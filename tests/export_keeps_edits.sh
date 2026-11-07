#!/bin/sh
# tools/export_track_light_install.sh: a file that exists is kept (and said so), the missing ones are written, FORCE=1 overwrites. No game run: a staged folder stands in for the export's.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
install=$here/../tools/export_track_light_install.sh
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_export_keeps.XXXXXX")
trap 'find "$work" -delete' EXIT
staged=$work/staged data=$work/data
mkdir -p "$staged" "$data/track" "$data/track_light"
printf 'new places\n' > "$staged/CL2.places.keep"
printf 'new places\n' > "$staged/TR7.places.keep"
printf 'new light\n' > "$staged/CL2B.txt"
printf 'new light\n' > "$staged/TR7B.txt"
printf 'mine\n' > "$data/track/CL2.txt"
printf 'mine\n' > "$data/track_light/CL2B.txt"
fail() { echo "FAIL: $*" >&2; exit 1; }
out=$(sh "$install" "$staged" "$data")
[ "$(cat "$data/track/CL2.txt")" = mine ] || fail "an existing places file was overwritten"
[ "$(cat "$data/track_light/CL2B.txt")" = mine ] || fail "an existing light file was overwritten"
[ "$(cat "$data/track/TR7.txt")" = "new places" ] || fail "a missing places file was not written"
[ "$(cat "$data/track_light/TR7B.txt")" = "new light" ] || fail "a missing light file was not written"
echo "$out" | grep -q "kept $data/track/CL2.txt (exists; FORCE=1 to overwrite)" || fail "no 'kept' line for the places file: $out"
echo "$out" | grep -q "kept $data/track_light/CL2B.txt (exists; FORCE=1 to overwrite)" || fail "no 'kept' line for the light file: $out"
echo "$out" | grep -q "TR7" && fail "a written file was reported as kept: $out"
out=$(FORCE=1 sh "$install" "$staged" "$data")
[ "$(cat "$data/track/CL2.txt")" = "new places" ] || fail "FORCE=1 did not overwrite the places file"
[ "$(cat "$data/track_light/CL2B.txt")" = "new light" ] || fail "FORCE=1 did not overwrite the light file"
[ -z "$out" ] || fail "FORCE=1 said something: $out"
echo "export_keeps_edits: ok"
