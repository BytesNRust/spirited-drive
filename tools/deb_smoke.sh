#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Installs the .deb on clean systems (docker images), as a player does with apt: it brings what it recommends, the program starts and lists no mods (no
# window), it upgrades over an older package of itself, and removing it leaves the player's folder.
#
#   tools/deb_smoke.sh DEB IMAGE...
set -eu

[ $# -ge 2 ] || { echo "usage: $0 DEB IMAGE..." >&2; exit 2; }
DEB=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
shift
[ -f "$DEB" ] || { echo "no .deb at $DEB" >&2; exit 1; }

for image in "$@"; do
    echo "== $image"
    docker run --rm -v "$DEB:/new.deb:ro" "$image" sh -euc '
        export DEBIAN_FRONTEND=noninteractive HOME=/root SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
        # as a user session has it (a container root has no /usr/games)
        export PATH="$PATH:/usr/games"
        apt-get update -qq
        cp /new.deb /tmp/new.deb
        # an older package of the same files, to upgrade from: older than any version a build stamps, an update test build (0.0.1~updatetest) too
        dpkg-deb -R /tmp/new.deb /tmp/older
        sed -i "s/^Version: .*/Version: 0.0.0~older/" /tmp/older/DEBIAN/control
        dpkg-deb --root-owner-group -Zxz --build /tmp/older /tmp/older.deb >/dev/null
        apt-get install -y -qq /tmp/older.deb >/tmp/apt.log 2>&1 || { tail -20 /tmp/apt.log; exit 1; }
        command -v zenity >/dev/null || { echo "apt did not bring zenity" >&2; exit 1; }
        command -v curl >/dev/null || { echo "apt did not bring curl" >&2; exit 1; }
        spirited-drive --user /tmp/u0 --mods-list
        apt-get install -y -qq /tmp/new.deb >/tmp/apt.log 2>&1 || { tail -20 /tmp/apt.log; exit 1; }
        want=$(dpkg-deb -f /tmp/new.deb Version)
        got=$(dpkg-query -W -f="\${Version}" spirited-drive)
        [ "$got" = "$want" ] || { echo "after the upgrade the version is $got, not $want" >&2; exit 1; }
        ldd /opt/spirited-drive/tnfs > /tmp/libs.txt
        if grep -q "not found" /tmp/libs.txt; then echo "the program wants a library the package does not depend on" >&2; exit 1; fi
        test -d /opt/spirited-drive/data/car_masks
        test -s /usr/share/applications/spirited-drive.desktop
        test -s /usr/share/icons/hicolor/256x256/apps/spirited-drive.png
        test -s /usr/share/doc/spirited-drive/copyright
        # the player folder as a first start makes it (no --user), then the package removed
        spirited-drive --mods-list
        mkdir -p "$HOME/.local/share/Spirited Drive"
        echo kept > "$HOME/.local/share/Spirited Drive/mine.txt"
        apt-get remove -y -qq spirited-drive >/tmp/apt.log 2>&1 || { tail -20 /tmp/apt.log; exit 1; }
        # the file itself: the shell remembers where it found a command it has run
        if [ -e /usr/games/spirited-drive ] || [ -L /usr/games/spirited-drive ]; then echo "the launcher is still there after remove" >&2; exit 1; fi
        if [ -e /opt/spirited-drive ]; then echo "/opt/spirited-drive is still there after remove" >&2; exit 1; fi
        grep -qx kept "$HOME/.local/share/Spirited Drive/mine.txt"
        echo "deb ok: $want"
    '
done
echo "deb_smoke: ok"
