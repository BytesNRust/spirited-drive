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
pista CY1 hora N nodos 12 banderas 3ff
filas 0 11 %s
presente tunel
presente pozo
tunel 4 7
pozo 1 2 2
pozo 3 3 4
reparto neutro 0 ciudad 1
tono_tunel noche 0x1p+0 0x1.666666p-2 0x1.99999ap-5 174
""" % ROW


def write(dir_, name, text):
    path = os.path.join(dir_, name)
    with open(path, "w") as f:
        f.write(text)
    return path


with tempfile.TemporaryDirectory() as d:
    base = write(d, "base.txt", BASE)
    # the new export: another header, the tunnel one node longer, a lamp pool moved, a row changed at nodes 9..11
    theirs_text = (BASE.replace("banderas 3ff", "banderas 7ff").replace("tunel 4 7", "tunel 4 8").replace("pozo 3 3 4", "pozo 3 3 3")
                   .replace("filas 0 11 " + ROW, "filas 0 8 " + ROW + "\nfilas 9 11 " + ROW.replace("255 255", "200 200", 1)))
    theirs = write(d, "theirs.txt", theirs_text)
    out = os.path.join(d, "out.txt")

    # no edits: the merge is the new export
    subprocess.check_call([sys.executable, TOOL, base, base, theirs, out])
    check(open(out).read() == theirs_text, "no edits: the merge is the new export, byte for byte")

    # the owner's edits: nodes 2..3 have another row, the tunnel's tono_tunel is his, a brillo and a tono_coche line, pozo 1..2 class 3
    mine_text = (BASE.replace("filas 0 11 " + ROW, "filas 0 1 " + ROW + "\nfilas 2 3 10 10 10 10 10 10 10 10 10 10 10\nfilas 4 11 " + ROW)
                 .replace("0x1.99999ap-5 174", "0x1p-1 150").replace("pozo 1 2 2", "pozo 1 2 3") + "brillo 5 6 70\ntono_coche ff8020:60\n")
    mine = write(d, "mine.txt", mine_text)
    subprocess.check_call([sys.executable, TOOL, mine, base, theirs, out])
    merged = tlm.parse(open(out).read())
    text = open(out).read()
    check("banderas 7ff" in text, "the header is the new export's")
    check(merged.rows[2] == (10,) * 11 and merged.rows[3] == (10,) * 11, "the owner's rows stay")
    check(merged.rows[0][0] == 255 and merged.rows[8][0] == 255, "rows he did not touch are the export's")
    check(merged.rows[9][0] == 200 and merged.rows[11][0] == 200, "the export's new rows come in where he did not edit")
    check(sorted(merged.flags["tunel"]) == [4, 5, 6, 7, 8], "the export's longer tunnel comes in")
    check(merged.classes["pozo"].get(1) == 3 and merged.classes["pozo"].get(2) == 3, "his pozo classes stay")
    check(merged.classes["pozo"].get(3) == 3, "the export's moved pozo comes in")
    check(merged.scalars["tono_tunel noche"].endswith("0x1p-1 150"), "his tono_tunel stays")
    check("brillo 5 6 70" in merged.extras and "tono_coche ff8020:60" in merged.extras, "his brillo and tono_coche lines stay")
    check(text.count("brillo") == 1, "a hand-written line is not doubled")

    # a second regeneration of the same export changes nothing more
    again = os.path.join(d, "again.txt")
    subprocess.check_call([sys.executable, TOOL, out, theirs, theirs, again])
    check(open(again).read() == text, "merging the same export again keeps the file")

    # no record of the last export: the file stays, the header is renewed
    subprocess.check_call([sys.executable, TOOL, mine, "-", theirs, out])
    kept = tlm.parse(open(out).read())
    check("banderas 7ff" in open(out).read() and kept.rows[2] == (10,) * 11 and sorted(kept.flags["tunel"]) == [4, 5, 6, 7], "no record: content kept, header renewed")

sys.exit(1 if failures else 0)
