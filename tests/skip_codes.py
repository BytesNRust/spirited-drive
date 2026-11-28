#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A test that skips is not a test that passed. Two rules, checked on the scripts of tests/ and on what ctest registered:
  1. no script says "skip" and exits 0: a whole test that cannot run exits 77 (a part left out of a test that ran may still say so and go on);
  2. every ctest test whose command is a script that can exit 77 has SKIP_RETURN_CODE 77, so ctest says "Skipped" and not "Failed".
usage: skip_codes.py CTEST BUILD REPO
"""
import json
import os
import re
import subprocess
import sys

# says it left a part out and exits 0 with what it did check: package_game_only without hdiutil ran the workflow checks
PARTIAL = {"package_game_only.sh"}

SH_SKIP_PASS = re.compile(r"""\b(echo|printf)\b[^#|;]*\bskip.*\bexit 0\b""", re.I)
PY_SKIP_PASS = re.compile(r"""print\([^\n]*skipped[^\n]*\n\s*sys\.exit\(0\)""", re.I)
CAN_SKIP = re.compile(r"""\bexit 77\b|sys\.exit\(77\)""")


def main(ctest, build, repo):
    failures = []
    tests_dir = os.path.join(repo, "tests")
    can_skip = set()
    for name in sorted(os.listdir(tests_dir)):
        if not name.endswith((".sh", ".py")) or name == os.path.basename(__file__):
            continue
        with open(os.path.join(tests_dir, name), encoding="utf-8", errors="replace") as f:
            text = f.read()
        if CAN_SKIP.search(text):
            can_skip.add(name)
        if name in PARTIAL:
            continue
        if name.endswith(".sh"):
            failures += ["%s:%d: skips and exits 0" % (name, i) for i, line in enumerate(text.splitlines(), 1) if SH_SKIP_PASS.search(line)]
        elif PY_SKIP_PASS.search(text):
            failures.append("%s: skips and exits 0" % name)
    listing = subprocess.run([ctest, "--test-dir", build, "--show-only=json-v1"], capture_output=True, text=True, check=True).stdout
    tests = json.loads(listing)["tests"]
    for test in tests:
        scripts = {os.path.basename(arg) for arg in test.get("command", []) if os.path.basename(os.path.dirname(arg)) == "tests"}
        code = next((p["value"] for p in test.get("properties", []) if p["name"] == "SKIP_RETURN_CODE"), None)
        if scripts & can_skip and code != 77:
            failures.append("test %s: %s can exit 77, and it has no SKIP_RETURN_CODE 77 (CMakeLists.txt, TNFS_SKIP_TESTS)" % (test["name"], " ".join(sorted(scripts & can_skip))))
    for line in failures:
        print("FAIL:", line)
    print("skip_codes: %d scripts can skip, %d tests, %d failures" % (len(can_skip), len(tests), len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    sys.exit(main(*sys.argv[1:4]))
