#!/usr/bin/env python3
"""tools/track_starter_psds.py with the game's files: TR2, TR5 and TR7 get one PSD for each texture, a second run writes nothing, and a PSD already there is kept.
usage: test_track_starter_psds.py <track_mask_draft> <original dir>. Skipped (exit 77) when Pillow is missing."""
import os
import subprocess
import sys
import tempfile

try:
    import PIL  # noqa: F401
except ImportError as error:
    print("test_track_starter_psds: skipped (%s)" % error)
    sys.exit(77)

here = os.path.dirname(os.path.abspath(__file__))
tool = os.path.join(here, "..", "tools", "track_starter_psds.py")
draft, cd = sys.argv[1], sys.argv[2]


def run(ws, *tracks):
    return subprocess.run([sys.executable, tool, draft, cd, ws] + list(tracks), capture_output=True, text=True)


failures = 0


def check(name, ok):
    global failures
    if not ok:
        failures += 1
        print("FAIL " + name)


with tempfile.TemporaryDirectory() as ws:
    r = run(ws)
    check("first run succeeds", r.returncode == 0)
    check("199 written", "starter PSDs: 199 written, 0 kept" in r.stdout)
    counts = {t: len([f for f in os.listdir(os.path.join(ws, t, "art-source", "masks")) if f.endswith(".psd")]) for t in ("TR2", "TR5", "TR7")}
    check("73 + 43 + 83 PSDs", counts == {"TR2": 73, "TR5": 43, "TR7": 83})
    keep = os.path.join(ws, "TR2", "art-source", "masks", "slot_00.psd")
    open(keep, "wb").write(b"owner's")
    r = run(ws)
    check("second run writes nothing", "starter PSDs: 0 written, 199 kept" in r.stdout)
    check("a PSD already there stays", open(keep, "rb").read() == b"owner's")
print("test_track_starter_psds: %s" % ("ok" if failures == 0 else "%d failures" % failures))
sys.exit(1 if failures else 0)
