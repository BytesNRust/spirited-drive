#!/bin/sh
# The first start's black window (a new install once sat black for eight seconds before the boot picture): the game draws the boot picture before the work of its first
# seconds (the link, the pads, the menus' files), so that work is not a black window. A fresh user folder (no art cache, no settings) and the headless drivers: the log's
# "startup:" lines say where the start stood when the picture was up, and the marks keep their order (intro, then boot).
# usage: tests/first_start_boot_picture.sh <tnfs binary> <original/ dir>      (run from the repo's root)  Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_firststart.XXXXXX")
printf 'profile=restored\npsx_dir=\n' > "$work/port.cfg"
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TNFS_VERBOSE=1 TNFS_PREF_DIR="$work/pref" timeout -k 3 14 "$bin" --data "$original" --user "$work" > /dev/null 2>&1
log="$work/spirited-drive.log"
line() { grep -an "$1" "$log" | head -1 | cut -d: -f1; }
status=0
fail() { echo "FAIL: $1"; status=1; }
picture=$(line 'startup: boot picture at'); link=$(line 'startup: link at'); pads=$(line 'startup: pads at'); intro=$(line 'mark:screen=intro'); boot=$(line 'mark:screen=boot'); hold=$(line 'mark:hold screen=boot')
for n in picture link pads intro boot hold; do eval "v=\$$n"; [ -n "$v" ] || fail "the log has no line for $n"; done
[ "$status" = 0 ] || { tail -20 "$log"; rm -rf "$work"; exit 1; }
[ "$picture" -lt "$link" ] || fail "the boot picture came after the link's start (the work of the first seconds runs behind a black window)"
[ "$link" -lt "$pads" ] || fail "the start's steps are out of order"
[ "$pads" -lt "$intro" ] && [ "$intro" -lt "$boot" ] && [ "$boot" -lt "$hold" ] || fail "the marks are out of order (intro, boot, hold)"
[ "$(grep -ac 'mark:screen=boot' "$log")" = 1 ] || fail "the boot screen was marked more than once"
[ "$status" = 0 ] && echo "ok: the boot picture is up before the link, the pads and the menus"
rm -rf "$work"
exit $status
