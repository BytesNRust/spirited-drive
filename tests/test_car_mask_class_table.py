#!/usr/bin/env python3
"""The golden table of the C classifier (tests/golden/car_mask_class.txt) is the PSD tool's rules still: it is made again from tools/car_mask_from_psd.py
(no Pillow needed, the import is stubbed) and compared. Fails when the tool's rules change without the table (and the C classifier) following."""
import importlib.util
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("gen", os.path.join(here, "gen_car_mask_class_golden.py"))
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)

want = []
want.append("blue " + " ".join(str(v) for v in gen.GRID))
for r in gen.GRID:
    for g in gen.GRID:
        want.append("%d %d %s" % (r, g, "".join(gen.LETTER[gen.classify(r, g, b)] for b in gen.GRID)))
have = [l.rstrip("\n") for l in open(os.path.join(here, "golden", "car_mask_class.txt")) if not l.startswith("#")]
if have != want:
    print("FAIL: tests/golden/car_mask_class.txt is not what tools/car_mask_from_psd.py gives: run tests/gen_car_mask_class_golden.py (and change car_mask_class_rgba to match)")
    sys.exit(1)
print("car_mask_class_table: ok (%d colours)" % (len(gen.GRID) ** 3))
