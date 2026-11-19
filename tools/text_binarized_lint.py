#!/usr/bin/env python3
"""No binarized text outside the kit: a text mask (coverage 0..15) cut to one bit and stored as a flat colour is what made the labels look pixelled
(dep59 point 27). Outside game/sys/text*.c, a line that tests a mask's coverage (`mask[...] >= N`) must not sit within three lines of a pixel
store (`to[...] = (uint8_t)...`): text goes through text_label.h, whose edges are mixes. Usage: text_binarized_lint.py <repo root>; exit 1 on a hit."""
import pathlib
import re
import sys

TEST = re.compile(r"\bmask\[[^\]]*\]\s*(>=|>)\s*\w+")
STORE = re.compile(r"\bto\[[^\]]*\]\s*=\s*\(uint8_t\)")


def hits(root):
    for path in sorted((root / "game").rglob("*.c")):
        if path.parent.name == "sys" and path.name.startswith("text"):
            continue
        lines = path.read_text(errors="replace").splitlines()
        for i, line in enumerate(lines):
            if TEST.search(line) and any(STORE.search(l) for l in lines[max(0, i - 3):i + 4]):
                yield f"{path.relative_to(root)}:{i + 1}: {line.strip()}"


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    found = list(hits(root))
    for f in found:
        print("binarized text:", f)
    print(f"text_binarized_lint: {len(found)} hit(s)")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
