#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs the AppImage by itself on clean systems (docker images with nothing of the build installed): it starts, lists no mods (no window), carries
# our data and the licences, and wants no library a bare system lacks.
#
#   tools/appimage_smoke.sh APPIMAGE IMAGE...
set -eu

[ $# -ge 2 ] || { echo "usage: $0 APPIMAGE IMAGE..." >&2; exit 2; }
APPIMAGE=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
shift
[ -f "$APPIMAGE" ] || { echo "no AppImage at $APPIMAGE" >&2; exit 1; }

for image in "$@"; do
    echo "== $image"
    docker run --rm -v "$APPIMAGE:/sd.AppImage:ro" "$image" sh -euc '
        cp /sd.AppImage /tmp/sd.AppImage
        chmod +x /tmp/sd.AppImage
        cd /tmp
        mkdir user
        ./sd.AppImage --appimage-extract-and-run --user user --mods-list
        ./sd.AppImage --appimage-extract >/dev/null
        test -x squashfs-root/AppRun
        test -d squashfs-root/usr/bin/data/car_masks
        grep -qx "Icon=spirited-drive" squashfs-root/spirited-drive.desktop
        test -s squashfs-root/.DirIcon
        test -s squashfs-root/usr/share/applications/spirited-drive.desktop
        test -s squashfs-root/usr/share/icons/hicolor/256x256/apps/spirited-drive.png
        test -f squashfs-root/usr/share/doc/spirited-drive/LICENSE
        test -f squashfs-root/usr/share/doc/spirited-drive/LICENSES/third-party/SDL3/LICENSE.txt
        ldd squashfs-root/usr/bin/tnfs > libs.txt
        cat libs.txt
        ! grep -q "not found" libs.txt
        squashfs-root/AppRun --user user --mods-list
    '
done
echo "appimage_smoke: ok"
