#!/usr/bin/env python3
"""ui_kit_title_check.py CONTENT.png: a screen's red title is the CD's (docs/ui-kit.md, E1): first column x 9 and capitals' rows 66-83 (18), each
+-1, in a 640x480 picture of CONTENT. Needs Pillow; exit 1 when it is off."""
import sys

from PIL import Image

BOX = (0, 50, 200, 110)   # x0, y0, x1, y1 where the title can be
X, TOP, BOTTOM, TOLERANCE = 9, 66, 83, 1


def main():
    im = Image.open(sys.argv[1]).convert("RGB")
    xs, ys = [], []
    for y in range(BOX[1], BOX[3]):
        for x in range(BOX[0], BOX[2]):
            r, g, b = im.getpixel((x, y))
            if r > 200 and g < 60 and b < 60:   # the title's red (238,0,0)
                xs.append(x)
                ys.append(y)
    if not xs:
        print("FAIL title: no red title found in", BOX)
        return 1
    got = (min(xs), min(ys), max(ys))
    bad = [name for name, a, b in (("first column", got[0], X), ("top row", got[1], TOP), ("bottom row", got[2], BOTTOM)) if abs(a - b) > TOLERANCE]
    for name in bad:
        print(f"FAIL title: {name} off the CD's (x {X}, rows {TOP}-{BOTTOM}): got x {got[0]}, rows {got[1]}-{got[2]}")
    print(f"title: x {got[0]}, rows {got[1]}-{got[2]} ({got[2] - got[1] + 1})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
