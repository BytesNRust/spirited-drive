#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# The gate's shards (tools/ci_shards.py, .github/workflows/gate.yml): ctest runs every registered test in exactly one shard, and a fixture's tests stay together. Asks ctest itself
# what each shard's -I runs. No CD data.
# usage: ci_shards.sh CTEST BUILD REPO PYTHON
set -eu
ctest=${1:?ctest} build=${2:?build} repo=${3:?repo} python=${4:?python}
work=$(mktemp -d)
trap 'rm -rf "${work:?}"' EXIT
"$ctest" --test-dir "$build" --show-only=json-v1 > "$work/tests.json"
names() { sed -n 's/^ *Test *#[0-9]*: \([^ ]*\).*/\1/p'; }
"$ctest" --test-dir "$build" -N | names | sort > "$work/all"
[ -s "$work/all" ] || { echo "FAIL: ctest lists no test"; exit 1; }
for pair in "release 5" "asan 8"; do
  set -- $pair
  costs="$repo/tests/ci/costs-$1.txt" n=$2
  : > "$work/ran"
  k=1
  while [ "$k" -le "$n" ]; do
    select=$("$python" "$repo/tools/ci_shards.py" "$work/tests.json" "$costs" "$n" "$k")
    # -FA .*: without the fixtures ctest would add by itself, so a setup left in another shard shows
    "$ctest" --test-dir "$build" -N -FA '.*' -I "$select" | names > "$work/shard"
    "$ctest" --test-dir "$build" -N -I "$select" | names | sort > "$work/with_fixtures"
    sort "$work/shard" | cmp -s - "$work/with_fixtures" || { echo "FAIL: $1 shard $k needs a fixture's setup from another shard"; exit 1; }
    cat "$work/shard" >> "$work/ran"
    k=$((k + 1))
  done
  sort "$work/ran" | cmp -s - "$work/all" || {
    echo "FAIL: the $n $1 shards do not run every test exactly once:"
    sort "$work/ran" | diff "$work/all" - | head -20
    exit 1
  }
  echo "$1: $(wc -l < "$work/all") tests in $n shards, each once"
done
