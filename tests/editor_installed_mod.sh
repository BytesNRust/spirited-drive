#!/bin/sh
# Studio opens projects, not installed mods (owner 2026-11-24): a car and a track folder in the player's mods/ given on the command line are refused with
# «installed mods are not edited here», the process ends with a failure, and not one file in them is written (no project.ini, trace.json) or changed; the same car folder copied into a workspace opens.
# usage: editor_installed_mod.sh EDITOR REPO
set -u
editor=$1
work=$(mktemp -d)
trap 'find "$work" -delete' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
mkdir -p "$work/u/mods/zzcar/SIMDATA/CARFAMS" "$work/u/mods/zztrk" "$work/ws"
printf 'id=zzcar\nname=Z\nversion=1\ntype=car\nstem=ZZCAR\nsound_from=viper\n' > "$work/u/mods/zzcar/mod.ini"
printf 'x' > "$work/u/mods/zzcar/SIMDATA/CARFAMS/ZZCAR.CFM"
printf 'id=zztrk\nname=T\nversion=1\ntype=track\nstem=XTRK\n' > "$work/u/mods/zztrk/mod.ini"
snap() { (cd "$work/u/mods" && find . -type f | sort | while read -r f; do printf '%s ' "$f"; cksum < "$f"; done); }
before=$(snap)
for d in zzcar zztrk; do
    out=$("$editor" --user "$work/u" --workspace "$work/ws" --frames 4 "$work/u/mods/$d" 2>&1) && fail "$d: the installed mod was opened: $out"
    echo "$out" | grep -q "installed mods are not edited here" || fail "$d: no refusal in words: $out"
done
[ "$(snap)" = "$before" ] || fail "a file in an installed mod was written or changed: $(snap)"
cp -R "$work/u/mods/zzcar" "$work/ws/zzcar"
out=$("$editor" --user "$work/u" --workspace "$work/ws" --frames 4 "$work/ws/zzcar" 2>&1) || fail "the same car in a workspace was not opened: $out"
[ "$failures" = 0 ] || exit 1
echo ok
