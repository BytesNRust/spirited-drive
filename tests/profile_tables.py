#!/usr/bin/env python3
"""The profile TABLES (game/sys/port_profile.c) against the Table struct and docs/settings.md.

usage: profile_tables.py REPO [--sabotage]

--sabotage makes seven mistakes in copies (a value dropped from a row, one added, a wrong value, a docs row deleted, a wrong docs value, a wrong column
count, a key with no docs row) and fails unless each is caught.

Checks, each failing with the names involved:
  1. every TABLES row has exactly as many values as the Table struct has fields (a short row compiles: C fills the rest with 0);
  2. docs/settings.md's "What each profile sets" table lists every field once, with each profile's value (the order is not checked);
     its "All N columns" sentence says the right N;
  3. every key port_settings.c reads from port.cfg (INT_KEYS) has a row in docs/settings.md.
"""
import re
import sys


def strip_comments(text):
    return re.sub(r"//.*", "", re.sub(r"/\*.*?\*/", "", text, flags=re.S))


def table_fields(src):
    end = src.index("} Table;")
    start = src.rindex("typedef struct {", 0, end)
    body = strip_comments(src[start + len("typedef struct {"):end])
    names = []
    for decl in body.split(";"):
        decl = decl.strip()
        if not decl:
            continue
        decl = re.sub(r"^(int|float)\s+", "", decl)
        names += [n.strip() for n in decl.split(",") if n.strip()]
    return names


def table_rows(src):
    rows = {}
    for m in re.finditer(r"\[PROFILE_(STOCK|RESTORED|RESTOMOD)\]\s*=\s*\{(.*?)\},[^\n]*\n\s*(?=\[PROFILE_|\};)", src, re.S):
        rows[m.group(1)] = [v.strip() for v in strip_comments(m.group(2)).split(",") if v.strip()]
    return rows


def doc_columns(doc):
    start = doc.index("## What each profile sets")
    end = doc.index("\n## ", start + 1)
    section = doc[start:end]
    said = re.search(r"All (\d+) columns", section)
    cols = []
    for line in section.splitlines():
        m = re.match(r"\|\s*`(\w+)`[^|]*\|[^|]*\|\s*(-?\d+)[^|]*\|\s*(-?\d+)[^|]*\|\s*(-?\d+)[^|]*\|\s*$", line)
        if m:
            cols.append((m.group(1), m.group(2), m.group(3), m.group(4)))
    return (int(said.group(1)) if said else None), cols


def int_keys(src):
    start = src.index("INT_KEYS[] = {")
    end = src.index("\n};", start)
    return re.findall(r'\{\s*"(\w+)",\s*offsetof', src[start:end])


def check(profile, settings, doc):
    """The list of what is wrong; empty when the three agree."""
    bad = []
    fields = table_fields(profile)
    rows = table_rows(profile)
    for name in ("STOCK", "RESTORED", "RESTOMOD"):
        if name not in rows:
            bad.append(f"no TABLES row for {name}")
        elif len(rows[name]) != len(fields):
            bad.append(f"TABLES row {name} has {len(rows[name])} values, the Table struct has {len(fields)} fields")
    said, cols = doc_columns(doc)
    if said != len(fields):
        bad.append(f"docs/settings.md says 'All {said} columns', the Table struct has {len(fields)}")
    doc_names = [c[0] for c in cols]
    missing = [f for f in fields if f not in doc_names]
    extra = [d for d in doc_names if d not in fields]
    twice = sorted({d for d in doc_names if doc_names.count(d) > 1})
    if missing or extra or twice:
        bad.append(f"docs/settings.md's TABLES table differs from the struct: missing {missing}, not in the struct {extra}, listed twice {twice}")
    elif all(n in rows and len(rows[n]) == len(fields) for n in ("STOCK", "RESTORED", "RESTOMOD")):
        shown_by_name = {name: shown for name, *shown in cols}
        for i, name in enumerate(fields):
            want = [rows["STOCK"][i], rows["RESTORED"][i], rows["RESTOMOD"][i]]
            if shown_by_name[name] != want:
                bad.append(f"docs/settings.md shows {name} as {'/'.join(shown_by_name[name])} (Stock/Restored/Restomod), TABLES has {'/'.join(want)}")
        # the settings tables' own "Stock / Restored / Restomod" cell, where it is three plain numbers
        section_end = doc.index("## What each profile sets")
        for m in re.finditer(r"^\|\s*`(\w+)`.*\|\s*(-?\d+) / (-?\d+) / (-?\d+)\s*\|[^|]*\|\s*$", doc[:section_end], re.M):
            name, shown = m.group(1), list(m.group(2, 3, 4))
            if name in fields:
                i = fields.index(name)
                want = [rows["STOCK"][i], rows["RESTORED"][i], rows["RESTOMOD"][i]]
                if shown != want:
                    bad.append(f"docs/settings.md's settings row for {name} says {'/'.join(shown)}, TABLES has {'/'.join(want)}")
    documented = set(re.findall(r"^\|\s*`(\w+)`", doc[:doc.index("## What each profile sets")], re.M))   # the TABLES table does not count: it lists columns, not what a key is
    for key in int_keys(settings):
        if key not in documented:
            bad.append(f"port.cfg key '{key}' (port_settings.c) has no row in docs/settings.md")
    return bad


def read(repo):
    return [open(f"{repo}/{path}").read() for path in ("game/sys/port_profile.c", "game/sys/port_settings.c", "docs/settings.md")]


def sabotage(profile, settings, doc):
    """Each mistake below, made in a copy, must be caught."""
    row = re.search(r"\[PROFILE_STOCK\]\s*=\s*\{[^\n]*?\},", profile).group(0)
    cell = "| `lamp_halos` | settings | 0 | 0 | 1 |"
    head_end = doc.index("## What each profile sets")   # every row of the key above the TABLES table: a key may have a second row (the table of the settings Restomod leaves with no visible effect)
    no_key_row = re.sub(r"^\| `wipers` \|[^\n]*\n", "", doc[:head_end], flags=re.M) + doc[head_end:]
    cases = {
        "a value dropped from a TABLES row": (profile.replace(row, re.sub(r",\s*\d+\s*\},?$", " },", row), 1), settings, doc),
        "a value added to a TABLES row": (profile.replace(row, row.replace("{ ", "{ 0, ", 1), 1), settings, doc),
        "a TABLES value changed against the docs": (profile.replace(row, row.replace("{ 0,", "{ 1,", 1), 1), settings, doc),
        "a docs TABLES row deleted": (profile, settings, doc.replace(cell + "\n", "", 1)),
        "a docs TABLES value changed": (profile, settings, doc.replace(cell, "| `lamp_halos` | settings | 0 | 0 | 0 |", 1)),
        "a docs column count that is wrong": (profile, settings, re.sub(r"All \d+ columns", "All 3 columns", doc, count=1)),
        "a port.cfg key with no docs row": (profile, settings, no_key_row),
    }
    failed = []
    for what, args in cases.items():
        if args == (profile, settings, doc):
            failed.append(f"{what}: the mutation changed nothing (the test's own pattern is stale)")
        elif not check(*args):
            failed.append(f"{what}: not caught")
    return failed


def main(repo, mode):
    profile, settings, doc = read(repo)
    if mode == "--sabotage":
        if check(profile, settings, doc):
            print("FAIL: the tables are wrong as they stand, the sabotage run needs them right")
            return 1
        failed = sabotage(profile, settings, doc)
        for line in failed:
            print("FAIL:", line)
        if not failed:
            print("PASS: every sabotaged copy is caught")
        return 1 if failed else 0
    bad = check(profile, settings, doc)
    for line in bad:
        print("FAIL:", line)
    if bad:
        return 1
    print(f"PASS: {len(table_fields(profile))} columns, 3 rows, {len(int_keys(settings))} keys documented")
    return 0


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    sys.exit(main(args[0] if args else ".", "--sabotage" if "--sabotage" in sys.argv else ""))
