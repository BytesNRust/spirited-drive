#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# tools/gate_pictures.sh with made-up tools in place of the picture checks: whatever they print, only its verdict lines come out (the CI log is public and the real tools' output
# carries what they read from the CD), and each way of failing is a failure. No game, no CD data.
# The same for tools/ci_skipped_parts.sh: the number of SKIPPED-PART lines of a test log, and nothing else of it.
# usage: gate_pictures.sh REPO
set -u
repo=${1:?usage: gate_pictures.sh REPO}
work=$(mktemp -d)
trap 'rm -rf "${work:?}"' EXIT
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }

tools=$work/tools
mkdir -p "$tools" "$work/build" "$work/data"
secret=CD-SECRET-WORDS
cat > "$tools/golden_frames.sh" <<'SH'
echo "$SECRET: a log of the game" >&2; echo "$SECRET"
[ "${FAKE_GOLDEN:-good}" = none ] && exit 1
if [ "${FAKE_GOLDEN:-good}" = diff ]; then sed '1s/^[0-9a-f]*/00000000000000000000000000000000/' "$FAKE_MD5" > "$2"; else cp "$FAKE_MD5" "$2"; fi
SH
cat > "$tools/frame_compare.sh" <<'SH'
echo "$SECRET: MISSING (see a log)" >&2
i=0; while [ $i -lt 89 ]; do echo "same  F$i"; i=$((i + 1)); done
case ${FAKE_FRAMES:-good} in
good) echo "same  F89" ;;
diff) echo "DIFF  TR1_v0_4x3_exe"; exit 1 ;;
short) exit 1 ;;
esac
SH
cat > "$tools/menu_compare.sh" <<'SH'
echo "$SECRET on stderr" >&2
case ${FAKE_MENUS:-good} in
good) echo "options: same"; echo "extras: new in B (binA never opened it, see /x/$SECRET); not compared"; echo "menu_compare: 1 same, 0 differ, 0 failed, 1 new in B, of 2" ;;
diff) echo "options: differs 12 px (diff: /x/$SECRET.png)"; echo "content: differs, no pixel count ($SECRET); diff: /x"; echo "menu_compare: 0 same, 2 differ, 0 failed, of 2"; exit 1 ;;
walk) echo "options: walk failed (B), see /x/$SECRET.log"; echo "menu_compare: 0 same, 0 differ, 1 failed, of 1"; exit 3 ;;
leftout) echo "added-setup: left out (no car piece: see $SECRET)" >&2; echo "options: same"; echo "menu_compare: 1 same, 0 differ, 0 failed, of 1" ;;
silent) exit 0 ;;
esac
echo "$SECRET after"
SH
printf '#!/bin/sh\necho %s\nexit "${FAKE_WINDOW:-124}"\n' "$secret" > "$work/build/tnfs"
cp "$work/build/tnfs" "$work/ref_tnfs"
chmod +x "$work/build/tnfs" "$work/ref_tnfs"

run() {   # run ENV...: the checks with the made-up tools; its output in $out, its status in $rc
    rm -rf "${work:?}/out"
    out=$(env SECRET=$secret FAKE_MD5="$repo/tests/golden/frames.md5" GATE_TOOLS="$tools" GATE_SH=sh GATE_DATA="$work/data" "$@" \
        sh "$repo/tools/gate_pictures.sh" "$work/ref_tnfs" "$work/build" "$work/out" 2>&1)
    rc=$?
    case $out in *"$secret"*) fail "a tool's own output came out ($*): $(printf '%s\n' "$out" | grep -m3 "$secret")" ;; esac
}
has() { printf '%s\n' "$out" | grep -qxF "$1" || fail "no line '$1' in: $out"; }

run FAKE_GOLDEN=good
[ $rc -eq 0 ] || fail "all good is status $rc: $out"
has "golden: 15 same, 0 DIFF, 0 missing"
has "frame_compare: 90 same, 0 DIFF of 90"
has "window_bands: ok"
has "menu options: same"
has "menu extras: new in B"
has "gate_pictures: ok"

refused() {   # refused LINE ENV...: not status 0, LINE said
    line=$1; shift
    run "$@"
    [ $rc -ne 0 ] || fail "$* passed: $out"
    has "$line"
}
refused "golden: 14 same, 1 DIFF, 0 missing" FAKE_GOLDEN=diff
refused "golden: 0 same, 0 DIFF, 15 missing" FAKE_GOLDEN=none
refused "frame DIFF TR1_v0_4x3_exe" FAKE_FRAMES=diff
refused "frame_compare: 89 same, 0 DIFF of 90" FAKE_FRAMES=short
refused "window_bands: FAILED (status 3)" FAKE_WINDOW=3
refused "window_bands: FAILED (status 0)" FAKE_WINDOW=0   # it must still be running when the time is up
refused "menu options: differs 12 px" FAKE_MENUS=diff
refused "menu content: differs, no pixel count" FAKE_MENUS=diff
refused "menu options: walk failed (B)" FAKE_MENUS=walk
refused "menu added-setup: left out" FAKE_MENUS=leftout
refused "menu_compare: FAILED (status 0, no summary)" FAKE_MENUS=silent

mkdir -p "$work/b/Testing/Temporary"
printf '%s\nSKIPPED-PART: a: %s\n%s\nSKIPPED-PART: b\n' $secret $secret $secret > "$work/b/Testing/Temporary/LastTest.log"
out=$(sh "$repo/tools/ci_skipped_parts.sh" "$work/b" 2>&1) || fail "ci_skipped_parts: $out"
[ "$out" = "skipped parts: 2" ] || fail "ci_skipped_parts says: $out"
if out=$(sh "$repo/tools/ci_skipped_parts.sh" "$work/none" 2>&1); then fail "ci_skipped_parts with no test log passed: $out"; fi

[ "$failures" -eq 0 ] && echo "gate_pictures: ok"
exit "$failures"
