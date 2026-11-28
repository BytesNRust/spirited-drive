#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# age on a macOS runner, which has no package of it from the system: one release's archive, taken only when its sha256 is the one written here (reviewed like code), and its
# "age" program left in DEST. It runs in the step list of the action that holds the key, so no third-party action or install tool comes between. URL and SHA256 are for the test.
# usage: install_age.sh DEST [URL SHA256]
set -eu
die() { echo "cd-data: $*" >&2; exit 1; }
dest=${1:?usage: install_age.sh DEST [URL SHA256]}
if [ $# -ge 3 ]; then
  url=$2 sha=$3
else
  case $(uname -m) in
    arm64) arch=arm64 sha=e2020b073c44f692685a24d6abc378817eb81ffaaf49fd0531ef8565f767f2f5 ;;
    x86_64) arch=amd64 sha=1d1e4bc66e1427edad7739ae7616157de0e79db8b6d2a1497d7d9925fb06a539 ;;
    *) die "no pinned age for the processor $(uname -m)" ;;
  esac
  url=https://github.com/FiloSottile/age/releases/download/v1.3.2/age-v1.3.2-darwin-$arch.tar.gz
fi
mkdir -p "$dest"
curl -fsSL --retry 3 --max-time 120 -o "$dest/age.tar.gz" "$url" || die "could not download age"
sum=$(python3 -I -c 'import hashlib, sys; print(hashlib.sha256(open(sys.argv[1], "rb").read()).hexdigest())' "$dest/age.tar.gz")
[ "$sum" = "$sha" ] || die "the age archive is not the pinned one (sha256 $sum)"
tar -xzf "$dest/age.tar.gz" -C "$dest" age/age || die "the age archive holds no age/age"
mv "$dest/age/age" "$dest/age.bin"
rmdir "$dest/age"
mv "$dest/age.bin" "$dest/age"
echo "cd-data: age installed"
