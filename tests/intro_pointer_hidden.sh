#!/bin/sh
# The game's own pointer is not on the screen during the intro's movies (owner 2026-11-25: visible over EA's movie with the mouse still): the boot screen drew it again after the intro had hidden it
# (pointer_read_buttons draws when the pointer is not where it was last drawn: at the start it is not). The first movie of the intro (ESC on the boot screen starts it) is followed by `mark:pointer=0`.
# usage: tests/intro_pointer_hidden.sh <tnfs binary> <original/ dir>      Exit 77 (skipped) without the CD's data.
set -u
bin=$1
original=$2
[ -d "$original/SIMDATA" ] || { echo "skipped: no $original"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/tnfs_intro_pointer.XXXXXX")
mkdir -p "$work/user"
printf 'profile=restomod\npsx_dir=\n' > "$work/user/port.cfg"
# ESC on the boot screen starts EA's first movie (its mark is in the game's log), ESC ends the intro
TNFS_INPUT_SETTLE=300 TNFS_INPUT_SCRIPT="1500:ESC,0:WAIT:mark:movie=:30,500:ESC" \
  SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy TNFS_PREF_DIR="$work/pref" timeout -k 5 30 "$bin" --data "$original" --user "$work/user" > "$work/out" 2>&1
log="$work/user/spirited-drive.log"
[ -f "$log" ] || log="$work/out"
first=$(grep -m1 -A1 '^mark:movie=' "$log" | tail -1)   # the line after the first movie's mark: the pointer's state as it began
echo "the intro's first movie: $(grep -m1 '^mark:movie=' "$log"), then: $first"
status=0
[ -n "$(grep -m1 '^mark:movie=' "$log")" ] || { echo "FAIL: no movie was played: $(tail -3 "$work/out")"; status=1; }
[ "$first" = "mark:pointer=0" ] || { echo "FAIL: the pointer was not hidden as the intro's first movie began ($first)"; status=1; }
[ $status -eq 0 ] && echo "intro_pointer_hidden: ok"
find "$work" -delete
exit $status
