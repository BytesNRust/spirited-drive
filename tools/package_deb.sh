#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Packs a built tnfs into Spirited-Drive-<version>-linux-amd64.deb (Ubuntu and the systems built on it, Debian): the same program as the AppImage's.
#
#   tools/package_deb.sh BUILD_DIR OUT_DIR
#
# BUILD_DIR holds tnfs and generated/tnfs_version.c, as for tools/package_appimage.sh; SDL_LICENSE is SDL's LICENSE.txt. The program and our own data/
# (only what git tracks, beside the program: SDL_GetBasePath) go to /opt/spirited-drive, /usr/games/spirited-drive points at it, with the menu entry, the
# icons, a man page and the licences. No game data. The player's folder (~/.local/share/Spirited Drive) is not the package's: removing it keeps it.
# The working folder is a new OUT_DIR/deb.XXXXXX each run and is left there: this script removes nothing.
set -eu

[ $# -eq 2 ] || { echo "usage: $0 BUILD_DIR OUT_DIR" >&2; exit 2; }
BUILD_DIR=$(cd "$1" && pwd)
mkdir -p "$2"
OUT_DIR=$(cd "$2" && pwd)
SRC_DIR=$(cd "$(dirname "$0")/.." && pwd)
EXE="$BUILD_DIR/tnfs"
VERSION_C="$BUILD_DIR/generated/tnfs_version.c"
: "${SDL_LICENSE:?set SDL_LICENSE to the LICENSE.txt of the SDL sources the program was built with}"
[ -f "$EXE" ] || { echo "no tnfs in $BUILD_DIR" >&2; exit 1; }
[ -f "$VERSION_C" ] || { echo "no generated/tnfs_version.c in $BUILD_DIR" >&2; exit 1; }
[ -f "$SDL_LICENSE" ] || { echo "no SDL licence at $SDL_LICENSE" >&2; exit 1; }
VERSION=$(sed -n 's/^const char tnfs_version\[\] = "\(.*\)";.*/\1/p' "$VERSION_C")
[ -n "$VERSION" ] || { echo "cannot read the version from $VERSION_C" >&2; exit 1; }
# dpkg sorts "~" before everything: 0.9.2~beta.1 comes before 0.9.2, as it should
DEB_VERSION=$(printf '%s' "$VERSION" | sed 's/-/~/')
MAINTAINER="Bytes n Rust <noreply@bytesnrust.com>"

if readelf -d "$EXE" | grep -q 'NEEDED.*libSDL3'; then
    echo "$EXE wants a libSDL3.so from the system: build SDL3 with SDL_STATIC=ON SDL_SHARED=OFF" >&2
    exit 1
fi

umask 022
ROOT=$(mktemp -d "$OUT_DIR/deb.XXXXXX")
: "${ROOT:?no working folder}"
APP="$ROOT/opt/spirited-drive"
DOC="$ROOT/usr/share/doc/spirited-drive"
mkdir -p "$APP" "$ROOT/DEBIAN" "$ROOT/usr/games" "$ROOT/usr/share/applications" "$ROOT/usr/share/man/man6" "$ROOT/usr/share/lintian/overrides" "$DOC"

install -m 755 "$EXE" "$APP/tnfs"
strip "$APP/tnfs"
git -C "$SRC_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "$SRC_DIR is not a git checkout: no data/ in the package" >&2; exit 1; }
git -C "$SRC_DIR" ls-files -- data | while IFS= read -r file; do
    mkdir -p "$APP/$(dirname "$file")"
    install -m 644 "$SRC_DIR/$file" "$APP/$file"
done
[ -d "$APP/data/car_masks" ] || { echo "the package has no data/car_masks" >&2; exit 1; }
# in /usr/games, where a game's command goes; absolute, as a link from /usr to /opt must be
ln -s /opt/spirited-drive/tnfs "$ROOT/usr/games/spirited-drive"

for size in 16 32 48 64 128 256 512; do
    dir="$ROOT/usr/share/icons/hicolor/${size}x${size}/apps"
    mkdir -p "$dir"
    install -m 644 "$SRC_DIR/data/icon/spirited_$size.png" "$dir/spirited-drive.png"
done

cat > "$ROOT/usr/share/applications/spirited-drive.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Spirited Drive
Comment=The Need for Speed SE (1996), rebuilt to run natively. Bring your own CD.
Exec=/usr/games/spirited-drive
Icon=spirited-drive
Categories=Game;
Terminal=false
StartupWMClass=tnfs
DESKTOP

cat > "$ROOT/usr/share/man/man6/spirited-drive.6" <<MANPAGE
.TH SPIRITED-DRIVE 6 "" "Spirited Drive $VERSION"
.SH NAME
spirited-drive \\- The Need for Speed SE (1996), rebuilt to run natively
.SH SYNOPSIS
.B spirited-drive
[\\fB\\-\\-data\\fR \\fIfolder\\fR] [\\fB\\-\\-user\\fR \\fIfolder\\fR]
.SH DESCRIPTION
Starts the game. It contains no game data: on the first start it asks for your own copy of
The Need for Speed Special Edition for PC (1996): the CD, a folder with its files, or an ISO image of the disc.
.SH OPTIONS
.TP
\\fB\\-\\-data\\fR \\fIfolder\\fR
The folder with the game's files.
.TP
\\fB\\-\\-user\\fR \\fIfolder\\fR
Keep settings, records and the copy of the game's files there.
.SH FILES
.TP
\\fI~/.local/share/Spirited Drive/\\fR
Settings, records, the copy of the game's files and the log (spirited\\-drive.log). Removing the package keeps it.
.SH BUGS
https://github.com/BytesNRust/spirited-drive/issues
MANPAGE
gzip -9n "$ROOT/usr/share/man/man6/spirited-drive.6"

{
    cat <<COPYRIGHT
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Spirited Drive
Source: https://github.com/BytesNRust/spirited-drive
Comment: no game data is here. The Need for Speed and Need for Speed are trademarks of Electronic Arts Inc.; this project is not affiliated with it.
 NOTICE.md, beside this file, says whose the rest is.

Files: *
Copyright: 2026 Bytes & Rust and contributors
License: GPL-3+
 This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free
 Software Foundation, either version 3 of the License, or (at your option) any later version.
 .
 On Debian systems the full text of the licence is in /usr/share/common-licenses/GPL-3.

Files: opt/spirited-drive/data/*
Copyright: 2026 Bytes & Rust and contributors
License: CC-BY-SA-4.0
 Creative Commons Attribution-ShareAlike 4.0 International. The full text is at https://creativecommons.org/licenses/by-sa/4.0/legalcode

Files: opt/spirited-drive/tnfs
Comment: SDL 3 is linked into the program
Copyright: Sam Lantinga and the SDL contributors
License: Zlib
COPYRIGHT
    sed 's/[[:space:]]*$//; s/^$/./; s/^/ /' "$SDL_LICENSE"
} > "$DOC/copyright"
install -m 644 "$SRC_DIR/NOTICE.md" "$DOC/NOTICE.md"

DATE=$(git -C "$SRC_DIR" log -1 --format=%cD)
cat > "$DOC/changelog" <<CHANGELOG
spirited-drive ($DEB_VERSION) unstable; urgency=medium

  * See https://github.com/BytesNRust/spirited-drive/releases

 -- $MAINTAINER  $DATE
CHANGELOG
gzip -9n "$DOC/changelog"

# a third party's package lives in /opt by design (the Filesystem Hierarchy Standard keeps it for that): lintian's one complaint left unfixed
cat > "$ROOT/usr/share/lintian/overrides/spirited-drive" <<OVERRIDES
# a package from outside the distribution: the program and its data in /opt, the launcher in /usr/games
spirited-drive: dir-or-file-in-opt
OVERRIDES

# the files' sums and the size, then the control file
(cd "$ROOT" && find . -type f ! -path './DEBIAN/*' -printf '%P\0' | sort -z | xargs -0 md5sum > DEBIAN/md5sums)
SIZE=$(du -sk --exclude=DEBIAN "$ROOT" | cut -f1)
cat > "$ROOT/DEBIAN/control" <<CONTROL
Package: spirited-drive
Version: $DEB_VERSION
Architecture: amd64
Maintainer: $MAINTAINER
Installed-Size: $SIZE
Depends: libc6 (>= 2.35)
Recommends: zenity
Section: games
Priority: optional
Homepage: https://bytesnrust.com
Description: native rebuild of The Need for Speed SE (1996)
 Spirited Drive runs The Need for Speed Special Edition (PC, 1996) natively on
 today's computers: widescreen, smooth motion and more. It contains no game
 data: you need your own copy of the game.
CONTROL
# the modes whatever the checkout's and the builder's umask say: folders 755, files 644, the program 755
find "${ROOT:?}" -type d -exec chmod 755 {} +
find "${ROOT:?}/usr" "${ROOT:?}/DEBIAN" "${APP:?}/data" -type f -exec chmod 644 {} +

DEB="$OUT_DIR/Spirited-Drive-$VERSION-linux-amd64.deb"
# xz: every dpkg of the systems it is for reads it
dpkg-deb --root-owner-group -Zxz --build "$ROOT" "$DEB" >/dev/null
echo "$DEB"
