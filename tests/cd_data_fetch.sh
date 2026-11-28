#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The cd-data action's script (.github/actions/cd-data/fetch.sh): without the key or the token it fails and says which; with a gh of our own that "downloads" a made-up archive,
# it unpacks the right one and refuses a wrong key or a wrong hash. The decrypting part needs age and zstd (not in CI's build jobs): left out, and said so, without them. No CD data.
# usage: cd_data_fetch.sh REPO
set -eu
fetch=${1:?repo}/.github/actions/cd-data/fetch.sh
work=$(mktemp -d)
trap 'rm -rf "${work:?}"' EXIT
failures=0
fail() { echo "FAIL: $*"; failures=$((failures + 1)); }
export CD_REPO=o/r CD_TAG=t CD_ASSET=cd.tar.zst.age CD_SHA256=0

refuses() {   # WORDS DEST ENV...: fetch.sh fails, says WORDS and makes no DEST
  words=$1 dest=$2; shift 2
  if out=$(env "$@" sh "$fetch" "$dest" 2>&1); then fail "it did not fail ($words): $out"; fi
  case $out in *"$words"*) ;; *) fail "the failure does not say '$words': $out";; esac
  [ ! -e "$dest/original" ] || fail "$dest/original was made though it failed ($words)"
}
refuses "CD_AGE_KEY is empty" "$work/d1" CD_AGE_KEY= CD_REPO_TOKEN=x
refuses "CD_REPO_TOKEN is empty" "$work/d2" CD_AGE_KEY=x CD_REPO_TOKEN=
refuses "CD_SHA256 is empty" "$work/d3" CD_AGE_KEY=x CD_REPO_TOKEN=x CD_SHA256=
mkdir "$work/there"
refuses "exists already" "$work/there" CD_AGE_KEY=x CD_REPO_TOKEN=x

if command -v age >/dev/null && command -v age-keygen >/dev/null && command -v zstd >/dev/null; then
  mkdir -p "$work/src/original/SIMDATA" "$work/bin"
  echo made-up > "$work/src/original/SIMDATA/A"
  tar -C "$work/src" -cf - original | zstd -q > "$work/plain.tar.zst"
  key=$(age-keygen 2>/dev/null)
  other=$(age-keygen 2>/dev/null)
  age -r "$(printf '%s\n' "$key" | age-keygen -y)" -o "$work/cd.tar.zst.age" "$work/plain.tar.zst"
  sha=$(python3 -I -c 'import hashlib, sys; print(hashlib.sha256(open(sys.argv[1], "rb").read()).hexdigest())' "$work/plain.tar.zst")
  cat > "$work/bin/gh" <<'SH'
#!/bin/sh
# gh release download TAG -R REPO -p ASSET -D DIR
[ "$GH_TOKEN" = good ] || exit 1
cp "$FAKE_ASSET" "${9}/$7"
SH
  chmod +x "$work/bin/gh"
  PATH="$work/bin:$PATH"
  export FAKE_ASSET="$work/cd.tar.zst.age"
  out=$(CD_AGE_KEY="$key" CD_REPO_TOKEN=good CD_SHA256="$sha" sh "$fetch" "$work/ok" 2>&1) || fail "the good case: $out"
  cmp -s "$work/ok/original/SIMDATA/A" "$work/src/original/SIMDATA/A" || fail "the unpacked file differs"
  case $out in *"$key"*) fail "the key was printed";; esac
  refuses "could not download" "$work/d4" CD_AGE_KEY="$key" CD_REPO_TOKEN=bad CD_SHA256="$sha"
  refuses "could not decrypt" "$work/d5" CD_AGE_KEY="$other" CD_REPO_TOKEN=good CD_SHA256="$sha"
  refuses "is not the expected data" "$work/d6" CD_AGE_KEY="$key" CD_REPO_TOKEN=good CD_SHA256=0
else
  echo "note: no age/zstd here: the download and decrypt cases were not run"
fi
[ "$failures" = 0 ] || exit 1
echo "cd_data_fetch: ok"
