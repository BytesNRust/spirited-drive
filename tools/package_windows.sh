#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Packs a built tnfs.exe into a zip a player unpacks and runs: Spirited-Drive-<version>-windows-x64.zip. The program in it is called "Spirited Drive.exe" (the build's own name stays tnfs.exe).
#
#   tools/package_windows.sh BUILD_DIR OUT_DIR
#
# Run it in MSYS2's UCRT64 shell (the Windows job does). BUILD_DIR holds tnfs.exe (configure it with -DTNFS_RELEASE=ON) and
# generated/tnfs_version.c. The zip holds one folder "Spirited Drive" with: Spirited Drive.exe, every DLL it needs from the UCRT64 tree
# (SDL3.dll and the compiler runtime: found with ldd, nothing guessed), our own data/ (only what git tracks, beside the exe where the
# game reads it: SDL_GetBasePath), the licences (ours and those of the DLLs) and a short README. No game data: players bring their own CD.
set -eu

[ $# -eq 2 ] || { echo "usage: $0 BUILD_DIR OUT_DIR" >&2; exit 2; }
BUILD_DIR=$(cd "$1" && pwd)
mkdir -p "$2"
OUT_DIR=$(cd "$2" && pwd)
SRC_DIR=$(cd "$(dirname "$0")/.." && pwd)
EXE="$BUILD_DIR/tnfs.exe"
VERSION_C="$BUILD_DIR/generated/tnfs_version.c"
[ -f "$EXE" ] || { echo "no tnfs.exe in $BUILD_DIR" >&2; exit 1; }
[ -f "$VERSION_C" ] || { echo "no generated/tnfs_version.c in $BUILD_DIR" >&2; exit 1; }
VERSION=$(sed -n 's/^const char tnfs_version\[\] = "\(.*\)";.*/\1/p' "$VERSION_C")
[ -n "$VERSION" ] || { echo "cannot read the version from $VERSION_C" >&2; exit 1; }

STAGE="$OUT_DIR/stage"
DIR="$STAGE/Spirited Drive"
rm -rf "$STAGE"
mkdir -p "$DIR"
cp "$EXE" "$DIR/Spirited Drive.exe"

# the icon and the name are a resource (game/sys/spirited.rc.in): an exe built without it has the system's default icon, and a package of it is not shipped
objdump -h "$EXE" | grep -q '\.rsrc' || { echo "$EXE has no resource section: the .rc was not compiled in (enable_language(RC)), so no Spirited Drive icon" >&2; exit 1; }
# and the manifest that makes its C library calls take UTF-8 paths (game/sys/utf8.manifest): the XML is stored as text in the resource
grep -aq 'activeCodePage' "$EXE" || { echo "$EXE carries no UTF-8 manifest: a user name outside the ANSI code page could not open its own folder" >&2; exit 1; }
# the release is a GUI program (no console window beside the game): PE subsystem 2, not 3 (CMakeLists.txt, WIN32_EXECUTABLE with TNFS_RELEASE)
objdump -p "$EXE" | grep -q '^Subsystem.*Windows GUI' || { echo "$EXE is a console program: Explorer would open a black console window beside the game (configure with -DTNFS_RELEASE=ON)" >&2; exit 1; }

# the DLLs the exe loads that are not Windows' own: ldd lists them with their path in the UCRT64 tree (/ucrt64/bin); the system's live under /c/Windows
ldd "$EXE" | awk '/=>/ {print $3}' | grep -i '/ucrt64/' | sort -u > "$STAGE/dlls.txt" || true
grep -qi 'SDL3' "$STAGE/dlls.txt" || { echo "ldd shows no SDL3.dll for tnfs.exe" >&2; cat "$STAGE/dlls.txt" >&2; exit 1; }
while IFS= read -r dll; do cp "$dll" "$DIR/"; done < "$STAGE/dlls.txt"

# our own data: only the files git tracks under data/ (a dev machine's untracked files never ship)
if git -C "$SRC_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    (cd "$SRC_DIR" && git ls-files -z -- data | xargs -0 -I{} sh -c 'mkdir -p "$1/$(dirname "$2")" && cp "$2" "$1/$2"' _ "$DIR" {})
else
    echo "$SRC_DIR is not a git checkout: no data/ in the package" >&2
    exit 1
fi
[ -d "$DIR/data/car_masks" ] || { echo "the package has no data/car_masks" >&2; exit 1; }
for doc in LICENSE NOTICE.md; do [ -f "$SRC_DIR/$doc" ] && cp "$SRC_DIR/$doc" "$DIR/"; done
[ -d "$SRC_DIR/LICENSES" ] && cp -R "$SRC_DIR/LICENSES" "$DIR/LICENSES"

# the licence texts of the DLLs shipped (SDL3: zlib, libgcc_s: GPL with the runtime exception, libwinpthread: its notice must travel with the binary):
# the files their MSYS2 packages install under share/licenses, found by asking pacman which package owns each DLL
mkdir -p "$DIR/LICENSES/third-party"
while IFS= read -r dll; do
    pkg=$(pacman -Qoq "$dll") || { echo "pacman does not know which package owns $dll" >&2; exit 1; }
    found=0
    for lic in $(pacman -Ql "$pkg" | awk '$2 ~ /\/share\/licenses\// && $2 !~ /\/$/ {print $2}'); do
        mkdir -p "$DIR/LICENSES/third-party/$pkg"
        cp "$lic" "$DIR/LICENSES/third-party/$pkg/"
        found=1
    done
    [ "$found" = 1 ] || { echo "package $pkg (for $dll) installs no licence text under share/licenses" >&2; exit 1; }
done < "$STAGE/dlls.txt"
rm "$STAGE/dlls.txt"

# the text is the approved wording of the README and the release notes (tests/readme_wording.py checks this file too)
cat > "$DIR/README.txt" <<EOF
Spirited Drive $VERSION (Windows)

The Need for Speed SE (1996), rebuilt to run natively on today's computers. Free and open source.

How to start
1. Unpack this folder anywhere you can write to (not Program Files).
2. Run Spirited Drive.exe. The first time, Windows will warn that the app is from an unknown publisher, because it is not signed yet: choose More info, then Run anyway.
3. On the first start, point the game at your own CD, a folder with its files, or an ISO image of your own disc. It copies what it needs, and you can put the CD away.

Bring your own CD. Spirited Drive contains no game data: you need your own copy of The Need for Speed Special Edition for PC (1996).

Three ways to play: Stock '96 (the 1996 game as it shipped), Restored '96 (the same game on a modern screen) and Restomod '26 (weather, four times of day, night lighting, cars that shine and more, all of it still 8-bit, palette-indexed art). Not sure? Take Restored '96. You can change it at any time on the Game Mode screen.

Network races are not in the Windows version yet.

Your player folder is %APPDATA%\\Spirited Drive\\ (type it in the Explorer address bar). It keeps your settings, your records and your copy of the game's files, and they stay when you replace the program with a newer one.
The log, spirited-drive.log, is in that folder too. In the game: Customize Game, page 3, Open the log folder. Attach it when you report a bug: https://github.com/BytesNRust/spirited-drive/issues (please do not attach files from the original game).

This game is not finished and is not meant to be: what you find shapes what comes next.
EOF

ZIP="Spirited-Drive-$VERSION-windows-x64.zip"
rm -f "$OUT_DIR/$ZIP"
(cd "$STAGE" && cmake -E tar cf "$OUT_DIR/$ZIP" --format=zip "Spirited Drive")
rm -rf "$STAGE"
echo "$OUT_DIR/$ZIP"
