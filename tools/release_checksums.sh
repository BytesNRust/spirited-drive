#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Writes DIR/SHA256SUMS.txt, the one checksum list of a release: every download of the three systems. DIR holds them and each build job's own list
# (SHA256SUMS-macos.txt, -windows.txt, -linux.txt). Every download must be there, and be in one of those lists with the same hash: what is published
# is what was built.
#
#   tools/release_checksums.sh DIR
set -eu

[ $# -eq 1 ] || { echo "usage: $0 DIR" >&2; exit 2; }
cd "$1"

sha() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }

for kind in '*.dmg' '*.zip' '*-setup.exe' '*.AppImage' '*.deb'; do
    for file in $kind; do
        [ -f "$file" ] || { echo "release_checksums: no $kind in $PWD" >&2; exit 1; }
    done
done

# the build jobs' lists in one form (Windows marks a binary read "hash *name")
built=$(cat SHA256SUMS-macos.txt SHA256SUMS-windows.txt SHA256SUMS-linux.txt 2>/dev/null | tr -d '\r' | sed 's/ \*/  /') || true
list=""
for file in *.dmg *.zip *-setup.exe *.AppImage *.deb; do
    line=$(sha "$file" | sed 's/ \*/  /')
    if ! printf '%s\n' "$built" | grep -qxF "$line"; then
        echo "release_checksums: $file is not what a build job listed (or no job listed it)" >&2
        exit 1
    fi
    list="$list$line
"
done
printf '%s' "$list" > SHA256SUMS.txt
cat SHA256SUMS.txt
