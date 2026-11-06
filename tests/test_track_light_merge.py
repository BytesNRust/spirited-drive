#!/usr/bin/env python3
"""tools/track_light_merge.py: a track light file the owner edited by hand survives a regeneration (the export's news go in, the edits stay), and with no edits the merge is the new export."""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "tools", "track_light_merge.py")
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import track_light_merge as tlm  # noqa: E402

failures = 0


def check(cond, what):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", what)


ROW = "255 255 61 61 62 63 255 60 61 62 63"
BASE = """# tnfs track light 1
track CY1 time N nodes 12 flags 3ff
rows 0 11 %s
present tunnel
present pool
tunnel 4 7
pool 1 2 2
pool 3 3 4
cast neutral 0 city 1
tunnel_tone night 0x1p+0 0x1.666666p-2 0x1.99999ap-5 174
""" % ROW


def write(dir_, name, text):
    path = os.path.join(dir_, name)
    with open(path, "w") as f:
        f.write(text)
    return path


with tempfile.TemporaryDirectory() as d:
    base = write(d, "base.txt", BASE)
    # the new export: another header, the tunnel one node longer, a lamp pool moved, a row changed at nodes 9..11
    theirs_text = (BASE.replace("flags 3ff", "flags 7ff").replace("tunnel 4 7", "tunnel 4 8").replace("pool 3 3 4", "pool 3 3 3")
                   .replace("rows 0 11 " + ROW, "rows 0 8 " + ROW + "\nrows 9 11 " + ROW.replace("255 255", "200 200", 1)))
    theirs = write(d, "theirs.txt", theirs_text)
    out = os.path.join(d, "out.txt")

    # no edits: the merge is the new export
    subprocess.check_call([sys.executable, TOOL, base, base, theirs, out])
    check(open(out).read() == theirs_text, "no edits: the merge is the new export, byte for byte")

    # the owner's edits (some lines still in the format's first, Spanish words: filas, tono_coche; they merge as the English ones and come out in English): nodes 2..3 have another row, the tunnel's tunnel_tone is his, a brightness and a car_tone line, pool 1..2 class 3
    mine_text = (BASE.replace("rows 0 11 " + ROW, "rows 0 1 " + ROW + "\nfilas 2 3 10 10 10 10 10 10 10 10 10 10 10\nfilas 4 11 " + ROW)
                 .replace("0x1.99999ap-5 174", "0x1p-1 150").replace("pool 1 2 2", "pool 1 2 3") + "brightness 5 6 70\ntono_coche ff8020:60\n")
    mine = write(d, "mine.txt", mine_text)
    subprocess.check_call([sys.executable, TOOL, mine, base, theirs, out])
    merged = tlm.parse(open(out).read())
    text = open(out).read()
    check("flags 7ff" in text, "the header is the new export's")
    check(merged.rows[2] == (10,) * 11 and merged.rows[3] == (10,) * 11, "the owner's rows stay")
    check(merged.rows[0][0] == 255 and merged.rows[8][0] == 255, "rows he did not touch are the export's")
    check(merged.rows[9][0] == 200 and merged.rows[11][0] == 200, "the export's new rows come in where he did not edit")
    check(sorted(merged.flags["tunnel"]) == [4, 5, 6, 7, 8], "the export's longer tunnel comes in")
    check(merged.classes["pool"].get(1) == 3 and merged.classes["pool"].get(2) == 3, "his pool classes stay")
    check(merged.classes["pool"].get(3) == 3, "the export's moved pool comes in")
    check(merged.scalars["tunnel_tone night"].endswith("0x1p-1 150"), "his tunnel_tone stays")
    check("brightness 5 6 70" in merged.extras and "car_tone ff8020:60" in merged.extras, "his brightness and car_tone lines stay")
    check(text.count("brightness") == 1, "a hand-written line is not doubled")

    # a second regeneration of the same export changes nothing more
    again = os.path.join(d, "again.txt")
    subprocess.check_call([sys.executable, TOOL, out, theirs, theirs, again])
    check(open(again).read() == text, "merging the same export again keeps the file")

    # no record of the last export: the file stays, the header is renewed
    subprocess.check_call([sys.executable, TOOL, mine, "-", theirs, out])
    kept = tlm.parse(open(out).read())
    check("flags 7ff" in open(out).read() and kept.rows[2] == (10,) * 11 and sorted(kept.flags["tunnel"]) == [4, 5, 6, 7], "no record: content kept, header renewed")

sys.exit(1 if failures else 0)
