#!/bin/sh
# check_dmg_contents.sh DMG STUDIO: mounts the DMG read only and checks what is in it: "Spirited Drive.app" and the Applications link, and
# "Spirited Drive Studio.app" only when STUDIO is 1 (Build Test's `studio` input). The downloads are the game only. Exit 1 on any other content.
set -eu
[ $# -eq 2 ] || { echo "usage: $0 DMG 0|1" >&2; exit 2; }
dmg=$1
studio=$2
mnt=$(mktemp -d)
trap 'hdiutil detach -quiet "$mnt" >/dev/null 2>&1 || true; rmdir "$mnt" 2>/dev/null || true' EXIT
hdiutil attach -quiet -nobrowse -readonly -mountpoint "$mnt" "$dmg"
found=$(ls -1 "$mnt" | sort)
want="Applications
Spirited Drive.app"
[ "$studio" != 1 ] || want="Applications
Spirited Drive Studio.app
Spirited Drive.app"
echo "in $dmg:"
echo "$found" | sed 's/^/  /'
if [ "$found" != "$want" ]; then
    echo "FAIL: the DMG should hold exactly:" >&2
    echo "$want" | sed 's/^/  /' >&2
    exit 1
fi
[ -x "$mnt/Spirited Drive.app/Contents/MacOS/tnfs" ] || { echo "FAIL: no game binary in Spirited Drive.app" >&2; exit 1; }
echo "ok"
