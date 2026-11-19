#!/usr/bin/env python3
"""ui_kit_selector_check.py CONTENT.png: a selector's triangles keep the CD's gaps from its text (docs/ui-kit.md, E2b). On a 640x480 picture of
CONTENT with an edition of the player's own (no note under the name), the selector's band holds three runs of bright ink: the left triangle, the
name, the right triangle. The blank columns between them are at least the pager's 14 (left) and 15 (right), measured on the CD's own. Needs Pillow."""
import sys

from PIL import Image

BAND = (230, 62, 600, 88)   # x0, y0, x1, y1 of the selector's name line
GAP_LEFT, GAP_RIGHT = 14, 15
BRIGHT = 150   # luminance of the cream ink; the page behind it is darker


def main():
    im = Image.open(sys.argv[1]).convert("RGB")
    cols = []
    for x in range(BAND[0], BAND[2]):
        cols.append(any(sum(im.getpixel((x, y))) / 3 > BRIGHT for y in range(BAND[1], BAND[3])))
    runs, start = [], None
    for i, ink in enumerate(cols + [False]):
        if ink and start is None:
            start = i
        elif not ink and start is not None:
            runs.append((start, i - 1))
            start = None
    # a letter's own blank columns are a few wide: runs closer than 8 columns are one word; the triangles are 9 wide, the gaps 14 and more
    merged = []
    for r in runs:
        if merged and r[0] - merged[-1][1] - 1 < 8:
            merged[-1] = (merged[-1][0], r[1])
        else:
            merged.append(r)
    merged = merged[-3:] if len(merged) > 3 and merged[0][1] < 20 else merged   # a bright speck of the page at the band's left edge is not ink of the selector
    if len(merged) != 3:
        print(f"FAIL selector: {len(merged)} runs of ink in the band, three wanted (triangle, name, triangle): {merged}")
        return 1
    left, name, right = merged
    gap_l, gap_r = name[0] - left[1] - 1, right[0] - name[1] - 1
    bad = []
    if gap_l < GAP_LEFT:
        bad.append(f"left gap {gap_l} columns, the CD's is {GAP_LEFT}")
    if gap_r < GAP_RIGHT:
        bad.append(f"right gap {gap_r} columns, the CD's is {GAP_RIGHT}")
    centre_error = abs((left[0] + right[1]) / 2 - (name[0] + name[1]) / 2)
    if centre_error > 3:
        bad.append(f"the block is {centre_error:.1f} columns off centre")
    for line in bad:
        print("FAIL selector:", line)
    print(f"selector: gaps {gap_l} / {gap_r}, triangles {left[1] - left[0] + 1} and {right[1] - right[0] + 1} wide, off centre {centre_error:.1f}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
