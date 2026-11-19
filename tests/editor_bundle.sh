#!/bin/sh
# The track editor runs from an app bundle copied to another folder of another Mac, with nothing of the source tree beside it: a new project carries its
# own copy of the generic art (Contents/Resources/GENERIC.FAM), builds without reading anything outside the project, and a project of the first Studio
# (art= a path only the source tree had) is given the copy; a program with no art beside it says so, and a project's own art= is never touched.
# usage: editor_bundle.sh EDITOR GENERIC_FAM     (headless: never run without a PROJECT, it would open a window on the real Documents)
set -u
editor=$1
fam=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }

app="$work/Applications/Spirited Drive Studio.app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" "$work/elsewhere" "$work/docs"
cp "$editor" "$app/Contents/MacOS/tnfs-track-editor"
cp "$fam" "$app/Contents/Resources/GENERIC.FAM"
bundled="$app/Contents/MacOS/tnfs-track-editor"
cd "$work/elsewhere" || exit 1

# 1. a new project: its own copy of the art, art=GENERIC.FAM, and a build that makes the zip
"$bundled" "$work/docs/my-track" --frames 2 --build >"$work/log1" 2>&1 || fail "new project build: $(cat "$work/log1")"
cmp -s "$fam" "$work/docs/my-track/GENERIC.FAM" || fail "the new project has no copy of the art"
grep -q '^art=GENERIC.FAM$' "$work/docs/my-track/project.ini" || fail "project.ini does not name the project's own art: $(grep art= "$work/docs/my-track/project.ini")"
[ -s "$work/docs/my-track/build/my-track.zip" ] || fail "no zip was built"
grep -q "cannot read the art" "$work/log1" && fail "the build could not read the art"

# 2. a project of the first Studio: art= the source tree's path, which is not here
mkdir -p "$work/docs/old"
cp "$work/docs/my-track/trace.json" "$work/docs/old/trace.json"
printf 'id=old\nname=old\nauthor=\nstem=EDIT\nart=../generic-straight/GENERIC.FAM\nredistributable=1\n' > "$work/docs/old/project.ini"
"$bundled" "$work/docs/old" --frames 2 --build >"$work/log2" 2>&1 || fail "old project build: $(cat "$work/log2")"
grep -q '^art=GENERIC.FAM$' "$work/docs/old/project.ini" || fail "the old project's art= was not rewritten: $(cat "$work/docs/old/project.ini")"
cmp -s "$fam" "$work/docs/old/GENERIC.FAM" || fail "the old project has no copy of the art"
[ -s "$work/docs/old/build/old.zip" ] || fail "the old project built no zip"
grep -q '^redistributable=1$' "$work/docs/old/project.ini" && grep -q '^stem=EDIT$' "$work/docs/old/project.ini" || fail "the rewrite lost a line of project.ini"

# 3. a project's own art is never touched, even when it is missing
mkdir -p "$work/docs/own"
cp "$work/docs/my-track/trace.json" "$work/docs/own/trace.json"
printf 'id=own\nname=own\nauthor=\nstem=EDIT\nart=mine/MY.FAM\nredistributable=1\n' > "$work/docs/own/project.ini"
"$bundled" "$work/docs/own" --frames 2 --build >"$work/log3" 2>&1 && fail "a build with a missing own art was accepted"
grep -q "MY.FAM: cannot read the art" "$work/log3" || fail "the refusal is not about the project's own art: $(cat "$work/log3")"
grep -q '^art=mine/MY.FAM$' "$work/docs/own/project.ini" || fail "a project's own art= was rewritten"
[ ! -e "$work/docs/own/GENERIC.FAM" ] || fail "a copy of the art was put in a project that has its own"

# 4. a program with no art beside it: a new project says what is wrong instead of pointing at a path that is not there
mkdir -p "$work/bare"
cp "$editor" "$work/bare/tnfs-track-editor"
"$work/bare/tnfs-track-editor" "$work/docs/bare" --frames 2 --build >"$work/log4" 2>&1 && fail "a build with no art was accepted"
grep -q "set art=" "$work/log4" || fail "no word on the missing art: $(cat "$work/log4")"
grep -q "generic-straight" "$work/log4" && fail "the message still points at the source tree"

# 5. a project made where the art was missing is given it once the editor has it (an empty art=), and a comment that quotes the old path is left as it is
printf '# the first Studio wrote art=../generic-straight/GENERIC.FAM\nid=bare\nname=bare\nauthor=\nstem=EDIT\nart=\nredistributable=1\n' > "$work/docs/bare/project.ini"
"$bundled" "$work/docs/bare" --frames 2 --build >"$work/log5" 2>&1 || fail "the project made without art did not build once the art was there: $(cat "$work/log5")"
grep -q '^art=GENERIC.FAM$' "$work/docs/bare/project.ini" || fail "the empty art= was not given the copy: $(cat "$work/docs/bare/project.ini")"
grep -q '^# the first Studio wrote art=../generic-straight/GENERIC.FAM$' "$work/docs/bare/project.ini" || fail "a comment was rewritten"

[ "$failures" -eq 0 ] && echo "editor_bundle: ok"
exit "$failures"
