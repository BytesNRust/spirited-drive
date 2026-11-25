#!/bin/sh
# The game's executable starts and leaves cleanly, with the SDL dummy drivers and no game data (nothing to download, no window): the Windows job runs it on
# build/tnfs.exe, and a packaged copy can be given instead. Two runs, each in a scratch player's folder:
#   --mods-list      lists the installed mods and exits 0 (the program starts, finds its folders, opens its log)
#   --data <empty>   says what is missing and exits 1 (a refusal, not a crash: a crash is another status or a hang the timeout ends)
# usage: tests/exe_smoke.sh <tnfs executable>
set -u
exe=$1
[ -f "$exe" ] || { echo "exe_smoke: no $exe"; exit 2; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_smoke.XXXXXX")
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

timeout -k 5 60 "$exe" --user "$work/u1" --mods-list > "$work/out1" 2>&1
rc=$?
[ "$rc" -eq 0 ] || fail "--mods-list ended with status $rc: $(tail -3 "$work/out1")"
[ -s "$work/u1/spirited-drive.log" ] || fail "--mods-list left no log in the player's folder: $(ls "$work/u1" 2>&1)"

mkdir -p "$work/empty"
timeout -k 5 60 "$exe" --user "$work/u2" --data "$work/empty" > "$work/out2" 2>&1
rc=$?
[ "$rc" -eq 1 ] || fail "a run with no game data ended with status $rc, not 1: $(tail -3 "$work/out2")"
grep -q "NFS_WIN.EXE not found" "$work/out2" || fail "a run with no game data did not say what is missing: $(tail -3 "$work/out2")"
[ -s "$work/u2/spirited-drive.log" ] || fail "the run with no game data left no log"

[ "$failures" -eq 0 ] && echo "exe_smoke: ok"
exit "$failures"
