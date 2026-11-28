#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Downloads the encrypted CD data from the private repository, decrypts it with age and unpacks it into DEST (DEST/original). The key is read from the environment and handed to age
# on stdin: it is never a file. Nothing of the data is printed. DEST must not exist yet, and must be outside the checkout (artifacts and caches take paths in it).
# usage: fetch.sh DEST BLOB
#   BLOB   a file with one line, "asset sha256" (# comments): the release asset (a tar.zst of original/, encrypted with age) and the sha256 of the decrypted tar.zst
#   CD_AGE_KEY     the age private key          (secret of the environment cd-data)
#   CD_REPO_TOKEN  read access to CD_REPO       (secret of the environment cd-data)
#   CD_REPO CD_TAG the private repository and the release that holds the asset
set -eu
die() { echo "cd-data: $*" >&2; exit 1; }
dest=${1:?usage: fetch.sh DEST BLOB}
blob=${2:?usage: fetch.sh DEST BLOB}
[ -n "${CD_AGE_KEY:-}" ] || die "CD_AGE_KEY is empty: the job must name the environment cd-data, and only main and the merge queue get its secrets"
[ -n "${CD_REPO_TOKEN:-}" ] || die "CD_REPO_TOKEN is empty: the environment cd-data needs a token that can read the data repository"
for v in CD_REPO CD_TAG; do
  eval "[ -n \"\${$v:-}\" ]" || die "$v is empty"
done
[ -f "$blob" ] || die "no $blob"
line=$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$blob")
read -r CD_ASSET CD_SHA256 rest <<LINE
$line
LINE
{ [ -n "$CD_ASSET" ] && [ -z "$rest" ] && [ "$(printf '%s\n' "$line" | wc -l)" -eq 1 ] && [ ${#CD_SHA256} -eq 64 ]; } || die "$blob names no data yet (one line: asset name and sha256)"
case $CD_ASSET in *[!A-Za-z0-9._-]*) die "$blob: '$CD_ASSET' is not an asset name";; esac
[ ! -e "$dest" ] || die "$dest exists already"

tmp=$(mktemp -d)
trap 'rm -rf "${tmp:?}"' EXIT
GH_TOKEN="$CD_REPO_TOKEN" gh release download "$CD_TAG" -R "$CD_REPO" -p "$CD_ASSET" -D "$tmp" ||
  die "could not download $CD_ASSET of $CD_TAG from $CD_REPO (token expired, or no such release asset)"
[ -f "$tmp/$CD_ASSET" ] || die "the release $CD_TAG has no asset $CD_ASSET"
printf '%s\n' "$CD_AGE_KEY" | age -d -i - -o "$tmp/cd.tar.zst" "$tmp/$CD_ASSET" || die "could not decrypt $CD_ASSET: the key is not the one it was encrypted for"
sum=$(python3 -I -c 'import hashlib, sys
h = hashlib.sha256()
with open(sys.argv[1], "rb") as f:
    for block in iter(lambda: f.read(1 << 20), b""):
        h.update(block)
print(h.hexdigest())' "$tmp/cd.tar.zst")
[ "$sum" = "$CD_SHA256" ] || die "$CD_ASSET is not the expected data: sha256 $sum, wanted $CD_SHA256"
mkdir -p "$dest"
zstd -dc "$tmp/cd.tar.zst" | tar -x -C "$dest" || die "could not unpack $CD_ASSET"
[ -d "$dest/original/SIMDATA" ] || die "$CD_ASSET holds no original/SIMDATA"
echo "cd-data: $(find "$dest/original" -type f | wc -l | tr -d ' ') files"
