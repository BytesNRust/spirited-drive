#!/usr/bin/env python3
"""gate_select.py [--cmake-patch FILE] DIFF REL_JSON ASAN_JSON OUT_PREFIX [SCOPE]

Decides what a gate runs. It only reads files and writes three small ones; it runs nothing.

  DIFF        the changed paths, one per line (git diff --name-only <merge-base of origin/main> HEAD)
  REL_JSON    ctest --show-only=json-v1 of build/release
  ASAN_JSON   the same of build/asan56
  OUT_PREFIX  writes OUT_PREFIX.scope (one word), OUT_PREFIX.rel and OUT_PREFIX.asan (test names, one per line;
              the word ALL alone when every test runs)
  SCOPE       auto (default) | full | editor | game | none: a forced scope, printed as forced

Scope from the paths:
  editor/ studio/                      -> editor
  game/ (not game/sys/sha256.c)        -> game
  docs/ notes/ LICENSES/ *.md          -> nothing
  tests/ or tools/ file a test names   -> nothing by itself: those tests run ("from the diff"), release and ASan
  anything else, an unknown path, a tests/ or tools/ file no test names, an empty diff -> full
  editor and game together             -> full

CMakeLists.txt: when its diff (--cmake-patch FILE: git diff -U0 of it) only ADDS lines of tests (add_test, set_tests_properties,
if(TARGET ...), endif(), add_executable of a test target and their continuation lines) it does not make the scope full:
the scope comes from the other files and the tests it adds run, release and ASan, "from the diff". Any other touched
line, a removed line, or no patch given: full.

Release runs: every unit test + the tests of the scope's product + the tests from the diff + every test that lacks a
layer or a product label; everything when the scope is full. ASan runs what release runs, except in the scopes named
in GATE_ASAN_REDUCED (default "editor none": a pure editor change, or only docs or tests), where it runs every unit test + the
tests from the diff + the unlabelled ones. In every other scope (game, full) ASan runs EVERY test, whatever GATE_CORE_E2E says. With GATE_CORE_E2E=night (default gate) a full scope leaves the e2e tests to the nightly sweep, in both trees.
Without labels in the tree (no test carries `unit`) the scope is full and nothing is left out.
Labels read: unit integration e2e, and core game editor (a test of the editor that starts the game carries editor and
game; `both` is read as the two).
"""
import json
import os
import re
import sys

LAYERS = {"unit", "integration", "e2e"}
PRODUCTS = {"core", "game", "editor", "both"}
DOC_DIRS = ("docs/", "notes/", "LICENSES/")
SCOPES = ("auto", "full", "editor", "game", "none")


def load_tests(path):
    with open(path) as fh:
        data = json.load(fh)
    tests = []
    for t in data.get("tests", []):
        labels = []
        for prop in t.get("properties", []):
            if prop.get("name") == "LABELS":
                value = prop.get("value")
                labels = value if isinstance(value, list) else [value]
        tests.append({"name": t["name"], "command": t.get("command") or [], "labels": set(labels)})
    return tests


def tests_naming(path, tests):
    """The tests whose command names this tests/ or tools/ file, or whose binary is its test_<x>."""
    stem = os.path.splitext(os.path.basename(path))[0]
    found = []
    for t in tests:
        cmd = t["command"]
        exe = os.path.basename(cmd[0]) if cmd else ""
        if any(a == path or a.endswith("/" + path) for a in cmd):
            found.append(t["name"])
        elif path.startswith("tests/") and stem.startswith("test_") and exe == stem:
            found.append(t["name"])
    return found


TEST_STATEMENTS = ("add_test", "set_tests_properties")


def _balance(text):
    return text.count("(") - text.count(")")


def cmake_test_only(patch):
    """The tests a CMakeLists.txt diff adds when it ONLY adds test lines, else None. `patch` is git diff -U0 text."""
    added, open_stmt = [], 0
    for raw in patch.splitlines():
        if raw.startswith(("+++", "---", "diff ", "index ", "@@", "\\")):
            if raw.startswith("---") and not raw.startswith("--- a/") and raw != "--- /dev/null":
                return None
            continue
        if raw.startswith("-"):
            return None   # a removed or changed line
        if not raw.startswith("+"):
            continue
        line = raw[1:].split("#", 1)[0].strip()
        if open_stmt > 0:   # the rest of a statement that began with an allowed line
            open_stmt += _balance(line)
            added.append(line)
            continue
        if not line:
            continue
        head = re.match(r"([A-Za-z_]+)\s*\(", line)
        name = head.group(1).lower() if head else ""
        if name in TEST_STATEMENTS or (name == "add_executable" and _is_test_target(line)):
            open_stmt = _balance(line)
        elif re.fullmatch(r"if\s*\(\s*TARGET\s+[^()\s]+\s*\)", line, re.I) or re.fullmatch(r"endif\s*\(\s*\)", line, re.I):
            pass
        else:
            return None
        added.append(line)
    return added


def _is_test_target(line):
    m = re.match(r"add_executable\s*\(\s*([^\s)]+)", line, re.I)
    return bool(m) and m.group(1).startswith("test_")


def tests_added(lines):
    """The names of the tests the added lines declare: add_test(NAME x ...) or add_test(x cmd ...)."""
    names = set()
    for line in lines:
        m = re.match(r"add_test\s*\(\s*(?:NAME\s+)?([^\s)]+)", line, re.I)
        if m:
            names.add(m.group(1))
    return names


def classify(paths, tests, cmake_patch=None):
    """Returns (products touched, tests from the diff, reason for full or '')."""
    products, from_diff, full_reason = set(), set(), ""
    for p in paths:
        if p == "game/sys/sha256.c":
            full_reason = full_reason or p + " (the editor compiles it)"
        elif p.startswith(("editor/", "studio/")):
            products.add("editor")
        elif p.startswith("game/"):
            products.add("game")
        elif p == "CMakeLists.txt" and cmake_patch is not None:
            lines = cmake_test_only(cmake_patch)
            if lines is None:
                full_reason = full_reason or p + " (a line other than a test's was touched)"
            else:
                from_diff.update(tests_added(lines))
        elif p.startswith(DOC_DIRS) or p.endswith(".md"):
            continue
        elif p.startswith(("tests/", "tools/")):
            named = tests_naming(p, tests)
            if named:
                from_diff.update(named)
            else:
                full_reason = full_reason or p + " (no test names it)"
        else:
            full_reason = full_reason or p
    return products, from_diff, full_reason


def decide_scope(paths, products, full_reason, forced, labelled):
    if forced != "auto":
        return forced, "forced by GATE_SCOPE"
    if not labelled:
        return "full", "the tree has no unit label"
    if not paths:
        return "full", "empty diff against the base"
    if full_reason:
        return "full", full_reason
    if len(products) > 1:
        return "full", "editor and game together"
    if products:
        return next(iter(products)), ""
    return "none", "only docs or tests"


def pick(tests, scope, from_diff, reduced, drop_e2e, everything=False):
    """The names to run, or None for all, and the count of unlabelled ones kept. `everything`: ASan outside the reduced scopes."""
    if everything or (scope == "full" and not drop_e2e):
        return None, 0
    names, loose = [], 0
    for t in tests:
        lab = t["labels"]
        unlabelled = not (lab & LAYERS) or not (lab & PRODUCTS)
        if scope == "full":
            product = "e2e" not in lab
        else:
            product = (scope in lab or "both" in lab) and scope != "none" and not reduced
        if "unit" in lab or t["name"] in from_diff or unlabelled or product:
            names.append(t["name"])
            loose += unlabelled
    return names, loose


def write_list(path, names):
    with open(path, "w") as fh:
        fh.write("ALL\n" if names is None else "".join(n + "\n" for n in names))


def main(argv):
    cmake_patch = None
    if len(argv) > 2 and argv[1] == "--cmake-patch":
        with open(argv[2]) as fh:
            cmake_patch = fh.read()
        argv = argv[:1] + argv[3:]
    if len(argv) not in (5, 6) or (len(argv) == 6 and argv[5] not in SCOPES):
        sys.stderr.write(__doc__)
        return 2
    diff, rel_json, asan_json, prefix = argv[1:5]
    forced = argv[5] if len(argv) == 6 else "auto"
    with open(diff) as fh:
        paths = [line.strip() for line in fh if line.strip()]
    rel, asan = load_tests(rel_json), load_tests(asan_json)
    labelled = any("unit" in t["labels"] for t in rel) and any("unit" in t["labels"] for t in asan)
    products, from_diff, full_reason = classify(paths, rel, cmake_patch)
    scope, why = decide_scope(paths, products, full_reason, forced, labelled)
    if scope != "full" and not labelled:
        scope, why = "full", "forced scope refused: the tree has no unit label"

    reduced = scope in os.environ.get("GATE_ASAN_REDUCED", "editor none").split()
    drop_e2e = labelled and os.environ.get("GATE_CORE_E2E", "gate") == "night"
    rel_names, rel_loose = pick(rel, scope, from_diff, False, drop_e2e)
    asan_names, asan_loose = pick(asan, scope, from_diff, reduced, drop_e2e, everything=not reduced)
    with open(prefix + ".scope", "w") as fh:
        fh.write(scope + "\n")
    write_list(prefix + ".rel", rel_names)
    write_list(prefix + ".asan", asan_names)

    print("scope %s%s  [%d paths changed]" % (scope, " (%s)" % why if why else "", len(paths)))
    if scope == "full" and not drop_e2e:
        print("tests release: all %d   asan: all %d" % (len(rel), len(asan)))
    elif scope == "full":
        print("tests release: %d of %d   asan: all %d (GATE_CORE_E2E=night: only release leaves the e2e tests to the nightly sweep)"
              % (len(rel_names), len(rel), len(asan)))
    else:
        product = "" if scope == "none" else " + %s" % scope
        label_re = "unit" if scope == "none" else "unit|%s|both" % scope
        print("tests release: %d of %d (unit%s + %d from the diff + %d unlabelled; as ctest -L '^(%s)$')"
              % (len(rel_names), len(rel), product, len(from_diff), rel_loose, label_re))
        if reduced:
            print("tests asan: %d of %d (reduced: unit + %d from the diff + %d unlabelled)"
                  % (len(asan_names), len(asan), len(from_diff), asan_loose))
        else:
            print("tests asan: all %d (scope %s is not in GATE_ASAN_REDUCED: ASan is never cut there)" % (len(asan), scope))
        if from_diff:
            shown = sorted(from_diff)
            print("from the diff: %s%s" % (" ".join(shown[:12]), " ..." if len(shown) > 12 else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
