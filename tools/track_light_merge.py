#!/usr/bin/env python3
"""Three-way merge of a track light file (data/track_light/<TRACK><A|B|C|N>.txt, see game/race/track_light_file.h) for tools/export_track_light.sh.

The export regenerates the files; the owner edits them by hand (a row, a tunnel run, a `tono_tunel`, a `brillo` line ...). To keep the edits through a regeneration the file is
read as what it says per node, and three versions are compared:

    base    the last export (kept gzipped in data/track_light/generated/)
    mine    the file now, with the owner's edits
    theirs  the new export

Each thing a file says is taken from `mine` where `mine` differs from `base` (the owner changed it), else from `theirs` (the export's news): each node's row, each node's membership
of tunel / sala / coche_tenue / canon, each node's pozo / pozo_coche class, the `presente` names, and the `coche_disco`, `reparto` and `tono_tunel` lines. The header line is the new
export's (it records the settings it was made with). Lines the export never writes (`brillo`, `tono_coche`, comments) are `mine`'s, as they are. The merged text is written the way the
game's exporter writes runs, so with no edits it is the new export, byte for byte.

usage: track_light_merge.py MINE BASE THEIRS OUT      (BASE may be a .gz; with no BASE ('-'), MINE counts as all edited: only the header is renewed)
"""
import gzip
import sys

FLAG_KEYS = ("tunel", "sala", "coche_tenue", "canon")
CLASS_KEYS = ("pozo", "pozo_coche")
SCALARS = ("coche_disco", "reparto", "tono_tunel dia", "tono_tunel noche")


class Model:
    def __init__(self):
        self.header = None
        self.nodes = 0
        self.rows = {}                              # node -> tuple of 11 ints
        self.flags = {k: set() for k in FLAG_KEYS}   # key -> nodes
        self.classes = {k: {} for k in CLASS_KEYS}   # key -> {node: class}
        self.present = set()
        self.scalars = {}                           # name -> line
        self.extras = []                            # lines the export does not write, in order


def read_text(path):
    if path.endswith(".gz"):
        with gzip.open(path, "rt", encoding="utf-8") as f:
            return f.read()
    with open(path, encoding="utf-8") as f:
        return f.read()


def parse(text):
    m = Model()
    for raw in text.split("\n"):
        line = raw.strip()
        if not line:
            continue
        w = line.split()
        key = w[0]
        if line == "# tnfs track light 1":
            continue
        if key == "pista":
            m.header = line
            m.nodes = int(w[5])
        elif key == "filas":
            a, b, row = int(w[1]), int(w[2]), tuple(int(v) for v in w[3:14])
            for n in range(a, b + 1):
                m.rows[n] = row
        elif key == "presente":
            m.present.add(w[1])
        elif key in FLAG_KEYS:
            m.flags[key].update(range(int(w[1]), int(w[2]) + 1))
        elif key in CLASS_KEYS:
            for n in range(int(w[1]), int(w[2]) + 1):
                m.classes[key][n] = int(w[3])
        elif key in ("coche_disco", "reparto"):
            m.scalars[key] = line
        elif key == "tono_tunel" and len(w) > 1 and w[1] in ("dia", "noche"):
            m.scalars["tono_tunel " + w[1]] = line
        else:
            m.extras.append(line)   # brillo, tono_coche, comments, anything else
    return m


def pick(mine, base, theirs):
    """`mine` when the owner changed it (it differs from `base`), else `theirs`."""
    return mine if mine != base else theirs


def merge(mine, base, theirs):
    out = Model()
    out.header = theirs.header
    out.nodes = theirs.nodes
    if mine.nodes != theirs.nodes:
        raise ValueError("the node count changed (%d -> %d): regenerate this file by hand" % (mine.nodes, theirs.nodes))
    for n in range(out.nodes):
        out.rows[n] = pick(mine.rows.get(n), base.rows.get(n), theirs.rows.get(n))
    for k in FLAG_KEYS:
        for n in range(out.nodes):
            if pick(n in mine.flags[k], n in base.flags[k], n in theirs.flags[k]):
                out.flags[k].add(n)
    for k in CLASS_KEYS:
        for n in range(out.nodes):
            c = pick(mine.classes[k].get(n, 0), base.classes[k].get(n, 0), theirs.classes[k].get(n, 0))
            if c:
                out.classes[k][n] = c
    for k in FLAG_KEYS + CLASS_KEYS:
        if pick(k in mine.present, k in base.present, k in theirs.present):
            out.present.add(k)
    for s in SCALARS:
        v = pick(mine.scalars.get(s), base.scalars.get(s), theirs.scalars.get(s))
        if v is not None:
            out.scalars[s] = v
    out.extras = list(mine.extras)
    return out


def runs(nodes, same):
    """(first, last) of each run of consecutive `nodes` (sorted) for which same(a, b) holds between neighbours."""
    i = 0
    while i < len(nodes):
        j = i
        while j + 1 < len(nodes) and nodes[j + 1] == nodes[j] + 1 and same(nodes[j + 1], nodes[i]):
            j += 1
        yield nodes[i], nodes[j]
        i = j + 1


def format_model(m):
    lines = ["# tnfs track light 1", m.header]
    rows = sorted(n for n in m.rows if m.rows[n] is not None)
    for a, b in runs(rows, lambda x, first: m.rows[x] == m.rows[first]):
        lines.append("filas %d %d %s" % (a, b, " ".join(str(v) for v in m.rows[a])))
    for k in FLAG_KEYS + CLASS_KEYS:
        if k in m.present:
            lines.append("presente " + k)
    for k in FLAG_KEYS:
        for a, b in runs(sorted(m.flags[k]), lambda x, first: True):
            lines.append("%s %d %d" % (k, a, b))
    for k in CLASS_KEYS:
        for a, b in runs(sorted(m.classes[k]), lambda x, first, c=m.classes[k]: c[x] == c[first]):
            lines.append("%s %d %d %d" % (k, a, b, m.classes[k][a]))
    for s in SCALARS:
        if s in m.scalars:
            lines.append(m.scalars[s])
    lines.extend(m.extras)
    return "\n".join(lines) + "\n"


def merge_files(mine_path, base_path, theirs_path):
    mine, theirs = parse(read_text(mine_path)), parse(read_text(theirs_path))
    if base_path in (None, "-"):
        # no record of the last export: nothing can be told from an edit, so the file stays as it is and only its header is renewed
        mine.header = theirs.header
        return format_model(mine)
    return format_model(merge(mine, parse(read_text(base_path)), theirs))


def main(argv):
    if len(argv) != 5:
        sys.stderr.write(__doc__)
        return 2
    text = merge_files(argv[1], argv[2], argv[3])
    with open(argv[4], "w", encoding="utf-8") as f:
        f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
