#!/usr/bin/env python3
"""Synthetic cases for tools/ci/gate_v2/gate_select.py: the scope rules, ASan complete outside the reduced scopes, and the
CMakeLists.txt rule (a diff that only adds test lines does not make the gate full). usage: gate_select_test.py REPO"""
import json
import os
import subprocess
import sys
import tempfile

repo = sys.argv[1]
SELECT = os.path.join(repo, "tools", "ci", "gate_v2", "gate_select.py")
failures = []


def test(name, labels, command=None):
    return {"name": name, "command": command or ["/b/" + name], "properties": [{"name": "LABELS", "value": labels}]}


TESTS = [
    test("u_core", ["unit", "core"]), test("u_game", ["unit", "game"]), test("u_editor", ["unit", "editor"]),
    test("i_game", ["integration", "game"]), test("i_editor", ["integration", "editor"]), test("e_editor", ["e2e", "editor"]),
    test("e_core", ["e2e", "core"]), test("loose", []),
    test("flow", ["e2e", "editor"], ["sh", "/repo/tests/flow.sh", "/b/ed"]),
    # the test a CMakeLists.txt diff below adds: the trees are built from the branch, so their lists have it
    test("new_flow", ["e2e", "editor"], ["sh", "/repo/tests/new.sh", "/b/ed"]),
]


def run(diff, patch=None, env=None, scope="auto"):
    with tempfile.TemporaryDirectory() as d:
        for tree in ("rel", "asan"):
            with open(os.path.join(d, tree + ".json"), "w") as fh:
                json.dump({"tests": TESTS}, fh)
        with open(os.path.join(d, "diff.txt"), "w") as fh:
            fh.write("\n".join(diff) + "\n")
        cmd = [sys.executable, SELECT]
        if patch is not None:
            with open(os.path.join(d, "cmake.patch"), "w") as fh:
                fh.write(patch)
            cmd += ["--cmake-patch", os.path.join(d, "cmake.patch")]
        cmd += [os.path.join(d, "diff.txt"), os.path.join(d, "rel.json"), os.path.join(d, "asan.json"), os.path.join(d, "out"), scope]
        proc = subprocess.run(cmd, capture_output=True, text=True, env=dict(os.environ, **(env or {})))
        out = {"rc": proc.returncode, "text": proc.stdout + proc.stderr}
        for key in ("scope", "rel", "asan"):
            path = os.path.join(d, "out." + key)
            out[key] = open(path).read().split() if os.path.exists(path) else None
        return out


def check(label, ok, got=""):
    if not ok:
        failures.append("%s: %s" % (label, got))


def patch_of(*lines, removed=()):
    body = "".join("-%s\n" % r for r in removed) + "".join("+%s\n" % l for l in lines)
    return "diff --git a/CMakeLists.txt b/CMakeLists.txt\n--- a/CMakeLists.txt\n+++ b/CMakeLists.txt\n@@ -10,0 +11,%d @@\n%s" % (len(lines), body)


# (1) ASan: cut only in a pure editor change and in docs/tests; the game and the full scope run every test
r = run(["editor/app.c"])
check("editor scope", r["scope"] == ["editor"], r)
check("editor: asan reduced (units + unlabelled only)", sorted(r["asan"]) == ["loose", "u_core", "u_editor", "u_game"], r["asan"])
check("editor: release has the editor tests", "i_editor" in r["rel"] and "e_editor" in r["rel"] and "i_game" not in r["rel"], r["rel"])
r = run(["docs/x.md"])
check("docs: scope none, asan reduced", r["scope"] == ["none"] and sorted(r["asan"]) == ["loose", "u_core", "u_editor", "u_game"], r)
r = run(["game/sys/x.c"])
check("game scope", r["scope"] == ["game"], r)
check("game: asan runs every test", r["asan"] == ["ALL"], r["asan"])
check("game: release is the game's", "i_game" in r["rel"] and "i_editor" not in r["rel"], r["rel"])
r = run(["core/x.c"])
check("core: full, asan all, release all", r["scope"] == ["full"] and r["asan"] == ["ALL"] and r["rel"] == ["ALL"], r)
r = run(["core/x.c"], env={"GATE_CORE_E2E": "night"})
check("core with night e2e: release leaves e2e, asan still all", "e_core" not in r["rel"] and r["asan"] == ["ALL"], r)
r = run(["game/sys/x.c"], env={"GATE_ASAN_REDUCED": "editor none game"})
check("the policy can still cut the game", r["asan"] != ["ALL"], r["asan"])
r = run(["editor/a.c"], env={"GATE_ASAN_REDUCED": ""})
check("an empty policy cuts nothing", r["asan"] == ["ALL"], r["asan"])

# (2) CMakeLists.txt that only adds tests: the scope comes from the other files, the new tests run in both trees
only_tests = patch_of("add_executable(test_new tests/test_new.c)", "if(TARGET tnfs-track-editor)", "    add_test(NAME new_flow COMMAND sh ${CMAKE_SOURCE_DIR}/tests/new.sh",
                      "        $<TARGET_FILE:tnfs-track-editor>)   # a comment", "    set_tests_properties(new_flow PROPERTIES TIMEOUT 300)", "endif()")
r = run(["CMakeLists.txt", "tests/new.sh"], patch=only_tests)
check("only tests added: scope none", r["scope"] == ["none"], r)
check("only tests added: the new tests run in release and ASan",
      all(n in r["rel"] and n in r["asan"] for n in ("new_flow",)), r)
r = run(["CMakeLists.txt", "editor/app.c"], patch=only_tests)
check("tests added + an editor file: scope editor (not full)", r["scope"] == ["editor"] and "new_flow" in r["asan"] and "new_flow" in r["rel"], r)
r = run(["CMakeLists.txt", "game/sys/x.c"], patch=only_tests)
check("tests added + a game file: scope game, ASan all", r["scope"] == ["game"] and r["asan"] == ["ALL"], r)
r = run(["CMakeLists.txt"], patch=None)
check("no patch given: full", r["scope"] == ["full"], r)
for label, p in (
    ("a source added to a target", patch_of("    game/sys/new.c")),
    ("a removed line", patch_of("add_test(NAME x COMMAND y)", removed=["add_test(NAME old COMMAND y)"])),
    ("a changed foreach list", patch_of("foreach(name a b c)")),
    ("a compile option", patch_of("target_compile_options(test_new PRIVATE -O2)")),
    ("an add_executable that is not a test", patch_of("add_executable(tnfs2 game/main.c)")),
    ("an if that is not TARGET", patch_of("if(APPLE)", "endif()")),
    ("a test line then another line", patch_of("add_test(NAME x COMMAND y)", "set(A 1)")),
):
    r = run(["CMakeLists.txt"], patch=p)
    check("CMakeLists: " + label + " makes it full", r["scope"] == ["full"], r)
r = run(["CMakeLists.txt", "editor/a.c"], patch=patch_of("set(A 1)"))
check("another line + an editor file: full", r["scope"] == ["full"], r)
r = run(["CMakeLists.txt"], patch=patch_of("", "# only a comment"))
check("blank and comment lines only: not full", r["scope"] == ["none"], r)

if failures:
    sys.stderr.write("\n".join("FAIL: %s" % f for f in failures) + "\n")
    sys.exit(1)
print("ok")
