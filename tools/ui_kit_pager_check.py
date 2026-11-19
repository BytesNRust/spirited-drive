"""tools/ui_kit_pager_check.py IMAGE...: measures the page indicator (the UI kit's pager, docs/ui-kit.md E4) on 640x480 captures of a screen's
middle page ("2/3": a triangle on each side) and fails when a measure is more than a pixel off the CD's.
The CD's, measured on its track select ("Road/track") and showcase ("Next car"): the letters are 11 rows of cream, each triangle's cream ink is 9 x 17, the blank columns
between the left one and the text are 14 and between the text and the right one 15, and the triangles' middle row is one below the text's.
Exit: 0 all in tolerance; 1 a measure off; 2 bad arguments or no Pillow (the caller treats that as a skip)."""
import sys

try:
    from PIL import Image
except ImportError:
    print("ui_kit_pager_check: no Pillow, skipping")
    sys.exit(2)

BOX = (230, 436, 420, 476)       # the pager's room on a 640x480 screen
CREAM, TOL, ARROW_GAP = (232, 211, 183), 12, 10   # the fill and its grain; the soft look's darker rim is not ink
WANT = {"arrow_w": 9, "arrow_h": 17, "gap_left": 14, "gap_right": 15, "centre_below": 1, "text_h": 11}
SLACK = 1


def runs(img):
    x0, y0, x1, y1 = BOX
    px = img.load()

    def ink(x, y):
        return all(abs(a - b) <= TOL for a, b in zip(px[x, y][:3], CREAM))

    cols = [x for x in range(x0, x1) if any(ink(x, y) for y in range(y0, y1))]
    groups, start, prev = [], None, None
    for x in cols:
        if start is None:
            start = prev = x
        elif x - prev <= ARROW_GAP:
            prev = x
        else:
            groups.append((start, prev))
            start = prev = x
    if start is not None:
        groups.append((start, prev))
    out = []
    for a, b in groups:
        rows = [y for y in range(y0, y1) if any(ink(x, y) for x in range(a, b + 1))]
        out.append((a, b, rows[0], rows[-1]))
    return out


def measure(path):
    g = runs(Image.open(path).convert("RGB"))
    if len(g) < 3:
        return None, f"{len(g)} blocks of ink in the pager's room, not a triangle, the count and a triangle: {g}"
    (la, lb, lt, lbm), (ra, rb, rt, rbm) = g[0], g[-1]
    ta, tb = g[1][0], g[-2][1]   # the count: what lies between the triangles (a "/" is another colour, so it is several blocks)
    tt, tbm = min(b[2] for b in g[1:-1]), max(b[3] for b in g[1:-1])
    mid = lambda t, b: (t + b) / 2
    return {
        "arrow_w": ((lb - la + 1) + (rb - ra + 1)) / 2,
        "arrow_h": ((lbm - lt + 1) + (rbm - rt + 1)) / 2,
        "gap_left": ta - lb - 1,
        "gap_right": ra - tb - 1,
        "centre_below": (mid(lt, lbm) + mid(rt, rbm)) / 2 - mid(tt, tbm),
        "text_h": tbm - tt + 1,
        "text_rows": (tt, tbm),
    }, None


def main(paths):
    if not paths:
        print(__doc__)
        return 2
    bad = 0
    for path in paths:
        got, why = measure(path)
        if got is None:
            print(f"FAIL {path}: {why}")
            bad += 1
            continue
        print(f"     {path}: the count's rows {got['text_rows'][0]}-{got['text_rows'][1]}")
        for k, want in WANT.items():
            ok = abs(got[k] - want) <= SLACK
            bad += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {path}: {k} {got[k]:g} (CD {want})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
