#!/bin/sh
# tools/check_release_notes.sh: notes with a draft marker ("DRAFT", "[X.Y.Z]", "[WIN]") fail and say which; clean notes pass; a missing file passes.
# usage: tests/check_release_notes.sh <repo root>
set -u
repo=$1
chk=$repo/tools/check_release_notes.sh
work=$(mktemp -d)
trap 'find "$work" -delete' EXIT
fails=0
fail() { echo "FAIL: $*" >&2; fails=$((fails + 1)); }
printf '# Spirited Drive 1.0.0\n\nA drafted rewrite, and a draft of the install steps.\n\n## Install\n\n1. Download the file.\n' > "$work/clean.md"
printf '# DRAFT: Spirited Drive\n\ntext\n' > "$work/draft.md"
printf '# Spirited Drive\n\nDownload Spirited-Drive-[X.Y.Z]-macos.dmg\n' > "$work/version.md"
printf '# Spirited Drive\n\n[WIN] the Windows install steps\n' > "$work/win.md"
"$chk" "$work/clean.md" > /dev/null 2>&1 || fail "notes without a marker did not pass (lower case 'draft' is text, not a marker)"
"$chk" "$work/missing.md" > /dev/null 2>&1 || fail "a missing file did not pass"
for f in draft:DRAFT version:'[X.Y.Z]' win:'[WIN]'; do
    name=${f%%:*}
    marker=${f#*:}
    out=$("$chk" "$work/$name.md" 2>&1) && fail "$name.md passed with the marker $marker"
    echo "$out" | grep -qF -- "$marker" || fail "the message does not name $marker: $out"
    echo "$out" | grep -q "line [0-9]" || fail "the message does not give the line: $out"
done
"$chk" a b > /dev/null 2>&1 && fail "two arguments did not fail"
# the repository's own notes must still be a file the check can read (they pass or fail by what they hold, never crash)
"$chk" "$repo/RELEASE_NOTES.md" > /dev/null 2>&1; rc=$?
[ "$rc" = 0 ] || [ "$rc" = 1 ] || fail "the real RELEASE_NOTES.md made the check crash (exit $rc)"
[ "$fails" = 0 ] && echo "ok"
[ "$fails" = 0 ]
