#!/usr/bin/env python3
"""Where the game's user folder is on Linux, asked of the program itself (--mods-list opens the log there and needs no window nor game data):
XDG_DATA_HOME when it is an absolute path, else ~/.local/share, as the XDG base directory spec says; --user DIR instead of both. Stdlib only.
usage: linux_user_dirs.py TNFS"""
import os
import subprocess
import sys
import tempfile

LOG = "spirited-drive.log"
FOLDER = "Spirited Drive"


def run(tnfs, work, xdg, args=()):
    """one run of the program from `work`/cwd with HOME `work`/home; `xdg` None: XDG_DATA_HOME not set"""
    os.makedirs(os.path.join(work, "cwd"))
    os.makedirs(os.path.join(work, "home"))
    env = {k: v for k, v in os.environ.items() if k not in ("XDG_DATA_HOME", "TNFS_PREF_DIR")}
    env["HOME"] = os.path.join(work, "home")
    if xdg is not None:
        env["XDG_DATA_HOME"] = xdg
    done = subprocess.run([tnfs, *args, "--mods-list"], cwd=os.path.join(work, "cwd"), env=env, capture_output=True, timeout=60)
    return done.returncode


def logs(work):
    """every log the run wrote, relative to `work`"""
    return sorted(os.path.relpath(os.path.join(d, f), work) for d, _, files in os.walk(work) for f in files if f == LOG)


def main():
    tnfs = os.path.abspath(sys.argv[1])
    in_home = os.path.join("home", ".local", "share", FOLDER, LOG)
    failed = 0
    with tempfile.TemporaryDirectory() as top:
        cases = [
            ("XDG_DATA_HOME not set", lambda w: None, (), in_home),
            ("XDG_DATA_HOME absolute", lambda w: os.path.join(w, "xdg"), (), os.path.join("xdg", FOLDER, LOG)),
            ("XDG_DATA_HOME empty", lambda w: "", (), in_home),
            ("XDG_DATA_HOME relative", lambda w: "relative", (), in_home),
            ("--user, a relative folder whose parents are not there", lambda w: os.path.join(w, "xdg"), ("--user", "a/b/c"),
             os.path.join("cwd", "a", "b", "c", LOG)),
        ]
        for n, (what, xdg, args, expected) in enumerate(cases):
            work = os.path.join(top, str(n))
            code = run(tnfs, work, xdg(work), args)
            got = logs(work)
            if code != 0 or got != [expected]:
                failed += 1
                print(f"FAIL {what}: exit {code}, log at {got}, expected {expected}")
    print(f"{len(cases)} cases, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
