#!/usr/bin/env python3
"""The banner's tool (tools/spirited_logo.py) and the file the boot screen loads (data/logo/spirited_logo.png): every letter of the name has an outline, the rows end
at the same place, the drawing is the same from run to run, and the PNG is what the game asks for, 580 x 140 RGBA with a soft shadow. Skipped (exit 0) when Pillow is
not installed; the rendering checks are skipped when rsvg-convert is not."""
import importlib.util
import os
import shutil
import sys

here = os.path.dirname(os.path.abspath(__file__))
try:
    spec = importlib.util.spec_from_file_location("spirited_logo", os.path.join(here, "..", "tools", "spirited_logo.py"))
    logo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(logo)
    from PIL import Image
except ImportError as error:
    print("test_spirited_logo: skipped (%s)" % error)
    sys.exit(0)

failures = 0


def check(name, got, want):
    global failures
    if got != want:
        failures += 1
        print("FAIL %s: got %r, want %r" % (name, got, want))


def shape_of(im):
    return (im.size, im.mode)


def soft_and_solid(im):
    """Whether the picture has clear corners, solid ink in both rows' colours, and a dark shadow that is only partly there."""
    px = im.load()
    corners = [px[0, 0][3], px[im.width - 1, im.height - 1][3]]
    red = amber = soft = 0
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if a == 255 and r > 200 and g < 70 and b < 90:
                red += 1
            if a == 255 and r > 200 and 90 < g < 200 and b < 60:
                amber += 1
            if 30 < a < 220 and max(r, g, b) < 40:
                soft += 1
    return corners == [0, 0], red > 2000, amber > 2000, soft > 2000


words = [word for word, _, _ in logo.ROWS]
check("the name", words, ["SPIRITED", "DRIVE"])
check("a letter for every character", sorted(set("".join(words)) - set(logo.LETTERS)), [])
check("the box", (logo.W, logo.H), (580, 140))
check("the longer row fits the box", logo.row_width("SPIRITED") * logo.SX < logo.RIGHT, True)
check("the rows stay inside the box's height", logo.ROW_TOP[1] + logo.CAP * logo.SY + logo.SHADOW < logo.H, True)
check("the rows do not overlap", logo.ROW_TOP[0] + logo.CAP * logo.SY <= logo.ROW_TOP[1], True)
check("the drawing repeats", logo.svg() == logo.svg(), True)
check("no font is used", "<text" in logo.svg() or "font-family" in logo.svg(), False)
check("the grain repeats", [logo.grain(x, 7) for x in range(5)], [logo.grain(x, 7) for x in range(5)])
check("the grain stays in range", all(-1 <= logo.grain(x, y) <= 1 for x in range(40) for y in range(40)), True)

shipped = os.path.join(here, "..", "data", "logo", "spirited_logo.png")
check("the shipped file is there", os.path.isfile(shipped), True)
if os.path.isfile(shipped):
    with Image.open(shipped) as im:
        check("the shipped file's size and mode", shape_of(im), ((580, 140), "RGBA"))
        check("the shipped file: clear corners, red, amber, soft shadow", soft_and_solid(im.convert("RGBA")), (True, True, True, True))

if shutil.which("rsvg-convert"):
    first = logo.finish(logo.render(logo.svg()))
    second = logo.finish(logo.render(logo.svg()))
    check("the render's size and mode", shape_of(first), ((580, 140), "RGBA"))
    check("the render repeats", first.tobytes() == second.tobytes(), True)
    check("the render: clear corners, red, amber, soft shadow", soft_and_solid(first), (True, True, True, True))
else:
    print("test_spirited_logo: rendering checks skipped (no rsvg-convert)")

print("test_spirited_logo: %s" % ("FAILED" if failures else "ok"))
sys.exit(1 if failures else 0)
