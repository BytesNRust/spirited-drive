#!/usr/bin/env python3
"""Writes tests/golden/car_mask_class.txt: the class tools/car_mask_from_psd.py gives every colour of a grid, the golden table of the C classifier
(car_mask_class_rgba, tested by test_car_mask_class.c). Needs no Pillow: the tool's colour functions are read with the import stubbed.
The grid: every 17 from 0 to 255, plus the values around each rule's threshold. A line is `R G <classes>`: one letter per blue value of the grid, in
order (see LETTER); every colour is opaque. Run it again only when the tool's rules change, and then say so in the commit."""
import importlib.util
import os
import sys
import types

here = os.path.dirname(os.path.abspath(__file__))
sys.modules.setdefault("PIL", types.ModuleType("PIL"))
sys.modules["PIL"].Image = types.ModuleType("PIL.Image")
sys.modules["PIL.Image"] = sys.modules["PIL"].Image
spec = importlib.util.spec_from_file_location("car_mask_from_psd", os.path.join(here, "..", "tools", "car_mask_from_psd.py"))
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)

LETTER = {0: ".", 128: "S", 192: "B", 224: "K", 240: "R", 244: "P", 248: "H", 252: "V", 255: "M"}
EDGES = [0, 1, 31, 32, 63, 64, 65, 95, 96, 97, 127, 128, 129, 159, 160, 175, 176, 177, 191, 192, 193, 223, 224, 254, 255]
GRID = sorted(set(range(0, 256, 17)) | set(EDGES))


def classify(r, g, b):
    """What keep_mask gives an opaque texel (its body, with the alpha test done by the caller)."""
    lamp = tool.lamp_class(r, g, b)
    if lamp:
        return lamp
    if tool.is_magenta(r, g, b):
        return tool.FIXED_BLACK
    if b > r and b > g:
        return 128
    if tool.dominant(r, g, b) == tool.BODY_KEEPS:
        return 192
    return 255 if max(r, g, b) >= 64 else 0


def main():
    out = os.path.join(here, "golden", "car_mask_class.txt")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        f.write("# the classes of tools/car_mask_from_psd.py for opaque colours, by tests/gen_car_mask_class_golden.py\n")
        f.write("blue " + " ".join(str(v) for v in GRID) + "\n")
        for r in GRID:
            for g in GRID:
                f.write("%d %d %s\n" % (r, g, "".join(LETTER[classify(r, g, b)] for b in GRID)))
    print(out, len(GRID) ** 2, "lines")


if __name__ == "__main__":
    main()
