#!/bin/sh
# ~ and $HOME in the command line's folders stand for the home folder (an installed app launched by `open --args` gets them as typed), and a folder that is not there is named.
# usage: editor_tilde_paths.sh EDITOR
set -u
editor=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
mkdir -p "$work/home/repo/data/track_light" "$work/home/repo/data/car_masks" "$work/home/ws" "$work/tmp"
export TMPDIR="$work/tmp/"
for t in AL1 AL2 AL3 CL1 CL2 CL3 CY1 CY2 CY3 TR1 TR2 TR3 TR4 TR5 TR6 TR7; do printf '# tnfs track light 2\ntrack %s\n' "$t" > "$work/home/repo/data/track_light/$t.txt"; done
out=$(HOME="$work/home" "$editor" --user "$work/u" --workspace '~/ws' --port-data '$HOME/repo/data' --light-dir '${HOME}/repo/data' --window 1280x800 --frames 8 --steps add-originals,print-workspace 2>&1) || fail "tilde run: $out"
echo "$out" | grep -q '^tracks: AL1 AL2 AL3 CL1' || fail "~, \$HOME and \${HOME} were not expanded: $out"
[ -f "$work/home/ws/tracks/CL1/light/track_light/CL1.txt" ] || fail "the light file was not copied from \$HOME/repo/data"
out=$(HOME="$work/home" "$editor" --user "$work/u2" --workspace '~/ws' --port-data '~/not/there' --window 1280x800 --frames 6 --steps status 2>&1)
echo "$out" | grep -q "folder not found" && echo "$out" | grep -q "$work/home/not/there" || fail "the missing folder is not named with its expanded path: $out"
[ "$failures" -eq 0 ] && echo "editor_tilde_paths: ok"
exit "$failures"
