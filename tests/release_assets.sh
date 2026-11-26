#!/bin/sh
# The files a release offers, and where the game's update notice sends a player: the notice opens UPDATE_RELEASES_PAGE ("the latest release"), so every platform's download has to be on the SAME release and a player has to
# find his file there by its name. Checks the release workflow and the Windows packer (pure text, no build): one workflow on the v* tags, ONE `gh release create` (a draft, with the tag verified) in the one job that may
# write, which needs both builds (no wait loop, no draft when one build fails), the names of the files do not collide (one checksum list a platform), the zip is named Spirited-Drive-<version>-windows-x64.zip by the packer
# and the workflow checks the build's version against the tag.
# usage: tests/release_assets.sh      (run from the repo's root)
set -u
fail=0
bad() { echo "FAIL: $1"; fail=1; }
rel=.github/workflows/release.yml
pack=tools/package_windows.sh
page=$(sed -n 's|^#define UPDATE_RELEASES_PAGE  *"\(.*\)"|\1|p' core/update/update_check.h)
for f in "$rel" "$pack" core/update/update_check.h; do [ -f "$f" ] || bad "$f is missing"; done
[ $fail -eq 0 ] || exit 1
case "$page" in */releases/latest) ;; *) bad "the update notice does not open the latest release's page ($page)" ;; esac
grep -q "tags: \['v\*'\]" "$rel" || bad "the release workflow does not trigger on the v* tags"
[ -e .github/workflows/release-macos.yml ] || [ -e .github/workflows/release-windows.yml ] && bad "an old per-platform release workflow is still there: two releases for one version, and the notice sees only one"
[ "$(grep -c 'gh release create' "$rel")" = 1 ] || bad "the release workflow does not open exactly one release"
# the Windows job's default shell is msys2: a `run` step before setup-msys2 installs it fails with "msys2: command not found" (the rc1 dry run)
first_run=$(awk '/^  build-windows:/ {f=1; next} /^  [a-z-]+:$/ {f=0} f && /^        run:/ {print NR; exit}' "$rel")
msys_setup=$(grep -n 'msys2/setup-msys2' "$rel" | head -1 | cut -d: -f1)
{ [ -n "$first_run" ] && [ -n "$msys_setup" ] && [ "$msys_setup" -lt "$first_run" ]; } || bad "a run step of build-windows comes before setup-msys2 (its shell is msys2, not there yet)"
grep -q 'gh release create "\$GITHUB_REF_NAME"' "$rel" || bad "the release workflow does not open the tag's release"
grep -q -- '--draft' "$rel" || bad "the release is not a draft"
grep -q -- '--verify-tag' "$rel" || bad "the release does not verify the tag exists"
grep -q 'gh release upload' "$rel" && bad "something uploads into a release after it was created: the wait loop is gone, the one publish job creates it whole"
grep -q 'needs: \[on-main, build-macos, build-windows\]' "$rel" || bad "the publish job does not need the on-main check and both builds"
grep -q 'merge-base --is-ancestor "\$GITHUB_SHA" origin/main' "$rel" || bad "the release does not check the tagged commit is an ancestor of origin/main"
[ "$(grep -c 'contents: write' "$rel")" = 1 ] || bad "more than one job (or none) may write to the repository: only publish may"
grep -q 'dist/\*.dmg' "$rel" && grep -q 'dist/\*.zip' "$rel" || bad "the release does not offer the DMG and the zip"
grep -q 'SHA256SUMS-windows.txt' "$rel" || bad "the Windows checksums are not in a file of their own"
grep -q 'SHA256SUMS.txt' "$rel" || bad "the macOS checksums are missing"
[ "$(grep -c 'tnfs_version' "$rel")" -ge 2 ] && grep -q 'GITHUB_REF_NAME#v' "$rel" || bad "the release workflow does not check each build's version against the tag"
grep -q 'Spirited-Drive-\$VERSION-windows-x64.zip' "$pack" || bad "the Windows zip is not named Spirited-Drive-<version>-windows-x64.zip"
grep -q 'windows-zip' "$rel" || bad "the release workflow does not carry the Windows zip"
grep -v '^ *#' "$rel" | grep -qi 'prerelease\|pre-release' && bad "the release workflow marks a release pre-release: the notice would skip it"
rc=game/sys/spirited.rc.in
grep -q 'cp "\$EXE" "\$DIR/Spirited Drive.exe"' "$pack" || bad "the packer does not ship the program as Spirited Drive.exe"
grep -q 'tnfs\.exe' "$rel" && bad "the release workflow still looks for tnfs.exe in the zip"
sed -n '/^cat > "\$DIR\/README.txt"/,/^EOF/p' "$pack" | grep -q 'tnfs\.exe' && bad "the zip's README sends the player to tnfs.exe"
grep -q 'rsrc' "$pack" || bad "the packer does not check the exe carries the icon resource"
grep -q 'activeCodePage' "$pack" || bad "the packer does not check the exe carries the UTF-8 manifest"
grep -q 'Windows GUI' "$pack" || bad "the packer does not check the exe is a GUI program"
grep -q 'WIN32_EXECUTABLE' CMakeLists.txt || bad "CMake does not make the release a GUI program: Explorer would open a console window beside the game"
grep -q 'enable_language(RC)' CMakeLists.txt || bad "CMake does not enable RC: the .rc (icon, name) is not compiled and the exe keeps the default icon"
grep -q '^1 ICON' "$rc" && grep -q 'VALUE "FileDescription", "Spirited Drive"' "$rc" || bad "$rc has no icon or no Spirited Drive name"
[ $fail -eq 0 ] && echo "release_assets: ok"
exit $fail
