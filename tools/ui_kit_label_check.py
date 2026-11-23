#!/usr/bin/env python3
"""ui_kit_label_check.py RECORDS.png: the labels' measures (docs/ui-kit.md) on a 640x480 picture of RECORDS (menu_walk.sh records): DONE's capital
height is the CD's 11 rows (+-1) and its edges are mixed with the page, not cut to one bit (a fraction of its edge pixels is neither the letter's
yellow nor the red page). exit 1 when a measure fails."""
import sys

import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rgb_image import Image   # the standard library only: no Pillow in CI or the gate

DONE_BOX = (0, 430, 100, 470)   # x0, y0, x1, y1: the footer's DONE (car_records_view.h's CAR_RECORDS_DONE_X, FOOT_CAP)
CAP_ROWS, CAP_TOLERANCE = 11, 1
MIN_SOFT = 0.05   # the fraction of the letters' edge pixels that are mixes: ours 0.12 (the CD's own: not measured)


def main():
    im = Image.open(sys.argv[1]).convert("RGB")
    rows = {}
    letter = soft = 0
    for y in range(DONE_BOX[1], DONE_BOX[3]):
        for x in range(DONE_BOX[0], DONE_BOX[2]):
            r, g, b = im.getpixel((x, y))
            if r > 180 and g > 150 and b < 110:   # the yellow
                letter += 1
                rows[y] = rows.get(y, 0) + 1
            elif r > 150 and 90 <= g <= 150 and b < 100:   # between the yellow and the red page
                soft += 1
    solid = [y for y, n in rows.items() if n >= 3]
    if not solid:
        print("FAIL label: no DONE letters found in", DONE_BOX)
        return 1
    cap = max(solid) - min(solid) + 1
    fraction = soft / (letter + soft)
    bad = []
    if abs(cap - CAP_ROWS) > CAP_TOLERANCE:
        bad.append(f"DONE is {cap} rows tall, the CD's capitals are {CAP_ROWS} +-{CAP_TOLERANCE}")
    if fraction < MIN_SOFT:
        bad.append(f"DONE's edges are cut to one bit: {fraction:.3f} of its pixels are mixes, at least {MIN_SOFT} wanted")
    for line in bad:
        print("FAIL label:", line)
    print(f"label: DONE {cap} rows, mixed edge pixels {fraction:.3f}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
