#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Packs a built tnfs.exe into a zip a player unpacks and runs: Spirited-Drive-<version>-windows-x64.zip.
#
#   tools/package_windows.sh BUILD_DIR OUT_DIR
#
# Run it in MSYS2's UCRT64 shell (the Windows job does). BUILD_DIR holds tnfs.exe (configure it with -DTNFS_RELEASE=ON) and
# generated/tnfs_version.c. The zip holds one folder "Spirited Drive" with: tnfs.exe, every DLL it needs from the UCRT64 tree
# (SDL3.dll and the compiler runtime: found with ldd, nothing guessed), our own data/ (only what git tracks, beside the exe where the
# game reads it: SDL_GetBasePath), the licences and a short README. No game data: players bring their own CD.
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
cp "$EXE" "$DIR/tnfs.exe"

# the DLLs the exe loads that are not Windows' own: ldd lists them with their path in the UCRT64 tree (/ucrt64/bin); the system's live under /c/Windows
ldd "$EXE" | awk '/=>/ {print $3}' | grep -i '/ucrt64/' | sort -u > "$STAGE/dlls.txt" || true
grep -qi 'SDL3' "$STAGE/dlls.txt" || { echo "ldd shows no SDL3.dll for tnfs.exe" >&2; cat "$STAGE/dlls.txt" >&2; exit 1; }
while IFS= read -r dll; do cp "$dll" "$DIR/"; done < "$STAGE/dlls.txt"
rm "$STAGE/dlls.txt"

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

cat > "$DIR/README.txt" <<EOF
Spirited Drive $VERSION (Windows)

1. Unpack this folder anywhere you can write to (not Program Files).
2. Run tnfs.exe. Windows may warn that the program is unsigned: More info > Run anyway.
3. On the first start pick your copy of the original game (the CD, a folder with its files, or an ISO) and a profile.

No game data is included: bring your own copy of the original game.
Your settings and records are kept between runs.
EOF

ZIP="Spirited-Drive-$VERSION-windows-x64.zip"
rm -f "$OUT_DIR/$ZIP"
(cd "$STAGE" && cmake -E tar cf "$OUT_DIR/$ZIP" --format=zip "Spirited Drive")
rm -rf "$STAGE"
echo "$OUT_DIR/$ZIP"
