#!/bin/sh
# tools/check_tag_version.sh: a tag that names the project's version passes (also with a -beta suffix); a different version, a tag that is not a version and a missing
# project line fail with a message that says what to do.
# usage: tests/check_tag_version.sh <repo root>
set -u
repo=$1
chk=$repo/tools/check_tag_version.sh
work=$(mktemp -d)
trap 'find "$work" -delete' EXIT
fails=0
fail() { echo "FAIL: $*" >&2; fails=$((fails + 1)); }
printf 'cmake_minimum_required(VERSION 3.20)\nproject(tnfs VERSION 1.2.3 LANGUAGES C)\n' > "$work/ok.txt"
printf 'cmake_minimum_required(VERSION 3.20)\n' > "$work/none.txt"
"$chk" v1.2.3 "$work/ok.txt" > /dev/null 2>&1 || fail "v1.2.3 did not pass for VERSION 1.2.3"
"$chk" v1.2.3-beta.1 "$work/ok.txt" > /dev/null 2>&1 || fail "v1.2.3-beta.1 did not pass for VERSION 1.2.3"
out=$("$chk" v1.2.4 "$work/ok.txt" 2>&1) && fail "v1.2.4 passed for VERSION 1.2.3"
echo "$out" | grep -q "names version 1.2.4" || fail "the message does not name the tag's version: $out"
echo "$out" | grep -q "VERSION 1.2.3" || fail "the message does not name the source's version: $out"
echo "$out" | grep -q "delete the draft" || fail "the message does not say what to do: $out"
"$chk" v2.0.0-rc.1 "$work/ok.txt" > /dev/null 2>&1 && fail "v2.0.0-rc.1 passed for VERSION 1.2.3"
"$chk" 1.2.3 "$work/ok.txt" > /dev/null 2>&1 && fail "a tag without the v passed"
"$chk" vfoo "$work/ok.txt" > /dev/null 2>&1 && fail "vfoo passed"
"$chk" v1.2.3 "$work/none.txt" > /dev/null 2>&1 && fail "a CMakeLists with no project line passed"
"$chk" > /dev/null 2>&1 && fail "no argument did not fail"
# the real file: the version it says passes as its own tag
real=$(sed -n 's/^project(tnfs VERSION \([0-9][0-9.]*\).*/\1/p' "$repo/CMakeLists.txt" | head -n 1)
"$chk" "v$real" "$repo/CMakeLists.txt" > /dev/null 2>&1 || fail "the repository's own version v$real did not pass"
[ "$fails" = 0 ] && echo "ok"
[ "$fails" = 0 ]
