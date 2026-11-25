#!/usr/bin/env python3
"""No call that reads a variable and passes its address to another call in the same argument list: `f(read(&size), size)`. C leaves the order of the
arguments to the compiler: clang on the Mac reads left to right, the Windows build's gcc read `size` first, and every saved config.dat was refused as
0 bytes. Take the pointer from the call on its own line first. Usage: unsequenced_size_lint.py <repo root>; exit 1 on a hit."""
import pathlib
import re
import sys

# outer( ..., inner(..., &x), ..., x ...) on one line, not joined by && or || (those do sequence)
HIT = re.compile(r"\w+\(\s*(?:[^()&|;]*,\s*)?\w+\([^()]*&(\w+)\)\s*,[^;&|]*\b\1\b")
DIRS = ("game", "core", "editor", "studio", "fuzz", "tools", "platform")


def hits(root):
    for d in DIRS:
        for path in sorted((root / d).rglob("*.[ch]")):
            for i, line in enumerate(path.read_text(errors="replace").splitlines()):
                if not line.lstrip().startswith(("/", "*")) and HIT.search(line):
                    yield f"{path.relative_to(root)}:{i + 1}: {line.strip()}"


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    found = list(hits(root))
    for f in found:
        print("unsequenced:", f)
    print(f"unsequenced_size_lint: {len(found)} hit(s)")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
