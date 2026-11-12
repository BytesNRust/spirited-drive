#!/usr/bin/env python3
"""The icon's tool (tools/spirited_icon.py) and the files it ships under data/icon: the hand-drawn 16 px picture is a full 16 x 16 grid of known inks, every size has its
PNG at that size, the .ico holds the sizes Windows asks for and the .icns is where the macOS packaging looks. Skipped (exit 0) when Pillow is not installed."""
import importlib.util
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
try:
    spec = importlib.util.spec_from_file_location("spirited_icon", os.path.join(here, "..", "tools", "spirited_icon.py"))
    icon = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(icon)
    from PIL import Image
except ImportError as error:
    print("test_spirited_icon: skipped (%s)" % error)
    sys.exit(0)

failures = 0


def check(name, got, want):
    global failures
    if got != want:
        failures += 1
        print("FAIL %s: got %r, want %r" % (name, got, want))


check("16 rows", len(icon.PIXELS_16), 16)
check("16 columns in every row", [len(row) for row in icon.PIXELS_16], [16] * 16)
check("every pixel has an ink", sorted(set("".join(icon.PIXELS_16)) - set(icon.INKS_16)), [])
check("the 16 px picture", icon.finish(None, 16).size, (16, 16))
check("the small sizes show the key alone", icon.SMALL, (16, 32))
check("every drawn size has a finish", sorted(set(min(n, icon.GRID) for n in icon.SMALL + icon.LARGE) - set(icon.FINISH) - {16}), [])
check("the big sizes are whole multiples of the grid", [n % icon.GRID for n in icon.LARGE if n > icon.GRID], [0, 0, 0])
check("the initials are the banner's letters", "<text" in icon.keys(), False)

shipped = os.path.join(here, "..", "data", "icon")
for size in icon.SMALL + icon.LARGE:
    path = os.path.join(shipped, "spirited_%d.png" % size)
    check("spirited_%d.png is there" % size, os.path.isfile(path), True)
    if os.path.isfile(path):
        with Image.open(path) as im:
            check("spirited_%d.png's size" % size, im.size, (size, size))
ico = os.path.join(shipped, "spirited.ico")
check("spirited.ico is there", os.path.isfile(ico), True)
if os.path.isfile(ico):
    with Image.open(ico) as im:
        check("the .ico's sizes", sorted(im.ico.sizes()), [(16, 16), (32, 32), (48, 48), (256, 256)])
check("tnfs.icns is there", os.path.isfile(os.path.join(shipped, "tnfs.icns")), True)


def solid_box(im):
    """The box of the fully opaque pixels: the body, not its shadow."""
    return im.split()[3].point(lambda v: 255 if v == 255 else 0).getbbox()


# macOS (Big Sur and later) app icon grid: an 824 x 824 body centred on 1024, 100 px of margin, corners of about 185 px,
# a soft shadow under it inside the margin. Every size in the .icns has the same proportions.
check("the macOS grid", (icon.MAC_BODY, icon.MAC_RADIUS), (824 / 1024, 185 / 824))
scene = icon.scene_small()
for size in (32, 1024):
    mac = icon.mac_icon(scene if size == 32 else icon.scene_large(), size)
    m = round(size * 100 / 1024)
    check("mac %d: size" % size, (mac.size, mac.mode), ((size, size), "RGBA"))
    check("mac %d: the body sits inside the margin" % size, solid_box(mac), (m, m, size - m, size - m))
    check("mac %d: the corner is clear" % size, mac.getpixel((m + 1, m + 1))[3] < 255, True)
    check("mac %d: nothing at the canvas edge" % size, mac.getpixel((0, 0))[3], 0)
big = icon.mac_icon(icon.scene_large(), 1024)
check("the shadow falls below the body", big.getpixel((512, 924 + 8))[3] > big.getpixel((512, 100 - 8))[3], True)
check("the shadow stays soft", 0 < big.getpixel((512, 924 + 8))[3] <= 128, True)
icns = os.path.join(shipped, "tnfs.icns")
if os.path.isfile(icns):
    with Image.open(icns) as im:
        im.size = (1024, 1024)
        im.load()
        check("the .icns has the margin", solid_box(im.convert("RGBA")), (100, 100, 924, 924))
with Image.open(os.path.join(shipped, "spirited_1024.png")) as im:
    check("the PNG set stays full-bleed", im.convert("RGBA").getpixel((60, 512))[3], 255)

print("test_spirited_icon: %s" % ("FAILED" if failures else "ok"))
sys.exit(1 if failures else 0)
