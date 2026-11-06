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
want = dict((name, int(value)) for name, value in re.findall(r"DASH_MASK_(AUTO|DASH|DIAL|LIT) = (\d+)", header))
failures = 0


def check(name, got, expected):
    global failures
    if got != expected:
        failures += 1
        print("FAIL %s: got %r, want %r" % (name, got, expected))


check("the tool's classes are the header's", (tool.AUTO, tool.DASH, tool.DIAL, tool.LIT), (want["AUTO"], want["DASH"], want["DIAL"], want["LIT"]))
check("black", tool.mask_class(0, 0, 0), tool.AUTO)
check("a transparent pixel", tool.mask_class(0, 255, 0, 10), tool.AUTO)
check("blue", tool.mask_class(0, 0, 255), tool.DASH)
check("green", tool.mask_class(0, 255, 0), tool.DIAL)
check("white", tool.mask_class(255, 255, 255), tool.LIT)
check("light grey", tool.mask_class(200, 200, 200), tool.LIT)
check("red is not marked", tool.mask_class(255, 0, 0), tool.AUTO)
check("a mid grey is not marked", tool.mask_class(128, 128, 128), tool.AUTO)

layer = Image.new("RGBA", (3, 2), (0, 0, 0, 0))
layer.putpixel((0, 0), (0, 255, 0, 255))
layer.putpixel((2, 1), (255, 255, 255, 255))
placed = tool.mask_image(8, 6, layer, 4, 3)   # the layer moved to (4, 3)
check("layer placed: green at (4, 3)", placed.getpixel((4, 3)), tool.DIAL)
check("layer placed: white at (6, 4)", placed.getpixel((6, 4)), tool.LIT)
check("layer placed: elsewhere AUTO", placed.getpixel((0, 0)), tool.AUTO)
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
