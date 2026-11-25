#!/bin/sh
# The downloads are the game only (owner 2026-11-25): Studio is in progress and stays out. Three things are checked:
#  - the workflows: Release builds with -DTNFS_EDITOR=OFF and packs with GAME_ONLY=1; Build Test has a `studio` input, off by default, that decides the editor's build and the package (and nothing else does);
#  - tools/package_macos.sh honours GAME_ONLY=1 (the game only, whatever the build holds);
#  - tools/check_dmg_contents.sh (the workflows run it on the DMG): a DMG with the game only passes with STUDIO 0 and fails with 1; one with Studio too passes with 1 and fails with 0; a stray file fails.
# usage: tests/package_game_only.sh <repo root>      Exit 77 (skipped) where there is no hdiutil (the DMG part).
set -u
repo=$1
fails=0
fail() { echo "FAIL: $*" >&2; fails=$((fails + 1)); }
rel=$repo/.github/workflows/release-macos.yml
bt=$repo/.github/workflows/build-test.yml
pk=$repo/tools/package_macos.sh
grep -q -- '-DTNFS_EDITOR=OFF' "$rel" || fail "the release workflow does not configure with -DTNFS_EDITOR=OFF"
grep -q 'GAME_ONLY=1' "$rel" || fail "the release workflow does not package with GAME_ONLY=1"
grep -q 'check_dmg_contents.sh' "$rel" || fail "the release workflow does not check the DMG's contents"
grep -q 'REQUIRE_EDITOR' "$rel" && fail "the release workflow still demands the editor (REQUIRE_EDITOR)"
awk '/^      studio:/ {f=1} f && /default:/ {print $2; exit}' "$bt" | grep -qx false || fail "Build Test has no \`studio\` input with default: false"
grep -q 'TNFS_EDITOR=${{ inputs.studio' "$bt" || fail "Build Test's editor build does not come from the studio input"
grep -q 'GAME_ONLY=1' "$bt" || fail "Build Test does not package with GAME_ONLY=1 when studio is off"
grep -q 'check_dmg_contents.sh' "$bt" || fail "Build Test does not check the DMG's contents"
grep -q 'GAME_ONLY' "$pk" || fail "package_macos.sh does not know GAME_ONLY"
if ! command -v hdiutil > /dev/null 2>&1; then
    [ "$fails" = 0 ] && { echo "ok (workflow checks only: no hdiutil, the DMG part is skipped)"; exit 0; }
    exit 1
fi
work=$(mktemp -d)
trap 'find "$work" -delete' EXIT
chk=$repo/tools/check_dmg_contents.sh
mk() {   # NAME [extra entry]...: a DMG with the game, the Applications link and the extra entries
    name=$1
    shift
    mkdir -p "$work/$name/Spirited Drive.app/Contents/MacOS"
    : > "$work/$name/Spirited Drive.app/Contents/MacOS/tnfs"
    chmod +x "$work/$name/Spirited Drive.app/Contents/MacOS/tnfs"
    ln -s /Applications "$work/$name/Applications"
    for extra in "$@"; do mkdir -p "$work/$name/$extra"; done
    hdiutil create -quiet -volname T -srcfolder "$work/$name" -format UDZO -fs HFS+ "$work/$name.dmg"
}
mk game
mk both "Spirited Drive Studio.app"
mk stray "Notes"
"$chk" "$work/game.dmg" 0 > /dev/null 2>&1 || fail "a game-only DMG did not pass with STUDIO 0"
"$chk" "$work/game.dmg" 1 > /dev/null 2>&1 && fail "a game-only DMG passed with STUDIO 1 (Studio is missing)"
"$chk" "$work/both.dmg" 1 > /dev/null 2>&1 || fail "a DMG with Studio did not pass with STUDIO 1"
"$chk" "$work/both.dmg" 0 > /dev/null 2>&1 && fail "a DMG with Studio passed with STUDIO 0 (it must be the game only)"
"$chk" "$work/stray.dmg" 0 > /dev/null 2>&1 && fail "a DMG with a stray folder passed"
[ "$fails" = 0 ] || exit 1
echo "ok"
