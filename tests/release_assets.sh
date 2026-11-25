#!/bin/sh
# The files a release offers, and where the game's update notice sends a player: the notice opens UPDATE_RELEASES_PAGE ("the latest release"), so every platform's download has to be on the SAME release and a player has to tell his
# file by its name. Checks the two release workflows and the Windows packer (pure text, no build): both trigger on the same tags, the Windows one attaches to the tag's release and never creates one, the names of the files do not
# collide (one checksum list a platform), the zip is named Spirited-Drive-<version>-windows-x64.zip by the packer and the workflow checks the build's version against the tag.
# usage: tests/release_assets.sh      (run from the repo's root)
set -u
fail=0
bad() { echo "FAIL: $1"; fail=1; }
mac=.github/workflows/release-macos.yml
win=.github/workflows/release-windows.yml
pack=tools/package_windows.sh
page=$(sed -n 's|^#define UPDATE_RELEASES_PAGE  *"\(.*\)"|\1|p' core/update/update_check.h)
for f in "$mac" "$win" "$pack" core/update/update_check.h; do [ -f "$f" ] || bad "$f is missing"; done
[ $fail -eq 0 ] || exit 1
case "$page" in */releases/latest) ;; *) bad "the update notice does not open the latest release's page ($page)" ;; esac
grep -q "tags: \['v\*'\]" "$mac" && grep -q "tags: \['v\*'\]" "$win" || bad "the two release workflows do not trigger on the same tags (v*)"
grep -q 'gh release upload "\$GITHUB_REF_NAME"' "$win" || bad "the Windows workflow does not attach to the tag's release"
grep -q 'gh release create' "$win" && bad "the Windows workflow creates a release of its own: two releases for one version, and the notice sees only one"
grep -q 'gh release create "\$GITHUB_REF_NAME"' "$mac" || bad "the macOS workflow does not open the tag's release"
grep -q -- '--draft' "$mac" || bad "the macOS workflow's release is not a draft"
grep -q 'SHA256SUMS-windows.txt' "$win" || bad "the Windows checksums are not in a file of their own"
grep -q 'SHA256SUMS.txt' "$mac" && ! grep -q 'SHA256SUMS.txt' "$win" || bad "the two checksum files have the same name: the second upload replaces the first"
grep -q 'tnfs_version' "$win" && grep -q 'GITHUB_REF_NAME#v' "$win" || bad "the Windows workflow does not check the build's version against the tag"
grep -q 'Spirited-Drive-\$VERSION-windows-x64.zip' "$pack" || bad "the Windows zip is not named Spirited-Drive-<version>-windows-x64.zip"
grep -q 'windows-x64.zip\|\*\.zip' "$win" || bad "the Windows workflow does not upload the zip"
grep -qi 'prerelease\|pre-release' "$win" && bad "the Windows workflow marks a release pre-release: the notice would skip it"
rc=game/sys/spirited.rc.in
grep -q 'cp "\$EXE" "\$DIR/Spirited Drive.exe"' "$pack" || bad "the packer does not ship the program as Spirited Drive.exe"
grep -q 'tnfs\.exe' "$win" && bad "the Windows workflow still looks for tnfs.exe in the zip"
sed -n '/^cat > "\$DIR\/README.txt"/,/^EOF/p' "$pack" | grep -q 'tnfs\.exe' && bad "the zip's README sends the player to tnfs.exe"
grep -q 'rsrc' "$pack" || bad "the packer does not check the exe carries the icon resource"
grep -q 'activeCodePage' "$pack" || bad "the packer does not check the exe carries the UTF-8 manifest"
grep -q 'Windows GUI' "$pack" || bad "the packer does not check the exe is a GUI program"
grep -q 'WIN32_EXECUTABLE' CMakeLists.txt || bad "CMake does not make the release a GUI program: Explorer would open a console window beside the game"
grep -q 'enable_language(RC)' CMakeLists.txt || bad "CMake does not enable RC: the .rc (icon, name) is not compiled and the exe keeps the default icon"
grep -q '^1 ICON' "$rc" && grep -q 'VALUE "FileDescription", "Spirited Drive"' "$rc" || bad "$rc has no icon or no Spirited Drive name"
[ $fail -eq 0 ] && echo "release_assets: ok"
exit $fail
