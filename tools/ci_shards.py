#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Splits ctest's tests into N shards by cost, for CI jobs that run side by side.

usage: ci_shards.py TESTS.json COSTS N [K]
  TESTS.json  ctest --show-only=json-v1 of the build
  COSTS       lines "seconds test_name" (# comments); a test it does not name counts DEFAULT_COST
  N           the number of shards
  K           1..N: print shard K as ctest's -I value (0,0,0,#,#,...); without it, one line per shard with its tests and cost

Tests that ctest ties together (a fixture's setup and the tests that require it) go to the same shard. The split is the same for the same input: longest group first, into the
lightest shard.
"""
import json
import sys

DEFAULT_COST = 5.0


def load_costs(path):
    costs = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            words = line.split()
            if not words or words[0].startswith("#"):
                continue
            costs[words[1]] = float(words[0])
    return costs


def prop(test, name, default):
    for p in test.get("properties", []):
        if p["name"] == name:
            return p["value"]
    return default


def groups(tests):
    """Indices of the tests, in lists that must run in one shard."""
    parent = list(range(len(tests)))

    def root(i):
        while parent[i] != i:
            i = parent[i]
        return i

    by_fixture = {}
    for i, test in enumerate(tests):
        for name in prop(test, "FIXTURES_SETUP", []) + prop(test, "FIXTURES_REQUIRED", []) + prop(test, "FIXTURES_CLEANUP", []):
            if name in by_fixture:
                parent[root(i)] = root(by_fixture[name])
            else:
                by_fixture[name] = i
    out = {}
    for i in range(len(tests)):
        out.setdefault(root(i), []).append(i)
    return list(out.values())


def split(tests, costs, n):
    def cost(i):
        return costs.get(tests[i]["name"], DEFAULT_COST) * float(prop(tests[i], "PROCESSORS", 1))

    weighed = sorted(((sum(cost(i) for i in g), g) for g in groups(tests)), key=lambda w: (-w[0], tests[w[1][0]]["name"]))
    shards = [{"cost": 0.0, "tests": []} for _ in range(n)]
    for weight, group in weighed:
        lightest = min(range(n), key=lambda k: (shards[k]["cost"], k))
        shards[lightest]["cost"] += weight
        shards[lightest]["tests"] += group
    for shard in shards:
        shard["tests"].sort()
    return shards


def main(argv):
    if len(argv) not in (4, 5):
        sys.exit(__doc__)
    with open(argv[1], encoding="utf-8") as f:
        tests = json.load(f)["tests"]
    costs, n = load_costs(argv[2]), int(argv[3])
    if n < 1 or not tests:
        sys.exit("ci_shards.py: no tests, or no shards")
    shards = split(tests, costs, n)
    unknown = sum(1 for t in tests if t["name"] not in costs)
    print("ci_shards.py: %d of %d tests have no cost (they count %.0f s)" % (unknown, len(tests), DEFAULT_COST), file=sys.stderr)
    if len(argv) == 5:
        k = int(argv[4])
        if not 1 <= k <= n:
            sys.exit("ci_shards.py: shard %d of %d" % (k, n))
        if not shards[k - 1]["tests"]:
            sys.exit("ci_shards.py: shard %d has no test" % k)   # an empty -I would run every test
        print("0,0,0," + ",".join(str(i + 1) for i in shards[k - 1]["tests"]))   # ctest numbers its tests from 1, in the json's order
    else:
        for k, shard in enumerate(shards, 1):
            print("shard %d: %.0f s, %d tests: %s" % (k, shard["cost"], len(shard["tests"]), " ".join(tests[i]["name"] for i in shard["tests"])))


if __name__ == "__main__":
    main(sys.argv)
