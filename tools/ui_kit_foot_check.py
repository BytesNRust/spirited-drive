"""tools/ui_kit_foot_check.py IMAGE...: measures a 640x480 capture of CONTENT's foot against the UI kit's foot rule (docs/ui-kit.md E3): the left item's
ink starts at the CD's DONE margin (x 10), the right item's ink ends the same from the right edge (x 630), the buttons between are centred in the room
between those two with equal gaps. The capture is of an edition of the player's own (all four buttons cream; a standard edition dims two).
Exit: 0 all in tolerance; 1 a measure off; 2 bad arguments or no Pillow (the caller treats that as a skip)."""
import sys

try:
    from PIL import Image
except ImportError:
    print("ui_kit_foot_check: no Pillow, skipping")
    sys.exit(2)

ROWS = (443, 460)                  # the foot's words
LIGHT, WORD_GAP = 500, 12   # a pixel is ink when R+G+B is over this (the letters, edges included, are lighter than the page); columns less than this apart are one word
MARGIN, WIDTH, SLACK = 10, 640, 1
GAP, GAP_SLACK, CENTRE_SLACK = 18, 2, 2   # UI_FOOT_GAP; the letters' edges blur the ink by a pixel


def words(img):
    px = img.load()
    cols = [x for x in range(WIDTH) if any(sum(px[x, y][:3]) > LIGHT for y in range(*ROWS))]
    out, start, prev = [], None, None
    for x in cols:
        if start is None:
            start = prev = x
        elif x - prev <= WORD_GAP:
            prev = x
        else:
            out.append((start, prev))
            start = prev = x
    if start is not None:
        out.append((start, prev))
    return out


def main(paths):
    if not paths:
        print(__doc__)
        return 2
    bad = 0
    for path in paths:
        w = words(Image.open(path).convert("RGB"))
        if len(w) != 6:
            print(f"FAIL {path}: {len(w)} words in the foot, not Done, New, Duplicate, Rename, Delete, Install...: {w}")
            bad += 1
            continue
        gaps = [w[i + 1][0] - w[i][1] - 1 for i in range(1, 4)]
        before, after = w[1][0] - w[0][1] - 1, w[5][0] - w[4][1] - 1
        checks = [("left margin", w[0][0], MARGIN, SLACK), ("right margin", WIDTH - 1 - w[5][1], MARGIN, SLACK),
                  ("smallest gap between the buttons", min(gaps), GAP, GAP_SLACK), ("largest gap between the buttons", max(gaps), GAP, GAP_SLACK),
                  ("room before the group less after", before - after, 0, CENTRE_SLACK)]
        for name, got, want, slack in checks:
            ok = abs(got - want) <= slack
            bad += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {path}: {name} {got} (want {want} +-{slack})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
