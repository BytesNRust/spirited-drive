#!/usr/bin/env python3
"""The dash mask tool (tools/dash_mask_from_psd.py): its classes are dash_mask.h's, a painted colour reads as the class the game reads it as, a layer placed
off the origin lands where it is, and a saved PNG reads back. Skipped (exit 0) when Pillow is not installed."""
import importlib.util
import os
import re
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
try:
    from PIL import Image
    spec = importlib.util.spec_from_file_location("dash_mask_from_psd", os.path.join(here, "..", "tools", "dash_mask_from_psd.py"))
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
except ImportError as error:
    print("test_dash_mask_psd: skipped (%s)" % error)
    sys.exit(0)

header = open(os.path.join(here, "..", "game", "race", "dash_mask.h")).read()
want = dict((name, int(value)) for name, value in re.findall(r"DASH_MASK_(AUTO|DASH|DIAL|NOSE|BLADE|LIT) = (\d+)", header))
failures = 0


def check(name, got, expected):
    global failures
    if got != expected:
        failures += 1
        print("FAIL %s: got %r, want %r" % (name, got, expected))


check("the tool's classes are the header's", (tool.AUTO, tool.DASH, tool.DIAL, tool.NOSE, tool.BLADE, tool.LIT),
      (want["AUTO"], want["DASH"], want["DIAL"], want["NOSE"], want["BLADE"], want["LIT"]))
check("black", tool.mask_class(0, 0, 0), tool.AUTO)
check("a transparent pixel", tool.mask_class(0, 255, 0, 10), tool.AUTO)
check("blue", tool.mask_class(0, 0, 255), tool.DASH)
check("green", tool.mask_class(0, 255, 0), tool.DIAL)
check("white", tool.mask_class(255, 255, 255), tool.LIT)
check("light grey", tool.mask_class(200, 200, 200), tool.LIT)
check("yellow is lit", tool.mask_class(255, 255, 0), tool.LIT)
check("a pale yellow is lit", tool.mask_class(200, 195, 60), tool.LIT)
check("orange is not marked", tool.mask_class(255, 160, 0), tool.AUTO)
check("a dark yellow is not marked", tool.mask_class(150, 150, 0), tool.AUTO)
check("a green with some red stays a dial", tool.mask_class(120, 255, 0), tool.DIAL)
check("a transparent yellow is not marked", tool.mask_class(255, 255, 0, 10), tool.AUTO)
check("red is a wiper blade", tool.mask_class(255, 0, 0), tool.BLADE)
check("a dark red is not marked", tool.mask_class(150, 0, 0), tool.AUTO)
check("cyan is the nose", tool.mask_class(0, 255, 255), tool.NOSE)
check("a pale cyan is the nose", tool.mask_class(40, 220, 200), tool.NOSE)
check("a dark cyan is not marked", tool.mask_class(0, 150, 150), tool.AUTO)
check("a mid grey is not marked", tool.mask_class(128, 128, 128), tool.AUTO)

layer = Image.new("RGBA", (3, 2), (0, 0, 0, 0))
layer.putpixel((0, 0), (0, 255, 0, 255))
layer.putpixel((2, 1), (255, 255, 255, 255))
placed = tool.mask_image(8, 6, layer, 4, 3)   # the layer moved to (4, 3)
check("layer placed: green at (4, 3)", placed.getpixel((4, 3)), tool.DIAL)
check("layer placed: white at (6, 4)", placed.getpixel((6, 4)), tool.LIT)
check("layer placed: elsewhere AUTO", placed.getpixel((0, 0)), tool.AUTO)
yellow_layer = Image.new("RGBA", (2, 1), (0, 0, 0, 0))
yellow_layer.putpixel((1, 0), (255, 255, 0, 255))
yellow_placed = tool.mask_image(4, 2, yellow_layer, 1, 1)   # a yellow pixel of a layer is LIT, and is saved as the white the game reads
check("yellow layer pixel is LIT", yellow_placed.getpixel((2, 1)), tool.LIT)
off = tool.mask_image(8, 6, layer, 7, 5)       # partly off the canvas: clipped, no error
check("layer clipped", off.getpixel((7, 5)), tool.DIAL)
check("no layer is all AUTO", tool.mask_image(4, 4, None, 0, 0).getextrema(), (0, 0))

with tempfile.TemporaryDirectory() as tmp:
    path = os.path.join(tmp, "dash_mask.png")
    tool.save_mask(placed, path)
    back = Image.open(path).convert("RGB")
    check("saved: dial is green", back.getpixel((4, 3)), (0, 255, 0))
    check("saved: lit is white", back.getpixel((6, 4)), (255, 255, 255))
    check("saved: auto is black", back.getpixel((0, 0)), (0, 0, 0))

# the optional "color" layer (dash_color.h): painted RGB kept, transparent dropped, placed and clipped like the mask, no layer is all transparent
paint = Image.new("RGBA", (3, 2), (0, 0, 0, 0))
paint.putpixel((0, 0), (255, 128, 0, 255))
paint.putpixel((1, 0), (0, 200, 255, 60))     # faint: dropped
paint.putpixel((2, 1), (0, 255, 0, 200))
colours = tool.color_image(8, 6, paint, 4, 3)
check("colour: orange at (4, 3)", colours.getpixel((4, 3)), (255, 128, 0, 255))
check("colour: faint pixel dropped", colours.getpixel((5, 3)), (0, 0, 0, 0))
check("colour: green made opaque at (6, 4)", colours.getpixel((6, 4)), (0, 255, 0, 255))
check("colour: elsewhere transparent", colours.getpixel((0, 0)), (0, 0, 0, 0))
check("colour: clipped without error", tool.color_image(8, 6, paint, 7, 5).getpixel((7, 5)), (255, 128, 0, 255))
check("colour: no layer is all transparent", tool.color_image(4, 4, None, 0, 0).getextrema()[3], (0, 0))


class FakeLayer:
    def __init__(self, name, group=()):
        self.name, self.group = name, list(group)

    def is_group(self):
        return bool(self.group)

    def __iter__(self):
        return iter(self.group)


check("layer lookup: Color found (case, spaces, in a group)", tool.find_layer([FakeLayer("a"), FakeLayer("g", [FakeLayer(" Color ")])], tool.COLOR_LAYERS).name, " Color ")
check("layer lookup: colour spelling", tool.find_layer([FakeLayer("COLOUR")], tool.COLOR_LAYERS).name, "COLOUR")
check("layer lookup: none", tool.find_layer([FakeLayer("mask")], tool.COLOR_LAYERS), None)

with tempfile.TemporaryDirectory() as tmp:
    path = os.path.join(tmp, "dash_color.png")
    colours.save(path, optimize=True)
    back = Image.open(path).convert("RGBA")
    check("colour saved: orange", back.getpixel((4, 3)), (255, 128, 0, 255))
    check("colour saved: none", back.getpixel((0, 0))[3], 0)

if failures:
    sys.exit(1)
print("test_dash_mask_psd: ok")


class FakeLayer:
    def __init__(self, name, image, left=0, top=0):
        self.name, self.image, self.left, self.top = name, image, left, top

    def is_group(self):
        return False

    def topil(self):
        return self.image


class FakePsd(list):
    width, height = 8, 6


mask_layer = Image.new("RGBA", (8, 6), (0, 0, 0, 0))
mask_layer.putpixel((1, 1), (0, 255, 0, 255))
mask_layer.putpixel((3, 2), (0, 0, 255, 255))
mask_layer.putpixel((6, 4), (0, 255, 255, 255))
wipers = Image.new("RGBA", (3, 1), (0, 0, 0, 0))
wipers.putpixel((0, 0), (200, 200, 200, 255))   # any colour but black: the layer's painting is the blade
wipers.putpixel((1, 0), (0, 0, 0, 255))         # black is nothing
wipers.putpixel((2, 0), (255, 0, 0, 40))        # nearly transparent is nothing
psd = FakePsd([FakeLayer("mask", mask_layer), FakeLayer("limpia", wipers, 3, 2)])
both = tool.psd_mask(psd)
check("the blades layer paints BLADE over the mask", both.getpixel((3, 2)), tool.BLADE)
check("black on it is nothing: the mask's own stays", both.getpixel((4, 2)), tool.AUTO)
check("a faint pixel is nothing", both.getpixel((5, 2)), tool.AUTO)
check("the mask's other marks stay", both.getpixel((1, 1)), tool.DIAL)
with tempfile.TemporaryDirectory() as tmp:
    path = os.path.join(tmp, "m.png")
    tool.save_mask(both, path)
    back = Image.open(path).convert("RGB")
    check("BLADE is saved as the red the game reads", back.getpixel((3, 2)), (255, 0, 0))
    check("and DIAL as the green", back.getpixel((1, 1)), (0, 255, 0))
    check("and NOSE as the cyan", back.getpixel((6, 4)), (0, 255, 255))
if failures:
    print("%d failures" % failures)
    sys.exit(1)
print("test_dash_mask_psd: blade layer ok")
