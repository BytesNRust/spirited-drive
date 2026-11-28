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
export CD_REPO=o/r CD_TAG=t
zeros=0000000000000000000000000000000000000000000000000000000000000000
echo "cd.tar.zst.age $zeros" > "$work/blob"

refuses() {   # WORDS DEST ENV...: fetch.sh fails, says WORDS and makes no DEST
  words=$1 dest=$2; shift 2
  if out=$(env "$@" sh "$fetch" "$dest" "${BLOB:-$work/blob}" 2>&1); then fail "it did not fail ($words): $out"; fi
  case $out in *"$words"*) ;; *) fail "the failure does not say '$words': $out";; esac
  [ ! -e "$dest/original" ] || fail "$dest/original was made though it failed ($words)"
}
refuses "CD_AGE_KEY is empty" "$work/d1" CD_AGE_KEY= CD_REPO_TOKEN=x
refuses "CD_REPO_TOKEN is empty" "$work/d2" CD_AGE_KEY=x CD_REPO_TOKEN=
printf '# nothing yet\n' > "$work/empty"
echo "cd.tar.zst.age 123" > "$work/short"
printf 'cd.tar.zst.age %s\nother %s\n' $zeros $zeros > "$work/two"
echo "../x $zeros" > "$work/path"
for b in empty short two; do BLOB="$work/$b"; refuses "names no data yet" "$work/d3$b" CD_AGE_KEY=x CD_REPO_TOKEN=x; done
BLOB="$work/path"; refuses "is not an asset name" "$work/d3p" CD_AGE_KEY=x CD_REPO_TOKEN=x
BLOB=
# the repository's own blob.txt is empty or well formed
own=$(dirname "$fetch")/blob.txt
[ -z "$(sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$own")" ] || sed -e 's/#.*//' -e '/^[[:space:]]*$/d' "$own" | grep -qxE '[A-Za-z0-9._-]+ [0-9a-f]{64}' || fail "$own is not 'asset sha256'"
mkdir "$work/there"
refuses "exists already" "$work/there" CD_AGE_KEY=x CD_REPO_TOKEN=x

# the public log of the CD jobs: the workflow never prints a test's output and uploads nothing
flow=${1}/.github/workflows/gate.yml
bad=$(sed 's/#.*//' "$flow" | grep -nE -e '--output-on-failure|--test-output-size|--verbose|--extra-verbose| -VV? |CTEST_OUTPUT_ON_FAILURE|upload-artifact|LastTest' || true)
[ -z "$bad" ] || fail "gate.yml would put test output or files in public: $bad"
[ "$(sed 's/#.*//' "$flow" | grep -c 'ctest --test-dir build -I ')" = 2 ] || fail "gate.yml's two test steps (Linux, macOS) are not where this check looks"
# every job that holds the key takes the data before any other action, and deletes it whatever happened
awkout=$(sed 's/#.*//' "$flow" | awk '
    /^  [a-z][a-z0-9-]*:[[:space:]]*$/ { job = $1; seen = 0 }
    /^      - / || /^        uses:/ { if (match($0, /uses: *[^ ]+/)) { u = substr($0, RSTART + 5, RLENGTH - 5); gsub(/^ +/, "", u); if (u !~ /^actions\/checkout@/ && !seen) { if (u == "./.github/actions/cd-data") { seen = 1; held[job] = 1 } else first[job] = u } } }
    /rm -rf "\$\{RUNNER_TEMP:\?\}\/cd"/ { del[job] = 1 }
    END { for (j in held) { n++; if (j in first) print j " runs " first[j] " before cd-data"; if (!(j in del)) print j " does not delete the data" } if (n != 2) print "jobs that take the data: " n+0 }')
[ -z "$awkout" ] || fail "gate.yml's jobs with the data: $awkout"

# age on a macOS runner (install_age.sh): a release archive of a pinned sha256. Here a made-up archive read from a file:// address
install=$(dirname "$fetch")/install_age.sh
mkdir -p "$work/rel/age"
printf '#!/bin/sh\necho made-up age\n' > "$work/rel/age/age"
chmod +x "$work/rel/age/age"
tar -C "$work/rel" -czf "$work/age.tar.gz" age
asum=$(python3 -I -c 'import hashlib, sys; print(hashlib.sha256(open(sys.argv[1], "rb").read()).hexdigest())' "$work/age.tar.gz")
out=$(sh "$install" "$work/agebin" "file://$work/age.tar.gz" "$asum" 2>&1) || fail "install_age, the good case: $out"
[ "$("$work/agebin/age" 2>/dev/null)" = "made-up age" ] || fail "install_age did not leave age in its folder: $out"
if out=$(sh "$install" "$work/agebad" "file://$work/age.tar.gz" $zeros 2>&1); then fail "install_age took an archive of another sha256: $out"; fi
case $out in *"not the pinned"*) ;; *) fail "install_age's failure does not say 'not the pinned': $out";; esac
[ ! -e "$work/agebad/age" ] || fail "install_age installed it though the sha256 was wrong"
if sh "$install" "$work/agenone" "file://$work/no-such.tar.gz" "$asum" >/dev/null 2>&1; then fail "install_age did not fail with no archive"; fi
# its own address is a release's, with a version, and its two hashes are written in full (never "latest", never read from the download)
grep -q 'github.com/FiloSottile/age/releases/download/v[0-9][0-9.]*/' "$install" || fail "install_age.sh's address is not a versioned release"
[ "$(grep -cE 'sha=[0-9a-f]{64}' "$install")" = 2 ] || fail "install_age.sh does not pin a sha256 for each of the two Mac processors"

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
  echo "cd.tar.zst.age $sha" > "$work/blob"
  out=$(CD_AGE_KEY="$key" CD_REPO_TOKEN=good sh "$fetch" "$work/ok" "$work/blob" 2>&1) || fail "the good case: $out"
  cmp -s "$work/ok/original/SIMDATA/A" "$work/src/original/SIMDATA/A" || fail "the unpacked file differs"
  case $out in *"$key"*) fail "the key was printed";; esac
  [ "$out" = "cd-data: unpacked" ] || fail "the good case prints more than its one line: $out"
  refuses "could not download" "$work/d4" CD_AGE_KEY="$key" CD_REPO_TOKEN=bad
  refuses "could not decrypt" "$work/d5" CD_AGE_KEY="$other" CD_REPO_TOKEN=good
  echo "cd.tar.zst.age $zeros" > "$work/blob"
  refuses "is not the expected data" "$work/d6" CD_AGE_KEY="$key" CD_REPO_TOKEN=good
else
  echo "note: no age/zstd here: the download and decrypt cases were not run"
fi
[ "$failures" = 0 ] || exit 1
echo "cd_data_fetch: ok"
