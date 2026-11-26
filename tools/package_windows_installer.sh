#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the Windows installer from the zip tools/package_windows.sh made: the same files, so the installer cannot differ from the zip.
#
#   tools/package_windows_installer.sh ZIP OUT_DIR
#
# Needs Inno Setup 6's ISCC (on the PATH, or in Program Files, or in the current user's Programs folder) and runs in MSYS2's shell. Writes
# OUT_DIR/Spirited-Drive-<version>-windows-x64-setup.exe, the version being the zip's own.
set -eu

[ $# -eq 2 ] || { echo "usage: $0 ZIP OUT_DIR" >&2; exit 2; }
[ -f "$1" ] || { echo "no $1" >&2; exit 1; }
ZIP=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
mkdir -p "$2"
OUT_DIR=$(cd "$2" && pwd)
SRC_DIR=$(cd "$(dirname "$0")/.." && pwd)
NAME=$(basename "$ZIP" .zip)
VERSION=${NAME#Spirited-Drive-}
VERSION=${VERSION%-windows-x64}
[ -n "$VERSION" ] && [ "$VERSION" != "$NAME" ] || { echo "cannot read the version from $NAME" >&2; exit 1; }
NUMERIC=$(printf '%s' "$VERSION" | sed -n 's/^\([0-9][0-9]*\)\.\([0-9][0-9]*\)\.\([0-9][0-9]*\).*/\1.\2.\3.0/p')
[ -n "$NUMERIC" ] || { echo "the version $VERSION does not start with MAJOR.MINOR.PATCH" >&2; exit 1; }

ISCC=
if command -v ISCC >/dev/null 2>&1; then ISCC=$(command -v ISCC); fi
for candidate in "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" "/c/Program Files/Inno Setup 6/ISCC.exe" "$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"; do
    [ -n "$ISCC" ] || { [ -f "$candidate" ] && ISCC=$candidate; }
done
[ -n "$ISCC" ] || { echo "Inno Setup 6 (ISCC.exe) is not installed" >&2; exit 1; }

STAGE="$OUT_DIR/installer-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"
(cd "$STAGE" && cmake -E tar xf "$ZIP")
SOURCE="$STAGE/Spirited Drive"
# the same checks the zip had: a package that cannot run is not turned into an installer
[ -f "$SOURCE/Spirited Drive.exe" ] || { echo "the zip has no Spirited Drive.exe" >&2; exit 1; }
[ -d "$SOURCE/data/car_masks" ] || { echo "the zip has no data/car_masks" >&2; exit 1; }
ls "$SOURCE"/SDL3.dll >/dev/null 2>&1 || { echo "the zip has no SDL3.dll" >&2; exit 1; }

SETUP_NAME="Spirited-Drive-$VERSION-windows-x64-setup"
# ISCC is a Windows program: Windows paths for it, and MSYS must not rewrite its /D... options as if they were paths
export MSYS2_ARG_CONV_EXCL='*'
win() { cygpath -w "$1"; }
"$ISCC" /Qp "/DSourceDir=$(win "$SOURCE")" "/DAppVersion=$VERSION" "/DVersionNumeric=$NUMERIC" "/DOutputDir=$(win "$OUT_DIR")" "/DOutputName=$SETUP_NAME" \
    "$(win "$SRC_DIR/packaging/windows/spirited-drive.iss")"
rm -rf "$STAGE"
[ -f "$OUT_DIR/$SETUP_NAME.exe" ] || { echo "ISCC left no $SETUP_NAME.exe" >&2; exit 1; }
echo "$OUT_DIR/$SETUP_NAME.exe"
