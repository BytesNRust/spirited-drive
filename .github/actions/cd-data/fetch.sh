#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Downloads the encrypted CD data from the private repository, decrypts it with age and unpacks it into DEST (DEST/original). The key is read from the environment and handed to age
# on stdin: it is never a file. Nothing of the data is printed. DEST must not exist yet, and must be outside the checkout (artifacts and caches take paths in it).
# usage: fetch.sh DEST
#   CD_AGE_KEY     the age private key          (secret of the environment cd-data)
#   CD_REPO_TOKEN  read access to CD_REPO       (secret of the environment cd-data)
#   CD_REPO CD_TAG CD_ASSET   the release asset: a tar.zst of original/, encrypted with age
#   CD_SHA256      the sha256 of the decrypted tar.zst
set -eu
die() { echo "cd-data: $*" >&2; exit 1; }
dest=${1:?usage: fetch.sh DEST}
[ -n "${CD_AGE_KEY:-}" ] || die "CD_AGE_KEY is empty: the job must name the environment cd-data, and only main and the merge queue get its secrets"
[ -n "${CD_REPO_TOKEN:-}" ] || die "CD_REPO_TOKEN is empty: the environment cd-data needs a token that can read the data repository"
for v in CD_REPO CD_TAG CD_ASSET CD_SHA256; do
  eval "[ -n \"\${$v:-}\" ]" || die "$v is empty"
done
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
